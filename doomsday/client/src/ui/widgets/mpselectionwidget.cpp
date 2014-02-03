/** @file mpselectionwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/mpselectionwidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/gamesessionwidget.h"
#include "network/serverlink.h"
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

DENG_GUI_PIMPL(MPSelectionWidget)
, DENG2_OBSERVES(ServerLink, DiscoveryUpdate)
, DENG2_OBSERVES(ButtonWidget, Press)
, public ChildWidgetOrganizer::IWidgetFactory
{
    static ServerLink &link() { return ClientApp::serverLink(); }
    static String hostId(serverinfo_t const &sv)  {
        return String("%1:%2").arg(sv.address).arg(sv.port);
    }

    /**
     * Data item with information about a found server.
     */
    class ServerListItem : public ui::Item
    {
    public:
        ServerListItem(serverinfo_t const &serverInfo)
        {
            setData(hostId(serverInfo));
            std::memcpy(&_info, &serverInfo, sizeof(serverInfo));
        }

        serverinfo_t const &info() const
        {
            return _info;
        }

        void setInfo(serverinfo_t const &serverInfo)
        {
            std::memcpy(&_info, &serverInfo, sizeof(serverInfo));
            notifyChange();
        }

    private:
        serverinfo_t _info;
    };

    /**
     * Widget representing a ServerListItem in the dialog's menu.
     */
    struct ServerWidget : public GameSessionWidget
    {
        struct JoinAction : public Action
        {
        public:
            JoinAction(serverinfo_t const &sv, ButtonWidget &owner)
                : _owner(&owner)
            {
                _gameId = sv.gameIdentityKey;
                _cmd = String("connect %1 %2").arg(sv.address).arg(sv.port);
            }

            void trigger()
            {
                Action::trigger();

                BusyMode_FreezeGameForBusyMode();

                // Closing the taskbar will cause this action to be deleted. Let's take
                // ownership of the action so we can delete after we're done.
                _owner->takeAction();

                ClientWindow::main().taskBar().close();

                App_ChangeGame(App_Games().byIdentityKey(_gameId), false /*no reload*/);
                Con_Execute(CMDS_DDAY, _cmd.toLatin1(), false, false);

                delete this;
            }

            Action *duplicate() const
            {
                DENG2_ASSERT(!"JoinAction: cannot duplicate");
                return 0;
            }

        private:
            ButtonWidget *_owner;
            String _gameId;
            String _cmd;
        };

        ServerWidget()
        {
            loadButton().disable();
            loadButton().setHeightPolicy(ui::Expand);
        }

        void updateFromItem(ServerListItem const &item)
        {
            try
            {
                Game const &svGame = App_Games().byIdentityKey(item.info().gameIdentityKey);

                if(style().images().has(svGame.logoImageId()))
                {
                    loadButton().setImage(style().images().image(svGame.logoImageId()));
                }

                serverinfo_t const &sv = item.info();

                loadButton().enable(sv.canJoin);
                if(sv.canJoin)
                {
                    loadButton().setAction(new JoinAction(sv, loadButton()));
                }

                loadButton().setText(String(_E(1) "%1 " _E(.)_E(2) "(%5/%6)" _E(.) "\n%2"
                                            _E(D)_E(l) "\n%7 %4")
                               .arg(sv.name)
                               .arg(svGame.title())
                               .arg(sv.gameConfig)
                               .arg(sv.numPlayers)
                               .arg(sv.maxPlayers)
                               .arg(sv.map));

                // Extra information.
                document().setText(ServerInfo_AsStyledText(&sv));
            }
            catch(Error const &)
            {
                /// @todo
            }
        }
    };

    Instance(Public *i) : Base(i)
    {
        self.organizer().setWidgetFactory(*this);
        link().audienceForDiscoveryUpdate += this;
    }

    ~Instance()
    {
        link().audienceForDiscoveryUpdate -= this;
    }

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        ServerWidget *w = new ServerWidget;
        w->loadButton().audienceForPress += this;
        w->rule().setInput(Rule::Height, w->loadButton().rule().height());

        // Automatically close the info popup if the dialog is closed.
        //QObject::connect(thisPublic, SIGNAL(closed()), w->info, SLOT(close()));

        return w;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        widget.as<ServerWidget>().updateFromItem(item.as<ServerListItem>());
    }

    void buttonPressed(ButtonWidget &)
    {
        // A load button has been pressed.
        emit self.gameSelected();
    }

    void linkDiscoveryUpdate(ServerLink const &link)
    {
        // Remove obsolete entries.
        for(ui::Data::Pos idx = 0; idx < self.items().size(); ++idx)
        {
            String const id = self.items().at(idx).data().toString();
            if(!link.isFound(Address::parse(id)))
            {
                self.items().remove(idx--);
            }
        }

        // Add new entries and update existing ones.
        foreach(de::Address const &host, link.foundServers())
        {
            serverinfo_t info;
            if(!link.foundServerInfo(host, &info)) continue;

            ui::Data::Pos found = self.items().findData(hostId(info));
            if(found == ui::Data::InvalidPos)
            {
                // Needs to be added.
                self.items().append(new ServerListItem(info));
            }
            else
            {
                // Update the info.
                self.items().at(found).as<ServerListItem>().setInfo(info);
            }
        }
    }

    /*
    void updateLayoutForWidth(int width)
    {
        // If the view is too small, we'll want to reduce the number of items in the menu.
        int const maxWidth = style().rules().rule("mpselection.max.width").valuei();

        qDebug() << maxWidth << width;

        int suitable = clamp(1, 3 * width / maxWidth, 3);
    }*/
};

MPSelectionWidget::MPSelectionWidget()
    : MenuWidget("mp-selection"), d(new Instance(this))
{
    setGridSize(3, ui::Filled, 0, ui::Expand);
}

void MPSelectionWidget::setColumns(int numberOfColumns)
{
    if(layout().maxGridSize().x != numberOfColumns)
    {
        setGridSize(numberOfColumns, ui::Filled, 0, ui::Expand);
    }
}
/*
void MPSelectionWidget::update()
{
    MenuWidget::update();

    Rectanglei rect;
    if(hasChangedPlace(rect))
    {
        d->updateLayoutForWidth(rect.width());
    }
}
*/
