/** @file
 *
 * @authors Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/*
 * net_event.h: Network Events
 */

#ifndef __DOOMSDAY_NETWORK_EVENT_H__
#define __DOOMSDAY_NETWORK_EVENT_H__

#include "net_main.h"

// Net events.
typedef enum neteventtype_e {
    NE_CLIENT_ENTRY,
    NE_CLIENT_EXIT,
} neteventtype_t;

typedef struct netevent_s {
    neteventtype_t  type;
    nodeid_t        id;
} netevent_t;

// If a master action fails, the action queue is emptied.
typedef enum {
    MAC_REQUEST, // Retrieve the list of servers from the master.
    MAC_WAIT, // Wait for the server list to arrive.
    MAC_LIST // Print the server list in the console.
} masteraction_t;

#ifdef __cplusplus
extern "C" {
#endif

void            N_MAPost(masteraction_t act);
dd_bool         N_MADone(void);
void            N_MAClear(void);

void            N_NEPost(netevent_t *nev);
dd_bool         N_NEPending(void);
dd_bool         N_NEGet(netevent_t *nev);
void            N_NETicker(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
