#ifndef __HERETIC_SETTINGS_H__
#define __HERETIC_SETTINGS_H__

#include "Doomdef.h"

typedef struct
{
	boolean	setsizeneeded;
	int		setblocks;
	int		screenblocks;
	int		sbarscale;
	int		chooseAndUse;
	int		usemlook;		// Mouse look (mouse Y => viewpitch)
	int		usejlook;		// Joy look (joy Y => viewpitch)
	int		alwaysRun;		// Always run.
	int		noAutoAim;		// No auto-aiming?
	int		mlookInverseY;	// Inverse mlook Y axis.
	int		joyaxis[8];
	int		jlookDeltaMode;
	int		jlookInverseY;	// Inverse jlook Y axis.
	int		showFullscreenMana, showFullscreenArmor, tomeCounter, tomeSound;
	int		showFullscreenKeys;
	int		showFPS, lookSpring;
	int		mouseSensiX, mouseSensiY;
	boolean	povLookAround;
	int		echoMsg;
	int		dclickuse, lookSpeed;
	int		ringFilter;
	int		eyeHeight;
	float	menuScale;
	int		levelTitle;
	int		corpseTime;
	float	counterCheatScale;
	int		cameraNoClip;
	float	bobWeapon, bobView;
	
	int		xhair, xhairSize;
	byte	xhairColor[4];
	boolean	messageson;
	
	int		jumpEnabled;
	float	jumpPower;
	boolean	fastMonsters;
	boolean	counterCheat;
	boolean customMusic;
	
	// Networking.
	boolean	netDeathmatch, netNomonsters, netRespawn, netJumping;
	byte	netEpisode, netMap, netSkill, netSlot;
	byte	netColor;

	int		PlayerColor[MAXPLAYERS];
} jheretic_config_t;

extern jheretic_config_t cfg; // in g_game.c

#endif 
