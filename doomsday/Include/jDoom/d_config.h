/* $Id$
 * Global settings. Most of these are console variables.
 * Could use a thorough clean-up.
 */
#ifndef __JDOOM_SETTINGS_H__
#define __JDOOM_SETTINGS_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

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

// WARNING: Do not use the boolean type. Its size can be either 1 or 4 bytes
//          depending on build settings.

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
	byte            setsizeneeded;
	int             setblocks;
	int             screenblocks;
	int             showFPS, lookSpring;
	byte            povLookAround /* = false */ ;
	int             jumpEnabled /* = false */ ;
	float           jumpPower;
	int             airborneMovement;
	int             slidingCorpses;
	int             sbarscale;
	byte            echoMsg;
	int             lookSpeed;
	float           menuScale;
	int             menuEffects;
	int             menuFog;
	float           menuGlitter;
	float           menuShadow;
	int             menuQuitSound;
	byte            menuSlam;
	float           flashcolor[3];
	int             flashspeed;
	byte            turningSkull;
	byte            hudShown[6];   // HUD data visibility.
	float           hudScale;	   // How to scale HUD data?
	float           hudColor[4];
	float           hudIconAlpha;
	byte            usePatchReplacement;
	byte            snd_3D;
	byte            snd_ReverbFactor;	// 0..100.
	byte            reverbDebug;
	byte            moveCheckZ;	   // if true, mobjs can move over/under each other.
	byte            weaponAutoSwitch;
	byte            secretMsg;
	int             plrViewHeight;
	byte            levelTitle, hideAuthorIdSoft;
	float           menuColor[3];
	float           menuColor2[3];
	//byte            mobjInter;
	byte            noCoopDamage;
    byte            noTeamDamage;
	byte            noCoopWeapons;
    byte            noCoopAnything;
	byte            noNetBFG;
	byte            coopRespawnItems;
    byte            respawnMonstersNightmare;

	float           statusbarAlpha;
	float           statusbarCounterAlpha;

	// Compatibility options.
	byte            raiseghosts;
	byte            maxskulls;
	byte            allowskullsinwalls;

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
	int             automapHudDisplay;
	byte            automapShowDoors;
	float           automapDoorGlow;
	byte            automapBabyKeys;

	int             msgCount;
	float           msgScale;
	int             msgUptime;
	int             msgBlink;
	int             msgAlign;
	byte            msgShow;
	float           msgColor[3];

	char           *chat_macros[10];

	int             corpseTime;
	byte            customMusic;
	byte            killMessages;
	float           bobWeapon, bobView;
	byte            bobWeaponLower;
	int             cameraNoClip;

	// Crosshair.
	int             xhair, xhairSize;
	byte            xhairColor[4];

	// Network.
	byte            netDeathmatch;
    byte            netNomonsters;
    byte            netRespawn;
    byte            netJumping;
	byte            netEpisode;
    byte            netMap;
    byte            netSkill;
    byte            netSlot;
	byte            netColor;

	int             PlayerColor[MAXPLAYERS];
} jdoom_config_t;

extern jdoom_config_t cfg;

// Other variables.
extern int      screenblocks;

int             GetDefInt(char *def, int *returned_value);

#endif
