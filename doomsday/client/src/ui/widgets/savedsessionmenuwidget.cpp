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
#include <de/ui/Item>

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
        SavegameWidget()
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

                loadButton().setText(String(_E(b) "%1" _E(.) "\n" _E(l)_E(D) "%2")
                                         .arg(item.title())
                                         .arg(sGame.identityKey()));

                // Extra information.
                document().setText(session.metadata().asStyledText() + "\n" +
                                   String(_E(D) "Resource: " _E(.)_E(i) "\"%1\"\n" _E(.)
                                          _E(l) "Modified: " _E(.)_E(i) "%2")
                                        .arg(session.path())
                                        .arg(session.status().modifiedAt.asText(Time::FriendlyFormat)));
            }
            catch(Error const &)
            {
                /// @todo
            }
        }
    };

    Instance(Public *i)
        : Base(i)
    {
        App::app().audienceForStartupComplete() += this;
        game::Session::savedIndex().audienceForAvailabilityUpdate() += this;
    }

    ~Instance()
    {
        Loop::appLoop().audienceForIteration() -= this;
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

    void updateItemsFromSavedIndex()
    {
        bool changed = false;

        // Remove obsolete entries.
        for(ui::Data::Pos idx = 0; idx < self.items().size(); ++idx)
        {
            String const savePath = self.items().at(idx).data().toString();
            if(!Session::savedIndex().find(savePath))
            {
                self.items().remove(idx--);
                changed = true;
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
        Loop::appLoop().audienceForIteration() += this;
    }

    void loopIteration()
    {
        Loop::appLoop().audienceForIteration() -= this;
        updateItemsFromSavedIndex();
    }
};

SavedSessionMenuWidget::SavedSessionMenuWidget()
    : SessionMenuWidget("savegame-session-menu"), d(new Instance(this))
{
    d->updateItemsFromSavedIndex();
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
