/** @file p_spec.h  Special map actions.
 *
 * @authors Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef LIBHEXEN_P_SPEC_H
#define LIBHEXEN_P_SPEC_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "common.h"

#define MO_TELEPORTMAN          14

DE_EXTERN_C mobj_t *P_LavaInflictor();
DE_EXTERN_C mobjtype_t TranslateThingType[];

#ifdef __cplusplus
extern "C" {
#endif

void P_InitLava(void);
void P_SpawnSectorSpecialThinkers(void);
void P_SpawnLineSpecialThinkers(void);
void P_SpawnAllSpecialThinkers(void);

dd_bool P_ExecuteLineSpecial(int special, byte args[5], Line *line, int side, mobj_t *mo);

dd_bool P_ActivateLine(Line *ld, mobj_t *mo, int side, int activationType);

void P_PlayerInSpecialSector(player_t *plr);
void P_PlayerOnSpecialFloor(player_t *plr);

void P_InitLightning(void);
void P_AnimateLightning(void);

dd_bool P_StartACScript(int scriptNumber, byte const args[4], struct mobj_s *activator,
    Line *line, int side);

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

/**
 * Move a plane (floor or ceiling) and check for crushing.
 */
result_e T_MovePlane(Sector *sector, float speed, coord_t dest, int crush, int floorOrCeiling, int direction);

int EV_BuildStairs(Line *line, byte *args, int direction, stairs_e type);
int EV_FloorCrushStop(Line *line, byte *args);

dd_bool P_Teleport(mobj_t *mo, coord_t x, coord_t y, angle_t angle, dd_bool useFog);
dd_bool EV_Teleport(int tid, mobj_t *thing, dd_bool fog);
void P_ArtiTele(player_t *player);

dd_bool EV_ThingProjectile(byte *args, dd_bool gravity);
dd_bool EV_ThingSpawn(byte *args, dd_bool fog);
dd_bool EV_ThingActivate(int tid);
dd_bool EV_ThingDeactivate(int tid);
dd_bool EV_ThingRemove(int tid);
dd_bool EV_ThingDestroy(int tid);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_P_SPEC_H
