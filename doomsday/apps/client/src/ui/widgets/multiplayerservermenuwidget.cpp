/** @file multiplayerservermenuwidget.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/multiplayerservermenuwidget.h"
#include "ui/home/multiplayerpanelbuttonwidget.h"
#include "ui/widgets/homemenuwidget.h"
#include "network/serverlink.h"
#include "clientapp.h"

#include <doomsday/games.h>
#include <de/menuwidget.h>
#include <de/address.h>
#include <de/textvalue.h>

using namespace de;

DE_PIMPL(MultiplayerServerMenuWidget)
, DE_OBSERVES(DoomsdayApp, GameChange)
, DE_OBSERVES(Games, Readiness)
, DE_OBSERVES(ServerLink, Discovery)
, DE_OBSERVES(MultiplayerPanelButtonWidget, AboutToJoin)
, public ChildWidgetOrganizer::IWidgetFactory
{
    static ServerLink &link() { return ClientApp::serverLink(); }

    static String hostId(const ServerInfo &sv)
    {
        if (sv.serverId())
        {
            return String::format("%x", sv.serverId());
        }
        return sv.address().asText();
    }

    /**
     * Data item with information about a found server.
     */
    class ServerListItem : public ui::Item
    {
    public:
        ServerListItem(const ServerInfo &serverInfo, bool isLocal)
            : _lan(isLocal)
        {
            setData(TextValue(hostId(serverInfo)));
            _info = serverInfo;
        }

        bool isLocal() const
        {
            return _lan;
        }

        void setLocal(bool isLocal)
        {
            _lan = isLocal;
        }

        const ServerInfo &info() const
        {
            return _info;
        }

        void setInfo(const ServerInfo &serverInfo)
        {
            _info = serverInfo;
            notifyChange();
        }

        String title() const
        {
            return _info.name();
        }

        String gameId() const
        {
            return _info.gameId();
        }

    private:
        ServerInfo _info;
        bool _lan;
    };

    DiscoveryMode                mode = NoDiscovery;
    ServerLink::FoundMask        mask = ServerLink::Any;

    Impl(Public *i) : Base(i)
    {
        DoomsdayApp::app().audienceForGameChange()  += this;
        DoomsdayApp::games().audienceForReadiness() += this;
        link().audienceForDiscovery() += this;

        self().organizer().setWidgetFactory(*this);
    }

    void serversDiscovered(const ServerLink &link) override
    {
        DE_ASSERT_IN_MAIN_THREAD()
        self().root().window().glActivate();

        ui::Data &items = self().items();

        Set<String> foundHosts;
        for (const Address &host : link.foundServers(mask))
        {
            ServerInfo info;
            if (link.foundServerInfo(host, info, mask))
            {
                foundHosts.insert(hostId(info));
            }
        }

        // Remove obsolete entries.
        for (ui::Data::Pos idx = 0; idx < items.size(); ++idx)
        {
            const String id = items.at(idx).data().asText();
            if (!foundHosts.contains(id))
            {
                items.remove(idx--);
            }
        }

        // Add new entries and update existing ones.
        for (const Address &host : link.foundServers(mask))
        {
            ServerInfo info;
            if (!link.foundServerInfo(host, info, mask)) continue;

            ui::Data::Pos found   = items.findData(TextValue(hostId(info)));
            const bool    isLocal = link.isServerOnLocalNetwork(info.address());

            if (found == ui::Data::InvalidPos)
            {
                // Needs to be added.
                items.append(new ServerListItem(info, isLocal));
            }
            else
            {
                // Update the info of an existing item.
                auto &it = items.at(found).as<ServerListItem>();

                // Prefer the info received via LAN, if the server is the same instance.
                if (!it.isLocal() || isLocal)
                {
                    it.setInfo(info);
                    it.setLocal(isLocal);
                }
            }
        }

        items.stableSort([] (const ui::Item &a, const ui::Item &b)
        {
            const auto &first  = a.as<ServerListItem>();
            const auto &second = b.as<ServerListItem>();

            // LAN games shown first.
            if (first.isLocal() == second.isLocal())
            {
                // Sort by number of players.
                if (first.info().playerCount() == second.info().playerCount())
                {
                    // Finally, by game ID.
                    int cmp = first.info().gameId().compareWithCase(second.info().gameId());
                    if (!cmp)
                    {
                        // Lastly by server name.
                        return first.info().name().compareWithoutCase(second.info().name()) < 0;
                    }
                    return cmp < 0;
                }
                return first.info().playerCount() - second.info().playerCount() > 0;
            }
            return first.isLocal();
        });
    }

    void currentGameChanged(const Game &newGame) override
    {
        if (newGame.isNull() && mode == DiscoverUsingMaster)
        {
            // If the session menu exists across game changes, it's good to
            // keep it up to date.
            link().discoverUsingMaster();
        }
    }

    void gameReadinessUpdated() override
    {
        for (GuiWidget *w : self().childWidgets())
        {
            updateAvailability(*w);
        }
    }

    void updateAvailability(GuiWidget &menuItemWidget)
    {
        const auto &item = self().organizer().findItemForWidget(menuItemWidget)->as<ServerListItem>();

        bool playable = false;
        String gameId = item.gameId();
        if (DoomsdayApp::games().contains(gameId))
        {
            playable = DoomsdayApp::games()[gameId].isPlayable();
        }
        menuItemWidget.enable(playable);
    }

    void aboutToJoinMultiplayerGame(const ServerInfo &sv) override
    {
        DE_NOTIFY_PUBLIC(AboutToJoin, i) i->aboutToJoinMultiplayerGame(sv);
    }

//- ChildWidgetOrganizer::IWidgetFactory --------------------------------------

    GuiWidget *makeItemWidget(const ui::Item &, const GuiWidget *) override
    {
        auto *b = new MultiplayerPanelButtonWidget;
        b->audienceForAboutToJoin() += this;
        return b;
    }

    void updateItemWidget(GuiWidget &widget, const ui::Item &item) override
    {
        const auto &serverItem = item.as<ServerListItem>();

        widget.as<MultiplayerPanelButtonWidget>()
                .updateContent(serverItem.info());

        // Is it playable?
        updateAvailability(widget);
    }

    DE_PIMPL_AUDIENCE(AboutToJoin)
};

DE_AUDIENCE_METHOD(MultiplayerServerMenuWidget, AboutToJoin);

MultiplayerServerMenuWidget::MultiplayerServerMenuWidget(DiscoveryMode discovery,
                                                         const String &name)
    : HomeMenuWidget(name)
    , d(new Impl(this))
{
    d->mode = discovery;

    switch (d->mode)
    {
    case DiscoverUsingMaster:
        d->mask = ServerLink::LocalNetwork | ServerLink::MasterServer;
        d->link().discoverUsingMaster();
        break;

    case DirectDiscoveryOnly:
        d->mask = ServerLink::Direct;
        break;

    case NoDiscovery:
        break;
    }
}
