/* $Id$
 * Global settings. Most of these are console variables.
 * Could use a thorough clean-up.
 */
#ifndef __JDOOM_SETTINGS_H__
#define __JDOOM_SETTINGS_H__

enum 
{
	HUD_HEALTH,
	HUD_ARMOR,
	HUD_AMMO,
	HUD_KEYS,
	HUD_FRAGS
};

// This struct should be cleaned up. Currently some of the data isn't
// even used any more.

typedef struct jdoom_config_s
{ // All of these might not be used any more.
	int		mouseSensiX/* = 8*/, mouseSensiY/* = 8*/;
	int		dclickuse/* = true*/;
	int		usemlook;		// Mouse look (mouse Y => viewpitch)
	int		usejlook;		// Joy look (joy Y => viewpitch)
	int		alwaysRun;		// Always run.
	int		noAutoAim;		// No auto-aiming?
	int		mlookInverseY;	// Inverse mlook Y axis.
	int		jlookInverseY;	// Inverse jlook Y axis.
	int		joyaxis[8];
	int		jlookDeltaMode;
	int		showFPS, lookSpring;
	boolean povLookAround/* = false*/;
	int		jumpEnabled/* = false*/;
	float	jumpPower;
	int		airborneMovement;
	int		slidingCorpses;
	int		sbarscale;
	boolean	echoMsg;
	int		lookSpeed;
	float	menuScale;
	int		menuEffects;
	int		menuFog;
	float	menuGlitter;
	float	flashcolor[3];
	int		flashspeed;
	boolean	turningSkull;
	boolean	hudShown[5];	// HUD data visibility.
	float	hudScale;		// How to scale HUD data?
	float	hudColor[3];
	boolean	usePatchReplacement;
	boolean	snd_3D;
	byte	snd_ReverbFactor;	// 0..100.
	boolean reverbDebug;
	boolean moveCheckZ;		// if true, mobjs can move over/under each other.
	boolean	weaponAutoSwitch;
	boolean	secretMsg;
	int		plrViewHeight;
	boolean	counterCheat;
	boolean	levelTitle, hideAuthorIdSoft;
	float	menuColor[3];
	boolean	mobjInter;
	boolean noCoopDamage, noTeamDamage;
	boolean noCoopWeapons, noCoopAnything;
	boolean noNetBFG;
	boolean coopRespawnItems;
	float	automapAlpha, automapLineAlpha;
	int		automapRotate;
	boolean	automapShowDoors;
	float	automapDoorGlow;
	int		msgCount;
	float	msgScale;
	int		msgUptime;
	int		msgBlink;
	int		corpseTime;
	boolean	customMusic;
	boolean killMessages;
	float	bobWeapon, bobView;
	boolean	bobWeaponLower;
	int		cameraNoClip;

	// Crosshair.
	int		xhair, xhairSize;
	byte	xhairColor[4];

	// Network.
	boolean	netDeathmatch, netNomonsters, netRespawn, netJumping;
	byte	netEpisode, netMap, netSkill, netSlot;
	byte	netColor;

	int		PlayerColor[MAXPLAYERS];
}
jdoom_config_t;

extern jdoom_config_t	cfg;

// Other variables.
extern int	screenblocks;

int GetDefInt(char *def, int *returned_value);

#endif
