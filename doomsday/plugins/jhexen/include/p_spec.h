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

boolean     P_ExecuteLineSpecial(int special, byte *args, linedef_t *line,
                                 int side, mobj_t *mo);
boolean     P_ActivateLine(linedef_t *ld, mobj_t *mo, int side,
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

int         EV_BuildStairs(linedef_t *line, byte *args, int direction,
                           stairs_e type);
int         EV_FloorCrushStop(linedef_t *line, byte *args);

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

void        P_InitSky(int map);

#endif
