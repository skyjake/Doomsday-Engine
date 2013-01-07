/**\file p_mobj.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Map Objects, MObj, definition and handling.
 */

#ifndef LIBHEXEN_P_MOBJ_H
#define LIBHEXEN_P_MOBJ_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "p_terraintype.h"

#define NOMOM_THRESHOLD     (0.0001) // (integer) 0
#define WALKSTOP_THRESHOLD  (0.062484741) // FIX2FLT(0x1000-1)
#define DROPOFFMOM_THRESHOLD (0.25) // FRACUNIT/4
#define MAXMOM              (30) // 30*FRACUNIT
#define MAXMOMSTEP          (15) // 30*FRACUNIT/2

#define FRICTION_LOW        (0.97265625) // 0xf900
#define FRICTION_FLY        (0.91796875) // 0xeb00
#define FRICTION_NORMAL     (0.90625000) // 0xe800
#define FRICTION_HIGH       (0.41992187) // 0xd700/2

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

#define MF_SPECIAL      1          // call P_SpecialThing when touched
#define MF_SOLID        2
#define MF_SHOOTABLE    4
#define MF_NOSECTOR     8          // don't use the sector links
                                   // (invisible but touchable)
#define MF_NOBLOCKMAP   16         // don't use the blocklinks
                                   // (inert but displayable)
#define MF_AMBUSH       32
#define MF_JUSTHIT      64         // try to attack right back
#define MF_JUSTATTACKED 128        // take at least one step before attacking
#define MF_SPAWNCEILING 256        // hang from ceiling instead of floor
#define MF_NOGRAVITY    512        // don't apply gravity every tic

// movement flags
#define MF_DROPOFF      0x400      // allow jumps from high places
#define MF_PICKUP       0x800      // for players to pick up items
#define MF_NOCLIP       0x1000     // player cheat
#define MF_SLIDE        0x2000     // keep info about sliding along walls
#define MF_FLOAT        0x4000     // allow moves to any height, no gravity
#define MF_TELEPORT     0x8000     // don't cross lines or look at heights
#define MF_MISSILE      0x10000    // don't hit same species, explode on block

#define MF_ALTSHADOW    0x20000    // alternate fuzzy draw
#define MF_SHADOW       0x40000    // use fuzzy draw (shadow demons / invis)
#define MF_NOBLOOD      0x80000    // don't bleed when shot (use puff)
#define MF_CORPSE       0x100000   // don't stop moving halfway off a step
#define MF_INFLOAT      0x200000   // floating to a height for a move, don't
                                   // auto float to target's height

#define MF_COUNTKILL    0x400000   // count towards intermission kill total
#define MF_ICECORPSE    0x800000   // a frozen corpse (for blasting)

#define MF_SKULLFLY     0x1000000  // skull in flight
#define MF_NOTDMATCH    0x2000000  // don't spawn in death match (key cards)

#define MF_TRANSLATION  0x1c000000 /**  Player color to use (0-7 << MF_TRANSHIFT),
                                        use R_GetTranslation() to convert to tclass/tmap.
                                        @see Mobj_UpdateTranslationClassAndMap() */
#define MF_TRANSSHIFT   26         ///< Bitshift for table for player colormaps.

#define MF_LOCAL            0x20000000

// If this flag is set, the sprite is aligned with the view plane.
#define MF_BRIGHTEXPLODE    0x40000000  // Make this brightshadow when exploding.
#define MF_VIEWALIGN        0x80000000
#define MF_BRIGHTSHADOW     (MF_SHADOW|MF_ALTSHADOW)

// --- mobj.flags2 ---

#define MF2_LOGRAV          0x00000001  // alternate gravity setting
#define MF2_WINDTHRUST      0x00000002  // gets pushed around by the wind
                                        // specials
#define MF2_FLOORBOUNCE     0x00000004  // bounces off the floor
#define MF2_BLASTED         0x00000008  // missile will pass through ghosts
#define MF2_FLY             0x00000010  // fly mode is active
#define MF2_FLOORCLIP       0x00000020  // if feet are allowed to be clipped
#define MF2_SPAWNFLOAT      0x00000040  // spawn random float z
#define MF2_NOTELEPORT      0x00000080  // does not teleport
#define MF2_RIP             0x00000100  // missile rips through solid
                                        // targets
#define MF2_PUSHABLE        0x00000200  // can be pushed by other moving
                                        // mobjs
#define MF2_SLIDE           0x00000400  // slides against walls
//#define MF2_UNUSED1         0x00000800  // Formerly 'MF2_ONMOBJ'
#define MF2_PASSMOBJ        0x00001000  // Enable z block checking.  If on,
                                        // this flag will allow the mobj to
                                        // pass over/under other mobjs.
#define MF2_CANNOTPUSH      0x00002000  // cannot push other pushable mobjs
#define MF2_DROPPED         0x00004000  // dropped by a demon
#define MF2_BOSS            0x00008000  // mobj is a major boss
#define MF2_FIREDAMAGE      0x00010000  // does fire damage
#define MF2_NODMGTHRUST     0x00020000  // does not thrust target when
                                        // damaging
