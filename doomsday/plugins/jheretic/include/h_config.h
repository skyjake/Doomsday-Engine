/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * h_config.h: jHeretic configuration.
 *
 * Global settings. Most of these are console variables.
 */

#ifndef __JHERETIC_CONFIG_H__
#define __JHERETIC_CONFIG_H__

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
    float           menuScale;
    int             menuEffects;
    int             hudFog;
    float           menuGlitter;
    float           menuShadow;
    byte            menuSlam;
    byte            menuHotkeys;
    byte            askQuickSaveLoad;
    float           flashColor[3];
    int             flashSpeed;
    byte            turningSkull;
    byte            usePatchReplacement;
    int             mapTitle;
    float           menuColor[3];
    float           menuColor2[3];

    int             echoMsg;
} gameconfig_t;

typedef struct {
    int             color; // User player color preference.

    struct {
        float           moveSpeed;
        float           lookSpeed;
        float           turnSpeed;
        byte            dclickUse;
        byte            alwaysRun;
        byte            useAutoAim;
        byte            airborneMovement; // 0..32
    } ctrl;
    struct {
        int             blocks;
        int             setBlocks;
        int             ringFilter;
    } screen;
    struct {
        float           bob;
        int             offsetZ;
        byte            povLookAround;
        byte            lookSpring;
        byte            useMLook; // Mouse look (mouse Y => viewpitch)
        byte            useJLook; // Joy look (joy Y => viewpitch)
        byte            jLookDeltaMode;
    } camera;
    struct {
        float           bob;
        byte            bobLower;
    } psprite;
    struct {
        int             scale;
        float           opacity;
        float           counterAlpha;
    } statusbar;
    struct {
        byte            shown[6];   // HUD data visibility.
        float           scale;      // How to scale HUD data?
        float           color[4];
        float           iconAlpha;
        float           timer; // Number of seconds until the hud/statusbar auto-hides.
        byte            unHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.
        byte            counterCheat;
        float           counterCheatScale;
        int             tomeCounter, tomeSound;
    } hud;
    struct {
        int             type;
        float           size;
        byte            vitality;
        float           color[4];
    } xhair;
    struct {
        int             chooseAndUse;
        int             nextOnNoUse;
        float           timer; // Number of seconds until the invetory auto-hides.
        byte            weaponAutoSwitch;
        byte            noWeaponAutoSwitchIfFiring;
        byte            ammoAutoSwitch;
        int             weaponOrder[NUM_WEAPON_TYPES];
        byte            weaponNextMode; // if true use the weaponOrder for next/previous.
        byte            artiSkip; // whether shift-enter skips an artifact
    } inventory;
    struct {
        float           mobj[3];
        float           line0[3];
        float           line1[3];
        float           line2[3];
        float           line3[3];
        float           background[3];
        float           opacity;
        float           lineAlpha;
        byte            rotate;
        int             hudDisplay;
        int             customColors;
        byte            showDoors;
        float           doorGlow;
        byte            babyKeys;
        float           zoomSpeed;
        float           panSpeed;
        byte            panResetOnOpen;
        float           openSeconds;
    } automap;
    struct {
        int             count;
        float           scale;
        int             upTime;
        int             blink;
        int             align;
        byte            show;
        float           color[3];
    } msgLog;
    struct {
        char*           macros[10];
        byte            playBeep;
    } chat;

    // Misc:
    int             corpseTime;
} playerprofile_t;

typedef struct {
    int             deathmatch; // Only if started as net death.

    float           turboMul; // multiplier for turbo
    byte            monsterInfight;
    byte            moveCheckZ; // if true, mobjs can move over/under each other.
    float           jumpPower;
    byte            slidingCorpses;
    byte            announceSecrets;
    byte            noCoopDamage;
    byte            noTeamDamage;
    byte            respawnMonstersNightmare;
    byte            monstersStuckInDoors;
    byte            avoidDropoffs;
    byte            moveBlock; // Dont handle large negative movement in P_TryMove.
    byte            wallRunNorthOnly; // If handle large make exception for wallrunning
    byte            fallOff; // Objects fall under their own weight.
    byte            cameraNoClip;
    byte            mobDamageModifier; // multiplier for non-player mobj damage
    byte            mobHealthModifier; // health modifier for non-player mobjs
    int             gravityModifier; // multiplayer custom gravity
    byte            noMaxZRadiusAttack; // radius attacks are infinitely tall
    byte            noMaxZMonsterMeleeAttack; // melee attacks are infinitely tall
    byte            noMonsters;
    byte            respawn;
    byte            jumpAllow;
    byte            fastMonsters;
} gamerules_t;

int             GetDefInt(char *def, int *returned_value);

#endif
