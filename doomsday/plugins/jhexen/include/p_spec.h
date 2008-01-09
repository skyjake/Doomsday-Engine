/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2004-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 *
 *\attention  2006-09-16 - We should be able to use jDoom version of this as
 * a base for replacement. - Yagisan
 */

/**
 * p_spec.h:
 */

#ifndef __P_SPEC_H__
#define __P_SPEC_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#define MO_TELEPORTMAN          14

extern int *TerrainTypes;

void        P_InitTerrainTypes(void);
void        P_InitLava(void);

void        P_SpawnSpecials(void);

void        P_UpdateSpecials(void);

boolean     P_ExecuteLineSpecial(int special, byte *args, line_t *line,
                                 int side, mobj_t *mo);
boolean     P_ActivateLine(line_t *ld, mobj_t *mo, int side,
                           int activationType);

int         P_GetTerrainType(sector_t* sec, int plane);
int         P_FlatToTerrainType(int flatlumpnum);

void        P_PlayerInSpecialSector(player_t *plr);
void        P_PlayerOnSpecialFlat(player_t *plr, int floorType);

void        P_AnimateSurfaces(void);
void        P_InitPicAnims(void);
void        P_InitLightning(void);
void        P_ForceLightning(void);
void        R_HandleSectorSpecials(void);

typedef enum {
    LITE_RAISEBYVALUE,
    LITE_LOWERBYVALUE,
    LITE_CHANGETOVALUE,
    LITE_FADE,
    LITE_GLOW,
    LITE_FLICKER,
    LITE_STROBE
} lighttype_t;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    lighttype_t     type;
    float           value1;
    float           value2;
    int             tics1;  //// \todo Type LITEGLOW uses this as a third light value. As such, it has been left as 0 - 255 for now.
    int             tics2;
    int             count;
} light_t;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    int             index;
    float           baseValue;
} phase_t;

#define LIGHT_SEQUENCE_START    2
#define LIGHT_SEQUENCE          3
#define LIGHT_SEQUENCE_ALT      4

void        T_Phase(phase_t *phase);
void        T_Light(light_t *light);
void        P_SpawnPhasedLight(sector_t *sec, float base, int index);
void        P_SpawnLightSequence(sector_t *sec, int indexStep);
boolean     EV_SpawnLight(line_t *line, byte *arg, lighttype_t type);

typedef struct {
    char            name1[9];
    char            name2[9];
    int             soundID;
} switchlist_t;

typedef enum linesection_e{
    LS_MIDDLE,
    LS_BOTTOM,
    LS_TOP
} linesection_t;

typedef struct button_s {
    line_t         *line;
    linesection_t   section;
    int             texture;
    int             timer;
    mobj_t         *soundOrg;

    struct button_s *next;
} button_t;

#define BUTTONTIME              TICSPERSEC // 1 second

extern button_t *buttonlist;

void        P_FreeButtons(void);

void        P_ChangeSwitchTexture(line_t *line, int useAgain);
void        P_InitSwitchList(void);

typedef enum {
    up,
    down,
    waiting
    //  PLAT_IN_STASIS
} plat_e;

typedef enum {
    perpetualRaise,
    downWaitUpStay,
    downByValueWaitUpStay,
    upWaitDownStay,
    upByValueWaitDownStay
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
    int             crush;
    int             tag;
    plattype_e      type;

    struct platlist_s *list;
} plat_t;

typedef struct platlist_s {
  plat_t           *plat;
  struct platlist_s *next,**prev;
} platlist_t;

#define PLATWAIT                3
#define PLATSPEED               1

void        T_PlatRaise(plat_t *plat);
int         EV_DoPlat(line_t *line, byte *args, plattype_e type,
                      int amount);
void        P_AddActivePlat(plat_t *plat);
void        P_RemoveActivePlat(plat_t *plat);
void        P_RemoveAllActivePlats(void);
boolean     EV_StopPlat(line_t *line, byte *args);

typedef enum {
    normal,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins,
} vldoor_e;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    vldoor_e        type;
    float           topHeight;
    float           speed;
    int             direction; // 1 = up, 0 = waiting at top, -1 = down
    int             topWait; // tics to wait at the top (keep in case a door going down is reset)
    int             topCountDown; // when it reaches 0, start going down
} vldoor_t;

#define VDOORSPEED              1*2
#define VDOORWAIT               150

boolean     EV_VerticalDoor(line_t *line, mobj_t *thing);
int         EV_DoDoor(line_t *line, byte *args, vldoor_e type);
void        T_VerticalDoor(vldoor_t *door);

