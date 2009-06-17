/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_mobj.h: Map Objects, MObj, definition and handling.
 */

#ifndef __P_MOBJ_H__
#define __P_MOBJ_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "p_terraintype.h"
#include "h_think.h"
#include "info.h"

/**
 * Map Spot Flags (MSF):
 */
#define MSF_EASY            0x00000001 // Can be spawned in Easy skill modes.
#define MSF_MEDIUM          0x00000002 // Can be spawned in Medium skill modes.
#define MSF_HARD            0x00000004 // Can be spawned in Hard skill modes.
#define MSF_AMBUSH          0x00000008 // Mobj will be deaf spawned deaf.
#define MSF_NOTSINGLE       0x00000010 // (BOOM) Can not be spawned in single player gamemodes.
#define MSF_NOTDM           0x00000020 // (BOOM) Can not be spawned in the Deathmatch gameMode.
#define MSF_NOTCOOP         0x00000040 // (BOOM) Can not be spawned in the Co-op gameMode.
#define MSF_FRIENDLY        0x00000080 // (BOOM) friendly monster.

#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_EASY|MSF_MEDIUM|MSF_HARD|MSF_AMBUSH|MSF_NOTSINGLE|MSF_NOTDM|MSF_NOTCOOP|MSF_FRIENDLY))

// New flags:
#define MSF_Z_FLOOR         0x20000000 // Spawn relative to floor height.
#define MSF_Z_CEIL          0x40000000 // Spawn relative to ceiling height (minus thing height).
#define MSF_Z_RANDOM        0x80000000 // Random point between floor and ceiling.

typedef struct mapspot_s {
    float           pos[3];
    angle_t         angle;
    mobjtype_t      type;
    int             flags; // MSF_* flags.
} mapspot_t;

/**
 * Mobj flags
 *
 * \attention IMPORTANT - Keep this current!!!
 * LEGEND:
 * p    = Flag is persistent (never changes in-game).
 * i    = Internal use (not to be used in defintions).
 *
 * \todo Persistent flags (p) don't need to be included in save games or sent to
 * clients in netgames. We should collect those in to a const flags setting which
 * is set only once when the mobj is spawned.
 *
 * \todo All flags for internal use only (i) should be put in another var and the
 * flags removed from those defined in GAME/objects.DED
 */

// --- mobj.flags ---

#define MF_SPECIAL          0x00000001  // Call P_SpecialThing when touched.
#define MF_SOLID            0x00000002  // Blocks.
#define MF_SHOOTABLE        0x00000004  // Can be hit.
#define MF_NOSECTOR         0x00000008  // (p) Don't use the sector links (invisible but touchable).
#define MF_NOBLOCKMAP       0x00000010  // (p) Don't use the blocklinks (inert but displayable)
#define MF_AMBUSH           0x00000020  // Not to be activated by sound, deaf monster.
#define MF_JUSTHIT          0x00000040  // Will try to attack right back.
#define MF_JUSTATTACKED     0x00000080  // Will take at least one step before attacking.
#define MF_SPAWNCEILING     0x00000100  // (p) Hang from ceiling instead of stand on floor.
#define MF_NOGRAVITY        0x00000200  // Don't apply gravity (every tic).

// Movement flags.
#define MF_DROPOFF          0x00000400  // This allows jumps from high places.
#define MF_PICKUP           0x00000800  // For players, will pick up items.
#define MF_NOCLIP           0x00001000  // (i) Player cheat.
//#define MF_SLIDE          0x00002000  // unused
#define MF_FLOAT            0x00004000  // Allow moves to any height, no gravity.
#define MF_TELEPORT         0x00008000  // (p) Don't cross lines or look at heights on teleport.
#define MF_MISSILE          0x00010000  // Don't hit same species, explode on block.

#define MF_DROPPED          0x00020000  // (i) Dropped by a demon, not level spawned.
#define MF_SHADOW           0x00040000  // Use fuzzy draw (shadow demons or spectres).
#define MF_NOBLOOD          0x00080000  // Don't bleed when shot (use puff).
#define MF_CORPSE           0x00100000  // (i) Don't stop moving halfway off a step.
#define MF_INFLOAT          0x00200000  // Floating to a height for a move,
                                        //  don't auto float to target's height.

#define MF_COUNTKILL        0x00400000  // count towards intermission kill total
#define MF_COUNTITEM        0x00800000  // count towards intermission item total

#define MF_SKULLFLY         0x01000000  // (i) skull in flight.

#define MF_NOTDMATCH        0x02000000  // (p) not spawned in deathmatch mode (e.g. key cards).

#define MF_TRANSLATION      0x0c000000  // (i) if 0x4 0x8 or 0xc, use a translation
#define MF_TRANSSHIFT       26          // (N/A) table for player colormaps

#define MF_LOCAL            0x10000000  // (p) Won't be sent to clients.
#define MF_BRIGHTSHADOW     0x20000000
#define MF_BRIGHTEXPLODE    0x40000000  // Make this brightshadow when exploding.
#define MF_VIEWALIGN        0x80000000

/*
 * The following flags are obsolete in a particular mobj version.
 * They will automatically be cleared when loading an old save game.
 */
#define MF_V6OBSOLETE       (0x00002000) // (MF_SLIDE)

