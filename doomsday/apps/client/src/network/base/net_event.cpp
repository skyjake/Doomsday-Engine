/** @file net_event.cpp  Network Events.
 *
 * Network events include clients joining and leaving.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "network/net_event.h"

#ifdef __SERVER__
#  include "dd_main.h"
#endif

#include <doomsday/network/masterserver.h>

#include "world/p_players.h"

#ifdef __SERVER__
#  include "serversystem.h"
#  include "server/sv_def.h"
#endif

using namespace de;

#define MASTER_QUEUE_LEN    16
#define NETEVENT_QUEUE_LEN  32

// The master action queue.
static masteraction_t masterQueue[MASTER_QUEUE_LEN];
static dint mqHead, mqTail;

// The net event queue (player arrive/leave).
static netevent_t netEventQueue[NETEVENT_QUEUE_LEN];
static dint neqHead, neqTail;

/**
 * Add a master action command to the queue. The master action stuff really
 * doesn't belong in this file...
 */
void N_MAPost(masteraction_t act)
{
    ::masterQueue[::mqHead++] = act;
    ::mqHead %= MASTER_QUEUE_LEN;
}

/**
 * Get a master action command from the queue.
 */
dd_bool N_MAGet(masteraction_t *act)
{
    // Empty queue?
    if(::mqHead == ::mqTail)
        return false;

    DE_ASSERT(act);
    *act = ::masterQueue[mqTail];
    return true;
}

/**
 * Remove a master action command from the queue.
 */
void N_MARemove()
{
    if(::mqHead != ::mqTail)
    {
        ::mqTail = (::mqTail + 1) % MASTER_QUEUE_LEN;
    }
}

/**
 * Clear the master action command queue.
 */
void N_MAClear()
{
    ::mqHead = ::mqTail = 0;
}

/**
 * Returns @c true if the master action command queue is empty.
 */
dd_bool N_MADone()
{
    return (::mqHead == ::mqTail);
}

/**
 * Add a net event to the queue, to wait for processing.
 */
void N_NEPost(netevent_t *nev)
{
    DE_ASSERT(nev);
    ::netEventQueue[::neqHead] = *nev;
    ::neqHead = (::neqHead + 1) % NETEVENT_QUEUE_LEN;
}

/**
 * Are there any net events awaiting processing?
 *
 * @note N_GetPacket() will not return a packet until all net events have processed.
 *
 * @return  @c true if there are net events waiting to be processed.
 */
dd_bool N_NEPending()
{
    return ::neqHead != ::neqTail;
}

/**
 * Get a net event from the queue.
 *
 * @return  @c true if an event was returned.
 */
dd_bool N_NEGet(netevent_t *nev)
{
    // Empty queue?
    if(!N_NEPending())
        return false;

    DE_ASSERT(nev);
    *nev = ::netEventQueue[::neqTail];
    ::neqTail = (::neqTail + 1) % NETEVENT_QUEUE_LEN;
    return true;
}

/**
 * Handles low-level net tick stuff: communication with the master server.
 */
void N_NETicker(void)
{
    // Is there a master action to worry about?
    masteraction_t act;
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

        case MAC_LIST: {
            const dint num = N_MasterGet(0, 0);
            dint i = num;
            while(--i >= 0)
            {
                ServerInfo info;
                N_MasterGet(i, &info);
                info.printToLog(i, i - 1 == num);
            }
            LOG_NET_VERBOSE("%i server%s found") << num << (num != 1 ? "s were" : " was");
            N_MARemove();
            break; }

        default: DE_ASSERT_FAIL("N_NETicker: Invalid value for 'act'"); break;
        }
    }
}
