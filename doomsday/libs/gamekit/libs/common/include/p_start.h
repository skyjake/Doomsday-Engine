/** @file p_start.h Common player (re)spawning logic.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#ifndef LIBCOMMON_PLAYSIM_START_H
#define LIBCOMMON_PLAYSIM_START_H

#include "common.h"

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
# include "r_defs.h"
#else
# include "xddefs.h"
#endif

/**
 * Map Spot Flags (MSF):
 * @todo Commonize these flags and introduce translations where needed.
 *       See gamefw/mapspot.cpp.
 */
#define MSF_UNUSED1         0x00000001 // Appears in easy skill modes.
#define MSF_UNUSED2         0x00000002 // Appears in medium skill modes.
#define MSF_UNUSED3         0x00000004 // Appears in hard skill modes.

#if __JDOOM__ || __JDOOM64__
# define MSF_DEAF           0x00000008 // Thing is deaf.
#elif __JHERETIC__ || __JHEXEN__
# define MSF_AMBUSH         0x00000008 // Mobj will be deaf spawned deaf.
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
# define MSF_NOTSINGLE      0x00000010 // Appears in multiplayer game modes only.
#elif __JHEXEN__
# define MTF_DORMANT        0x00000010
#endif

#if __JDOOM__ || __JHERETIC__
# define MSF_NOTDM          0x00000020 // (BOOM) Can not be spawned in the Deathmatch gameMode.
# define MSF_NOTCOOP        0x00000040 // (BOOM) Can not be spawned in the Co-op gameMode.
#elif __JDOOM64__
# define MSF_DONTSPAWNATSTART 0x00000020 // Do not spawn this thing at map start.
# define MSF_SCRIPT_TOUCH   0x00000040 // Mobjs spawned from this spot will envoke a script when touched.
# define MSF_SCRIPT_DEATH   0x00000080 // Mobjs spawned from this spot will envoke a script on death.
# define MSF_SECRET         0x00000100 // A secret (bonus) item.
# define MSF_NOTARGET       0x00000200 // Mobjs spawned from this spot will not target their attacker when hurt.
# define MSF_NOTDM          0x00000400 // Can not be spawned in the Deathmatch gameMode.
# define MSF_NOTCOOP        0x00000800 // Can not be spawned in the Co-op gameMode.
#elif __JHEXEN__
# define MSF_FIGHTER        0x00000020
# define MSF_CLERIC         0x00000040
# define MSF_MAGE           0x00000080
# define MSF_NOTSINGLE      0x00000100
# define MSF_NOTCOOP        0x00000200
# define MSF_NOTDM          0x00000400
#endif

// The following are not currently implemented.
#define MSF_FRIENDLY        0x00001000 // (BOOM) friendly monster.
#define MSF_SHADOW          0x00002000 // (ZDOOM) Thing is 25% translucent.
#define MSF_INVISIBLE       0x00004000 // (ZDOOM) Makes the thing invisible.
#define MSF_STILL           0x00008000 // (ZDOOM) Thing stands still (only useful for specific Strife monsters or friendlies).

// New flags:
#define MSF_Z_FLOOR         0x20000000 // Spawn relative to floor height.
#define MSF_Z_CEIL          0x40000000 // Spawn relative to ceiling height (minus thing height).
#define MSF_Z_RANDOM        0x80000000 // Random point between floor and ceiling.

// Unknown flag mask:
#if __JDOOM__
#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_UNUSED1|MSF_UNUSED2|MSF_UNUSED3|MSF_DEAF|MSF_NOTSINGLE|MSF_NOTDM|MSF_NOTCOOP|MSF_FRIENDLY))
#elif __JDOOM64__
#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_UNUSED1|MSF_UNUSED2|MSF_UNUSED3|MSF_DEAF|MSF_NOTSINGLE|MSF_DONTSPAWNATSTART|MSF_SCRIPT_TOUCH|MSF_SCRIPT_DEATH|MSF_SECRET|MSF_NOTARGET|MSF_NOTDM|MSF_NOTCOOP))
#elif __JHERETIC__
#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_UNUSED1|MSF_UNUSED2|MSF_UNUSED3|MSF_AMBUSH|MSF_NOTSINGLE|MSF_NOTDM|MSF_NOTCOOP|MSF_FRIENDLY))
#elif __JHEXEN__
#define MASK_UNKNOWN_MSF_FLAGS (0xffffffff \
    ^ (MSF_UNUSED1|MSF_UNUSED2|MSF_UNUSED3|MSF_AMBUSH|MSF_DORMANT|MSF_FIGHTER|MSF_CLERIC|MSF_MAGE|MSF_GSINGLE|MSF_GCOOP|MSF_GDEATHMATCH|MSF_SHADOW|MSF_INVISIBLE|MSF_FRIENDLY|MSF_STILL))
