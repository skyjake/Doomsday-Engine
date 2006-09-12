/* $Id$
 * Global settings. Most of these are console variables.
 * Could use a thorough clean-up.
 */
#ifndef __JHEXEN_SETTINGS_H__
#define __JHEXEN_SETTINGS_H__

enum {
    HUD_MANA,
    HUD_HEALTH,
    HUD_ARTI
};

// This struct should be cleaned up. Currently some of the data isn't
// even used any more.

// WARNING: Do not use the boolean type. Its size can be either 1 or 4 bytes
//          depending on build settings.

typedef struct {
    float           playerMoveSpeed;
    int             chooseAndUse;
    int             inventoryNextOnUnuse;
    float           lookSpeed;
    float           turnSpeed;
    int             quakeFly;
    byte            fastMonsters;
    int             usemlook, usejlook;
    int             mouseSensiX, mouseSensiY;

    int             screenblocks;
    int             setblocks;
    byte            hudShown[4];   // HUD data visibility.
    float           hudScale;
    float           hudColor[4];
    float           hudIconAlpha;
    byte            usePatchReplacement;
    int             showFPS, lookSpring;
    int             mlookInverseY;
    int             echoMsg;
    int             translucentIceCorpse;

    byte            netMap, netClass, netColor, netSkill;
    byte            netEpisode;    // unused in Hexen
    byte            netDeathmatch, netNomonsters, netRandomclass;
    byte            netJumping;
    byte            netMobDamageModifier;   // multiplier for non-player mobj damage
    byte            netMobHealthModifier;   // health modifier for non-player mobjs
    int             netGravity;              // multiplayer custom gravity
    byte            netNoMaxZRadiusAttack;   // radius attacks are infinitely tall
    byte            netNoMaxZMonsterMeleeAttack;    // melee attacks are infinitely tall
    byte            overrideHubMsg; // skip the transition hub message when 1
    int             cameraNoClip;
    float           bobView, bobWeapon;
    byte            askQuickSaveLoad;
    int             jumpEnabled;
    float           jumpPower;
    int             airborneMovement;
    int             usemouse, noAutoAim, alwaysRun;
    byte            povLookAround;
    int             joySensitivity, jlookInverseY;
    int             joyaxis[8];
    int             jlookDeltaMode;

    int             xhair, xhairSize;
    byte            xhairColor[4];

    int             msgCount;
    float           msgScale;
    int             msgUptime;
    int             msgBlink;
    int             msgAlign;
    byte            msgShow;
    float           msgColor[3];
    byte            weaponAutoSwitch;
    int             weaponOrder[NUMWEAPONS];
    byte            weaponNextMode; // if true use the weaponOrder for next/previous.
    float           inventoryTimer; // Number of seconds until the invetory auto-hides.

    // Automap stuff.
/*    int             automapPos;
    float           automapWidth;
    float           automapHeight; */
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

    byte            counterCheat;
    float           counterCheatScale;

    int             snd_3D, messageson;
    char           *chat_macros[10];
    float           snd_ReverbFactor;
    byte            reverbDebug;

    int             dclickuse;
    int             plrViewHeight;
    int             levelTitle;
    float           menuScale;
    int             menuEffects;
    int             menuFog;
    float           menuGlitter;
    float           menuShadow;
    float           flashcolor[3];
    int             flashspeed;
    byte            turningSkull;
    float           menuColor[3];
    float           menuColor2[3];
    byte            menuSlam;

    pclass_t        PlayerClass[MAXPLAYERS];
    byte            PlayerColor[MAXPLAYERS];
} game_config_t;

extern game_config_t cfg;

extern char     SavePath[];

#endif                          //__JHEXEN_SETTINGS_H__
