/** @file config.h  Common configuration variables.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCOMMON_CONFIG_H
#define LIBCOMMON_CONFIG_H

#include <de/legacy/types.h>

#if __JHEXEN__
#  include "h2def.h"
#else
#  include "doomdef.h"
#endif
#include "hu_menu.h"

typedef struct libcommon_config_s {
    // Player movement:
    int             alwaysRun;      // Always run.
    int             noAutoAim;      // No auto-aiming?

    int             lookSpring;
    float           lookSpeed;
    byte            povLookAround;
    int             useJLook;       // Joy look (joy Y => viewpitch)
    int             jLookDeltaMode;

    float           turnSpeed;
    float           playerMoveSpeed;

    int             jumpEnabled;
    float           jumpPower;
    int             airborneMovement;

    int             cameraNoClip;
    float           bobView;
    float           bobWeapon;
    int             plrViewHeight;

    // Gameplay:
    byte            switchSoundOrigin;
    byte            defaultRuleFastMonsters;
    byte            pushableMomentumLimitedToPusher;

    // Weapons:
    byte            weaponCycleSequential; // if true multiple next/prev weapon impulses can be chained to allow the user to "count-click-switch".
    int             weaponOrder[NUM_WEAPON_TYPES];
    byte            weaponNextMode; // if true use the weaponOrder for next/previous.
    byte            weaponAutoSwitch;
    byte            noWeaponAutoSwitchIfFiring;
    byte            ammoAutoSwitch;

    // User interface:
    int             screenBlocks;
    int             setBlocks;

    float           menuScale;
    int             menuEffectFlags;
    float           menuShadow;
    byte            menuSlam;
    byte            menuShortcutsEnabled;
    byte            menuScaleMode;
    int             menuPatchReplaceMode;
    byte            menuGameSaveSuggestDescription;
    byte            menuCursorRotate;

    float           menuTextColors[MENU_COLOR_COUNT][3];
    float           menuTextFlashColor[3];
    int             menuTextFlashSpeed;
    float           menuTextGlitter;

    byte            echoMsg;

    // HUD:
    float           statusbarScale;
    float           statusbarOpacity;
    float           statusbarCounterAlpha;

    float           filterStrength;

    int             hudFog;
    int             hudPatchReplaceMode;
    float           hudScale; // How to scale HUD data?
    float           hudColor[4];
    float           hudIconAlpha;
    float           hudTimer; // Number of seconds until the hud/statusbar auto-hides.

    byte            hudShownCheatCounters;
    float           hudCheatCounterScale;
    byte            hudCheatCounterShowWithAutomap; ///< Only show when the automap is open.

    int             msgCount;
    float           msgScale;
    float           msgUptime;
    int             msgBlink;
    int             msgAlign;
    float           msgColor[3];

    char*           chatMacros[10];
    byte            chatBeep;

    byte            mapTitle;
    byte            hideIWADAuthor;
    byte            hideUnknownAuthor;

    // Crosshair:
    int             xhair;
    float           xhairAngle;
    float           xhairSize;
    byte            xhairVitality;
    float           xhairColor[4];
    float           xhairLineWidth;

    /// Reference hue value for the crosshair at 0% health
    float           xhairLiveRed;
    float           xhairLiveGreen;
    float           xhairLiveBlue;

    /// Reference hue value for the crosshair at 100% health
    float           xhairDeadRed;
    float           xhairDeadGreen;
    float           xhairDeadBlue;

    // Automap:
    float           automapMobj[3];
    float           automapL0[3];
    float           automapL1[3];
    float           automapL2[3];
    float           automapL3[3];
    float           automapBack[3];
    float           automapOpacity;
    byte            automapNeverObscure;
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
    byte            automapTitleAtBottom;

    // Intermission:
    byte            inludeScaleMode;
    int             inludePatchReplaceMode;

    // Savegames:
    byte            confirmQuickGameSave;
    byte            confirmRebornLoad;
    byte            loadLastSaveOnReborn;

    // Multiplayer:
    char *          netEpisode;
    Uri *           netMap;
    byte            netSkill;
    byte            netColor;
    int             netGravity; // Custom gravity multiplier.
    byte            netDeathmatch;
    byte            netNoMonsters;
    byte            netJumping;
    byte            netMobDamageModifier; // Multiplier for non-player mobj damage.
    byte            netMobHealthModifier; // Health modifier for non-player mobjs.
    byte            netNoMaxZRadiusAttack; // Radius attacks are infinitely tall.
    byte            netNoMaxZMonsterMeleeAttack; // Melee attacks are infinitely tall.

} libcommon_config_t;

#endif // LIBCOMMON_CONFIG_H
