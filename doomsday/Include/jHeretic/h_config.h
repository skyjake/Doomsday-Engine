/* $Id$
 * Global settings. Most of these are console variables.
 * Could use a thorough clean-up.
 */
#ifndef __HERETIC_SETTINGS_H__
#define __HERETIC_SETTINGS_H__

#include "Doomdef.h"

enum {
	HUD_AMMO,
	HUD_ARMOR,
	HUD_KEYS,
	HUD_HEALTH,
	HUD_ARTI
};

// This struct should be cleaned up. Currently some of the data isn't
// even used any more.

// WARNING: Do not use the boolean type. Its size can be either 1 or 4 bytes
//          depending on build settings.

typedef struct {
	float			playerMoveSpeed;
	byte            setsizeneeded;
	int             setblocks;
	int             screenblocks;
	int             sbarscale;
	int             chooseAndUse;
	int             usemlook;		// Mouse look (mouse Y => viewpitch)
	int             usejlook;		// Joy look (joy Y => viewpitch)
	int             alwaysRun;		// Always run.
	int             noAutoAim;		// No auto-aiming?
	int             mlookInverseY;		// Inverse mlook Y axis.
	int             joyaxis[8];
	int             jlookDeltaMode;
	int             jlookInverseY;		// Inverse jlook Y axis.
	int             tomeCounter, tomeSound;
	byte            hudShown[6];   		// HUD data visibility.
    float           hudScale;
	float           hudColor[4];
	float			hudIconAlpha;
	byte            usePatchReplacement;
	int             showFPS, lookSpring;
	int             mouseSensiX, mouseSensiY;
	byte            povLookAround;
	int             echoMsg;
	int             dclickuse, lookSpeed;
	int             ringFilter;
	int             eyeHeight;
	float           menuScale;
	int             menuEffects;
	int             menuFog;
	float           menuGlitter;
	float           menuShadow;
	byte            menuSlam;
	float           flashcolor[3];
	int             flashspeed;
	byte            turningSkull;
	float           menuColor[3];
	float           menuColor2[3];
	int             levelTitle;
	int             corpseTime;
	int             cameraNoClip;
	float           bobWeapon, bobView;

	int             xhair, xhairSize;
	byte            xhairColor[4];

	int             msgCount;
	float           msgScale;
	int             msgUptime;
	int             msgBlink;
	int             msgAlign;
	byte            msgShow;
	float           msgColor[3];

	int             jumpEnabled;
	float           jumpPower;
	byte            fastMonsters;
	byte            customMusic;

	float           statusbarAlpha;
	float           statusbarCounterAlpha;

	char           *chat_macros[10];

	// Automap stuff.
	byte            counterCheat;
	float           counterCheatScale;
	int             automapPos;
	float           automapWidth;
	float           automapHeight;
	float           automapL0[3];
	float           automapL1[3];
	float           automapL2[3];
	float           automapL3[3];
	float           automapBack[4];
	float           automapLineAlpha;
	byte            automapRotate;
	byte            automapHudDisplay;
	byte            automapShowDoors;
	float           automapDoorGlow;
    byte            automapBabyKeys;

	// Networking.
	byte            netDeathmatch, netNomonsters, netRespawn, netJumping;
	byte            netEpisode, netMap, netSkill, netSlot;
	byte            netColor;

	int             PlayerColor[MAXPLAYERS];
} jheretic_config_t;

extern jheretic_config_t cfg;	   // in g_game.c

#endif
