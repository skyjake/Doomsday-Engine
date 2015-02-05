/** @file mpsessionmenuwidget.cpp
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

#include "ui/widgets/mpsessionmenuwidget.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/gamesessionwidget.h"
#include "network/serverlink.h"
#include "clientapp.h"
#include "ui/clientwindow.h"
#include "dd_main.h"

#include <doomsday/console/exec.h>
#include <de/charsymbols.h>
#include <de/SignalAction>
#include <de/SequentialLayout>
#include <de/DocumentPopupWidget>
#include <de/ui/Item>
#include <de/IndirectRule>

using namespace de;

DENG_GUI_PIMPL(MPSessionMenuWidget)
, DENG2_OBSERVES(App, GameChange)
, DENG2_OBSERVES(ServerLink, DiscoveryUpdate)
{
    static ServerLink &link() { return ClientApp::serverLink(); }
    static String hostId(serverinfo_t const &sv)  {
        return String("%1:%2").arg(sv.address).arg(sv.port);
    }

    /**
     * Action for joining a game on a multiplayer server.
     */
    class JoinAction : public de::Action
    {
        String gameId;
        String cmd;

    public:
        JoinAction(serverinfo_t const &sv)
        {
            gameId = sv.gameIdentityKey;
            cmd    = String("connect %1 %2").arg(sv.address).arg(sv.port);
        }

        void trigger()
        {
            Action::trigger();

            BusyMode_FreezeGameForBusyMode();
            ClientWindow::main().taskBar().close();

            // Automatically leave the current MP game.
            if(netGame && isClient)
            {
                ClientApp::serverLink().disconnect();
            }

            App_ChangeGame(App_Games().byIdentityKey(gameId), false /*no reload*/);
            Con_Execute(CMDS_DDAY, cmd.toLatin1(), false, false);
        }
    };

    /**
     * Data item with information about a found server.
     */
    class ServerListItem : public ui::Item, public SessionItem
    {
    public:
        ServerListItem(serverinfo_t const &serverInfo, SessionMenuWidget &owner)
            : SessionItem(owner)
        {
            setData(hostId(serverInfo));
            _info = serverInfo;
        }

        serverinfo_t const &info() const
        {
            return _info;
        }

        void setInfo(serverinfo_t const &serverInfo)
        {
            _info = serverInfo;
            notifyChange();
        }

        String title() const
        {
            return _info.name;
        }

        String gameIdentityKey() const
        {
            return _info.gameIdentityKey;
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

                loadButton().enable(sv.canJoin &&
                                    sv.version == DOOMSDAY_VERSION &&
                                    svGame.allStartupFilesFound());

                loadButton().setText(String(_E(F)_E(s) "%2\n" _E(.)_E(.)_E(C) "(%4) " _E(.)
                                            _E(1)_E(>) "%1" _E(.)_E(<)_E(D)_E(l) "\n%5 %3")
                               .arg(sv.name)
                               .arg(svGame.title())
                               .arg(sv.gameConfig)
                               .arg(sv.numPlayers)
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

    DiscoveryMode mode;
    ServerLink::FoundMask mask;
    IndirectRule *maxHeightRule = new IndirectRule;

    Instance(Public *i)
        : Base(i)
        , mask(ServerLink::Any)
    {
        link().audienceForDiscoveryUpdate += this;
        App::app().audienceForGameChange() += this;
    }

    ~Instance()
    {
        releaseRef(maxHeightRule);
        link().audienceForDiscoveryUpdate -= this;
        App::app().audienceForGameChange() -= this;
    }

    /**
     * Puts together a rule that determines the tallest load button of those present
     * in the menu. This will be used to size all the buttons uniformly.
     */
    void updateItemMaxHeight()
    {
        // Form a rule that is the maximum of all load button heights.
        Rule const *maxHgt = nullptr;
        foreach(Widget *w, self.childWidgets())
        {
            if(ServerWidget *sw = w->maybeAs<ServerWidget>())
            {
                auto const &itemHeight = sw->loadButton().contentHeight();
                if(!maxHgt)
                {
                    maxHgt = holdRef(itemHeight);
                }
                else
                {
                    changeRef(maxHgt, OperatorRule::maximum(*maxHgt, itemHeight));
                }
            }
        }

        if(maxHgt)
        {
            maxHeightRule->setSource(*maxHgt);
        }
        else
        {
            maxHeightRule->unsetSource();
        }
        releaseRef(maxHgt);
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
                self.items().append(new ServerListItem(info, self));
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
            updateItemMaxHeight();
            self.sort();

            // Let others know that one or more games have appeared or disappeared
            // from the menu.
            emit self.availabilityChanged();
        }
    }

    void currentGameChanged(game::Game const &newGame)
    {
        if(newGame.isNull() && mode == DiscoverUsingMaster)
        {
            // If the session menu exists across game changes, it's good to
            // keep it up to date.
            link().discoverUsingMaster();
        }
    }
};

MPSessionMenuWidget::MPSessionMenuWidget(DiscoveryMode discovery)
    : SessionMenuWidget("mp-session-menu"), d(new Instance(this))
{
    d->mode = discovery;

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

Action *MPSessionMenuWidget::makeAction(ui::Item const &item)
{
    return new Instance::JoinAction(item.as<Instance::ServerListItem>().info());
}

GuiWidget *MPSessionMenuWidget::makeItemWidget(ui::Item const &, GuiWidget const *)
{
    auto *sw = new Instance::ServerWidget;
    sw->rule().setInput(Rule::Height, *d->maxHeightRule);
    return sw;
}

void MPSessionMenuWidget::updateItemWidget(GuiWidget &widget, ui::Item const &item)
{
    Instance::ServerWidget &sv = widget.as<Instance::ServerWidget>();
    sv.updateFromItem(item.as<Instance::ServerListItem>());
}