typedef enum {
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    lowerByValue,
    raiseByValue,
    crushRaiseAndStay,
    moveToValueTimes8
} ceiling_e;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    ceiling_e       type;
    float           bottomHeight;
    float           topHeight;
    float           speed;
    int             crush;
    int             direction; // 1 = up, 0 = waiting, -1 = down
    int             tag; // ID
    int             oldDirection;

    struct ceilinglist_s *list;
} ceiling_t;

typedef struct ceilinglist_s {
    ceiling_t      *ceiling;
    struct ceilinglist_s *next,**prev;
} ceilinglist_t;

#define CEILSPEED               1
#define CEILWAIT                150

int         EV_DoCeiling(line_t *line, byte *args, ceiling_e type);
void        T_MoveCeiling(ceiling_t *ceiling);
void        P_AddActiveCeiling(ceiling_t *c);
void        P_RemoveActiveCeiling(ceiling_t *c);
void        P_RemoveAllActiveCeilings(void);
int         EV_CeilingCrushStop(line_t *line, byte *args);

typedef enum {
    FLEV_LOWERFLOOR, // lower floor to highest surrounding floor
    FLEV_LOWERFLOORTOLOWEST, // lower floor to lowest surrounding floor
    FLEV_LOWERFLOORBYVALUE,
    FLEV_RAISEFLOOR, // raise floor to lowest surrounding CEILING
    FLEV_RAISEFLOORTONEAREST, // raise floor to next highest surrounding floor
    FLEV_RAISEFLOORBYVALUE,
    FLEV_RAISEFLOORCRUSH,
    FLEV_RAISEBUILDSTEP, // One step of a staircase
    FLEV_RAISEBYVALUETIMES8,
    FLEV_LOWERBYVALUETIMES8,
    FLEV_LOWERTIMES8INSTANT,
    FLEV_RAISETIMES8INSTANT,
    FLEV_MOVETOVALUETIMES8
} floor_e;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    floor_e         type;
    int             crush;
    int             direction;
    int             newSpecial;
    short           texture;
    float           floorDestHeight;
    float           speed;
    int             delayCount;
    int             delayTotal;
    float           stairsDelayHeight;
    float           stairsDelayHeightDelta;
    float           resetHeight;
    short           resetDelay;
    short           resetDelayCount;
    byte            textureChange;
} floormove_t;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    float           ceilingSpeed;
    float           floorSpeed;
    float           floorDest;
    float           ceilingDest;
    int             direction;
    int             crush;
} pillar_t;

typedef struct {
    thinker_t       thinker;
    sector_t       *sector;
    float           originalHeight;
    float           accumulator;
    float           accDelta;
    float           targetScale;
    float           scale;
    float           scaleDelta;
    int             ticker;
    int             state;
} floorWaggle_t;

#define FLOORSPEED              1

typedef enum {
    ok,
    crushed,
    pastdest
} result_e;

typedef enum {
    STAIRS_NORMAL,
    STAIRS_SYNC,
    STAIRS_PHASED
} stairs_e;

result_e    T_MovePlane(sector_t *sector, float speed, float dest,
                        int crush, int floorOrCeiling, int direction);

int         EV_BuildStairs(line_t *line, byte *args, int direction,
                           stairs_e type);
int         EV_DoFloor(line_t *line, byte *args, floor_e floortype);
void        T_MoveFloor(floormove_t *floor);
void        T_BuildPillar(pillar_t *pillar);
void        T_FloorWaggle(floorWaggle_t *waggle);
int         EV_BuildPillar(line_t *line, byte *args, boolean crush);
int         EV_OpenPillar(line_t *line, byte *args);
int         EV_DoFloorAndCeiling(line_t *line, byte *args, boolean raise);
int         EV_FloorCrushStop(line_t *line, byte *args);
boolean     EV_StartFloorWaggle(int tag, int height, int speed, int offset,
                                int timer);

#define TELEFOGHEIGHTF          (32)

boolean     P_Teleport(mobj_t *mo, float x, float y, angle_t angle,
                       boolean useFog);
boolean     EV_Teleport(int tid, mobj_t *thing, boolean fog);
void        P_ArtiTele(player_t *player);

extern mobjtype_t TranslateThingType[];

boolean     EV_ThingProjectile(byte *args, boolean gravity);
boolean     EV_ThingSpawn(byte *args, boolean fog);
boolean     EV_ThingActivate(int tid);
boolean     EV_ThingDeactivate(int tid);
boolean     EV_ThingRemove(int tid);
boolean     EV_ThingDestroy(int tid);

//#pragma pack()

#endif
