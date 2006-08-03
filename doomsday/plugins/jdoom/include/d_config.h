/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * d_config.h: jDoom configuration.
 * Global settings. Most of these are console variables.
 */

#ifndef __JDOOM_SETTINGS_H__
#define __JDOOM_SETTINGS_H__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "doomdef.h"

enum {
    HUD_HEALTH,
    HUD_ARMOR,
    HUD_AMMO,
    HUD_KEYS,
    HUD_FRAGS,
    HUD_FACE,
    NUMHUDDISPLAYS
};

// Hud Unhide Events (the hud will unhide on these events if enabled).
typedef enum {
    HUE_FORCE = -1,
    HUE_ON_DAMAGE,
    HUE_ON_PICKUP_HEALTH,
    HUE_ON_PICKUP_ARMOR,
    HUE_ON_PICKUP_POWER,
    HUE_ON_PICKUP_WEAPON,
    HUE_ON_PICKUP_AMMO,
    HUE_ON_PICKUP_KEY,
    NUMHUDUNHIDEEVENTS
} hueevent_t;

// WARNING: Do not use the boolean type. Its size can be either 1 or 4 bytes
//          depending on build settings.

typedef struct jdoom_config_s {
    float           playerMoveSpeed;
    int             mouseSensiX, mouseSensiY;
    int             dclickuse;
    int             usemlook;      // Mouse look (mouse Y => viewpitch)
    int             usejlook;      // Joy look (joy Y => viewpitch)
    int             alwaysRun;     // Always run.
    int             noAutoAim;     // No auto-aiming?
    int             mlookInverseY; // Inverse mlook Y axis.
    int             jlookInverseY; // Inverse jlook Y axis.
    int             joyaxis[8];
    int             jlookDeltaMode;
    int             lookSpring;
    float           lookSpeed;
    float           turnSpeed;
    byte            povLookAround;
    int             jumpEnabled;
    float           jumpPower;
    int             airborneMovement;
    byte            setsizeneeded;
    int             setblocks;
    int             screenblocks;
    byte            deathLookUp; // look up when killed
    int             slidingCorpses;
    int             sbarscale;
    byte            echoMsg;
    float           menuScale;
    int             menuEffects;
    int             menuFog;
    float           menuGlitter;
    float           menuShadow;
    int             menuQuitSound;
    byte            menuSlam;
    byte            askQuickSaveLoad;
    float           flashcolor[3];
    int             flashspeed;
    byte            turningSkull;
    byte            hudShown[NUMHUDDISPLAYS];   // HUD data visibility.
    float           hudScale;      // How to scale HUD data?
    float           hudColor[4];
    float           hudIconAlpha;
    float           hudTimer; // Number of seconds until the hud/statusbar auto-hides.
    byte            hudUnHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.
    byte            usePatchReplacement;
    byte            moveCheckZ;    // if true, mobjs can move over/under each other.
    byte            weaponAutoSwitch;
    byte            berserkAutoSwitch;
    int             weaponOrder[NUMWEAPONS];
    byte            weaponNextMode; // if true use the weaponOrder for next/previous.
    byte            secretMsg;
    int             plrViewHeight;
    byte            levelTitle, hideAuthorIdSoft;
    float           menuColor[3];
    float           menuColor2[3];
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
    // TODO: Put these into an array so we can use a bit array to change
    //       multiple options based on a compatibility mode (ala PrBoom).
    byte            raiseghosts;
    byte            maxskulls;
    byte            allowskullsinwalls;
    byte            anybossdeath;
    byte            monstersStuckInDoors;
    byte            avoidDropoffs;
    byte            moveBlock; // Dont handle large negative movement in P_TryMove.
    byte            wallRunNorthOnly; // If handle large make exception for wallrunning
    byte            zombiesCanExit; // Zombie players can exit levels.
    byte            fallOff; // Objects fall under their own weight.

    // Automap stuff.
    byte            counterCheat;
    float           counterCheatScale;
/*  int             automapPos;
    float           automapWidth;
    float           automapHeight;*/
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
    byte            killMessages;
    float           bobWeapon, bobView;
    byte            bobWeaponLower;
    int             cameraNoClip;

    // Crosshair.
    int             xhair, xhairSize;
    byte            xhairColor[4];

    // Network.
    byte            netDeathmatch;
    byte            netBFGFreeLook;    // allow free-aim with BFG
    byte            netMobDamageModifier;    // multiplier for non-player mobj damage
    byte            netMobHealthModifier;    // health modifier for non-player mobjs
    int             netGravity;              // multiplayer custom gravity
    byte            netNoMaxZRadiusAttack;   // radius attacks are infinitely tall
    byte            netNoMaxZMonsterMeleeAttack;    // melee attacks are infinitely tall
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
