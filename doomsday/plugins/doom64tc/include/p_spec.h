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

#ifndef __DOOM64TC__
#  error "Using Doom64TC headers without __DOOM64TC__"
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
void            P_ThunderSector(void); // d64tc

// when needed
int             P_GetTerrainType(sector_t* sec, int plane);
int             P_FlatToTerrainType(int flatid);
boolean         P_ActivateLine(linedef_t *ld, mobj_t *mo, int side,
                               int activationType);

void            P_PlayerInSpecialSector(player_t *player);

typedef enum {
    up,
    down,
    waiting,
    in_stasis
} plat_e;

typedef enum {
    perpetualRaise,
    downWaitUpStay,
    upWaitDownStay, //d64tc kaiser - outcast
    downWaitUpDoor, //d64tc kaiser - outcast
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS,
    blazeDWUSplus16 //d64tc
} plattype_e;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    float           speed;
    float           low;
    float           high;
    int             wait;
    int             count;
    plat_e          status;
    plat_e          oldStatus;
    boolean         crush;
    int             tag;
    plattype_e      type;

    struct platlist_s *list;
} plat_t;

typedef struct platlist_s {
  plat_t           *plat;
  struct platlist_s *next, **prev;
} platlist_t;

#define PLATWAIT        3
#define PLATSPEED       1

void        T_PlatRaise(plat_t *plat);

int         EV_DoPlat(linedef_t *line, plattype_e type, int amount);
boolean     EV_StopPlat(linedef_t *line);

void        P_AddActivePlat(plat_t *plat);
void        P_RemoveActivePlat(plat_t *plat);
void        P_RemoveAllActivePlats(void);
int         P_ActivateInStasisPlat(int tag);

typedef enum {
    normal,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins,
    blazeRaise,
    blazeOpen,
    instantOpen, //d64tc kaiser
    instantClose, //d64tc kaiser
    instantRaise, //d64tc kaiser
    blazeClose
} vldoor_e;

typedef struct {
    thinker_t       thinker;
    vldoor_e        type;
    sector_t       *sector;
    float           topHeight;
    float           speed;

    // 1 = up, 0 = waiting at top, -1 = down
    int             direction;

    // tics to wait at the top
    int             topWait;
    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int             topCountDown;
} vldoor_t;

#define VDOORSPEED          2
#define VDOORWAIT           150

boolean     EV_VerticalDoor(linedef_t *line, mobj_t *thing);
int         EV_DoDoor(linedef_t *line, vldoor_e type);
int         EV_DoLockedDoor(linedef_t *line, vldoor_e type, mobj_t *thing);

void        T_VerticalDoor(vldoor_t *door);
void        P_SpawnDoorCloseIn30(sector_t *sec);
void        P_SpawnDoorRaiseIn5Mins(sector_t *sec);

// d64tc >
int         EV_DoSplitDoor(linedef_t *line, int ftype, int ctype);
int         EV_DestoryLineShield(linedef_t *line);
int         EV_SwitchTextureFree(linedef_t *line);
int         EV_ActivateSpecial(linedef_t *line);
void        P_SetSectorColor(linedef_t *line);
int         EV_AnimateDoor(linedef_t *line, mobj_t *thing);
// < d64tc

typedef enum {
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise,
    customCeiling // d64tc
} ceiling_e;

typedef struct {
    thinker_t       thinker;
    ceiling_e       type;
    sector_t       *sector;
    float           bottomHeight;
    float           topHeight;
    float           speed;
    boolean         crush;

    // 1 = up, 0 = waiting, -1 = down
    int             direction;

    // ID
    int             tag;
    int             oldDirection;

    struct ceilinglist_s *list;
} ceiling_t;

typedef struct ceilinglist_s {
    ceiling_t      *ceiling;
    struct ceilinglist_s *next, **prev;
} ceilinglist_t;

#define CEILSPEED           1
#define CEILWAIT            150

int         EV_DoCeiling(linedef_t *line, ceiling_e type);

void        T_MoveCeiling(ceiling_t *ceiling);
void        P_AddActiveCeiling(ceiling_t *c);
void        P_RemoveActiveCeiling(ceiling_t *c);
void        P_RemoveAllActiveCeilings(void);
int         EV_CeilingCrushStop(linedef_t *line);
int         P_ActivateInStasisCeiling(linedef_t *line);

typedef enum {
    ok,
    crushed,
    pastdest
} result_e;

result_e    T_MovePlane(sector_t *sector, float speed, float dest,
                        int crush, int floorOrCeiling, int direction);

int         EV_DoFloor(linedef_t *line, floor_e floortype);
void        T_MoveFloor(floormove_t *floor);

#endif
