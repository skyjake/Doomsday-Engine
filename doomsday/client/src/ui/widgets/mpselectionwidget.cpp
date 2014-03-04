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
                    loadButton().setAction(new JoinAction(sv));
                }

                loadButton().setText(String(_E(1) "%1 " _E(.)_E(2) "(%5/%6)" _E(.) " "DENG2_CHAR_MDASH" %2"
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

    ServerLink::FoundMask mask;
    bool joinWhenSelected;

    Instance(Public *i)
        : Base(i)
        , mask(ServerLink::Any)
        , joinWhenSelected(true)
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
        w->loadButton().audienceForPress() += this;
        w->rule().setInput(Rule::Height, w->loadButton().rule().height());

        // Automatically close the info popup if the dialog is closed.
        //QObject::connect(thisPublic, SIGNAL(closed()), w->info, SLOT(close()));

        return w;
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &item)
    {
        ServerWidget &sv = widget.as<ServerWidget>();
        sv.updateFromItem(item.as<ServerListItem>());

        if(!joinWhenSelected)
        {
            // Only send notification.
            sv.loadButton().setAction(0);
        }
    }

    void buttonPressed(ButtonWidget &loadButton)
    {
        if(ServerListItem const *it = self.organizer().findItemForWidget(
                    loadButton.parentWidget()->as<GuiWidget>())->maybeAs<ServerListItem>())
        {
            DENG2_FOR_PUBLIC_AUDIENCE(Selection, i)
            {
                i->gameSelected(it->info());
            }
        }

        // A load button has been pressed.
        emit self.gameSelected();
    }

    void linkDiscoveryUpdate(ServerLink const &link)
    {
        bool changed = false;

        // Remove obsolete entries.
        for(ui::Data::Pos idx = 0; idx < self.items().size(); ++idx)
        {
            String const id = self.items().at(idx).data().toString();
            if(!link.isFound(Address::parse(id), mask))
            {
                self.items().remove(idx--);
                changed = true;
            }
        }

        // Add new entries and update existing ones.
        foreach(de::Address const &host, link.foundServers(mask))
        {
            serverinfo_t info;
            if(!link.foundServerInfo(host, &info, mask)) continue;

            ui::Data::Pos found = self.items().findData(hostId(info));
            if(found == ui::Data::InvalidPos)
            {
                // Needs to be added.
                self.items().append(new ServerListItem(info));
                changed = true;
            }
            else
            {
                // Update the info.
                self.items().at(found).as<ServerListItem>().setInfo(info);
            }
        }

        if(changed)
        {
            // Let others know that one or more games have appeared or disappeared
            // from the menu.
            emit self.availabilityChanged();
        }
    }
};

MPSelectionWidget::MPSelectionWidget(DiscoveryMode discovery)
    : MenuWidget("mp-selection"), d(new Instance(this))
{
    setGridSize(3, ui::Filled, 0, ui::Expand);

    switch(discovery)
    {
    case DiscoverUsingMaster:
        d->link().discoverUsingMaster();
        break;

    case DirectDiscoveryOnly:
        // Only show servers found via direct connection.
        d->mask = ServerLink::Direct;
        break;

    default:
        break;
    }
}

void MPSelectionWidget::setJoinGameWhenSelected(bool enableJoin)
{
    d->joinWhenSelected = enableJoin;
}

void MPSelectionWidget::setColumns(int numberOfColumns)
{
    if(layout().maxGridSize().x != numberOfColumns)
    {
        setGridSize(numberOfColumns, ui::Filled, 0, ui::Expand);
    }
}

serverinfo_t const &MPSelectionWidget::serverInfo(ui::DataPos pos) const
{
    DENG2_ASSERT(pos < items().size());
    return items().at(pos).as<Instance::ServerListItem>().info();
}

DENG2_PIMPL_NOREF(MPSelectionWidget::JoinAction)
{
    String gameId;
    String cmd;
};

MPSelectionWidget::JoinAction::JoinAction(serverinfo_t const &sv)
    : d(new Instance)
{
    d->gameId = sv.gameIdentityKey;
    d->cmd = String("connect %1 %2").arg(sv.address).arg(sv.port);
}

void MPSelectionWidget::JoinAction::trigger()
{
    Action::trigger();

    BusyMode_FreezeGameForBusyMode();
    ClientWindow::main().taskBar().close();

    App_ChangeGame(App_Games().byIdentityKey(d->gameId), false /*no reload*/);
    Con_Execute(CMDS_DDAY, d->cmd.toLatin1(), false, false);
}
