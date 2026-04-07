/** @file p_xgsec.h  Extended generalized sector types.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_XG_SECTORTYPE_H
#define LIBCOMMON_XG_SECTORTYPE_H

#include "g_common.h"
#ifdef __cplusplus
#  include <de/vector.h>
#  include <doomsday/world/material.h>
#endif

#ifdef __cplusplus
class MapStateReader;
class MapStateWriter;
#endif

// Sector chain event types.
enum {
    XSCE_FLOOR,
    XSCE_CEILING,
    XSCE_INSIDE,
    XSCE_TICKER,
    XSCE_NUM_CHAINS,

    XSCE_FUNCTION
};

// Sector Type flags.
#define STF_GRAVITY         0x00000001 // Use custom gravity gravity.
#define STF_FRICTION        0x00000002 // Use custom friction.
#define STF_CRUSH           0x00000004
#define STF_PLAYER_WIND     0x00000008 // Wind affects players.
#define STF_OTHER_WIND      0x00000010
#define STF_MONSTER_WIND    0x00000020
#define STF_MISSILE_WIND    0x00000040
#define STF_ANY_WIND        0x00000018 // Player + nonplayer.
#define STF_ACT_TAG_MATERIALMOVE 0x00000080 // Materialmove from act tagged line.
#define STF_ACT_TAG_WIND    0x00000100
#define STF_FLOOR_WIND      0x00000200 // Wind only when touching the floor.
#define STF_CEILING_WIND    0x00000400

// Sector Chain Event flags.
#define SCEF_PLAYER_A       0x00000001 // Activate for players.
#define SCEF_OTHER_A        0x00000002 // Act. for non-players.
#define SCEF_MONSTER_A      0x00000004 // Countkills.
#define SCEF_MISSILE_A      0x00000008 // Missiles.
#define SCEF_ANY_A          0x00000010 // All mobjs.
#define SCEF_TICKER_A       0x00000020 // Activate by ticker.

#define SCEF_PLAYER_D       0x00000040 // Deactivate for players.
#define SCEF_OTHER_D        0x00000080 // Deact. for non-players.
#define SCEF_MONSTER_D      0x00000100 // Countkills.
#define SCEF_MISSILE_D      0x00000200 // Missiles.
#define SCEF_ANY_D          0x00000400 // All mobjs.
#define SCEF_TICKER_D       0x00000800 // Deactivate by ticker.

// Plane mover flags.
#define PMF_CRUSH                   0x1 // Crush things inside.

// (De)activate origin if move is blocked (when not crushing) and
// destroy mover. Normally the mover waits until it can move again.
#define PMF_ACTIVATE_ON_ABORT       0x2
#define PMF_DEACTIVATE_ON_ABORT     0x4

// (De)activate origin when move is done.
#define PMF_ACTIVATE_WHEN_DONE      0x8
#define PMF_DEACTIVATE_WHEN_DONE    0x10

#define PMF_OTHER_FOLLOWS           0x20 // Other plane follows.
#define PMF_WAIT                    0x40 // Wait until timer counts to 0.
#define PMF_SET_ORIGINAL            0x80 // Set origfloor/ceil.

// Only play sound for one sector.
#define PMF_ONE_SOUND_ONLY          0x100

typedef struct function_s {
    struct function_s *link; // Linked to another func?
    char *func;
    int flags;
    int pos;
    int repeat;
    int timer, maxTimer;
    int minInterval, maxInterval;
    float scale, offset;
    float value, oldValue;
} function_t;

enum {
    XGSP_FLOOR,
    XGSP_CEILING,

    XGSP_RED = 0,
    XGSP_GREEN,
    XGSP_BLUE
};

typedef struct {
    thinker_t thinker;
    Sector *sector;
} xsthinker_t;

typedef struct {
    dd_bool disabled;
    function_t rgb[3]; // Don't move the functions around in the struct.
    function_t plane[2];
    function_t light;
    sectortype_t info;
    int timer;
    int chainTimer[DDLT_MAX_CHAINS];
} xgsector_t;

typedef struct xgplanemover_s {
    thinker_t thinker;

    Sector *sector;
    dd_bool ceiling; // True if operates on the ceiling.

    int flags;
    Line *origin;

    coord_t destination;
    float speed;
    float crushSpeed; // Speed to use when crushing.

    world_Material *setMaterial; // Set material when move done.
    int setSectorType; // Sector type to set when move done
    // (-1 if no change).
    int startSound; // Played after waiting.
    int endSound; // Play when move done.
    int moveSound; // Sound to play while moving.
    int minInterval, maxInterval; // Sound playing intervals.
    int timer; // Counts down to zero.

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} xgplanemover_t;

#ifdef __cplusplus
extern "C" {
#endif

void XS_Init(void);
void XS_Update(void);

void XS_Thinker(void *xsThinker);

coord_t XS_Gravity(Sector *sector);
coord_t XS_Friction(const Sector *sector);

void XS_InitMovePlane(Line *line);

int C_DECL XSTrav_MovePlane(Sector *sector, dd_bool ceiling, void *context, void *context2, struct mobj_s *activator);

int C_DECL XSTrav_SectorType(Sector *sec, dd_bool ceiling, void *context, void *context2, struct mobj_s *activator);

int C_DECL XSTrav_SectorLight(Sector *sector, dd_bool ceiling, void *context, void *context2, struct mobj_s *activator);

int C_DECL XSTrav_PlaneMaterial(Sector *sec, dd_bool ceiling, void *context, void *context2, struct mobj_s *activator);

void XS_InitStairBuilder(Line *line);

int C_DECL XSTrav_BuildStairs(Sector *sector, dd_bool ceiling, void *context, void *context2, struct mobj_s *activator);

int C_DECL XSTrav_SectorSound(Sector *sec, dd_bool ceiling, void *context, void *context2, struct mobj_s *activator);

int C_DECL XSTrav_MimicSector(Sector *sector, dd_bool ceiling, void *context, void *context2, struct mobj_s *activator);

int C_DECL XSTrav_Teleport(Sector *sector, dd_bool ceiling, void *context, void *context2, struct mobj_s *activator);

void XS_SetSectorType(Sector *sec, int special);

xgplanemover_t *XS_GetPlaneMover(Sector *sector, dd_bool ceiling);

void XS_PlaneMover(xgplanemover_t *mover);  // A thinker for plane movers.

void SV_WriteXGSector(Sector *sec, Writer1 *writer);

void SV_ReadXGSector(Sector *sec, Reader1 *reader, int mapVersion);

D_CMD(MovePlane);

#ifdef __cplusplus
} // extern "C"

void XS_ChangePlaneMaterial(Sector &sector, bool ceiling, world_Material &newMaterial);

void XS_ChangePlaneColor(Sector &sector, bool ceiling, const de::Vec3f &newColor, bool isDelta = false);

#endif

#endif // LIBCOMMON_XG_SECTORTYPE_H
