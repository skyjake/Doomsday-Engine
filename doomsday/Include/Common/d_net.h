#ifndef __GAME_NETWORK_DEF_H__
#define __GAME_NETWORK_DEF_H__

#include "../dd_share.h"

#ifdef __JHERETIC__
#include "../jHeretic/Doomdef.h"
#endif

#ifdef __JHEXEN__
#define PLR_COLOR(pl, x)	(((unsigned)(x)) > 7? (pl) % 8 : (x))
#else
#define PLR_COLOR(pl, x)	(((unsigned)(x)) > 3? (pl) % 4 : (x))
#endif

// This playerstate is used to signal that a player should be removed
// from the world (he has quit netgame).
#define PST_GONE		0x1000

// Game packet types. (DON'T CHANGE THESE)
enum
{
	GPT_GAME_STATE = DDPT_FIRST_GAME_EVENT,
	GPT_WEAPON_FIRE,
	GPT_PLANE_MOVE,
	GPT_MESSAGE,			// Non-chat messages.
	GPT_CONSOLEPLAYER_STATE,
	GPT_PLAYER_STATE,
	GPT_PSPRITE_STATE,
	GPT_SOUND,
	GPT_SECTOR_SOUND,
	GPT_FLOOR_MOVE_SOUND,
	GPT_CEILING_MOVE_SOUND,
	GPT_INTERMISSION,
	GPT_FINALE,
	GPT_PLAYER_INFO,
	GPT_SAVE,
	GPT_LOAD,
	GPT_CLASS,				// jHexen: player class notification.
	GPT_CONSOLEPLAYER_STATE2,
	GPT_PLAYER_STATE2,
	GPT_YELLOW_MESSAGE,		// jHexen: yellow message.
	GPT_PAUSE,
	GPT_FINALE2,
	GPT_CHEAT_REQUEST,
	GPT_JUMP_POWER,			// Jump power (0 = no jumping)
};

// This packet is sent by servers to clients when the game state
// changes.
typedef struct
{
	byte	gamemode;
	byte	flags;
	byte	episode, map;
	byte	deathmatch : 2;
	byte	monsters : 1;
	byte	respawn : 1;
	byte	jumping : 1;
#if __JHEXEN__
	byte	randomclass : 1;
#endif
	byte	skill : 3;
	short	gravity;					// signed fixed-8.8
#if __JHEXEN__
    float   damagemod;					// netMobDamageModifier
    float   healthmod;					// netMobHealthModifier
#endif
} packet_gamestate_t;

// Game state flags.
#define GSF_CHANGE_MAP		0x01		// Level has changed.
#define GSF_CAMERA_INIT		0x02		// After gamestate follows camera init.
#define GSF_DEMO			0x04		// Only valid during demo playback.

// Player state update flags.
#define PSF_STATE			0x0001		// Dead or alive / armor type.
#define PSF_ARMOR_TYPE		0x0001		// Upper four bits of the 1st byte.
#define PSF_HEALTH			0x0002		
#define PSF_ARMOR_POINTS	0x0004
#define PSF_POWERS			0x0010
#define PSF_KEYS			0x0020
#define PSF_FRAGS			0x0040		
#define PSF_VIEW_HEIGHT		0x0080
#define PSF_OWNED_WEAPONS	0x0100
#define PSF_AMMO			0x0200
#define PSF_MAX_AMMO		0x0400
#define PSF_COUNTERS		0x0800		// Kill, item and secret counts.
#define PSF_PENDING_WEAPON	0x1000
#define PSF_READY_WEAPON	0x2000

// Player state update II flags.
#define PSF2_OWNED_WEAPONS	0x00000001
#define PSF2_STATE			0x00000002	// Includes cheatflags.

#ifdef __JDOOM__
#define PSF_REBORN			0x37f7
#endif

#ifdef __JHERETIC__
#define PSF_INVENTORY		0x0008		// ArtifactCount and invSlotNum, too.
#define PSF_CHICKEN_TIME	0x4000
#define PSF_REBORN			0x77ff
#endif

#ifdef __JHEXEN__
#define PSF_ARMOR			PSF_ARMOR_POINTS // For convenience.
#define PSF_WEAPONS			(PSF_PENDING_WEAPON | PSF_READY_WEAPON)
#define PSF_INVENTORY		0x0008		// ArtifactCount and invSlotNum, too.
#define PSF_MORPH_TIME		0x4000
#define PSF_LOCAL_QUAKE		0x8000
#define PSF_REBORN			0xf7ff
#endif

// Intermission flags.
#define IMF_BEGIN			0x01
#define IMF_END				0x02
#define IMF_STATE			0x04
#define IMF_TIME			0x08

// Finale flags.
#define FINF_BEGIN			0x01
#define FINF_END			0x02
#define FINF_SCRIPT			0x04		// Script included.
#define FINF_AFTER			0x08		// Otherwise before.
#define FINF_SKIP			0x10
#define FINF_OVERLAY		0x20		// Otherwise before (or after).

// Ticcmd flags.
#define CMDF_FORWARDMOVE	0x01
#define CMDF_SIDEMOVE		0x02
#define CMDF_ANGLE			0x04
#define CMDF_LOOKDIR		0x08
#define CMDF_BUTTONS		0x10
#define CMDF_LOOKFLY		0x20
#define CMDF_ARTI			0x40
#define CMDF_MORE_FLAGS		0x80		// Reserved for extensions.

// Networking.
int D_NetServerOpen(int before);
int D_NetServerClose(int before);
int D_NetServerStarted(int before);
int D_NetConnect(int before);
int D_NetDisconnect(int before);
int D_NetPlayerEvent(int plrNumber, int peType, void *data);
int D_NetWorldEvent(int type, int tic, void *data);
void D_HandlePacket(int fromplayer, int type, void *data, int length);
void D_NetConsoleRegistration(void);
void D_NetMessage(char *msg);
void D_NetMessageNoSound(char *msg);

// Console commands.
extern ccmd_t netCCmds[];

extern float netJumpPower;

#include "d_netsv.h"
#include "d_netcl.h"

#endif
