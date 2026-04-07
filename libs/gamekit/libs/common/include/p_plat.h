/** @file p_plat.h  Common playsim routines relating to moving platforms.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#ifndef LIBCOMMON_THINKER_PLAT_H
#define LIBCOMMON_THINKER_PLAT_H

#include "doomsday.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

#define PLATWAIT        (3)
#define PLATSPEED       (1)

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
    thinker_t thinker;
    Sector *sector;
    float speed;
    coord_t low;
    coord_t high;
    int wait;
    int count;
    platstate_e state;
    platstate_e oldState;
    dd_bool crush;
    int tag;
    plattype_e type;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} plat_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Move a plat up and down.
 *
 * @param plat  Ptr to the plat to be moved.
 */
void T_PlatRaise(void *platThinkerPtr);

/**
 * Do Platforms.
 *
 * @param amount: is only used for SOME platforms.
 */
#if __JHEXEN__
int EV_DoPlat(Line *li, byte *args, plattype_e type, int amount);
#else
int EV_DoPlat(Line* li, plattype_e type, int amount);
#endif

#if !__JHEXEN__
/**
 * Activate a plat that has been put in stasis
 * (stopped perpetual floor, instant floor/ceil toggle)
 *
 * @param tag  Tag of plats that should be reactivated.
 */
int P_PlatActivate(short tag);
#endif

/**
 * Handler for "stop perpetual floor" line type.
 *
 * @param tag  Tag of plats to put into stasis.
 *
 * @return  Number of plats put into stasis.
 */
int P_PlatDeactivate(short tag);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_THINKER_PLAT_H
