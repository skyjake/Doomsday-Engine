/**\file x_config.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef __JHEXEN_CONFIG_H__
#define __JHEXEN_CONFIG_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "h2def.h"
#include "hu_lib.h"

enum {
    HUD_MANA,
    HUD_HEALTH,
    HUD_READYITEM,
    HUD_LOG,
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
    float           playerMoveSpeed;
    float           lookSpeed;
    float           turnSpeed;
    int             quakeFly;
    byte            fastMonsters;
    int             useJLook;
    int             screenBlocks;
    int             setBlocks;

    int             hudPatchReplaceMode;
    byte            hudShown[4]; // HUD data visibility.
    float           hudScale;
    float           hudColor[4];
    float           hudIconAlpha;
    float           hudTimer; // Number of seconds until the hud/statusbar auto-hides.
    byte            hudUnHide[NUMHUDUNHIDEEVENTS]; // when the hud/statusbar unhides.
    int             showFPS, lookSpring;
    int             mlookInverseY;
    int             echoMsg;
    int             translucentIceCorpse;

    byte            overrideHubMsg; // skip the transition hub message when 1
    int             cameraNoClip;
    float           bobView, bobWeapon;

    byte            confirmQuickGameSave;
    byte            loadLastSaveOnReborn;

    int             jumpEnabled;
    float           jumpPower;
    int             airborneMovement;
    int             useMouse, noAutoAim, alwaysRun;
    byte            povLookAround;
    int             jLookDeltaMode;

    int             xhair;
    float           xhairAngle;
    float           xhairSize;
    byte            xhairVitality;
    float           xhairColor[4];

    float           statusbarScale;
    float           statusbarOpacity;
    float           statusbarCounterAlpha;

    int             msgCount;
    float           msgScale;
    float           msgUptime;
    int             msgBlink;
    int             msgAlign;
    float           msgColor[3];
    byte            weaponAutoSwitch;
    byte            noWeaponAutoSwitchIfFiring;
    byte            ammoAutoSwitch;
    byte            allowMonsterFloatOverBlocking; // if true, floating mobjs are allowed to climb over mobjs blocking the way.
    byte            weaponCycleSequential; // if true multiple next/prev weapon impulses can be chained to allow the user to "count-click-switch".
    int             weaponOrder[NUM_WEAPON_TYPES];
    byte            weaponNextMode; // if true use the weaponOrder for next/previous.
    float           filterStrength;

    byte            hudShownCheatCounters;
    float           hudCheatCounterScale;
    byte            hudCheatCounterShowWithAutomap; ///< Only show when the automap is open.

    // Automap stuff.
/*    int             automapPos;
    float           automapWidth;
    float           automapHeight; */
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
    byte            automapHudDisplay;
    int             automapCustomColors;
    byte            automapShowDoors;
    float           automapDoorGlow;
    byte            automapBabyKeys;
    float           automapZoomSpeed;
    float           automapPanSpeed;
    byte            automapPanResetOnOpen;
    float           automapOpenSeconds;

    int             messagesOn;
    char*           chatMacros[10];
    byte            chatBeep;
    int             snd3D;
    float           sndReverbFactor;
    byte            reverbDebug;

    int             plrViewHeight;
    byte            mapTitle, hideIWADAuthor;
    int             hudFog;

    float           menuScale;
    int             menuEffectFlags;
    float           menuShadow;

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

    byte            netMap, netClass, netColor, netSkill;
    byte            netEpisode; // Unused in Hexen.
    byte            netDeathmatch, netNoMonsters, netRandomClass;
    byte            netJumping;
    byte            netMobDamageModifier; // Multiplier for non-player mobj damage.
    byte            netMobHealthModifier; // Health modifier for non-player mobjs.
    int             netGravity; // Custom gravity multiplier.
    byte            netNoMaxZRadiusAttack; // Radius attacks are infinitely tall.
    byte            netNoMaxZMonsterMeleeAttack; // Melee attacks are infinitely tall.

    playerclass_t   playerClass[MAXPLAYERS];
    byte            playerColor[MAXPLAYERS];

    float           inventoryTimer; // Number of seconds until the invetory auto-hides.
    byte            inventoryWrap;
    byte            inventoryUseNext;
    byte            inventoryUseImmediate;
    int             inventorySlotMaxVis;
    byte            inventorySlotShowEmpty;
    byte            inventorySelectMode;
} game_config_t;

extern game_config_t cfg;

#endif
