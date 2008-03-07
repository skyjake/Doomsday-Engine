/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2008 Daniel Swanson <danij@dengine.net>
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
 * p_floor.h: Stairs and donuts.
 */

#ifndef __P_FLOOR_H__
#define __P_FLOOR_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "doomsday.h"

#define FLOORSPEED              (1)

typedef enum {
    // lower floor to highest surrounding floor
    lowerFloor,

    // lower floor to lowest surrounding floor
    lowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    turboLower,

    lowerToEight, // jd64
    customFloor, // jd64
    customChangeSec, // jd64

    // raise floor to lowest surrounding CEILING
    raiseFloor,

    // raise floor to next highest surrounding floor
    raiseFloorToNearest,

    // raise floor to shortest height texture around it
    raiseToTexture,

    // lower floor to lowest surrounding floor
    //  and change floorpic
    lowerAndChange,

    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,

    // raise to next highest floor, turbo-speed
    raiseFloorTurbo,
    donutRaise,
    raiseFloor512,
    raiseFloor32 // jd64
} floor_e;

typedef struct {
    thinker_t       thinker;
    floor_e         type;
    boolean         crush;
    sector_t       *sector;
    int             direction;
    int             newSpecial;
    short           texture;
    float           floorDestHeight;
    float           speed;
} floormove_t;

typedef enum {
    build8, // slowly build by 8
    turbo16 // quickly build by 16
} stair_e;

int             EV_BuildStairs(linedef_t *line, stair_e type);
int             EV_DoDonut(linedef_t *line);

#endif
