/** @file d_main.cpp  Doom64-specific game initialization.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/app.h>
#include <de/commandline.h>
#include "d_netsv.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_menu.h"
#include "hu_stuff.h"
#include "hud/widgets/automapwidget.h"
#include "m_argv.h"
#include "p_inventory.h"
#include "p_map.h"
#include "saveslots.h"

using namespace de;
using namespace common;

gamemode_t gameMode;
int gameModeBits;

// Default font colors.
float const defFontRGB[]  = { 1, 1, 1 };
float const defFontRGB2[] = { .85f, 0, 0 };

// The patches used in drawing the view border.
// Percent-encoded.
const char *borderGraphics[] = {
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

int D64_GetInteger(int id)
{
    return Common_GetInteger(id);
}

void *D64_GetVariable(int id)
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

void D64_PreInit()
{
    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.common.playerMoveSpeed = 1;
    cfg.common.povLookAround = true;
    cfg.common.screenBlocks = cfg.common.setBlocks = 10;
    cfg.common.echoMsg = true;
    cfg.common.lookSpeed = 3;
    cfg.common.turnSpeed = 1;
    cfg.common.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.common.menuScale = .9f;
    cfg.common.menuTextGlitter = .5f;
    cfg.common.menuShadow = 0.33f;
    cfg.menuQuitSound = true;
    // TODO: Config options present in jDoom but not here:
    //          - menuSlam
    //          - menuShortcutsEnabled
    //          - menuGameSaveSuggestDescription
    cfg.common.menuEffectFlags = MEF_TEXT_TYPEIN | MEF_TEXT_SHADOW | MEF_TEXT_GLITTER;
    cfg.common.menuTextFlashColor[0] = .7f;
    cfg.common.menuTextFlashColor[1] = .9f;
    cfg.common.menuTextFlashColor[2] = 1;
    cfg.common.menuTextFlashSpeed = 4;
    cfg.common.menuCursorRotate = false;

    cfg.common.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.common.hudPatchReplaceMode = PRM_ALLOW_TEXT;
    // TODO: jDoom parity: hudKeysCombine
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
    cfg.common.hudScale = .6f;
    cfg.common.hudColor[0] = 1;
    cfg.common.hudColor[1] = cfg.common.hudColor[2] = 0;
    cfg.common.hudColor[3] = 0.75f;
    cfg.common.hudFog = 5;
    cfg.common.hudIconAlpha = 0.5f;
    cfg.common.xhairAngle = 0;
    cfg.common.xhairSize = .5f;
    cfg.common.xhairLineWidth = 1;
    cfg.common.xhairVitality = false;
    cfg.common.xhairColor[0] = 1;
    cfg.common.xhairColor[1] = 1;
    cfg.common.xhairColor[2] = 1;
    cfg.common.xhairColor[3] = 1;
    cfg.common.filterStrength = .8f;
    cfg.moveCheckZ = true;
    cfg.common.jumpPower = 9;
    cfg.common.airborneMovement = 1;
    cfg.common.weaponAutoSwitch = 1; // "If better" mode.
    cfg.common.noWeaponAutoSwitchIfFiring = false;
    cfg.common.ammoAutoSwitch = 0; // Never.
    cfg.secretMsg = true;
    cfg.slidingCorpses = false;
    //cfg.fastMonsters = false;
    cfg.common.netJumping = true;
    cfg.common.netEpisode = (char *) "";
    cfg.common.netMap = 0;
    cfg.common.netSkill = SM_MEDIUM;
    cfg.common.netColor = 4;
    cfg.netBFGFreeLook = 0; // Allow free-aim 0=none 1=not BFG 2=All.
    cfg.common.netMobDamageModifier = 1;
    cfg.common.netMobHealthModifier = 1;
    cfg.common.netGravity = -1; // Use map default.
    cfg.common.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.common.mapTitle = true;
    cfg.common.automapTitleAtBottom = true;
    cfg.common.hideIWADAuthor = true;
    cfg.common.hideUnknownAuthor = true;
    cfg.common.menuTextColors[0][CR] = 1;
    cfg.common.menuTextColors[0][CG] = 0;
    cfg.common.menuTextColors[0][CB] = 0;
    cfg.common.menuTextColors[1][CR] = 1;
    cfg.common.menuTextColors[1][CG] = 0;
    cfg.common.menuTextColors[1][CB] = 0;
    cfg.common.menuTextColors[2][CR] = 1;
    cfg.common.menuTextColors[2][CG] = 0;
    cfg.common.menuTextColors[2][CB] = 0;
    cfg.common.menuTextColors[3][CR] = 1;
    cfg.common.menuTextColors[3][CG] = 0;
    cfg.common.menuTextColors[3][CB] = 0;
    cfg.common.menuSlam = false;
    cfg.common.menuShortcutsEnabled = true;
    cfg.common.menuGameSaveSuggestDescription = true;

    cfg.common.statusbarScale = 1;

    cfg.common.confirmQuickGameSave = true;
    cfg.common.confirmRebornLoad = true;
    cfg.common.loadLastSaveOnReborn = false;

    cfg.maxSkulls = true;
    cfg.allowSkullsInWalls = false;
    cfg.anyBossDeath = false;
    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;
    // TODO jDoom feature parity
    //      - ouchFace
    //      - statusbarScale
    //      - statusbarOpacity
    //      - statusbarCounterAlpha

    cfg.common.automapCustomColors = 0; // Never.
    cfg.common.automapL0[0] = .4f; // Unseen areas.
    cfg.common.automapL0[1] = .4f;
    cfg.common.automapL0[2] = .4f;

    cfg.common.automapL1[0] = 1.f; // Onesided lines.
    cfg.common.automapL1[1] = 0.f;
    cfg.common.automapL1[2] = 0.f;

    cfg.common.automapL2[0] = .77f; // Floor height change lines.
    cfg.common.automapL2[1] = .6f;
    cfg.common.automapL2[2] = .325f;

    cfg.common.automapL3[0] = 1.f; // Ceiling change lines.
    cfg.common.automapL3[1] = .95f;
    cfg.common.automapL3[2] = 0.f;

    cfg.common.automapMobj[0] = 0.f;
    cfg.common.automapMobj[1] = 1.f;
    cfg.common.automapMobj[2] = 0.f;

    cfg.common.automapBack[0] = 0.f;
    cfg.common.automapBack[1] = 0.f;
    cfg.common.automapBack[2] = 0.f;
    cfg.common.automapOpacity = .7f;
    cfg.common.automapLineAlpha = .7f;
    cfg.common.automapLineWidth = 3.0f;
    cfg.common.automapShowDoors = true;
    cfg.common.automapDoorGlow = 8;
    cfg.common.automapHudDisplay = 2;
    cfg.common.automapRotate = true;
    cfg.common.automapBabyKeys = false;
    cfg.common.automapZoomSpeed = .1f;
    cfg.common.automapPanSpeed = .5f;
    cfg.common.automapPanResetOnOpen = true;
    cfg.common.automapOpenSeconds = AUTOMAPWIDGET_OPEN_SECONDS;

    cfg.common.hudCheatCounterScale = .7f;
    cfg.common.hudCheatCounterShowWithAutomap = true;

    cfg.common.msgCount = 1;
    cfg.common.msgScale = .8f;
    cfg.common.msgUptime = 5;
    cfg.common.msgAlign = 0; // Left.
    cfg.common.msgBlink = 5;

    cfg.common.msgColor[0] = cfg.common.msgColor[1] = cfg.common.msgColor[2] = 1;

    cfg.common.chatBeep = true;

    cfg.killMessages = true;
    cfg.common.bobWeapon = 1;
    cfg.common.bobView = 1;
    cfg.bobWeaponLower = true;
    cfg.common.cameraNoClip = true;

    cfg.common.weaponOrder[0] = WT_TENTH;
    cfg.common.weaponOrder[1] = WT_SIXTH;
    cfg.common.weaponOrder[2] = WT_NINETH;
    cfg.common.weaponOrder[3] = WT_FOURTH;
    cfg.common.weaponOrder[4] = WT_THIRD;
    cfg.common.weaponOrder[5] = WT_SECOND;
    cfg.common.weaponOrder[6] = WT_EIGHTH;
    cfg.common.weaponOrder[7] = WT_FIFTH;
    cfg.common.weaponOrder[8] = WT_SEVENTH;
    cfg.common.weaponOrder[9] = WT_FIRST;
    cfg.weaponRecoil = true;

    cfg.common.weaponCycleSequential = true;
    cfg.berserkAutoSwitch = true;

    // Use the crossfade transition by default.
    Con_SetInteger("con-transition", 0);

    // Do the common pre init routine.
    G_CommonPreInit();
}

void D64_PostInit()
{
    CommandLine &cmdLine = DE_APP->commandLine();

    // Common post init routine.
    G_CommonPostInit();

    // Initialize ammo info.
    P_InitAmmoInfo();

    // Initialize weapon info.
    P_InitWeaponInfo();

    // Game parameters.
    ::monsterInfight = 0;
    if(const ded_value_t *infight = Defs().getValueById("AI|Infight"))
    {
        ::monsterInfight = String(infight->text).toInt();
    }

    // Get skill / episode / map from parms.
    gfw_SetDefaultRule(skill, /*startSkill =*/ SM_MEDIUM);

    if(cmdLine.check("-altdeath"))
    {
        ::cfg.common.netDeathmatch = 2;
    }
    else if(cmdLine.check("-deathmatch"))
    {
        ::cfg.common.netDeathmatch = 1;
    }

    // Apply these rules.
    gfw_SetDefaultRule(noMonsters     , cmdLine.has("-nomonsters"));
    gfw_SetDefaultRule(respawnMonsters, cmdLine.has("-respawn")   );
    gfw_SetDefaultRule(fast           , cmdLine.has("-fast")      );

    if (gfw_DefaultRule(deathmatch))
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
    if (auto arg = cmdLine.check("-skill", 1))
    {
        const int skillNumber = arg.params.first().toInt();
        gfw_SetDefaultRule(skill, skillmode_t(skillNumber > 0? skillNumber - 1 : skillNumber));
    }

    G_AutoStartOrBeginTitleLoop();
}

void D64_Shutdown()
{
    P_ShutdownInventory();
    G_CommonShutdown();
}