// --- mobj.flags2 ---

#define MF2_LOGRAV          0x00000001  // alternate gravity setting
#define MF2_WINDTHRUST      0x00000002  // (p) gets pushed around by the wind specials
                                        //
#define MF2_FLOORBOUNCE     0x00000004  // bounces off the floor
#define MF2_THRUGHOST       0x00000008  // (p) missile will pass through ghosts
#define MF2_FLY             0x00000010  // (i) fly mode is active
#define MF2_FLOORCLIP       0x00000020  // if feet are allowed to be clipped
#define MF2_SPAWNFLOAT      0x00000040  // (p) spawn random float z
#define MF2_NOTELEPORT      0x00000080  // does not teleport
#define MF2_RIP             0x00000100  // (p) missile rips through solid targets
#define MF2_PUSHABLE        0x00000200  // can be pushed by other moving mobjs
#define MF2_SLIDE           0x00000400  // slides against walls
#define MF2_ALWAYSLIT       0x00000800
#define MF2_PASSMOBJ        0x00001000  // Enable z block checking.  If on,
                                        // this flag will allow the mobj to
                                        // pass over/under other mobjs.
#define MF2_CANNOTPUSH      0x00002000  // cannot push other pushable mobjs
#define MF2_INFZBOMBDAMAGE  0x00004000  // (p) Don't check z height with radius attacks
#define MF2_BOSS            0x00008000  // (p) mobj is a major boss
#define MF2_FIREDAMAGE      0x00010000  // does fire damage
#define MF2_NODMGTHRUST     0x00020000  // does not thrust target when
                                        // damaging
#define MF2_TELESTOMP       0x00040000  // mobj can stomp another
#define MF2_FLOATBOB        0x00080000  // (p) use float bobbing z movement
#define MF2_DONTDRAW        0X00100000  // don't generate a vissprite

// --- mobj.flags3 ---

#define MF3_NOINFIGHT       0x00000001  // Mobj will never be targeted for in-fighting

// --- mobj.intflags --- (added in MOBJ_SAVEVERSION 6)
// Internal mobj flags cannot be set using an external definition.

#define MIF_FALLING         0x00000001  // $dropoff_fix: Object is falling from a ledge.

/*
 * end mobj flags
 */

// For torque simulation:
#define OVERDRIVE 6
#define MAXGEAR (OVERDRIVE+16)

typedef enum dirtype_s {
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
} dirtype_t;

// Map Object definition.
typedef struct mobj_s {
    // Defined in dd_share.h; required mobj elements.
    DD_BASE_MOBJ_ELEMENTS()

    // Heretic-specific data:
    mobjinfo_t     *info;           // &mobjinfo[mobj->type]
    int             damage;         // For missiles
    int             flags;
    int             flags2;         // Heretic flags
    int             flags3;
    int             special1;       // Special info
    int             special2;       // Special info
    int             special3;       // Special info
    int             health;
    int             moveDir;        // 0-7
    int             moveCount;      // when 0, select a new dir
    struct mobj_s  *target;         // thing being chased/attacked (or NULL)
    // also the originator for missiles
    // used by player to freeze a bit after
    // teleporting
    int             threshold;      // if >0, the target will be chased

    int             intFlags;       // $dropoff_fix: internal flags
    float           dropOffZ;       // $dropoff_fix
    short           gear;           // used in torque simulation
    boolean         wallRun;        // true = last move was the result of a wallrun

    // no matter what (even if shot)
    struct player_s *player;        // only valid if type == MT_PLAYER
    int             lastLook;       // player number last looked for

    // For nightmare/multiplayer respawn.
    struct {
        float           pos[3];
        angle_t         angle;
        int             flags; // MSF_* flags.
    } spawnSpot;

    // Thing being chased/attacked for tracers.
    struct mobj_s  *tracer;

    // Used for pod generating.
    struct mobj_s  *generator;

    int             turnTime;       // $visangle-facetarget
    int             corpseTics;     // $vanish: how long has this been dead?
} mobj_t;

typedef struct polyobj_s {
    // Defined in dd_share.h; required polyobj elements.
    DD_BASE_POLYOBJ_ELEMENTS()

    // Heretic-specific data:
} polyobj_t;

extern uint numMapSpots;
extern mapspot_t* mapSpots;

mobj_t*     P_SpawnMobj3f(mobjtype_t type, float x, float y, float z,
                          angle_t angle, int spawnFlags);
mobj_t*     P_SpawnMobj3fv(mobjtype_t type, const float pos[3],
                           angle_t angle, int spawnFlags);

void        P_SpawnPuff(float x, float y, float z, angle_t angle);
void        P_SpawnBlood(float x, float y, float z, int damage,
                         angle_t angle);
mobj_t*     P_SpawnMissile(mobjtype_t type, mobj_t* source, mobj_t* dest,
                           boolean checkSpawn);
mobj_t*     P_SpawnMissileAngle(mobjtype_t type, mobj_t *source,
                                angle_t angle, float momz);
mobj_t*     P_SpawnTeleFog(float x, float y, angle_t angle);

const terraintype_t* P_MobjGetFloorTerrainType(mobj_t *thing);
float       P_MobjGetFriction(mobj_t *mo);

#endif
