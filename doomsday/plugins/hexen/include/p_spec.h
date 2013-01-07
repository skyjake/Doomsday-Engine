/**\file p_spec.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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

#ifndef LIBHEXEN_P_SPEC_H
#define LIBHEXEN_P_SPEC_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "common.h"

#define MO_TELEPORTMAN          14

void P_InitLava(void);

// at map load
void P_SpawnSectorSpecialThinkers(void);
void P_SpawnLineSpecialThinkers(void);
void P_SpawnAllSpecialThinkers(void);

boolean P_ExecuteLineSpecial(int special, byte* args, LineDef* line, int side, mobj_t* mo);
boolean P_ActivateLine(LineDef* ld, mobj_t* mo, int side, int activationType);

void P_PlayerInSpecialSector(player_t* plr);
void P_PlayerOnSpecialFloor(player_t* plr);

/**
 * Parse an ANIMDEFS definition for flat/texture animations.
 */
void P_InitPicAnims(void);

void P_InitLightning(void);
void P_ForceLightning(void);
void P_AnimateLightning(void);

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

result_e    T_MovePlane(Sector *sector, float speed, coord_t dest,
                        int crush, int floorOrCeiling, int direction);

int         EV_BuildStairs(LineDef *line, byte *args, int direction,
                           stairs_e type);
int         EV_FloorCrushStop(LineDef *line, byte *args);

#define TELEFOGHEIGHTF          (32)

boolean     P_Teleport(mobj_t* mo, coord_t x, coord_t y, angle_t angle, boolean useFog);
boolean     EV_Teleport(int tid, mobj_t *thing, boolean fog);
void        P_ArtiTele(player_t *player);

extern mobjtype_t TranslateThingType[];

boolean     EV_ThingProjectile(byte *args, boolean gravity);
boolean     EV_ThingSpawn(byte *args, boolean fog);
boolean     EV_ThingActivate(int tid);
boolean     EV_ThingDeactivate(int tid);
boolean     EV_ThingRemove(int tid);
boolean     EV_ThingDestroy(int tid);

void P_InitSky(uint map);
void P_AnimateSky(void);

#endif /* LIBHEXEN_P_SPEC_H */
