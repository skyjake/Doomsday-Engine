/**\file d_config.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * jDoom64 configuration.
 * Global settings. Most of these are console variables.
 */

#ifndef __JDOOM64_SETTINGS_H__
#define __JDOOM64_SETTINGS_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

#include "doomdef.h"
#include "hu_lib.h"

enum {
    HUD_HEALTH,
    HUD_ARMOR,
    HUD_AMMO,
    HUD_KEYS,
    HUD_FRAGS,
    HUD_LOG,
    HUD_INVENTORY, // jd64
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

// Counter Cheat flags.
#define CCH_KILLS           0x01
#define CCH_ITEMS           0x02
#define CCH_SECRETS         0x04
#define CCH_KILLS_PRCNT     0x08
#define CCH_ITEMS_PRCNT     0x10
#define CCH_SECRETS_PRCNT   0x20

// WARNING: Do not use the boolean type. Its size can be either 1 or 4 bytes
//          depending on build settings.

typedef struct jdoom64_config_s {
    float           playerMoveSpeed;
    int             useJLook; // Joy look (joy Y => viewpitch).
    int             alwaysRun; // Always run.
    int             noAutoAim; // No auto-aiming?
    int             jLookDeltaMode;
    int             lookSpring;
    float           lookSpeed;
    float           turnSpeed;
    byte            povLookAround;
    int             jumpEnabled;
    float           jumpPower;
    int             airborneMovement;
    byte            setSizeNeeded;
    int             setBlocks;
    int             screenBlocks;
    byte            deathLookUp; // Look up when killed.
    byte            slidingCorpses;
    byte            fastMonsters;
    byte            echoMsg;
    int             hudFog;

    float           menuScale;
    int             menuEffectFlags;
    float           menuShadow;
    int             menuQuitSound;
    byte            menuSlam;
    byte            menuShortcutsEnabled;
    byte            menuScaleMode;
    int             menuPatchReplaceMode;
    byte            menuGameSaveSuggestName;
    byte            menuCursorRotate;
    float           menuTextColors[MENU_COLOR_COUNT][3];
    float           menuTextFlashColor[3];
    int             menuTextFlashSpeed;
    float           menuTextGlitter;

    byte            inludeScaleMode;
    int             inludePatchReplaceMode;

    byte            confirmQuickGameSave;
    byte            loadAutoSaveOnReborn;
    byte            loadLastSaveOnReborn;

    int             hudPatchReplaceMode;
    byte            hudShown[NUMHUDDISPLAYS]; // HUD data visibility.
    float           hudScale; // How to scale HUD data?
    float           hudColor[4];
    float           hudIconAlpha;
    float           hudTimer; // Number of seconds until the hud auto-hides.
    byte            hudUnHide[NUMHUDUNHIDEEVENTS]; // When the hud unhides.
    byte            moveCheckZ; // If true, mobjs can move over/under each other.
    byte            allowMonsterFloatOverBlocking; // if true, floating mobjs are allowed to climb over mobjs blocking the way.
    byte            weaponAutoSwitch;
    byte            noWeaponAutoSwitchIfFiring;
    byte            weaponCycleSequential; // if true multiple next/prev weapon impulses can be chained to allow the user to "count-click-switch".
    byte            weaponNextMode; // if true use the weaponOrder for next/previous.
    byte            ammoAutoSwitch;
    byte            berserkAutoSwitch;
    int             weaponOrder[NUM_WEAPON_TYPES];
    byte            weaponRecoil; // jd64
    byte            secretMsg;
    float           filterStrength;
    int             plrViewHeight;
    byte            mapTitle, hideIWADAuthor;
    byte            noCoopDamage;
    byte            noTeamDamage;
    byte            noCoopWeapons;
    byte            noCoopAnything;
    byte            noNetBFG;
    byte            coopRespawnItems;

    /**
     * Compatibility options.
     * \todo Put these into an array so we can use a bit array to change
     * multiple options based on a compatibility mode (ala PrBoom).
     */
    byte            maxSkulls;
    byte            allowSkullsInWalls;
    byte            anyBossDeath;
    byte            monstersStuckInDoors;
    byte            avoidDropoffs;
    byte            moveBlock; // Dont handle large negative movement in P_TryMoveXY.
    byte            wallRunNorthOnly; // If handle large make exception for wallrunning
    byte            zombiesCanExit; // Zombie players can exit maps.
    byte            fallOff; // Objects fall under their own weight.

    byte            hudShownCheatCounters;
    float           hudCheatCounterScale;
    byte            hudCheatCounterShowWithAutomap; ///< Only show when the automap is open.

    // Automap stuff.
/*  int             automapPos;
    float           automapWidth;
    float           automapHeight;*/
    float           automapMobj[3];
    float           automapL0[3];
    float           automapL1[3];
    float           automapL2[3];
    float           automapL3[3];
    float           automapBack[3];
    float           automapOpacity;
    float           automapLineAlpha;
    float           automapLineWidth; ///< In fixed 320x200 pixels.
    byte            automapRotate;
    int             automapHudDisplay;
    int             automapCustomColors;
    byte            automapShowDoors;
    float           automapDoorGlow;
    byte            automapBabyKeys;
    float           automapZoomSpeed;
    float           automapPanSpeed;
    byte            automapPanResetOnOpen;
    float           automapOpenSeconds;

    int             msgCount;
    float           msgScale;
    float           msgUptime;
    int             msgBlink;
    int             msgAlign;
    float           msgColor[3];

    char           *chatMacros[10];
    byte            chatBeep;

    int             corpseTime;
    byte            killMessages;
    float           bobWeapon, bobView;
    byte            bobWeaponLower;
    int             cameraNoClip;

    // Crosshair.
    int             xhair;
    float           xhairAngle;
    float           xhairSize;
    byte            xhairVitality;
    float           xhairColor[4];

    // Network.
    byte            netDeathmatch;
    byte            netBFGFreeLook; // Allow free-aim with BFG.
    byte            netMobDamageModifier; // Multiplier for non-player mobj damage.
    byte            netMobHealthModifier; // Health modifier for non-player mobjs.
    int             netGravity; // Custom gravity multiplier.
    byte            netNoMaxZRadiusAttack; // Radius attacks are infinitely tall.
    byte            netNoMaxZMonsterMeleeAttack; // Melee attacks are infinitely tall.
    byte            netNoMonsters;
    byte            netRespawn;
    byte            netJumping;
    byte            netMap;
    byte            netSkill;
    byte            netSlot;
    byte            netColor;

    int             playerColor[MAXPLAYERS];
} game_config_t;

extern game_config_t cfg;

#endif
