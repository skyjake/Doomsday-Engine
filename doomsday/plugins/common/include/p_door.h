/**\file p_door.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * Common playsim routines relating to doors.
 */

#ifndef LIBCOMMON_THINKER_DOOR_H
#define LIBCOMMON_THINKER_DOOR_H

typedef enum {
    DS_DOWN = -1,
    DS_WAIT,
    DS_UP,
    DS_INITIALWAIT
} doorstate_e;

typedef enum {
    DT_NORMAL,
    DT_CLOSE30THENOPEN,
    DT_CLOSE,
    DT_OPEN,
    DT_RAISEIN5MINS,
#if __JDOOM__ || __JDOOM64__
    DT_BLAZERAISE,
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    DT_BLAZEOPEN,
#endif
#if __JDOOM64__
    DT_INSTANTOPEN, //jd64 kaiser
    DT_INSTANTCLOSE, //jd64 kaiser
    DT_INSTANTRAISE, //jd64 kaiser
#endif
#if __JDOOM__ || __JDOOM64__
    DT_BLAZECLOSE,
#endif
    NUMDOORTYPES
} doortype_e;

typedef struct {
    thinker_t thinker;
    doortype_e type;
    Sector* sector;
    coord_t topHeight;
    float speed;
    doorstate_e state;
    int topWait; // Tics to wait at the top.
    int topCountDown;
} door_t;

#define DOORSPEED          (2)
#define DOORWAIT           (150)

void T_Door(door_t* door);

boolean EV_VerticalDoor(LineDef* li, mobj_t* mo);

#if __JHEXEN__
int EV_DoDoor(LineDef* li, byte* args, doortype_e type);
#else
int EV_DoDoor(LineDef* li, doortype_e type);
#endif

#if __JDOOM__ || __JDOOM64__
int EV_DoLockedDoor(LineDef* li, doortype_e type, mobj_t* mo);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_SpawnDoorCloseIn30(Sector* sec);
void  P_SpawnDoorRaiseIn5Mins(Sector* sec);
#endif

#if __JDOOM64__
int EV_AnimateDoor(LineDef* li, mobj_t* mo);
#endif

#endif /// LIBCOMMON_THINKER_DOOR_H
