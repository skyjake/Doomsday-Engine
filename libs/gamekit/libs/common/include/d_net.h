/** @file d_net.h  Common code related to net games.
 *
 * Connecting to/from a netgame server. Netgame events (player and world) and
 * netgame commands.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCOMMON_NETWORK_DEF_H
#define LIBCOMMON_NETWORK_DEF_H

#include "doomsday.h"
#include <de/legacy/reader.h>
#include <de/legacy/writer.h>
#ifdef __cplusplus
#  include <de/string.h>
#  include <doomsday/uri.h>
#endif

#define NETBUFFER_MAXMESSAGE 255

#ifdef __JHEXEN__
#define PLR_COLOR(pl, x)    (((unsigned)(x)) > 7? (pl) % 8 : (x))
#else
#define PLR_COLOR(pl, x)    (((unsigned)(x)) > 3? (pl) % 4 : (x))
#endif

// This playerstate is used to signal that a player should be removed
// from the world (he has quit netgame).
#define PST_GONE        0x1000

// Game packet types. (DON'T CHANGE THESE)
enum {
    GPT_GAME_STATE = DDPT_FIRST_GAME_EVENT,
    GPT_WEAPON_FIRE,
    GPT_PLANE_MOVE,
    GPT_MESSAGE,                   // Non-chat messages.
    GPT_CONSOLEPLAYER_STATE,
    GPT_PLAYER_STATE,
    GPT_PSPRITE_STATE,
    GPT_SOUND,
    GPT_SECTOR_SOUND,
    GPT_FLOOR_MOVE_SOUND,
    GPT_CEILING_MOVE_SOUND,
    GPT_INTERMISSION,
    GPT_RESERVED1,                 // Old GPT_FINALE, now handled by the engine.
    GPT_PLAYER_INFO,
    GPT_SAVE,
    GPT_LOAD,
    GPT_CLASS,                     // jHexen: player class notification.
    GPT_CONSOLEPLAYER_STATE2,
    GPT_PLAYER_STATE2,
    GPT_YELLOW_MESSAGE,            // jHexen: yellow message.
    GPT_PAUSE,
    GPT_RESERVED2,                 // Old GPT_FINALE2, now handled by the engine.
    GPT_CHEAT_REQUEST,
    GPT_JUMP_POWER,                // Jump power (0 = no jumping)
    GPT_ACTION_REQUEST,
    GPT_PLAYER_SPAWN_POSITION,
    GPT_DAMAGE_REQUEST,            // Client requests damage on a target.
    GPT_MOBJ_IMPULSE,              // Momenum to apply on a mobj.
    GPT_FLOOR_HIT_REQUEST,
    GPT_MAYBE_CHANGE_WEAPON,       // Server suggests weapon change.
    GPT_FINALE_STATE,              // State of the InFine script.
    GPT_LOCAL_MOBJ_STATE,          // Set a state on a mobj and enable local actions.
    GPT_TOTAL_COUNTS,              // Total kill, item, secret counts in the map.
    GPT_DISMISS_HUDS               // Hide client's automap, inventory (added in 1.15)
};

#if 0
// This packet is sent by servers to clients when the game state
// changes.
typedef struct {
    byte            gameMode;
    byte            flags;
    byte            episode, map;
    byte            deathmatch:2;
    byte            monsters:1;
    byte            respawn:1;
    byte            jumping:1;
#if __JHEXEN__
    byte            randomclass:1;
#endif
    byte            skill:3;
    short           gravity;       // signed fixed-8.8
#if __JHEXEN__
    float           damagemod;     // netMobDamageModifier (UNUSED)
    float           healthmod;     // netMobHealthModifier (UNUSED)
#elif __JSTRIFE__
    float           damagemod;     // netMobDamageModifier (UNUSED)
    float           healthmod;     // netMobHealthModifier (UNUSED)
#endif
} packet_gamestate_t;
#endif

// Player action requests.
enum {
    GPA_FIRE = 1,
    GPA_USE = 2,
    GPA_CHANGE_WEAPON = 3,
    GPA_USE_FROM_INVENTORY = 4
};

// Game state flags.
#define GSF_CHANGE_MAP      0x01   // Map has changed.
#define GSF_CAMERA_INIT     0x02   // After gamestate follows camera init.
#define GSF_DEMO            0x04   // Only valid during demo playback.

// Player state update flags.
#define PSF_STATE           0x0001 // Dead or alive / armor type.
#define PSF_ARMOR_TYPE      0x0001 // Upper four bits of the 1st byte.
#define PSF_HEALTH          0x0002
#define PSF_ARMOR_POINTS    0x0004
#define PSF_INVENTORY       0x0008
#define PSF_POWERS          0x0010
#define PSF_KEYS            0x0020
#define PSF_FRAGS           0x0040
#define PSF_VIEW_HEIGHT     0x0080
#define PSF_OWNED_WEAPONS   0x0100
#define PSF_AMMO            0x0200
#define PSF_MAX_AMMO        0x0400
#define PSF_COUNTERS        0x0800 // Kill, item and secret counts.
#define PSF_PENDING_WEAPON  0x1000
#define PSF_READY_WEAPON    0x2000
#define PSF_MORPH_TIME      0x4000
#define PSF_LOCAL_QUAKE     0x8000

// Player state update II flags.
#define PSF2_OWNED_WEAPONS  0x00000001
#define PSF2_STATE          0x00000002  // Includes cheatflags.

#if __JDOOM__ || __JDOOM64__
#define PSF_REBORN          0x37f7
#endif

#ifdef __JHERETIC__
#define PSF_REBORN          0x77ff
#endif

#ifdef __JHEXEN__
#define PSF_ARMOR           PSF_ARMOR_POINTS    // For convenience.
#define PSF_WEAPONS         (PSF_PENDING_WEAPON | PSF_READY_WEAPON)
#define PSF_REBORN          0xf7ff
#endif

// Intermission flags.
#define IMF_BEGIN           0x01
#define IMF_END             0x02
#define IMF_STATE           0x04
#define IMF_TIME            0x08

// Ticcmd flags.
#define CMDF_FORWARDMOVE    0x01
#define CMDF_SIDEMOVE       0x02
#define CMDF_ANGLE          0x04
#define CMDF_LOOKDIR        0x08
#define CMDF_BUTTONS        0x10
#define CMDF_LOOKFLY        0x20
#define CMDF_ARTI           0x40
#define CMDF_CHANGE_WEAPON  0x80

#define CMDF_BTN_ATTACK     0x01
#define CMDF_BTN_USE        0x02
#define CMDF_BTN_JUMP       0x04
#define CMDF_BTN_PAUSE      0x08
#define CMDF_BTN_SUICIDE    0x10 // Now ignored in ticcmds

// Console commands.
DE_EXTERN_C ccmdtemplate_t netCCmds[];

DE_EXTERN_C float netJumpPower;

#ifdef __cplusplus
extern "C" {
#endif

Writer1 *D_NetWrite(void);

Reader1 *D_NetRead(const byte *buffer, size_t len);

void D_NetClearBuffer(void);

// Networking.
int D_NetServerOpen(int before);

/**
 * Called when a network server closes.
 *
 * Duties include:
 * Restoring global state variables
 */
