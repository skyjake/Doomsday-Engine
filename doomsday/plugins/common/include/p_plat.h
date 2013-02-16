/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_plat.h: Common playsim routines relating to moving platforms.
 */

#ifndef __COMMON_THINKER_PLAT_H__
#define __COMMON_THINKER_PLAT_H__

typedef enum {
    PS_UP, // Moving up.
    PS_DOWN, // Moving down.
    PS_WAIT
} platstate_e;

typedef enum {
    PT_PERPETUALRAISE,
    PT_DOWNWAITUPSTAY,
#if __JHEXEN__
    PT_DOWNBYVALUEWAITUPSTAY,
    PT_UPWAITDOWNSTAY,
    PT_UPBYVALUEWAITDOWNSTAY,
#else
# if __JDOOM64__
    PT_UPWAITDOWNSTAY, //jd64 kaiser - outcast
    PT_DOWNWAITUPDOOR, //jd64 kaiser - outcast
# endif
    PT_RAISEANDCHANGE,
    PT_RAISETONEARESTANDCHANGE,
# if __JDOOM__ || __JDOOM64__
    PT_DOWNWAITUPSTAYBLAZE,
#  if __JDOOM64__
    PT_DOWNWAITUPPLUS16STAYBLAZE, //jd64
#  endif
# endif
#endif
    NUMPLATTYPES
} plattype_e;

typedef struct plat_s {
    thinker_t       thinker;
    Sector*         sector;
    float           speed;
    coord_t         low;
    coord_t         high;
    int             wait;
    int             count;
    platstate_e     state;
    platstate_e     oldState;
    boolean         crush;
    int             tag;
    plattype_e      type;
} plat_t;

#define PLATWAIT        (3)
#define PLATSPEED       (1)

void        T_PlatRaise(void *platThinkerPtr);

#if __JHEXEN__
int         EV_DoPlat(LineDef* li, byte* args, plattype_e type,
                      int amount);
int         P_PlatDeactivate(short tag);
#else
int         EV_DoPlat(LineDef* li, plattype_e type, int amount);
int         P_PlatActivate(short tag);
int         P_PlatDeactivate(short tag);
#endif

#endif
