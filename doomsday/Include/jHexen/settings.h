// Common settings/defaults.

#ifndef __JHEXEN_SETTINGS_H__
#define __JHEXEN_SETTINGS_H__

extern char SavePath[];

typedef struct
{
	int	chooseAndUse, lookSpeed, quakeFly;
	boolean fastMonsters;
	int usemlook, usejlook;
	int	mouseSensiX, mouseSensiY;
	
	int screenblocks;
	int setblocks;
	int /*skyDetail, */ showFullscreenMana /*,mipmapping, linearRaw*/;
	int /*useDynLights, */showFPS, lookSpring;
	int /*snd_MusicDevice, */mlookInverseY;
	int echoMsg;//, simpleSky;
	int translucentIceCorpse;//, consoleAlpha, consoleLight;
	//int repWait1, repWait2;
	//int maxDynLights, dlBlend, missileBlend;
	//int haloMode, flareBoldness, flareSize;
	int /*alwaysAlign, */sbarscale;
	
	byte netMap, netClass, netColor, netSkill; 
	byte netEpisode; // unused in Hexen
	byte netDeathmatch, netNomonsters, netRandomclass;
	byte netMobDamageModifier; // multiplier for non-player mobj damage
	byte netMobHealthModifier; // health modifier for non-player mobjs
	byte overrideHubMsg; // skip the transition hub message when 1
	byte demoDisabled; // disable demos
	int cameraNoClip;
	float bobView, bobWeapon;
	
	boolean jumpEnabled;	// Always true
	float jumpPower;
	int usemouse, noAutoAim, alwaysRun;
	boolean povLookAround;
	int joySensitivity, jlookInverseY;
	int	joyaxis[8];
	int	jlookDeltaMode;
	
	int xhair, xhairSize;
	byte xhairColor[4];
	
	int snd_3D, messageson;
	char *chat_macros[10];
	float snd_ReverbFactor;
	boolean reverbDebug;
	
	int dclickuse;
	int mapTitle;
	float menuScale;

	pclass_t PlayerClass[MAXPLAYERS];
	byte PlayerColor[MAXPLAYERS];
}
jhexen_config_t;

extern jhexen_config_t cfg;

#endif //__JHEXEN_SETTINGS_H__

