/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

/*
 * Implements special effects:
 * Texture animation, height or lighting changes according to adjacent
 * sectors, respective utility functions, etc.
 *
 * Line Tag handling. Line and Sector triggers.
 *
 * Events are operations triggered by using, crossing,
 * or shooting special lines, or by timed thinkers.
 */

#ifndef __P_SPEC__
#define __P_SPEC__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "d_player.h"
#include "r_data.h"

//
// End-level timer (-TIMER option)
//
extern boolean  levelTimer;
extern int      levelTimeCount;

//      Define values for map objects
#define MO_TELEPORTMAN          14

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
int             P_FlatToTerrainType(int flatlumpnum);
boolean         P_ActivateLine(line_t *ld, mobj_t *mo, int side,
                               int activationType);

void            P_PlayerInSpecialSector(player_t *player);

fixed_t         P_FindLowestFloorSurrounding(sector_t *sec);
fixed_t         P_FindHighestFloorSurrounding(sector_t *sec);

fixed_t         P_FindNextHighestFloor(sector_t *sec, int currentheight);

fixed_t         P_FindLowestCeilingSurrounding(sector_t *sec);
fixed_t         P_FindHighestCeilingSurrounding(sector_t *sec);

int             P_FindSectorFromLineTag(line_t *line, int start);

int             P_FindMinSurroundingLight(sector_t *sector, int max);

sector_t       *getNextSector(line_t *line, sector_t *sec);

//
// SPECIAL
//
int             EV_DoDonut(line_t *line);

//
// P_LIGHTS
//
typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    int             count;
    int             maxlight;
    int             minlight;
} fireflicker_t;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    int             count;
    int             maxlight;
    int             minlight;
    int             maxtime;
    int             mintime;
} lightflash_t;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    int             count;
    int             maxlight;
    int             minlight;
    int             maxtime;
    int             mintime;
} lightblink_t;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    int             count;
    int             minlight;
    int             maxlight;
    int             darktime;
    int             brighttime;
} strobe_t;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    int             minlight;
    int             maxlight;
    int             direction;
} glow_t;

#define GLOWSPEED           8
#define STROBEBRIGHT        5
#define FASTDARK            15
#define SLOWDARK            35

void T_FireFlicker(fireflicker_t *flick);
void P_SpawnFireFlicker(sector_t *sector);

void T_LightFlash(lightflash_t *flash);
void P_SpawnLightFlash(sector_t *sector);

void T_LightBlink(lightblink_t *flash); // d64tc
void P_SpawnLightBlink(sector_t *sector); // d64tc

void T_StrobeFlash(strobe_t *flash);
void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync);

void EV_StartLightStrobing(line_t *line);
void EV_TurnTagLightsOff(line_t *line);

void EV_LightTurnOn(line_t *line, int bright);

void T_Glow(glow_t *g);
void P_SpawnGlowingLight(sector_t *sector);

//
// P_SWITCH
//
// This struct is used to provide byte offsets when reading a custom
// SWITCHES lump thus it must be packed and cannot be altered.

#pragma pack(1)
typedef struct {
    /* Do NOT change these members in any way */
    char            name1[9];
    char            name2[9];
    short           episode;
} switchlist_t;
#pragma pack()

typedef enum {
    top,
    middle,
    bottom
} bwhere_e;

typedef struct {
    line_t         *line;
    bwhere_e        where;
    int             btexture;
    int             btimer;
    mobj_t         *soundorg;

} button_t;

 // 4 players, 4 buttons each at once, max.
#define MAXBUTTONS      16

 // 1 second, in ticks.
#define BUTTONTIME      35

extern button_t buttonlist[MAXBUTTONS];

void            P_ChangeSwitchTexture(line_t *line, int useAgain);

void            P_InitSwitchList(void);

//
// P_PLATS
//
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
    fixed_t         speed;
    fixed_t         low;
    fixed_t         high;
    int             wait;
    int             count;
    plat_e          status;
    plat_e          oldstatus;
    boolean         crush;
    int             tag;
    plattype_e      type;

    struct platlist *list;   // killough
} plat_t;

// New limit-free plat structure -- killough
typedef struct platlist {
  plat_t *plat;
  struct platlist *next,**prev;
} platlist_t;

#define PLATWAIT        3
#define PLATSPEED       FRACUNIT

extern platlist_t *activeplats;

void            T_PlatRaise(plat_t * plat);

int             EV_DoPlat(line_t *line, plattype_e type, int amount);
int             EV_StopPlat(line_t* line);

