/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * net_event.c: Network Events
 *
 * Network events include clients joining and leaving.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_system.h"
#include "de_console.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

#define MASTER_QUEUE_LEN    16
#define NETEVENT_QUEUE_LEN  32
#define MASTER_HEARTBEAT    120 // seconds
#define MASTER_UPDATETIME   3 // seconds

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// The master action queue.
static masteraction_t masterQueue[MASTER_QUEUE_LEN];
static int mqHead, mqTail;

// The net event queue (player arrive/leave).
static netevent_t netEventQueue[NETEVENT_QUEUE_LEN];
static int neqHead, neqTail;

// Countdown for master updates.
static timespan_t masterHeartbeat = 0;

// CODE --------------------------------------------------------------------

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

    if(netGame)
    {
        masterHeartbeat -= time;

        // Update master every 2 minutes.
        if(masterAware && N_UsingInternet() && masterHeartbeat < 0)
        {
            masterHeartbeat = MASTER_HEARTBEAT;
            N_MasterAnnounceServer(true);
        }
    }

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
    netevent_t  nevent;
    char        name[256];

    // Are there any events to process?
    while(N_NEGet(&nevent))
    {
        switch(nevent.type)
        {
        case NE_CLIENT_ENTRY:
            // Find out the name of the new player.
            memset(name, 0, sizeof(name));
            N_GetNodeName(nevent.id, name);

            // Assign a console to the new player.
            Sv_PlayerArrives(nevent.id, name);

            // Update the master.
            masterHeartbeat = MASTER_UPDATETIME;
            break;

        case NE_CLIENT_EXIT:
            Sv_PlayerLeaves(nevent.id);

            // Update the master.
            masterHeartbeat = MASTER_UPDATETIME;
            break;

        case NE_TERMINATE_NODE:
            // The server receives this event when a client's connection is broken.
            N_TerminateNode(nevent.id);
            break;

#ifdef __CLIENT__
        case NE_END_CONNECTION:
            // A client receives this event when the connection is
            // terminated.
            if(netGame)
            {
                // We're still in a netGame, which means we didn't disconnect
                // voluntarily.
                Con_Message("N_Update: Connection was terminated.\n");
                N_Disconnect();
            }
            break;
#endif

        default:
            Con_Error("N_Update: Invalid value, nevent.type = %i.", (int) nevent.type);
            break;
        }
    }
}

/**
 * The client is removed from the game without delay. This is used when the
 * server needs to terminate a client's connection abnormally.
 */
void N_TerminateClient(int console)
{
    if(!N_IsAvailable() || !clients[console].connected || !netServerMode)
        return;

    Con_Message("N_TerminateClient: '%s' from console %i.\n",
                clients[console].name, console);

    N_TerminateNode(clients[console].nodeID);

    // Update the master.
    masterHeartbeat = MASTER_UPDATETIME;
}
