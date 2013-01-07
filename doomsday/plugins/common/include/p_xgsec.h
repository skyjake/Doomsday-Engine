/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * p_xgsec.c: Extended Generalized Sector Types.
 */

#ifndef __XG_SECTORTYPE_H__
#define __XG_SECTORTYPE_H__

#include "g_common.h"

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
    struct function_s* link; // Linked to another func?
    char*           func;
    int             flags;
    int             pos;
    int             repeat;
    int             timer, maxTimer;
    int             minInterval, maxInterval;
    float           scale, offset;
    float           value, oldValue;
} function_t;

enum {
    XGSP_FLOOR,
    XGSP_CEILING,

    XGSP_RED = 0,
    XGSP_GREEN,
    XGSP_BLUE
};

typedef struct {
    thinker_t       thinker;
    Sector*         sector;
} xsthinker_t;

typedef struct {
    boolean         disabled;
    function_t      rgb[3]; // Don't move the functions around in the struct.
    function_t      plane[2];
    function_t      light;
    sectortype_t    info;
    int             timer;
    int             chainTimer[DDLT_MAX_CHAINS];
} xgsector_t;

typedef struct {
    thinker_t       thinker;

    struct sector_s* sector;
    boolean         ceiling; // True if operates on the ceiling.

    int             flags;
    struct linedef_s* origin;

    coord_t         destination;
    float           speed;
    float           crushSpeed; // Speed to use when crushing.

    material_t*     setMaterial; // Set material when move done.
    int             setSectorType; // Sector type to set when move done
    // (-1 if no change).
    int             startSound; // Played after waiting.
    int             endSound; // Play when move done.
    int             moveSound; // Sound to play while moving.
    int             minInterval, maxInterval; // Sound playing intervals.
    int             timer; // Counts down to zero.
} xgplanemover_t;

void            XS_Init(void);
void            XS_Update(void);

void            XS_Thinker(xsthinker_t* xs);

coord_t         XS_Gravity(struct sector_s *sector);
coord_t         XS_Friction(struct sector_s *sector);
coord_t         XS_ThrustMul(struct sector_s *sector);

void            XS_InitMovePlane(struct linedef_s *line);
int C_DECL      XSTrav_MovePlane(struct sector_s *sector, boolean ceiling,
                                 void *context, void *context2, struct mobj_s *activator);
int C_DECL      XSTrav_SectorType(struct sector_s *sec, boolean ceiling,
                                  void *context, void *context2, struct mobj_s *activator);
int C_DECL      XSTrav_SectorLight(struct sector_s *sector, boolean ceiling,
                                   void *context, void *context2, struct mobj_s *activator);
int C_DECL      XSTrav_PlaneMaterial(struct sector_s *sec, boolean ceiling,
                                     void *context, void *context2, struct mobj_s *activator);
void            XS_InitStairBuilder(struct linedef_s *line);
int C_DECL      XSTrav_BuildStairs(struct sector_s *sector, boolean ceiling,
                                   void *context, void *context2, struct mobj_s *activator);
int C_DECL      XSTrav_SectorSound(struct sector_s *sec, boolean ceiling,
                                   void *context, void *context2, struct mobj_s *activator);
int C_DECL      XSTrav_MimicSector(struct sector_s *sector, boolean ceiling,
                                   void *context, void *context2, struct mobj_s *activator);
int C_DECL      XSTrav_Teleport(struct sector_s *sector, boolean ceiling,
                                   void *context, void *context2, struct mobj_s *activator);
void            XS_SetSectorType(struct sector_s *sec, int special);
void            XS_ChangePlaneMaterial(struct sector_s *sector, boolean ceiling,
                                       material_t* mat, float *rgb);
xgplanemover_t *XS_GetPlaneMover(struct sector_s *sector, boolean ceiling);
void            XS_PlaneMover(xgplanemover_t *mover);  // A thinker for plane movers.

void            SV_WriteXGSector(struct sector_s *sec);
void            SV_ReadXGSector(struct sector_s *sec);
void            SV_WriteXGPlaneMover(thinker_t *th);
int             SV_ReadXGPlaneMover(xgplanemover_t* mov);

D_CMD(MovePlane);

#endif
