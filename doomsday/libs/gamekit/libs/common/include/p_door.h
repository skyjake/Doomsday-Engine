/** @file p_door.h  Vertical door (opening/closing) thinker.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 id Software, Inc.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_THINKER_DOOR_H
#define LIBCOMMON_THINKER_DOOR_H

#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

#define DOORSPEED          (2)
#define DOORWAIT           (150)

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

typedef struct door_s {
    thinker_t thinker;
    doortype_e type;
    Sector *sector;
    coord_t topHeight;
    float speed;
    doorstate_e state;
    int topWait; // Tics to wait at the top.
    int topCountDown;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} door_t;

#ifdef __cplusplus
extern "C" {
#endif

void T_Door(void *doorThinkerPtr);

dd_bool EV_VerticalDoor(Line *li, mobj_t *mo);

#if __JHEXEN__
int EV_DoDoor(Line *li, byte *args, doortype_e type);
#else
int EV_DoDoor(Line *li, doortype_e type);
#endif

#if __JDOOM__ || __JDOOM64__
int EV_DoLockedDoor(Line *li, doortype_e type, mobj_t *mo);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
void P_SpawnDoorCloseIn30(Sector *sec);

void  P_SpawnDoorRaiseIn5Mins(Sector *sec);
#endif

#if __JDOOM64__
int EV_AnimateDoor(Line* li, mobj_t *mo);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_THINKER_DOOR_H
