/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
    perpetualRaise,
    downWaitUpStay,
#if __JHEXEN__
    downByValueWaitUpStay,
    upWaitDownStay,
    upByValueWaitDownStay,
#else
# if __JDOOM64__
    upWaitDownStay, //jd64 kaiser - outcast
    downWaitUpDoor, //jd64 kaiser - outcast
# endif
    raiseAndChange,
    raiseToNearestAndChange,
# if __JDOOM__ || __JDOOM64__ || __WOLFTC__
    blazeDWUS,
#  if __JDOOM64__
    blazeDWUSplus16, //jd64
#  endif
# endif
#endif
    NUMPLATTYPES
} plattype_e;

typedef struct plat_s {
    thinker_t       thinker;
    sector_t*       sector;
    float           speed;
    float           low;
    float           high;
    int             wait;
    int             count;
    platstate_e     state;
    platstate_e     oldState;
#if __JHEXEN__
    int             crush;
#else
    boolean         crush;
#endif
    int             tag;
    plattype_e      type;
} plat_t;

#define PLATWAIT        (3)
#define PLATSPEED       (1)

void        T_PlatRaise(plat_t* pl);

#if __JHEXEN__
int         EV_DoPlat(linedef_t* li, byte* args, plattype_e type,
                      int amount);
int         P_PlatDeactivate(linedef_t* li, byte* args);
#else
int         EV_DoPlat(linedef_t* li, plattype_e type, int amount);
int         P_PlatActivate(short tag);
int         P_PlatDeactivate(short tag);
#endif

#endif
