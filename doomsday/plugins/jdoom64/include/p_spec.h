/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * p_spec.h: World texture animation, height or lighting changes
 * according to adjacent sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing, or shooting special
 * lines, or by timed thinkers.
 */

#ifndef __P_SPEC__
#define __P_SPEC__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "d_player.h"

// Base plane ids.
enum {
    PLN_FLOOR,
    PLN_CEILING
};

// Special activation types:
#define SPAC_CROSS              0 // Player crosses linedef.
#define SPAC_USE                1 // Player uses linedef.
#define SPAC_IMPACT             3 // Projectile hits linedef.

// at game start
void            P_InitPicAnims(void);
void            P_InitTerrainTypes(void);

// at map load
void            P_SpawnSpecials(void);

// every tic
void            P_UpdateSpecials(void);
void            P_ThunderSector(void); // jd64

// when needed
int             P_GetTerrainType(sector_t* sec, int plane);
int             P_FlatToTerrainType(int flatid);
boolean         P_ActivateLine(linedef_t *ld, mobj_t *mo, int side,
                               int activationType);

void            P_PlayerInSpecialSector(player_t *player);

typedef enum {
    PS_UP,
    PS_DOWN,
    PS_WAIT
} platstate_e;

typedef enum {
    perpetualRaise,
    downWaitUpStay,
    upWaitDownStay, //jd64 kaiser - outcast
    downWaitUpDoor, //jd64 kaiser - outcast
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS,
    blazeDWUSplus16 //jd64
} plattype_e;

typedef struct {
    thinker_t       thinker;
    sector_t*       sector;
    float           speed;
    float           low;
    float           high;
    int             wait;
    int             count;
    platstate_e     state;
    platstate_e     oldState;
    boolean         crush;
    int             tag;
    plattype_e      type;
} plat_t;

#define PLATWAIT        3
#define PLATSPEED       1

void        T_PlatRaise(plat_t* plat);

int         EV_DoPlat(linedef_t* line, plattype_e type, int amount);
int         P_PlatActivate(short tag);
int         P_PlatDeactivate(short tag);

typedef enum {
    DS_DOWN = -1,
    DS_WAIT,
    DS_UP,
    DS_INITIALWAIT
} doorstate_e;

typedef enum {
    normal,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins,
    blazeRaise,
    blazeOpen,
    instantOpen, //jd64 kaiser
    instantClose, //jd64 kaiser
    instantRaise, //jd64 kaiser
    blazeClose
} doortype_e;

typedef struct {
    thinker_t       thinker;
    doortype_e      type;
    sector_t*       sector;
    float           topHeight;
    float           speed;
    doorstate_e     state;
    int             topWait; // Tics to wait at the top.
    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int             topCountDown;
} door_t;

#define DOORSPEED          2
#define DOORWAIT           150

boolean     EV_VerticalDoor(linedef_t* line, mobj_t* mo);
int         EV_DoDoor(linedef_t* line, doortype_e type);
int         EV_DoLockedDoor(linedef_t* line, doortype_e type, mobj_t* mo);

void        T_Door(door_t* door);
void        P_SpawnDoorCloseIn30(sector_t* sec);
void        P_SpawnDoorRaiseIn5Mins(sector_t* sec);

// jd64 >
int         EV_DoSplitDoor(linedef_t* line, int ftype, int ctype);
int         EV_AnimateDoor(linedef_t* line, mobj_t* thing);
// < d64tc

typedef enum {
    CS_DOWN,
    CS_UP
} ceilingstate_e;

typedef enum {
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise,
    customCeiling // jd64
} ceilingtype_e;

typedef struct {
    thinker_t       thinker;
    ceilingtype_e   type;
    sector_t*       sector;
    float           bottomHeight;
    float           topHeight;
    float           speed;
    boolean         crush;
    ceilingstate_e  state;
    ceilingstate_e  oldState;
    int             tag;
} ceiling_t;

#define CEILSPEED           1
#define CEILWAIT            150

int         EV_DoCeiling(linedef_t* line, ceilingtype_e type);

void        T_MoveCeiling(ceiling_t* c);
int         P_CeilingActivate(short tag);
int         P_CeilingDeactivate(short tag);

typedef enum {
    ok,
    crushed,
    pastdest
} result_e;

result_e    T_MovePlane(sector_t* sector, float speed, float dest,
                        int crush, int floorOrCeiling, int direction);

int         EV_DoFloor(linedef_t* line, floor_e floortype);
void        T_MoveFloor(floormove_t* floor);

#endif
