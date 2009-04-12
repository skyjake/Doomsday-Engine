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
 * x_config.h:
 */

#ifndef __JHEXEN_CONFIG_H__
#define __JHEXEN_CONFIG_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

enum {
    HUD_MANA,
    HUD_HEALTH,
    HUD_ARTI,
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
    HUE_ON_PICKUP_INVITEM,
    NUMHUDUNHIDEEVENTS
} hueevent_t;

// This struct should be cleaned up. Currently some of the data isn't
// even used any more.

// WARNING: Do not use the boolean type. Its size can be either 1 or 4 bytes
//          depending on build settings.

typedef struct {
    int             mapTitle;
    float           menuScale;
    int             menuEffects;
    int             hudFog;
    float           menuGlitter;
    float           menuShadow;
    float           flashColor[3];
    int             flashSpeed;
    byte            turningSkull;
    float           menuColor[3];
    float           menuColor2[3];
    byte            menuSlam;
    byte            menuHotkeys;
    byte            askQuickSaveLoad;
    byte            usePatchReplacement;

    int             echoMsg;
} gameconfig_t;

typedef struct {
    int             color; // Player color default, preference.
    playerclass_t   pClass; // Player class default, preference.

    struct {
        float           moveSpeed;
        float           lookSpeed;
        float           turnSpeed;
        byte            airborneMovement; // 0..32
        byte            dclickUse;
        byte            useAutoAim;
        byte            alwaysRun;
    } ctrl;
    struct {
        int             blocks;
        int             setBlocks;
    } screen;
    struct {
        int             offsetZ; // Relative to mobj origin.
        float           bob;
        byte            povLookAround;
        byte            lookSpring;
        byte            useMLook;
        byte            useJLook;
        byte            jLookDeltaMode;
    } camera;
    struct {
        float           bob;
    } psprite;
    struct {
        int             scale;
        float           opacity;
        float           counterAlpha;
    } statusbar;
    struct {
        byte            shown[NUMHUDDISPLAYS]; // HUD data visibility.
        float           scale;
        float           color[4];
        float           iconAlpha;
        float           timer; // Number of seconds until the hud/statusbar auto-hides.
        byte            unHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.
    } hud;
    struct {
        int             type;
        float           size;
        byte            vitality;
        float           color[4];
    } xhair;
    struct {
        byte            weaponAutoSwitch;
        byte            noWeaponAutoSwitchIfFiring;
        byte            ammoAutoSwitch;
        int             weaponOrder[NUM_WEAPON_TYPES];
        byte            weaponNextMode; // if true use the weaponOrder for next/previous.

        float           timer; // Number of seconds until the inventory auto-hides.
        int             chooseAndUse;
        int             nextOnNoUse;
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
        byte            hudDisplay;
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
    byte            translucentIceCorpse;
} playerprofile_t;

typedef struct {
    byte            cameraNoClip;
    byte            fastMonsters;
    float           jumpPower;
    byte            deathmatch;
    byte            noMonsters;
    byte            randomClass;
    byte            jumpAllow;
    byte            mobDamageModifier; // Multiplier for non-player mobj damage.
    byte            mobHealthModifier; // Health modifier for non-player mobjs.
    int             gravityModifier; // Multiplayer custom gravity.
    byte            noMaxZRadiusAttack; // Radius attacks are infinitely tall.
    byte            noMaxZMonsterMeleeAttack; // Melee attacks are infinitely tall.
} gamerules_t;

typedef struct {
    playerprofile_t playerProfile;
    struct {
        playerclass_t   pClass; // Original class, current may differ.
        byte            color; // Current color.
    } players[MAXPLAYERS];

    byte            netEpisode; // Unused in jHexen.
    byte            netMap;
    byte            netSkill;

    gamerules_t     rules;
    gameconfig_t    cfg;
} game_state_t;

#define PLRPROFILE          (gs.playerProfile)
#define GAMERULES           (gs.rules)

extern game_state_t gs;

#endif
