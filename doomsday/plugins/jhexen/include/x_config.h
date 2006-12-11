/**\file
 *\section Copyright and License Summary
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

    int             sbarscale;
    float           statusbarAlpha;
    float           statusbarCounterAlpha;

    int             msgCount;
    float           msgScale;
    int             msgUptime;
    int             msgBlink;
    int             msgAlign;
    byte            msgShow;
    float           msgColor[3];
    byte            weaponAutoSwitch;
    byte            noWeaponAutoSwitchIfFiring;
    byte            ammoAutoSwitch;
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
