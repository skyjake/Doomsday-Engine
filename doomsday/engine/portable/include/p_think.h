/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * p_think.h: Thinkers
 */

#ifndef __DOOMSDAY_THINKER_H__
#define __DOOMSDAY_THINKER_H__

// think_t is a function pointer to a routine to handle an actor
typedef void    (*think_t) ();

typedef struct thinker_s {
    struct thinker_s *prev, *next;
    think_t         function;
    thid_t          id;            // Only used for mobjs (zero is not an ID).
} thinker_t;

extern thinker_t thinkercap;       // both the head and tail of the thinker list

void            P_RunThinkers(void);
void            P_InitThinkers(void);
void            P_AddThinker(thinker_t *thinker);
void            P_RemoveThinker(thinker_t *thinker);
void            P_SetMobjID(thid_t id, boolean state);
boolean         P_IsUsedMobjID(thid_t id);
boolean         P_IsMobjThinker(think_t thinker);

#endif
