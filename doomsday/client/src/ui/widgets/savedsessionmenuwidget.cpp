/** @file savedsessionmenuwidget.cpp
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/widgets/savedsessionmenuwidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/gamesessionwidget.h"
#include "clientapp.h"
#include "ui/clientwindow.h"
#include "dd_main.h"

#include <doomsday/console/exec.h>
#include <de/game/Session>
#include <de/SignalAction>
#include <de/SequentialLayout>
#include <de/DocumentPopupWidget>
#include <de/ui/SubwidgetItem>

using namespace de;
using de::game::Session;
using de::game::SavedSession;

DENG_GUI_PIMPL(SavedSessionMenuWidget)
, DENG2_OBSERVES(App,                 StartupComplete)
, DENG2_OBSERVES(Session::SavedIndex, AvailabilityUpdate)
, DENG2_OBSERVES(Loop,                Iteration) // deferred refresh
{
    /**
     * Action for loading a saved session.
     */
    class LoadAction : public de::Action
    {
        String gameId;
        String cmd;

    public:
        LoadAction(SavedSession const &session)
        {
            gameId = session.metadata().gets("gameIdentityKey");
            cmd    = "loadgame " + session.name().fileNameWithoutExtension() + " confirm";
        }

        void trigger()
        {
            Action::trigger();

            BusyMode_FreezeGameForBusyMode();
            ClientWindow::main().taskBar().close();

            App_ChangeGame(App_Games().byIdentityKey(gameId), false /*no reload*/);
            Con_Execute(CMDS_DDAY, cmd.toLatin1(), false, false);
        }
    };

    /**
     * Data item with information about a saved game session.
     */
    class SavegameListItem : public ui::Item, public SessionItem
    {
    public:
        SavegameListItem(SavedSession const &session, SessionMenuWidget &owner)
            : SessionItem(owner)
        {
            setData(session.path().toLower());
            _session  = &session;
        }

        SavedSession const &savedSession() const
        {
            DENG2_ASSERT(_session != 0);
            return *_session;
        }

        String title() const
        {
            return savedSession().metadata().gets("userDescription");
        }

        String gameIdentityKey() const
        {
            return savedSession().metadata().gets("gameIdentityKey");
        }

    private:
        SavedSession const *_session;
    };

    /**
     * Widget representing a SavegameItem in the dialog's menu.
     */
    struct SavegameWidget : public GameSessionWidget
    {
        /// Action that deletes a savegame folder.
        struct DeleteAction : public Action {
            SavegameWidget *widget;
            String savePath;

            DeleteAction(SavegameWidget *wgt, String const &path)
                : widget(wgt), savePath(path) {}
            void trigger() {
                widget->menu().close(0);
                App::rootFolder().removeFile(savePath);
                App::fileSystem().refresh();
            }
        };

        SavegameWidget() : GameSessionWidget(PopupMenu, ui::Up)
        {
            loadButton().disable();

            // Show all available information without clipping.
            loadButton().setHeightPolicy(ui::Expand);
        }

        void updateFromItem(SavegameListItem const &item)
        {
            try
            {
                SavedSession const &session = item.savedSession();

                Game const &sGame = App_Games().byIdentityKey(item.gameIdentityKey());
                if(style().images().has(sGame.logoImageId()))
                {
                    loadButton().setImage(style().images().image(sGame.logoImageId()));
                }

                loadButton().disable(sGame.status() == Game::Incomplete);

                loadButton().setText(String(_E(l)_E(F) "%1" _E(.)_E(.) "\n%2")
                                         .arg(sGame.identityKey())
                                         .arg(item.title()));

                menuButton().setImage(style().images().image("close.ringless"));
                menuButton().setImageScale(.75f);
                menuButton().setImageColor(style().colors().colorf("altaccent"));

                // Metadata.
                document().setText(session.metadata().asStyledText() + "\n" +
                                   String(_E(D)_E(b) "Resource: " _E(.)_E(.)_E(C) "\"%1\"\n" _E(.)
                                          _E(l) "Modified: " _E(.) "%2")
                                   .arg(session.path())
                                   .arg(session.status().modifiedAt.asText(Time::FriendlyFormat)));

                // Actions to be done on the saved game.
                menu().items()
                        << new ui::Item(ui::Item::Separator, tr("Really delete the savegame?"))
                        << new ui::ActionItem(tr("Delete Savegame"), new DeleteAction(this, session.path()))
                        << new ui::ActionItem(tr("Cancel"), new SignalAction(&menu(), SLOT(close())));
            }
            catch(Error const &)
            {
                /// @todo
            }
        }
    };

    Instance(Public *i) : Base(i)
    {
        App::app().audienceForStartupComplete() += this;
        game::Session::savedIndex().audienceForAvailabilityUpdate() += this;
    }

    ~Instance()
    {
        Loop::get().audienceForIteration() -= this;
        App::app().audienceForStartupComplete() -= this;
        game::Session::savedIndex().audienceForAvailabilityUpdate() -= this;
    }

    void appStartupCompleted()
    {
        // Startup resources for all games have been located.
        // We can now determine which of the saved sessions are loadable.
        for(ui::Data::Pos idx = 0; idx < self.items().size(); ++idx)
        {
            ui::Item const &item = self.items().at(idx);
            self.updateItemWidget(*self.organizer().itemWidget(item), item);
        }

        emit self.availabilityChanged();
    }

    QSet<String> pendingDismiss;

    void updateItemsFromSavedIndex()
    {
        bool changed = false;

        // Remove obsolete entries.
        for(ui::Data::Pos idx = 0; idx < self.items().size(); ++idx)
        {
            String const savePath = self.items().at(idx).data().toString();
            if(!Session::savedIndex().find(savePath))
            {
                if(!pendingDismiss.contains(savePath))
                {
                    // Make this item disappear after a delay.
                    pendingDismiss.insert(savePath);

                    auto &wgt = self.itemWidget<GuiWidget>(self.items().at(idx));
                    wgt.setBehavior(DisableEventDispatch); // can't trigger any more
                    wgt.setOpacity(0, .5f);
                }
            }
        }

        // Add new entries.
        DENG2_FOR_EACH_CONST(Session::SavedIndex::All, i, Session::savedIndex().all())
        {
            ui::Data::Pos found = self.items().findData(i.key());
            if(found == ui::Data::InvalidPos)
            {
                SavedSession &session = *i.value();
                if(session.path().beginsWith("/home/savegames")) // Ignore non-user savegames.
                {
                    // Needs to be added.
                    self.items().append(new SavegameListItem(session, self));
                    changed = true;
                }
            }
        }

        if(changed)
        {
            // Let others know that one or more games have appeared or disappeared from the menu.
            emit self.availabilityChanged();
        }
    }

    void removeDismissedItems()
    {
        if(pendingDismiss.isEmpty()) return;

        bool changed = false;

        QMutableSetIterator<String> iter(pendingDismiss);
        while(iter.hasNext())
        {
            String const path = iter.next();
            ui::DataPos idx = self.items().findData(path);
            if(idx == ui::Data::InvalidPos)
            {
                iter.remove(); // It's already gone?
                continue;
            }
            auto &wgt = self.itemWidget<GuiWidget>(self.items().at(idx));
            if(fequal(wgt.opacity(), 0.f))
            {
                // Time to erase this item.
                self.items().remove(idx);
                changed = true;
            }
        }

        if(changed)
        {
            // Let others know that one or more games have appeared or disappeared from the menu.
            emit self.availabilityChanged();
        }
    }

    void savedIndexAvailabilityUpdate(Session::SavedIndex const &)
    {
        if(!App::inMainThread())
        {
            // We'll have to defer the update for now.
            deferUpdate();
            return;
        }
        updateItemsFromSavedIndex();
    }

    void deferUpdate()
    {
        Loop::get().audienceForIteration() += this;
    }

    void loopIteration()
    {
        Loop::get().audienceForIteration() -= this;
        updateItemsFromSavedIndex();
    }
};

SavedSessionMenuWidget::SavedSessionMenuWidget()
    : SessionMenuWidget("savegame-session-menu"), d(new Instance(this))
{
    d->updateItemsFromSavedIndex();
}

void SavedSessionMenuWidget::update()
{
    SessionMenuWidget::update();
    d->removeDismissedItems();
}

Action *SavedSessionMenuWidget::makeAction(ui::Item const &item)
{
    return new Instance::LoadAction(item.as<Instance::SavegameListItem>().savedSession());
}

GuiWidget *SavedSessionMenuWidget::makeItemWidget(ui::Item const &, GuiWidget const *)
{
    return new Instance::SavegameWidget;
}

void SavedSessionMenuWidget::updateItemWidget(GuiWidget &widget, ui::Item const &item)
{
    Instance::SavegameWidget &w = widget.as<Instance::SavegameWidget>();
    w.updateFromItem(item.as<Instance::SavegameListItem>());
}
