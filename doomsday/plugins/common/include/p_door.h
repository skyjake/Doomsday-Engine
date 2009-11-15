/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_door.h: Common playsim routines relating to doors.
 */

#ifndef __COMMON_THINKER_DOOR_H__
#define __COMMON_THINKER_DOOR_H__

#if __JDOOM__
#   include "d_identifiers.h"
#endif

#include <de/Thinker>

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

#define DOORSPEED          2
#define DOORWAIT           150

class DoorThinker : public de::Thinker
{
public:
    doortype_e      type;
    sector_t*       sector;
    float           topHeight;
    float           speed;
    doorstate_e     state;
    int             topWait; // Tics to wait at the top.
    int             topCountDown;

public:
    DoorThinker() 
        : de::Thinker(SID_DOOR),
          type(DT_NORMAL),
          sector(0),
          topHeight(0),
          speed(0),
          state(DS_WAIT),
          topWait(0),
          topCountDown(0) {}

    void think(const de::Time::Delta& elapsed);

    // Implements ISerializable.
    void operator >> (de::Writer& to) const;
    void operator << (de::Reader& from);

    static Thinker* construct() {
        return new DoorThinker;
    }
};

typedef DoorThinker door_t;

//void        T_Door(door_t* door);

boolean     EV_VerticalDoor(linedef_t* li, mobj_t* mo);
#if __JHEXEN__
int         EV_DoDoor(linedef_t* li, byte* args, doortype_e type);
#else
int         EV_DoDoor(linedef_t* li, doortype_e type);
#endif
#if __JDOOM__ || __JDOOM64__
int         EV_DoLockedDoor(linedef_t* li, doortype_e type, mobj_t* mo);
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void        P_SpawnDoorCloseIn30(sector_t* sec);
void        P_SpawnDoorRaiseIn5Mins(sector_t* sec);
#endif
#if __JDOOM64__
int         EV_AnimateDoor(linedef_t* li, mobj_t* mo);
#endif

#endif
