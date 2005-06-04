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

typedef struct {
	float			playerMoveSpeed;
	boolean         setsizeneeded;
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
	boolean         hudShown[6];   		// HUD data visibility.
    	float           hudScale;
	float           hudColor[4];
	float			hudIconAlpha;
	boolean         usePatchReplacement;
	int             showFPS, lookSpring;
	int             mouseSensiX, mouseSensiY;
	boolean         povLookAround;
	int             echoMsg;
	int             dclickuse, lookSpeed;
	int             ringFilter;
	int             eyeHeight;
	float           menuScale;
	int             menuEffects;
	int             menuFog;
	float           menuGlitter;
	float           menuShadow;
	boolean		menuSlam;
	float           flashcolor[3];
	int             flashspeed;
	boolean         turningSkull;
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
	int		msgAlign;
	boolean         msgShow;
	float           msgColor[3];

	int             jumpEnabled;
	float           jumpPower;
	boolean         fastMonsters;
	boolean         customMusic;

	float		statusbarAlpha;
	float		statusbarCounterAlpha;

	char           *chat_macros[10];

	//Automap stuff
	boolean		counterCheat;
	float           counterCheatScale;
	int		automapPos;
	float		automapWidth;
	float		automapHeight;
	float           automapL0[3];
	float           automapL1[3];
	float           automapL2[3];
	float           automapL3[3];
	float           automapBack[4];
	float		automapLineAlpha;
	boolean         automapRotate;
	boolean		automapHudDisplay;
	boolean         automapShowDoors;
	float           automapDoorGlow;
	boolean		automapBabyKeys;

	// Networking.
	boolean         netDeathmatch, netNomonsters, netRespawn, netJumping;
	byte            netEpisode, netMap, netSkill, netSlot;
	byte            netColor;

	int             PlayerColor[MAXPLAYERS];
} jheretic_config_t;

extern jheretic_config_t cfg;	   // in g_game.c

#endif
