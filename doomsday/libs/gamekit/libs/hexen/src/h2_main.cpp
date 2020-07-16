/** @file h2_main.cpp  Hexen specifc game initialization.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 * @authors Copyright © 1999 Activision
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

#include "jhexen.h"

#include <cstring>
#include <de/app.h>
#include <de/commandline.h>
#include "d_netsv.h"
#include "g_common.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hu_menu.h"
#include "hu_stuff.h"
#include "hud/widgets/automapwidget.h"
#include "m_argv.h"
#include "p_inventory.h"
#include "p_map.h"
#include "player.h"
#include "p_saveg.h"
#include "p_sound.h"
#include "saveslots.h"

using namespace de;
using namespace common;

gamemode_t gameMode;
int gameModeBits;

// Default font colours.
float const defFontRGB[]   = {  .9f, .0f,  .0f };
float const defFontRGB2[]  = { 1.f,  .65f, .275f };
float const defFontRGB3[] = {  .9f, .9f,  .9f };

// The patches used in drawing the view border.
// Percent-encoded.
const char *borderGraphics[] = {
    "Flats:F_022", // Background.
    "BORDT", // Top.
    "BORDR", // Right.
    "BORDB", // Bottom.
    "BORDL", // Left.
    "BORDTL", // Top left.
    "BORDTR", // Top right.
    "BORDBR", // Bottom right.
    "BORDBL" // Bottom left.
};

int X_GetInteger(int id)
{
    return Common_GetInteger(id);
}

void *X_GetVariable(int id)
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
        return 0;

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

void X_PreInit()
{
    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        cfg.playerClass[i] = PCLASS_FIGHTER;
    }
    cfg.common.playerMoveSpeed = 1;
    cfg.common.statusbarScale = 1;
    cfg.common.screenBlocks = cfg.common.setBlocks = 10;
    cfg.hudShown[HUD_MANA] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_READYITEM] = true;
    cfg.hudShown[HUD_LOG] = true;
    for(int i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
    {
        cfg.hudUnHide[i] = 1;
    }
    cfg.common.lookSpeed = 3;
    cfg.common.turnSpeed = 1;
    cfg.common.xhairAngle = 0;
    cfg.common.xhairSize = .5f;
    cfg.common.xhairLineWidth = 1;
    cfg.common.xhairVitality = false;
    cfg.common.xhairColor[0] = 1;
    cfg.common.xhairColor[1] = 1;
    cfg.common.xhairColor[2] = 1;
    cfg.common.xhairColor[3] = 1;
    cfg.common.filterStrength = .8f;
    cfg.common.jumpEnabled = cfg.common.netJumping = true; // true by default in Hexen
    cfg.common.jumpPower = 9;
    cfg.common.airborneMovement = 1;
    cfg.common.weaponAutoSwitch = 1; // IF BETTER
    cfg.common.noWeaponAutoSwitchIfFiring = false;
    cfg.common.ammoAutoSwitch = 0; // never
    //cfg.fastMonsters = false;
    cfg.common.netEpisode = (char *) "";
    cfg.common.netMap = 0;
    cfg.common.netSkill = SM_MEDIUM;
    cfg.common.netColor = 8; // Use the default color by default.
    cfg.common.netMobDamageModifier = 1;
    cfg.common.netMobHealthModifier = 1;
    cfg.common.netGravity = -1;        // use map default
    cfg.common.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.common.mapTitle = true;
    cfg.common.automapTitleAtBottom = true;
    cfg.common.hideIWADAuthor = true;
    cfg.common.hideUnknownAuthor = true;
    cfg.common.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.common.menuScale = .75f;
    cfg.common.menuTextColors[0][0] = defFontRGB[0];
    cfg.common.menuTextColors[0][1] = defFontRGB[1];
    cfg.common.menuTextColors[0][2] = defFontRGB[2];
    cfg.common.menuTextColors[1][0] = defFontRGB2[0];
    cfg.common.menuTextColors[1][1] = defFontRGB2[1];
    cfg.common.menuTextColors[1][2] = defFontRGB2[2];
    cfg.common.menuTextColors[2][0] = defFontRGB3[0];
    cfg.common.menuTextColors[2][1] = defFontRGB3[1];
    cfg.common.menuTextColors[2][2] = defFontRGB3[2];
    cfg.common.menuTextColors[3][0] = defFontRGB3[0];
    cfg.common.menuTextColors[3][1] = defFontRGB3[1];
    cfg.common.menuTextColors[3][2] = defFontRGB3[2];
    cfg.common.menuEffectFlags = MEF_TEXT_SHADOW;
    cfg.common.menuShortcutsEnabled = true;

    cfg.common.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.common.confirmQuickGameSave = true;
    cfg.common.confirmRebornLoad = true;
    cfg.common.loadLastSaveOnReborn = false;

    cfg.common.hudFog = 5;
    cfg.common.menuSlam = true;
    cfg.common.menuGameSaveSuggestDescription = true;
    cfg.common.menuTextFlashColor[0] = 1.0f;
    cfg.common.menuTextFlashColor[1] = .5f;
    cfg.common.menuTextFlashColor[2] = .5f;
    cfg.common.menuTextFlashSpeed = 4;
    cfg.common.menuCursorRotate = false;

    cfg.common.hudPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.common.hudScale = .7f;
    cfg.common.hudColor[0] = defFontRGB[0];
    cfg.common.hudColor[1] = defFontRGB[1];
    cfg.common.hudColor[2] = defFontRGB[2];
    cfg.common.hudColor[3] = 1;
    cfg.common.hudIconAlpha = 1;
    cfg.common.cameraNoClip = true;
    cfg.common.bobView = cfg.common.bobWeapon = 1;

    cfg.common.statusbarOpacity = 1;
    cfg.common.statusbarCounterAlpha = 1;
    cfg.inventoryTimer = 5;

    cfg.common.automapCustomColors = 0; // Never.
    cfg.common.automapL0[0] = .42f; // Unseen areas
    cfg.common.automapL0[1] = .42f;
    cfg.common.automapL0[2] = .42f;

    cfg.common.automapL1[0] = .41f; // onesided lines
    cfg.common.automapL1[1] = .30f;
    cfg.common.automapL1[2] = .15f;

    cfg.common.automapL2[0] = .82f; // floor height change lines
    cfg.common.automapL2[1] = .70f;
    cfg.common.automapL2[2] = .52f;

    cfg.common.automapL3[0] = .47f; // ceiling change lines
    cfg.common.automapL3[1] = .30f;
    cfg.common.automapL3[2] = .16f;

    cfg.common.automapMobj[0] = 1.f;
    cfg.common.automapMobj[1] = 1.f;
    cfg.common.automapMobj[2] = 1.f;

    cfg.common.automapBack[0] = 1.0f;
    cfg.common.automapBack[1] = 1.0f;
    cfg.common.automapBack[2] = 1.0f;
    cfg.common.automapOpacity = 1.0f;
    cfg.common.automapLineAlpha = 1.0f;
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

    cfg.common.msgCount = 4;
    cfg.common.msgScale = .8f;
    cfg.common.msgUptime = 5;
    cfg.common.msgAlign = 1; // Center.
    cfg.common.msgBlink = 5;
    cfg.common.msgColor[0] = defFontRGB3[0];
    cfg.common.msgColor[1] = defFontRGB3[1];
    cfg.common.msgColor[2] = defFontRGB3[2];
    cfg.common.echoMsg = true;

    cfg.inventoryTimer = 5;
    cfg.inventoryWrap = false;
    cfg.inventoryUseNext = true;
    cfg.inventoryUseImmediate = false;
    cfg.inventorySlotMaxVis = 7;
    cfg.inventorySlotShowEmpty = true;
    cfg.inventorySelectMode = 0; // Cursor select.

    cfg.common.chatBeep = true;

    cfg.common.weaponOrder[0] = WT_FOURTH;
    cfg.common.weaponOrder[1] = WT_THIRD;
    cfg.common.weaponOrder[2] = WT_SECOND;
    cfg.common.weaponOrder[3] = WT_FIRST;

    cfg.common.weaponCycleSequential = true;

    // Use the crossfade transition by default.
    Con_SetInteger("con-transition", 0);

    // Hexen's torch light attenuates with distance.
    DD_SetInteger(DD_FIXEDCOLORMAP_ATTENUATE, 1);

    cfg.deathkingsAutoRespawnChance = 100; // 100% spawn chance

    // Do the common pre init routine.
    G_CommonPreInit();
}

void X_PostInit()
{
    CommandLine &cmdLine = DE_APP->commandLine();

    // Do this early as other systems need to know.
    P_InitPlayerClassInfo();

    // Common post init routine.
    G_CommonPostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Defaults for skill, episode and map.
    gfw_SetDefaultRule(skill, /*startSkill =*/ SM_MEDIUM);

    ::cfg.common.netDeathmatch = cmdLine.check("-deathmatch") ? true : false;

    gfw_SetDefaultRule(noMonsters,
                       cmdLine.has("-nomonsters") ||
                           gfw_GameProfile()->optionValue("noMonsters").isTrue());
    gfw_SetDefaultRule(randomClasses, cmdLine.has("-randclass"));

    // Process sound definitions.
    SndInfoParser(AutoStr_FromText("Lumps:SNDINFO"));

    // Process sound sequence scripts.
    String scriptPath("Lumps:SNDSEQ");
    if (auto arg = cmdLine.check("-scripts", 1))
    {
        scriptPath = arg.params.first()/"SNDSEQ.txt";
    }
    SndSeqParser(AutoStr_FromTextStd(scriptPath));

    // Load a saved game?
    if (auto arg = cmdLine.check("-loadgame", 1))
    {
        if (SaveSlot *sslot = G_SaveSlots().slotByUserInput(arg.params.first()))
        {
            if (sslot->isUserWritable() && G_SetGameActionLoadSession(sslot->id()))
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

    // Change the default player class?
    playerclass_t defPlayerClass = PCLASS_NONE;
    if (auto arg = cmdLine.check("-class", 1))
    {
        bool isNumber;
        const int pClass = arg.params.first().toInt(&isNumber);
        if (isNumber && VALID_PLAYER_CLASS(pClass))
        {
            if (!PCLASS_INFO(pClass)->userSelectable)
            {
                LOG_WARNING("Non-user-selectable player class '%i' specified with -class") << pClass;
            }
        }
        else
        {
            LOG_WARNING("Invalid player class '%i' specified with -class") << pClass;
        }
    }
    if (defPlayerClass != PCLASS_NONE)
    {
        ::cfg.playerClass[CONSOLEPLAYER] = defPlayerClass;
        LOG_NOTE("Player Class: '%s'") << PCLASS_INFO(defPlayerClass)->niceName;
    }

    G_AutoStartOrBeginTitleLoop();
}

void X_Shutdown()
{
    P_ShutdownInventory();
    X_DestroyLUTs();
    G_CommonShutdown();
}
