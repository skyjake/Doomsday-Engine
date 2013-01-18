/**\file
 *
 * @authors Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

// Net events.
typedef enum neteventtype_e {
	NE_CLIENT_ENTRY,
	NE_CLIENT_EXIT,
	NE_END_CONNECTION,
	NE_TERMINATE_NODE
} neteventtype_t;

typedef struct netevent_s {
	neteventtype_t  type;
	nodeid_t        id;
} netevent_t;

#ifdef __cplusplus
extern "C" {
#endif

void            N_MAPost(masteraction_t act);
boolean         N_MADone(void);
void            N_MAClear(void);

void            N_NEPost(netevent_t * nev);
boolean         N_NEPending(void);
void            N_NETicker(timespan_t time);

void            N_TerminateClient(int console);
void            N_Update(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
