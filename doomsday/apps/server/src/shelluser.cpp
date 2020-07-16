/** @file shelluser.cpp  Remote user of a shell connection.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#include "shelluser.h"

#include "api_console.h"
#include "dd_main.h"
#include "network/net_main.h"
#include "world/p_object.h"
#include "world/p_players.h"

#include <doomsday/console/exec.h>
#include <doomsday/console/knownword.h>
#include <doomsday/games.h>
#include <doomsday/network/protocol.h>
#include <doomsday/world/map.h>
#include <de/lexicon.h>
#include <de/log.h>
#include <de/logbuffer.h>
#include <de/logsink.h>
#include <de/loop.h>

using namespace de;

DE_PIMPL(ShellUser), public LogSink
{
    /// Log entries to be sent are collected here.
    LockableT<network::LogEntryPacket> logEntryPacket;

    Impl(Public &i) : Base(i)
    {
        // We will send all log entries to a shell user.
        LogBuffer::get().addSink(*this);
    }

    ~Impl()
    {
        LogBuffer::get().removeSink(*this);
    }

    LogSink &operator << (const LogEntry &entry)
    {
        DE_GUARD(logEntryPacket);
        logEntryPacket.value.add(entry);
        return *this;
    }

    LogSink &operator << (const String &)
    {
        return *this;
    }

    /**
     * Sends the accumulated log entries over the link.
     * Note that any thread can flush the log sinks.
     */
    void flush()
    {
        Loop::mainCall([this] ()
        {
            DE_GUARD(logEntryPacket);
            if (!logEntryPacket.value.isEmpty() && self().status() == network::Link::Connected)
            {
                self() << logEntryPacket.value;
                logEntryPacket.value.clear();
            }
        });
    }
};

ShellUser::ShellUser(Socket *socket) : network::Link(socket), d(new Impl(*this))
{
    audienceForDisconnected() += [this]()
    {
        DE_NOTIFY_VAR(Disconnect, i)
        {
            i->userDisconnected(*this);
        }
    };
    audienceForPacketsReady() += [this](){ handleIncomingPackets(); };
}

void ShellUser::sendInitialUpdate()
{
    // Console lexicon.
    std::unique_ptr<RecordPacket> packet(protocol().newConsoleLexicon(Con_Lexicon()));
    *this << *packet;

    sendGameState();
    sendMapOutline();
    sendPlayerInfo();
}

void ShellUser::sendGameState()
{
    String mode = App_CurrentGame().id();

    /**
     * @todo The server is not the right place to compose a packet about
     * game state. Work needed:
     * - World class that contains the game world as a whole
     * - WorldFactory that produces world and map related instances
     * - Game plugins can extend the world with their own code (games can
     *   provide a Factory of their own for constructing world/map instances)
     *
     * The server should just ask the World for the information for the game
     * state packet.
     */

    String rules = reinterpret_cast<const char *>(gx.GetPointer(DD_GAME_CONFIG));

    // Check the map's information.
    String mapId;
    String mapTitle;
    if (world::World::get().hasMap())
    {
        world::Map &map = App_World().map();

        mapId = (map.hasManifest()? map.manifest().composeUri().path() : "(unknown map)");

        /// @todo A cvar is not an appropriate place to ask for this --
        /// should be moved to the Map class.
        mapTitle = Con_GetString("map-name");
    }

    std::unique_ptr<RecordPacket> packet(protocol().newGameState(mode, rules, mapId, mapTitle));
    *this << *packet;
}

void ShellUser::sendMapOutline()
{
    if (!world::World::get().hasMap()) return;

    network::MapOutlinePacket packet;
    App_World().map().initMapOutlinePacket(packet);
    *this << packet;
}

void ShellUser::sendPlayerInfo()
{
    if (!world::World::get().hasMap()) return;

    std::unique_ptr<network::PlayerInfoPacket> packet(new network::PlayerInfoPacket);

    for (uint i = 1; i < DDMAXPLAYERS; ++i)
    {
        ServerPlayer *plr = DD_Player(i);

        if (!plr->isInGame())
            continue;

        network::PlayerInfoPacket::Player info;

        info.number   = i;
        info.name     = plr->name;
        info.position = Vec2i(plr->publicData().mo->origin[VX],
                              plr->publicData().mo->origin[VY]);

        /**
         * @todo Player color is presently game-side data. Therefore, this
         * packet should be constructed by libcommon (or player color should be
         * moved to the engine).
         */
        // info.color = ?

        packet->add(info);
    }

    *this << *packet;
}

Address ShellUser::address() const
{
    return Link::address();
}

void ShellUser::handleIncomingPackets()
{
    for (;;)
    {
        std::unique_ptr<Packet> packet(nextPacket());
        if (!packet) break;

        try
        {
            switch (protocol().recognize(packet.get()))
            {
            case network::Protocol::Command:
                Con_Execute(CMDS_CONSOLE, protocol().command(*packet), false, true);
                break;

            default:
                break;
            }
        }
        catch (const Error &er)
        {
            LOG_NET_WARNING("Error while processing packet from %s: %s") << packet->from() << er.asText();
        }
    }
}