#define MF2_TELESTOMP       0x00040000  // mobj can stomp another
#define MF2_FLOATBOB        0x00080000  // use float bobbing z movement
#define MF2_DONTDRAW        0x00100000  // don't generate a vissprite
#define MF2_IMPACT          0x00200000  // an MF_MISSILE mobj can activate
                                        // SPAC_IMPACT
#define MF2_PUSHWALL        0x00400000  // mobj can push walls
#define MF2_MCROSS          0x00800000  // can activate monster cross lines
#define MF2_PCROSS          0x01000000  // can activate projectile cross lines
#define MF2_CANTLEAVEFLOORPIC 0x02000000    // stay within a certain floor type
#define MF2_NONSHOOTABLE    0x04000000  // mobj is totally non-shootable,
                                        // but still considered solid
#define MF2_INVULNERABLE    0x08000000  // mobj is invulnerable
#define MF2_DORMANT         0x10000000  // thing is dormant
#define MF2_ICEDAMAGE       0x20000000  // does ice damage
#define MF2_SEEKERMISSILE   0x40000000  // is a seeker (for reflection)
#define MF2_REFLECTIVE      0x80000000  // reflects missiles

// --- mobj.flags3 ---

#define MF3_NOINFIGHT       0x00000001  // Mobj will never be targeted for in-fighting
#define MF3_CLIENTACTION    0x00000002  // States' action funcs are executed by client

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

#define VALID_MOVEDIR(v)    ((v) >= DI_EAST && (v) <= DI_SOUTHEAST)

typedef struct mobj_s {
    // Defined in dd_share.h; required mobj elements.
    DD_BASE_MOBJ_ELEMENTS()

    // Hexen-specific data:
    struct player_s* player; // Only valid if type == MT_PLAYER
    int             damage; // For missiles
    int             special1; // Special info
    int             special2; // Special info
    int             moveDir; // 0-7
    int             moveCount; // When 0, select a new dir
    struct mobj_s*  target; // Thing being chased/attacked (or NULL)
                            // also the originator for missiles
                            // used by player to freeze a bit after
                            // teleporting
    int             threshold; // if > 0, the target will be chased
                               // no matter what (even if shot)
    int             lastLook; // player number last looked for
    short           tid; // thing identifier
    byte            special; // special
    union {
        byte        args[5]; // special arguments
        uint        argsUInt; // used with minotaur
    };
    int             turnTime; // $visangle-facetarget
    int             alpha; // $mobjalpha

    // Thing being chased/attacked for tracers.
    struct mobj_s*  tracer;

    // Used by lightning zap
    struct mobj_s*  lastEnemy;
} mobj_t;

mobj_t* P_SpawnMobjXYZ(mobjtype_t type, coord_t x, coord_t y, coord_t z, angle_t angle, int spawnFlags);
mobj_t* P_SpawnMobj(mobjtype_t type, coord_t const pos[3], angle_t angle, int spawnFlags);

void P_SpawnPuff(coord_t x, coord_t y, coord_t z, angle_t angle);
void P_SpawnBlood(coord_t x, coord_t y, coord_t z, int damage, angle_t angle);

void P_SpawnDirt(mobj_t* actor, coord_t radius);

void P_SpawnBloodSplatter(coord_t x, coord_t y, coord_t z, mobj_t* origin);
void P_SpawnBloodSplatter2(coord_t x, coord_t y, coord_t z, mobj_t* origin);

/**
 * @return  @c NULL, if the missile exploded immediately, otherwise returns a
 *          mobj_t pointer to the spawned missile.
 */
mobj_t* P_SpawnMissile(mobjtype_t type, mobj_t* source, mobj_t* dest);
mobj_t* P_SpawnMissileXYZ(mobjtype_t type, coord_t x, coord_t y, coord_t z, mobj_t* source, mobj_t* dest);
mobj_t* P_SpawnMissileAngle(mobjtype_t type, mobj_t* source, angle_t angle, coord_t momZ);
mobj_t* P_SpawnMissileAngleSpeed(mobjtype_t type, mobj_t* source, angle_t angle, coord_t momZ, float speed);

mobj_t* P_SpawnPlayerMissile(mobjtype_t type, mobj_t* source);

mobj_t* P_SPMAngle(mobjtype_t type, mobj_t* source, angle_t angle);
mobj_t* P_SPMAngleXYZ(mobjtype_t type, coord_t x, coord_t y, coord_t z, mobj_t* source, angle_t angle);

mobj_t* P_SpawnTeleFog(coord_t x, coord_t y, angle_t angle);

mobj_t* P_SpawnKoraxMissile(mobjtype_t type, coord_t x, coord_t y, coord_t z, mobj_t* source, mobj_t* dest);

void P_ExplodeMissile(mobj_t* mo);

#endif /// LIBHEXEN_P_MOBJ_H
