/** @file d_main.cpp  Doom64-specific game initialization.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 * @authors Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 * @authors Copyright © 1993-1996 id Software, Inc.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "jdoom64.h"

#include <cstring>
#include <de/App>
#include "am_map.h"
#include "d_netsv.h"
#include "g_defs.h"
#include "gamesession.h"
#include "m_argv.h"
#include "p_inventory.h"
#include "p_map.h"
#include "saveslots.h"

using namespace de;
using namespace common;

int verbose;
float turboMul; // Multiplier for turbo.

gamemode_t gameMode;
int gameModeBits;

// Default font colors.
float const defFontRGB[]  = { 1, 1, 1 };
float const defFontRGB2[] = { .85f, 0, 0 };

// The patches used in drawing the view border.
// Percent-encoded.
char const *borderGraphics[] = {
    "Flats:FTILEABC", // Background.
    "BRDR_T", // Top.
    "BRDR_R", // Right.
    "BRDR_B", // Bottom.
    "BRDR_L", // Left.
    "BRDR_TL", // Top left.
    "BRDR_TR", // Top right.
    "BRDR_BR", // Bottom right.
    "BRDR_BL" // Bottom left.
};

int D_GetInteger(int id)
{
    return Common_GetInteger(id);
}

void *D_GetVariable(int id)
{
    static float bob[2];

    switch(id)
    {
    case DD_PLUGIN_NAME:
        return (void*)PLUGIN_NAMETEXT;

    case DD_PLUGIN_NICENAME:
        return (void*)PLUGIN_NICENAME;

    case DD_PLUGIN_VERSION_SHORT:
        return (void*)PLUGIN_VERSION_TEXT;

    case DD_PLUGIN_VERSION_LONG:
        return (void*)(PLUGIN_VERSION_TEXTLONG "\n" PLUGIN_DETAILS);

    case DD_PLUGIN_HOMEURL:
        return (void*)PLUGIN_HOMEURL;

    case DD_PLUGIN_DOCSURL:
        return (void*)PLUGIN_DOCSURL;

    case DD_GAME_CONFIG:
        return gameConfigString;

    case DD_ACTION_LINK:
        return actionlinks;

    case DD_XGFUNC_LINK:
        return xgClasses;

    case DD_PSPRITE_BOB_X:
        R_GetWeaponBob(DISPLAYPLAYER, &bob[0], 0);
        return &bob[0];

    case DD_PSPRITE_BOB_Y:
        R_GetWeaponBob(DISPLAYPLAYER, 0, &bob[1]);
        return &bob[1];

    case DD_TM_FLOOR_Z:
        return (void*) &tmFloorZ;

    case DD_TM_CEILING_Z:
        return (void*) &tmCeilingZ;

    default:
        break;
    }
    return 0;
}

void D_PreInit()
{
    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.povLookAround = true;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.menuScale = .9f;
    cfg.menuTextGlitter = .5f;
    cfg.menuShadow = 0.33f;
    cfg.menuQuitSound = true;
    cfg.menuEffectFlags = MEF_TEXT_TYPEIN|MEF_TEXT_SHADOW|MEF_TEXT_GLITTER;
    cfg.menuTextFlashColor[0] = .7f;
    cfg.menuTextFlashColor[1] = .9f;
    cfg.menuTextFlashColor[2] = 1;
    cfg.menuTextFlashSpeed = 4;
    cfg.menuCursorRotate = false;

    cfg.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.hudPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_FRAGS] = true;
    cfg.hudShown[HUD_INVENTORY] = false; // They will be visible when the automap is.
    cfg.hudShown[HUD_LOG] = true;
    for(int i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
    {
        cfg.hudUnHide[i] = 1;
    }
    cfg.hudScale = .6f;
    cfg.hudColor[0] = 1;
    cfg.hudColor[1] = cfg.hudColor[2] = 0;
    cfg.hudColor[3] = 0.75f;
    cfg.hudFog = 1;
    cfg.hudIconAlpha = 0.5f;
    cfg.xhairAngle = 0;
    cfg.xhairSize = .5f;
    cfg.xhairVitality = false;
    cfg.xhairColor[0] = 1;
    cfg.xhairColor[1] = 1;
    cfg.xhairColor[2] = 1;
    cfg.xhairColor[3] = 1;
    cfg.filterStrength = .8f;
    cfg.moveCheckZ = true;
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = 1; // "If better" mode.
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // Never.
    cfg.secretMsg = true;
    cfg.slidingCorpses = false;
    //cfg.fastMonsters = false;
    cfg.netJumping = true;
    cfg.netMap = 0;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4;
    cfg.netBFGFreeLook = 0; // Allow free-aim 0=none 1=not BFG 2=All.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1; // Use map default.
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.automapTitleAtBottom = true;
    cfg.hideIWADAuthor = true;
    cfg.menuTextColors[0][CR] = 1;
    cfg.menuTextColors[0][CG] = 0;
    cfg.menuTextColors[0][CB] = 0;
    cfg.menuTextColors[1][CR] = 1;
    cfg.menuTextColors[1][CG] = 0;
    cfg.menuTextColors[1][CB] = 0;
    cfg.menuTextColors[2][CR] = 1;
    cfg.menuTextColors[2][CG] = 0;
    cfg.menuTextColors[2][CB] = 0;
    cfg.menuTextColors[3][CR] = 1;
    cfg.menuTextColors[3][CG] = 0;
    cfg.menuTextColors[3][CB] = 0;
    cfg.menuSlam = false;
    cfg.menuShortcutsEnabled = true;
    cfg.menuGameSaveSuggestDescription = true;

    cfg.statusbarScale = 1;

    cfg.confirmQuickGameSave = true;
    cfg.confirmRebornLoad = true;
    cfg.loadLastSaveOnReborn = false;

    cfg.maxSkulls = true;
    cfg.allowSkullsInWalls = false;
    cfg.anyBossDeath = false;
    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;

    cfg.automapCustomColors = 0; // Never.
    cfg.automapL0[0] = .4f; // Unseen areas.
    cfg.automapL0[1] = .4f;
    cfg.automapL0[2] = .4f;

    cfg.automapL1[0] = 1.f; // Onesided lines.
    cfg.automapL1[1] = 0.f;
    cfg.automapL1[2] = 0.f;

    cfg.automapL2[0] = .77f; // Floor height change lines.
    cfg.automapL2[1] = .6f;
    cfg.automapL2[2] = .325f;

    cfg.automapL3[0] = 1.f; // Ceiling change lines.
    cfg.automapL3[1] = .95f;
    cfg.automapL3[2] = 0.f;

    cfg.automapMobj[0] = 0.f;
    cfg.automapMobj[1] = 1.f;
    cfg.automapMobj[2] = 0.f;

    cfg.automapBack[0] = 0.f;
    cfg.automapBack[1] = 0.f;
    cfg.automapBack[2] = 0.f;
    cfg.automapOpacity = .7f;
    cfg.automapLineAlpha = .7f;
    cfg.automapLineWidth = 1.1f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;

    cfg.hudCheatCounterScale = .7f;
    cfg.hudCheatCounterShowWithAutomap = true;

    cfg.msgCount = 1;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5;
    cfg.msgAlign = 0; // Left.
    cfg.msgBlink = 5;

    cfg.msgColor[0] = cfg.msgColor[1] = cfg.msgColor[2] = 1;

    cfg.chatBeep = true;

    cfg.killMessages = true;
    cfg.bobWeapon = 1;
    cfg.bobView = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;

    cfg.weaponOrder[0] = WT_TENTH;
    cfg.weaponOrder[1] = WT_SIXTH;
    cfg.weaponOrder[2] = WT_NINETH;
    cfg.weaponOrder[3] = WT_FOURTH;
    cfg.weaponOrder[4] = WT_THIRD;
    cfg.weaponOrder[5] = WT_SECOND;
    cfg.weaponOrder[6] = WT_EIGHTH;
    cfg.weaponOrder[7] = WT_FIFTH;
    cfg.weaponOrder[8] = WT_SEVENTH;
    cfg.weaponOrder[9] = WT_FIRST;
    cfg.weaponRecoil = true;

    cfg.weaponCycleSequential = true;
    cfg.berserkAutoSwitch = true;

    // Use the crossfade transition by default.
    Con_SetInteger("con-transition", 0);

    // Do the common pre init routine.
    G_CommonPreInit();
}

void D_PostInit()
{
    CommandLine &cmdLine = DENG2_APP->commandLine();

    // Common post init routine.
    G_CommonPostInit();

    // Initialize ammo info.
    P_InitAmmoInfo();

    // Initialize weapon info.
    P_InitWeaponInfo();

    // Game parameters.
    ::monsterInfight = GetDefInt("AI|Infight", 0);

    // Get skill / episode / map from parms.
    ::defaultGameRules.skill = /*startSkill =*/ SM_MEDIUM;

    if(cmdLine.check("-altdeath"))
    {
        ::cfg.netDeathmatch = 2;
    }
    else if(cmdLine.check("-deathmatch"))
    {
        ::cfg.netDeathmatch = 1;
    }

    // Apply these rules.
    ::defaultGameRules.noMonsters      = cmdLine.check("-nomonsters")? true : false;
    ::defaultGameRules.respawnMonsters = cmdLine.check("-respawn")   ? true : false;
    ::defaultGameRules.fast            = cmdLine.check("-fast")      ? true : false;

    if(::defaultGameRules.deathmatch)
    {
        if(int arg = cmdLine.check("-timer", 1))
        {
            bool isNumber;
            int mins = cmdLine.at(arg + 1).toInt(&isNumber);
            if(isNumber)
            {
                LOG_NOTE("Maps will end after %i %s")
                        << mins << (mins == 1? "minute" : "minutes");
            }
        }
    }

    // Change the turbo multiplier?
    ::turboMul = 1.0f;
    if(int arg = cmdLine.check("-turbo"))
    {
        int scale = 200;
        if(arg + 1 < cmdLine.count() && !cmdLine.isOption(arg + 1))
        {
            scale = cmdLine.at(arg + 1).toInt();
        }
        scale = de::clamp(10, scale, 400);

        LOG_NOTE("Turbo scale: %i%%") << scale;
        ::turboMul = scale / 100.f;
    }

    // Load a saved game?
    if(int arg = cmdLine.check("-loadgame", 1))
    {
        if(SaveSlot *sslot = G_SaveSlots().slotByUserInput(cmdLine.at(arg + 1)))
        {
            if(sslot->isUserWritable() && G_SetGameActionLoadSession(sslot->id()))
            {
                // No further initialization is to be done.
                return;
            }
        }
    }

    // Change the default skill mode?
    if(int arg = cmdLine.check("-skill", 1))
    {
        int skillNumber = cmdLine.at(arg + 1).toInt();
        ::defaultGameRules.skill = (skillmode_t)(skillNumber > 0? skillNumber - 1 : skillNumber);
    }

    G_AutoStartOrBeginTitleLoop();
}

void D_Shutdown()
{
    P_ShutdownInventory();
    G_CommonShutdown();
}