#endif

typedef struct {
#if __JHEXEN__
    short tid;
#endif
    coord_t origin[3];
    angle_t angle;
    int doomEdNum;
    int skillModes;
    int flags;
#if __JHEXEN__
    byte special;
    byte arg1;
    byte arg2;
    byte arg3;
    byte arg4;
    byte arg5;
#endif
} mapspot_t;

typedef uint mapspotid_t;

typedef struct {
    int plrNum;
    uint entryPoint;
    mapspotid_t spot;
} playerstart_t;

DE_EXTERN_C uint numMapSpots;
DE_EXTERN_C mapspot_t *mapSpots;

#if __JHERETIC__
DE_EXTERN_C mapspotid_t *maceSpots;
DE_EXTERN_C uint maceSpotCount;
DE_EXTERN_C mapspotid_t* bossSpots;
DE_EXTERN_C uint bossSpotCount;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize various playsim related data and structures.
 */
void P_Init(void);

/**
 * Update playsim related data and structures. Should be called after
 * an engine/renderer reset.
 */
void P_Update(void);

void P_Shutdown(void);

/**
 * Reset all requested player class changes.
 */
void P_ResetPlayerRespawnClasses(void);

/**
 * Sets a new player class for a player. It will be applied when the player
 * respawns.
 */
void P_SetPlayerRespawnClass(int plrNum, playerclass_t pc);

/**
 * Returns the class of a player when respawning.
 *
 * @param plrNum  Player number.
 * @param clear   @c true when the change request should be cleared.
 *
 * @return  Current/updated class for the player.
 */
playerclass_t P_ClassForPlayerWhenRespawning(int plrNum, dd_bool clear);

/**
 * Given a doomednum, look up the associated mobj type.
 *
 * @param doomEdNum     Doom Editor (Thing) Number to look up.
 * @return              The associated mobj type if found else @c MT_NONE.
 */
mobjtype_t P_DoomEdNumToMobjType(int doomEdNum);

/**
 * Spawns all players, using the method appropriate for current game mode.
 * Called during map setup.
 */
void P_SpawnPlayers(void);

#if __JHERETIC__ || __JHEXEN__
/**
 * Only affects torches, which are often placed inside walls in the
 * original maps. The DOOM engine allowed these kinds of things but a
 * Z-buffer doesn't. Also turns the torches so they face the nearest line.
 */
void P_MoveThingsOutOfWalls(void);
#endif

#if __JHERETIC__
/**
 * @note Fails in some places, but works most of the time.
 */
void P_TurnGizmosAwayFromDoors(void);
#endif

#if __JHERETIC__
/**
 * Add a new map spot to the list of mace spawn spots.
 *
 * @param id  Unique identifier of the map spot to add.
 */
void P_AddMaceSpot(mapspotid_t id);

/**
 * Choose a random map spot from the list of mace spawn spots which passes
 * validation according to the current game rules configuration.
 *
 * @note Randomization depends on the seeded playsim RNG. Ensure to call
 * this at the correct time otherwise the RNG will "drift", resulting in
 * behavior which differs from that of the original game logic.
 *
 * @return  The chosen map spot; otherwise @c 0.
 */
const mapspot_t *P_ChooseRandomMaceSpot(void);

void P_AddBossSpot(mapspotid_t id);
#endif

void P_CreatePlayerStart(int defaultPlrNum, uint entryPoint, dd_bool deathmatch, mapspotid_t spot);

void P_DestroyPlayerStarts(void);

uint P_GetNumPlayerStarts(dd_bool deathmatch);

/**
 * @return  The correct start for the player. The start is in the given
 *          group for specified entry point.
 */
const playerstart_t *P_GetPlayerStart(uint entryPoint, int pnum, dd_bool deathmatch);

/**
 * Gives all the players in the game a playerstart.
 * Only needed in co-op games (start spots are random in deathmatch).
 */
void P_DealPlayerStarts(uint entryPoint);

/**
 * Called when a player is spawned into the map. Most of the player
 * structure stays unchanged between maps.
 */
void P_SpawnPlayer(int plrNum, playerclass_t pClass, coord_t x, coord_t y, coord_t z,
    angle_t angle, int spawnFlags, dd_bool makeCamera, dd_bool pickupItems);

/**
 * Spawns a player at one of the random death match spots.
 */
void G_DeathMatchSpawnPlayer(int playernum);

void P_RebornPlayerInMultiplayer(int plrNum);

/**
 * @return  @c false if the player cannot be respawned at the
 *          given location because something is occupying it.
 */
dd_bool P_CheckSpot(coord_t x, coord_t y);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_PLAYSIM_START_H */
