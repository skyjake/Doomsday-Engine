/** @file net_ping.cpp  Pinging Clients and the Server.
 *
 * Warning: This is not a very accurate ping.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2015 Daniel Swanson <danij@dengine.net>
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

#include "network/net_main.h"
#include "network/net_buf.h"
#include "world/p_players.h"

#include <doomsday/network/protocol.h>
#include <doomsday/network/net.h>
#include <de/legacy/timer.h>

using namespace de;

void Net_ShowPingSummary(int player)
{
    DE_ASSERT(player >= 0 && player < DDMAXPLAYERS);
    const auto &  cl   = *DD_Player(player);
    const Pinger &ping = cl.pinger();

    if (player < 0 && ping.total > 0) return;

    int    goodCount = 0;
    dfloat avgTime   = 0;
    for (int i = 0; i < ping.total; ++i)
    {
        if (ping.times[i] < 0) continue;

        goodCount++;
        avgTime += ping.times[i];
    }
    avgTime /= goodCount;

    LOG_NET_NOTE("Player %i (%s): average ping %.0f ms") << player << cl.name << (avgTime * 1000);
}

void Net_SendPing(int player, int count)
{
    DE_ASSERT(player >= 0 && player < DDMAXPLAYERS);
    Pinger &ping = DD_Player(player)->pinger();

    // Valid destination?
    if (player == ::consolePlayer || (netState.isClient && player)) return;

    if (count)
    {
        // We can't start a new ping run until the old one is done.
        if (ping.sent) return;

        // Start a new ping session.
        ping.current = 0;
        ping.total   = de::min(count, MAX_PINGS);
    }
    else
    {
        // Continue or finish the current pinger.
        if (++ping.current >= ping.total)
        {
            // We're done.
            ping.sent = 0;
            // Print a summary (average ping, loss %).
            Net_ShowPingSummary(::netBuffer.player);
            return;
        }
    }

    // Send a new ping.
    Msg_Begin(PKT_PING);
    ping.sent = Timer_RealMilliseconds();
    Writer_WriteUInt32(::msgWriter, ping.sent);
    Msg_End();

    // Update the length of the message.
    ::netBuffer.player = player;
    N_SendPacket();
}

// Called when a ping packet comes in.
void Net_PingResponse()
{
    const int player = ::netBuffer.player;
    DE_ASSERT(player >= 0 && player < DDMAXPLAYERS);
    Pinger &ping = DD_Player(player)->pinger();

    // Is this a response to our ping?
    const int time = Reader_ReadUInt32(msgReader);
    if (time == ping.sent)
    {
        // Record the time and send the next ping.
        ping.times[ping.current] = (Timer_RealMilliseconds() - time) / 1000.0f;
        // Send the next ping.
        Net_SendPing(::netBuffer.player, 0);
    }
    else
    {
        // Not ours, just respond.
        Net_SendBuffer(::netBuffer.player, 10000);
    }
}

D_CMD(Ping)
{
    DE_UNUSED(src);

    if (!netState.netGame)
    {
        LOG_SCR_ERROR("Ping is only for netgames");
        return true;
    }

    if (netState.isServer && argc == 1)
    {
        LOG_SCR_NOTE("Usage: %s (plrnum) (count)") << argv[0];
        LOG_SCR_MSG("(count) is optional. 4 pings are sent by default.");
        return true;
    }

    int dest = 0, count = 4;
    if (netState.isServer)
    {
        dest = String(argv[1]).toInt();
        if (argc >= 3) count = String(argv[2]).toInt();
    }
    else
    {
        if (argc >= 2) count = String(argv[1]).toInt();
    }

    // Check that the given parameters are valid.
    if (count <= 0 || count > MAX_PINGS || dest < 0 || dest >= DDMAXPLAYERS ||
        dest == ::consolePlayer || (dest && !DD_Player(dest)->publicData().inGame))
    {
        return false;
    }
    Net_SendPing(dest, count);
    return true;
}
