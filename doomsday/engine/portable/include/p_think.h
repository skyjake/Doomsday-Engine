/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_think.h: Thinkers
 */

#ifndef __DOOMSDAY_THINKER_H__
#define __DOOMSDAY_THINKER_H__

// think_t is a function pointer to a routine to handle an actor
typedef void    (*think_t) ();

typedef struct thinker_s {
    struct thinker_s *prev, *next;
    think_t         function;
    boolean         inStasis;
    thid_t          id; // Only used for mobjs (zero is not an ID).
} thinker_t;

boolean         P_ThinkerListInited(void);

void            P_InitThinkerLists(byte flags);
boolean         P_IterateThinkers(think_t type, byte flags,
                                  boolean (*callback) (thinker_t* th, void*),
                                  void* context);

void            P_ThinkerAdd(thinker_t* th, boolean makePublic);
void            P_ThinkerRemove(thinker_t* th);

void            P_SetMobjID(thid_t id, boolean state);
boolean         P_IsUsedMobjID(thid_t id);
boolean         P_IsMobjThinker(think_t thinker);

// Public interface:
void            DD_InitThinkers(void);
void            DD_RunThinkers(void);
void            DD_ThinkerAdd(thinker_t* th);
void            DD_ThinkerRemove(thinker_t* th);
void            DD_ThinkerSetStasis(thinker_t* th, boolean on);
boolean         DD_IterateThinkers(think_t func, boolean (*callback) (thinker_t*, void*),
                                   void* context);
#endif
