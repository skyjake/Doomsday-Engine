/** @file savegameselectionwidget.cpp
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

#include "ui/widgets/savegameselectionwidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/gamesessionwidget.h"
#include "clientapp.h"
#include "dd_main.h"
#include "con_main.h"

#include <de/charsymbols.h>
#include <de/SignalAction>
#include <de/SequentialLayout>
#include <de/ChildWidgetOrganizer>
#include <de/DocumentPopupWidget>
#include <de/ui/Item>

using namespace de;
using de::game::SavedSession;
using de::game::SavedSessionRepository;

DENG_GUI_PIMPL(SavegameSelectionWidget)
, DENG2_OBSERVES(App,                    StartupComplete)
, DENG2_OBSERVES(SavedSessionRepository, AvailabilityUpdate)
, DENG2_OBSERVES(ButtonWidget,           Press)
, public ChildWidgetOrganizer::IWidgetFactory
{
    /**
     * Data item with information about a saved game session.
     */
    class SavegameListItem : public ui::Item
    {
    public:
        SavegameListItem(SavedSession const &session)
        {
            setData(session.path().toLower());
            _session  = &session;
        }

        SavedSession const &savedSession() const
        {
            DENG2_ASSERT(_session != 0);
            return *_session;
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
            loadButton().setHeightPolicy(ui::Expand);
        }

        void updateFromItem(SavegameListItem const &item)
        {
            try
            {
                SavedSession const &session = item.savedSession();

                Game const &sGame = App_Games().byIdentityKey(session.metadata().gets("gameIdentityKey", ""));
                if(style().images().has(sGame.logoImageId()))
                {
                    loadButton().setImage(style().images().image(sGame.logoImageId()));
                }

                loadButton().enable(sGame.status() == Game::Loaded ||
                                    sGame.status() == Game::Complete);
                if(loadButton().isEnabled())
                {
                    loadButton().setAction(new LoadAction(session));
                }

                loadButton().setText(String(_E(b) "%1" _E(.) "\n" _E(l)_E(D) "%2")
                                         .arg(session.metadata().gets("userDescription"))
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

    bool loadWhenSelected;
    bool needUpdateFromRepository;

    Instance(Public *i)
        : Base(i)
        , loadWhenSelected(true)
        , needUpdateFromRepository(false)
    {
        self.organizer().setWidgetFactory(*this);

        App::app().audienceForStartupComplete() += this;
        ClientApp::resourceSystem().savedSessionRepository().audienceForAvailabilityUpdate() += this;
    }

    ~Instance()
    {
        App::app().audienceForStartupComplete() -= this;
        ClientApp::resourceSystem().savedSessionRepository().audienceForAvailabilityUpdate() -= this;
    }

    GuiWidget *makeItemWidget(ui::Item const & /*item*/, GuiWidget const *)
    {
        SavegameWidget *w = new SavegameWidget;
        w->loadButton().audienceForPress() += this;
        w->rule().setInput(Rule::Height, w->loadButton().rule().height());

        // Automatically close the info popup if the dialog is closed.
        //QObject::connect(thisPublic, SIGNAL(closed()), w->info, SLOT(close()));

        return w;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        SavegameWidget &w = widget.as<SavegameWidget>();
        w.updateFromItem(item.as<SavegameListItem>());

        if(!loadWhenSelected)
        {
            // Only send notification.
            w.loadButton().setAction(0);
        }
    }

    void buttonPressed(ButtonWidget &loadButton)
    {
        if(SavegameListItem const *it = self.organizer().findItemForWidget(
                    loadButton.parentWidget()->as<GuiWidget>())->maybeAs<SavegameListItem>())
        {
            DENG2_FOR_PUBLIC_AUDIENCE(Selection, i)
            {
                i->gameSelected(it->savedSession());
            }
        }

        // A load button has been pressed.
        emit self.gameSelected();
    }

    void appStartupCompleted()
    {
        // Startup resources for all games have been located.
        // We can now determine which of the saved sessions are loadable.
        for(ui::Data::Pos idx = 0; idx < self.items().size(); ++idx)
        {
            ui::Item const &item = self.items().at(idx);
            updateItemWidget(*self.organizer().itemWidget(item), item);
        }
    }

    void updateItemsFromRepository()
    {
        SavedSessionRepository const &repository = ClientApp::resourceSystem().savedSessionRepository();
        bool changed = false;

        // Remove obsolete entries.
        for(ui::Data::Pos idx = 0; idx < self.items().size(); ++idx)
        {
            String const savePath = self.items().at(idx).data().toString();
            if(!repository.find(savePath))
            {
                self.items().remove(idx--);
                changed = true;
            }
        }

        // Add new entries.
        DENG2_FOR_EACH_CONST(SavedSessionRepository::All, i, repository.all())
        {
            ui::Data::Pos found = self.items().findData(i.key());
            if(found == ui::Data::InvalidPos)
            {
                // Needs to be added.
                self.items().append(new SavegameListItem(*i.value()));
                changed = true;
            }
        }

        if(changed)
        {
            // Let others know that one or more games have appeared or disappeared from the menu.
            emit self.availabilityChanged();
        }
    }

    void repositoryAvailabilityUpdate(SavedSessionRepository const &)
    {
        if(!App::inMainThread())
        {
            // We'll have to defer the update for now.
            needUpdateFromRepository = true;
            return;
        }
        updateItemsFromRepository();
    }
};

SavegameSelectionWidget::SavegameSelectionWidget()
    : MenuWidget("savegame-selection"), d(new Instance(this))
{
    setGridSize(3, ui::Filled, 0, ui::Expand);
    d->needUpdateFromRepository = true;
}

void SavegameSelectionWidget::setLoadGameWhenSelected(bool enableLoad)
{
    d->loadWhenSelected = enableLoad;
}

void SavegameSelectionWidget::setColumns(int numberOfColumns)
{
    if(layout().maxGridSize().x != numberOfColumns)
    {
        setGridSize(numberOfColumns, ui::Filled, 0, ui::Expand);
    }
}

void SavegameSelectionWidget::update()
{
    if(d->needUpdateFromRepository)
    {
        d->needUpdateFromRepository = false;
        d->updateItemsFromRepository();
    }
    MenuWidget::update();
}

SavedSession const &SavegameSelectionWidget::savedSession(ui::DataPos pos) const
{
    DENG2_ASSERT(pos < items().size());
    return items().at(pos).as<Instance::SavegameListItem>().savedSession();
}

DENG2_PIMPL_NOREF(SavegameSelectionWidget::LoadAction)
{
    String gameId;
    String cmd;
};

SavegameSelectionWidget::LoadAction::LoadAction(SavedSession const &session)
    : d(new Instance)
{
    d->gameId = session.metadata().gets("gameIdentityKey");
    d->cmd = "loadgame " + session.name().fileNameWithoutExtension() + " confirm";
}

void SavegameSelectionWidget::LoadAction::trigger()
{
    Action::trigger();

    BusyMode_FreezeGameForBusyMode();
    ClientWindow::main().taskBar().close();

    App_ChangeGame(App_Games().byIdentityKey(d->gameId), false /*no reload*/);
    Con_Execute(CMDS_DDAY, d->cmd.toLatin1(), false, false);
}
