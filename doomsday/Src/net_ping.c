/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * net_ping.c: Pinging Clients and the Server
 *
 * Warning: This is not a very accurate ping.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Net_ShowPingSummary(int player)
{
    client_t *cl = clients + player;
    pinger_t *ping = &cl->ping;
    float   avgTime = 0, loss;
    int     i, goodCount = 0;

    if(player < 0)
        return;

    for(i = 0; i < ping->total; i++)
    {
        if(ping->times[i] < 0)
            continue;
        goodCount++;
        avgTime += ping->times[i];
    }
    avgTime /= goodCount;
    loss = 1 - goodCount / (float) ping->total;
    Con_Printf("Plr %i (%s): average ping %.0f ms, loss %.0f%%.\n", player,
               cl->name, avgTime * 1000, loss * 100);
}

void Net_SendPing(int player, int count)
{
    client_t *cl = clients + player;

    // Valid destination?
    if((player == consoleplayer) || (isClient && player))
        return;

    if(count)
    {
        // We can't start a new ping run until the old one is done.
        if(cl->ping.sent)
            return;
        // Start a new ping session.
        if(count > MAX_PINGS)
            count = MAX_PINGS;
        cl->ping.current = 0;
        cl->ping.total = count;
    }
    else
    {
        // Continue or finish the current pinger.
        if(++cl->ping.current >= cl->ping.total)
        {
            // We're done.
            cl->ping.sent = 0;
            // Print a summary (average ping, loss %).
            Net_ShowPingSummary(netBuffer.player);
            return;
        }
    }
    // Send a new ping.
    Msg_Begin(pkt_ping);
    cl->ping.sent = Sys_GetRealTime();
    Msg_WriteLong(cl->ping.sent);

    // Update the length of the message.
    netBuffer.length = netBuffer.cursor - netBuffer.msg.data;
    netBuffer.player = player;
    N_SendPacket(10000);
}

// Called when a ping packet comes in.
void Net_PingResponse(void)
{
    client_t *cl = &clients[netBuffer.player];
    int     time = Msg_ReadLong();

    // Is this a response to our ping?
    if(time == cl->ping.sent)
    {
        // Record the time and send the next ping.
        cl->ping.times[cl->ping.current] =
            (Sys_GetRealTime() - time) / 1000.0f;
        // Show a notification.
        /*Con_Printf( "Ping to plr %i: %.0f ms.\n", netbuffer.player,
           cl->ping.times[cl->ping.current] * 1000); */
        // Send the next ping.
        Net_SendPing(netBuffer.player, 0);
    }
    else
    {
        // Not ours, just respond.
        Net_SendBuffer(netBuffer.player, 10000);
    }
}

D_CMD(Ping)
{
    int     dest, count = 4;

    if(!netgame)
    {
        Con_Printf("Ping is only for netgames.\n");
        return true;
    }
    if(isServer && argc == 1)
    {
        Con_Printf("Usage: %s (plrnum) (count)\n", argv[0]);
        Con_Printf("(count) is optional. 4 pings are sent by default.\n");
        return true;
    }
    if(isServer)
    {
        dest = atoi(argv[1]);
        if(argc >= 3)
            count = atoi(argv[2]);
    }
    else
    {
        dest = 0;
        if(argc >= 2)
            count = atoi(argv[1]);
    }
    // Check that the given parameters are valid.
    if(count <= 0 || count > MAX_PINGS || dest < 0 || dest >= MAXPLAYERS ||
       dest == consoleplayer || (dest && !players[dest].ingame))
        return false;

    Net_SendPing(dest, count);
    return true;
}
