/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/*
 * h_config.h: jHeretic configuration.
 * Global settings. Most of these are console variables.
 */

#ifndef __HERETIC_SETTINGS_H__
#define __HERETIC_SETTINGS_H__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "doomdef.h"

enum {
    HUD_AMMO,
    HUD_ARMOR,
    HUD_KEYS,
    HUD_HEALTH,
    HUD_ARTI
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
    HUE_ON_PICKUP_INVITEM,
    NUMHUDUNHIDEEVENTS
} hueevent_t;

// WARNING: Do not use the boolean type. Its size can be either 1 or 4 bytes
//          depending on build settings.

typedef struct jheretic_config_s {
    float           playerMoveSpeed;
    int             dclickuse;
    int             usemlook;      // Mouse look (mouse Y => viewpitch)
    int             usejlook;      // Joy look (joy Y => viewpitch)
    int             alwaysRun;     // Always run.
    int             noAutoAim;     // No auto-aiming?
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

    int             slidingCorpses;
    int             sbarscale;
    int             echoMsg;
    float           menuScale;
    int             menuEffects;
    int             menuFog;
    float           menuGlitter;
    float           menuShadow;

    byte            menuSlam;
    byte            askQuickSaveLoad;
    float           flashcolor[3];
    int             flashspeed;
    byte            turningSkull;
    byte            hudShown[6];   // HUD data visibility.
    float           hudScale;      // How to scale HUD data?
    float           hudColor[4];
    float           hudIconAlpha;
    float           hudTimer; // Number of seconds until the hud/statusbar auto-hides.
    byte            hudUnHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.
    byte            usePatchReplacement;
    byte            moveCheckZ;    // if true, mobjs can move over/under each other.
    byte            weaponAutoSwitch;
    byte            noWeaponAutoSwitchIfFiring;
    byte            ammoAutoSwitch;
    int             weaponOrder[NUM_WEAPON_TYPES];
    byte            weaponNextMode; // if true use the weaponOrder for next/previous.
    byte            secretMsg;
    int             plrViewHeight;
    int             levelTitle;
    float           menuColor[3];
    float           menuColor2[3];
    byte            respawnMonstersNightmare;






    float           statusbarAlpha;
    float           statusbarCounterAlpha;

    float           inventoryTimer; // Number of seconds until the invetory auto-hides.

    // Compatibility options.
    // TODO: Put these into an array so we can use a bit array to change
    //       multiple options based on a compatibility mode (ala PrBoom).
    byte            monstersStuckInDoors;
    byte            avoidDropoffs;
    byte            moveBlock; // Dont handle large negative movement in P_TryMove.
    byte            wallRunNorthOnly; // If handle large make exception for wallrunning

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
    byte            chatBeep;

    int             corpseTime;

    float           bobWeapon, bobView;
    byte            bobWeaponLower;
    int             cameraNoClip;

    // Crosshair.
    int             xhair, xhairSize;
    byte            xhairColor[4];

    // Network.
    byte            netDeathmatch;

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

    playerclass_t   playerClass[MAXPLAYERS];
    int             playerColor[MAXPLAYERS];

    // jHeretic specific
    int             ringFilter;
    int             chooseAndUse;
    int             inventoryNextOnUnuse;
    int             tomeCounter, tomeSound;
    byte            fastMonsters;
} game_config_t;

extern game_config_t cfg;      // in g_game.c

int             GetDefInt(char *def, int *returned_value);

#endif
