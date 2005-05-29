/* $Id$
 * Global settings. Most of these are console variables.
 * Could use a thorough clean-up.
 */
#ifndef __JDOOM_SETTINGS_H__
#define __JDOOM_SETTINGS_H__

enum {
	HUD_HEALTH,
	HUD_ARMOR,
	HUD_AMMO,
	HUD_KEYS,
	HUD_FRAGS,
	HUD_FACE
};

// This struct should be cleaned up. Currently some of the data isn't
// even used any more.

typedef struct jdoom_config_s {	   // All of these might not be used any more.
	float			playerMoveSpeed;
	int             mouseSensiX /* = 8 */ , mouseSensiY /* = 8 */ ;
	int             dclickuse /* = true */ ;
	int             usemlook;	   // Mouse look (mouse Y => viewpitch)
	int             usejlook;	   // Joy look (joy Y => viewpitch)
	int             alwaysRun;	   // Always run.
	int             noAutoAim;	   // No auto-aiming?
	int             mlookInverseY; // Inverse mlook Y axis.
	int             jlookInverseY; // Inverse jlook Y axis.
	int             joyaxis[8];
	int             jlookDeltaMode;
	boolean         setsizeneeded;
	int             setblocks;
	int             screenblocks;
	int             showFPS, lookSpring;
	boolean         povLookAround /* = false */ ;
	int             jumpEnabled /* = false */ ;
	float           jumpPower;
	int             airborneMovement;
	int             slidingCorpses;
	int             sbarscale;
	boolean         echoMsg;
	int             lookSpeed;
	float           menuScale;
	int             menuEffects;
	int             menuFog;
	float           menuGlitter;
	float           menuShadow;
	int             menuQuitSound;
	boolean		menuSlam;
	float           flashcolor[3];
	int             flashspeed;
	boolean         turningSkull;
	boolean         hudShown[6];   // HUD data visibility.
	float           hudScale;	   // How to scale HUD data?
	float           hudColor[4];
	float		hudIconAlpha;
	boolean         usePatchReplacement;
	boolean         snd_3D;
	byte            snd_ReverbFactor;	// 0..100.
	boolean         reverbDebug;
	boolean         moveCheckZ;	   // if true, mobjs can move over/under each other.
	boolean         weaponAutoSwitch;
	boolean         secretMsg;
	int             plrViewHeight;
	boolean         levelTitle, hideAuthorIdSoft;
	float           menuColor[3];
	float           menuColor2[3];
	boolean         mobjInter;
	boolean         noCoopDamage, noTeamDamage;
	boolean         noCoopWeapons, noCoopAnything;
	boolean         noNetBFG;
	boolean         coopRespawnItems;

	float		statusbarAlpha;
	float		statusbarCounterAlpha;

	//Compatibility options
	boolean		raiseghosts;
	boolean		maxskulls;
	boolean		allowskullsinwalls;

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
	int		automapHudDisplay;
	boolean         automapShowDoors;
	float           automapDoorGlow;
	boolean		automapBabyKeys;

	int             msgCount;
	float           msgScale;
	int             msgUptime;
	int             msgBlink;
	int		msgAlign;
	boolean         msgShow;
	float           msgColor[3];

	char           *chat_macros[10];

	int             corpseTime;
	boolean         customMusic;
	boolean         killMessages;
	float           bobWeapon, bobView;
	boolean         bobWeaponLower;
	int             cameraNoClip;

	// Crosshair.
	int             xhair, xhairSize;
	byte            xhairColor[4];

	// Network.
	boolean         netDeathmatch, netNomonsters, netRespawn, netJumping;
	byte            netEpisode, netMap, netSkill, netSlot;
	byte            netColor;

	int             PlayerColor[MAXPLAYERS];
} jdoom_config_t;

extern jdoom_config_t cfg;

// Other variables.
extern int      screenblocks;

int             GetDefInt(char *def, int *returned_value);

#endif