void            P_AddActivePlat(plat_t * plat);
void            P_RemoveActivePlat(plat_t * plat);
void            P_RemoveAllActivePlats( void );    // killough
void            P_ActivateInStasis(int tag);

//
// P_DOORS
//
typedef enum {
    normal,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins,
    blazeRaise,
    blazeOpen,
    instantOpen,    //d64tc kaiser
    instantClose, //d64tc kaiser
    instantRaise, //d64tc kaiser
    blazeClose
} vldoor_e;

typedef struct {
    thinker_t       thinker;
    vldoor_e        type;
    sector_t       *sector;
    fixed_t         topheight;
    fixed_t         speed;

    // 1 = up, 0 = waiting at top, -1 = down
    int             direction;

    // tics to wait at the top
    int             topwait;
    // (keep in case a door going down is reset)
    // when it reaches 0, start going down
    int             topcountdown;
} vldoor_t;

#define VDOORSPEED      FRACUNIT*2
#define VDOORWAIT       150

void            EV_VerticalDoor(line_t *line, mobj_t *thing);
int             EV_DoDoor(line_t *line, vldoor_e type);
int             EV_DoLockedDoor(line_t *line, vldoor_e type, mobj_t *thing);

void            T_VerticalDoor(vldoor_t * door);
void            P_SpawnDoorCloseIn30(sector_t *sec);
void            P_SpawnDoorRaiseIn5Mins(sector_t *sec, int secnum);

// d64tc >
int             EV_DoSplitDoor(line_t * line, int ftype, int ctype);
int             EV_DestoryLineShield(line_t *line);
int             EV_SwitchTextureFree(line_t *line);
int             EV_ActivateSpecial(line_t *line);
void            P_SetSectorColor(line_t *line);
int             EV_AnimateDoor(line_t *line, mobj_t *thing);
// < d64tc

//
// P_CEILNG
//
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
    fixed_t         bottomheight;
    fixed_t         topheight;
    fixed_t         speed;
    boolean         crush;

    // 1 = up, 0 = waiting, -1 = down
    int             direction;

    // ID
    int             tag;
    int             olddirection;

    struct ceilinglist *list;   // jff 2/22/98 copied from killough's plats
} ceiling_t;

typedef struct ceilinglist {
    ceiling_t *ceiling;
    struct ceilinglist *next,**prev;
} ceilinglist_t;

#define CEILSPEED       FRACUNIT
#define CEILWAIT        150

extern ceilinglist_t *activeceilings;

int             EV_DoCeiling(line_t *line, ceiling_e type);

void            T_MoveCeiling(ceiling_t * ceiling);
void            P_AddActiveCeiling(ceiling_t * c);
void            P_RemoveActiveCeiling(ceiling_t * c);
void            P_RemoveAllActiveCeilings(void);
int             EV_CeilingCrushStop(line_t *line);
int             P_ActivateInStasisCeiling(line_t *line);

//
// P_FLOOR
//
typedef enum {
    // lower floor to highest surrounding floor
    lowerFloor,

    // lower floor to lowest surrounding floor
    lowerFloorToLowest,

    // lower floor to highest surrounding floor VERY FAST
    turboLower,

    lowerToEight, // d64tc
    customFloor, // d64tc
    customChangeSec, // d64tc

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
    raiseFloor32 // d64tc
} floor_e;

typedef enum {
    build8,                        // slowly build by 8
    turbo16                        // quickly build by 16
} stair_e;

typedef struct {
    thinker_t       thinker;
    floor_e         type;
    boolean         crush;
    sector_t       *sector;
    int             direction;
    int             newspecial;
    short           texture;
    fixed_t         floordestheight;
    fixed_t         speed;
} floormove_t;

#define FLOORSPEED      FRACUNIT

typedef enum {
    ok,
    crushed,
    pastdest
} result_e;

result_e        T_MovePlane(sector_t *sector, fixed_t speed, fixed_t dest,
                            boolean crush, int floorOrCeiling, int direction);

int             EV_BuildStairs(line_t *line, stair_e type);
int             EV_DoFloor(line_t *line, floor_e floortype);
void            T_MoveFloor(floormove_t * floor);

//
// P_TELEPT
//
#define         TELEFOGHEIGHT 0
int             EV_Teleport(line_t *line, int side, mobj_t *thing, boolean spawnFog);

int             EV_FadeSpawn(line_t *line, mobj_t *thing); // d64tc
int             EV_FadeAway(line_t *line, mobj_t *thing); // d64tc

#endif
