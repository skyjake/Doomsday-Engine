/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * net_event.c: Network Events
 *
 * Network events include clients joining and leaving.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"
#include "de_system.h"
#include "de_console.h"
#include "de_network.h"

#ifdef __SERVER__
#  include "serversystem.h"
#  include "map/gamemap.h"
#endif

using namespace de;

#define MASTER_QUEUE_LEN    16
#define NETEVENT_QUEUE_LEN  32
#define MASTER_HEARTBEAT    120 // seconds
#define MASTER_UPDATETIME   3 // seconds

// The master action queue.
static masteraction_t masterQueue[MASTER_QUEUE_LEN];
static int mqHead, mqTail;

// The net event queue (player arrive/leave).
static netevent_t netEventQueue[NETEVENT_QUEUE_LEN];
static int neqHead, neqTail;

#ifdef __SERVER__
// Countdown for master updates.
static timespan_t masterHeartbeat = 0;
#endif

/**
 * Add a master action command to the queue. The master action stuff really
 * doesn't belong in this file...
 */
void N_MAPost(masteraction_t act)
{
    masterQueue[mqHead++] = act;
    mqHead %= MASTER_QUEUE_LEN;
}

/**
 * Get a master action command from the queue.
 */
boolean N_MAGet(masteraction_t *act)
{
    // Empty queue?
    if(mqHead == mqTail)
        return false;
    *act = masterQueue[mqTail];
    return true;
}

/**
 * Remove a master action command from the queue.
 */
void N_MARemove(void)
{
    if(mqHead != mqTail)
    {
        mqTail = (mqTail + 1) % MASTER_QUEUE_LEN;
    }
}

/**
 * Clear the master action command queue.
 */
void N_MAClear(void)
{
    mqHead = mqTail = 0;
}

/**
 * @return              @c true, if the master action command queue is empty.
 */
boolean N_MADone(void)
{
    return (mqHead == mqTail);
}

/**
 * Add a net event to the queue, to wait for processing.
 */
void N_NEPost(netevent_t * nev)
{
    netEventQueue[neqHead] = *nev;
    neqHead = (neqHead + 1) % NETEVENT_QUEUE_LEN;
}

/**
 * Are there any net events awaiting processing?
 *
 * \note N_GetPacket() will not return a packet until all net events have
 * processed.
 *
 * @return              @c true if there are net events waiting to be
 *                      processed.
 */
boolean N_NEPending(void)
{
    return neqHead != neqTail;
}

/**
 * Get a net event from the queue.
 *
 * @return              @c true, if an event was returned.
 */
boolean N_NEGet(netevent_t *nev)
{
    // Empty queue?
    if(!N_NEPending())
        return false;
    *nev = netEventQueue[neqTail];
    neqTail = (neqTail + 1) % NETEVENT_QUEUE_LEN;
    return true;
}

/**
 * Handles low-level net tick stuff: communication with the master server.
 */
void N_NETicker(timespan_t time)
{
    masteraction_t act;
    int i, num;

#ifdef __SERVER__
    if(netGame)
    {
        masterHeartbeat -= time;

        // Update master every 2 minutes.
        if(masterAware && App_ServerSystem().isListening() && theMap && masterHeartbeat < 0)
        {
            masterHeartbeat = MASTER_HEARTBEAT;
            N_MasterAnnounceServer(true);
        }
    }
#endif

    // Is there a master action to worry about?
    if(N_MAGet(&act))
    {
        switch(act)
        {
        case MAC_REQUEST:
            // Send the request for servers.
            N_MasterRequestList();
            N_MARemove();
            break;

        case MAC_WAIT:
            // Handle incoming messages.
            if(N_MasterGet(0, 0) >= 0)
            {
                // The list has arrived!
                N_MARemove();
            }
            break;

        case MAC_LIST:
            Net_PrintServerInfo(0, NULL);
            num = i = N_MasterGet(0, 0);
            while(--i >= 0)
            {
                serverinfo_t info;
                N_MasterGet(i, &info);
                Net_PrintServerInfo(i, &info);
            }
            Con_Printf("%i server%s found.\n", num, num != 1 ? "s were" : " was");
            N_MARemove();
            break;

        default:
            Con_Error("N_NETicker: Invalid value, act = %i.", (int) act);
            break;
        }
    }
}

/**
 * The event list is checked for arrivals and exits, and the 'clients'
 * and 'players' arrays are updated accordingly.
 */
void N_Update(void)
{
#ifdef __SERVER__
    netevent_t  nevent;

    // Are there any events to process?
    while(N_NEGet(&nevent))
    {
        switch(nevent.type)
        {
        case NE_CLIENT_ENTRY: {
            // Assign a console to the new player.
            Sv_PlayerArrives(nevent.id, App_ServerSystem().user(nevent.id).name().toUtf8());

            // Update the master.
            masterHeartbeat = MASTER_UPDATETIME;
            break; }

        case NE_CLIENT_EXIT:
            Sv_PlayerLeaves(nevent.id);

            // Update the master.
            masterHeartbeat = MASTER_UPDATETIME;
            break;

        default:
            Con_Error("N_Update: Invalid value, nevent.type = %i.", (int) nevent.type);
            break;
        }
    }
#endif // __SERVER__
}

/**
 * The client is removed from the game without delay. This is used when the
 * server needs to terminate a client's connection abnormally.
 */
void N_TerminateClient(int console)
{
#ifdef __SERVER__
    if(!clients[console].connected)
        return;

    Con_Message("N_TerminateClient: '%s' from console %i.",
                clients[console].name, console);

    App_ServerSystem().terminateNode(clients[console].nodeID);

    // Update the master.
    masterHeartbeat = MASTER_UPDATETIME;
#endif
}