int D_NetServerClose(int before);

/**
 * Called when the network server starts.
 *
 * Duties include:
 * Updating global state variables and initializing all players' settings
 */
int D_NetServerStarted(int before);

int D_NetConnect(int before);

int D_NetDisconnect(int before);

long int D_NetPlayerEvent(int plrNumber, int peType, void *data);

/**
 * Issues a damage request when a client is trying to damage another player's mobj.
 *
 * @return  @c true = no further processing of the damage should be done else, process the
 * damage as normally.
 */
dd_bool D_NetDamageMobj(struct mobj_s *target, struct mobj_s *inflictor, struct mobj_s *source, int damage);

int D_NetWorldEvent(int type, int tic, void *data);

void D_HandlePacket(int fromplayer, int type, void *data, size_t length);

void *D_NetWriteCommands(int numCommands, void *data);

void *D_NetReadCommands(size_t pktLength, void *data);

/**
 * Register the console commands and variables of the common netcode.
 */
void D_NetConsoleRegister(void);

/**
 * Show message on screen and play chat sound.
 *
 * @param msg  Ptr to the message to print.
 */
void D_NetMessage(int player, const char *msg);

/**
 * Show message on screen.
 *
 * @param msg
 */
void D_NetMessageNoSound(int player, const char *msg);

#ifdef __cplusplus
} // extern "C"

de::String D_NetDefaultEpisode();
res::Uri D_NetDefaultMap();
#endif

#endif  // LIBCOMMON_NETWORK_DEF_H
