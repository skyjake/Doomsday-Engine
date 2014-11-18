/** @file h_main.cpp  Heretic-specific game initialization.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

#include "jheretic.h"

#include <cstring>
#include <de/App>
#include "am_map.h"
#include "d_netsv.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hereticv13mapstatereader.h"
#include "hu_menu.h"
#include "m_argv.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_saveg.h"
#include "saveslots.h"

using namespace de;
using namespace common;

float turboMul; // Multiplier for turbo.

gamemode_t gameMode;
int gameModeBits;

// Default font colours.
float const defFontRGB[]  = { .425f, .986f, .378f };
float const defFontRGB2[] = { 1, .65f, .275f };
float const defFontRGB3[] = { 1.0f, 1.0f, 1.0f };

// The patches used in drawing the view border.
// Percent-encoded.
char const *borderGraphics[] = {
    "Flats:FLAT513", // Background.
    "BORDT", // Top.
    "BORDR", // Right.
    "BORDB", // Bottom.
    "BORDL", // Left.
    "BORDTL", // Top left.
    "BORDTR", // Top right.
    "BORDBR", // Bottom right.
    "BORDBL" // Bottom left.
};

int H_GetInteger(int id)
{
    return Common_GetInteger(id);
}

void *H_GetVariable(int id)
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
        R_GetWeaponBob(DISPLAYPLAYER, &bob[0], NULL);
        return &bob[0];

    case DD_PSPRITE_BOB_Y:
        R_GetWeaponBob(DISPLAYPLAYER, NULL, &bob[1]);
        return &bob[1];

    case DD_TM_FLOOR_Z:
        return (void*) &tmFloorZ;

    case DD_TM_CEILING_Z:
        return (void*) &tmCeilingZ;

    default:
        break;
    }

    // ID not recognized, return NULL.
    return 0;
}

void H_PreInit()
{
    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.povLookAround = true;
    cfg.statusbarScale = 1;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.menuScale = .9f;
    cfg.menuTextGlitter = 0;
    cfg.menuShadow = 0;
  //cfg.menuQuitSound = true;
    cfg.menuTextFlashColor[0] = .7f;
    cfg.menuTextFlashColor[1] = .9f;
    cfg.menuTextFlashColor[2] = 1;
    cfg.menuTextFlashSpeed = 4;
    cfg.menuCursorRotate = false;

    cfg.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.hudPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_READYITEM] = true;
    cfg.hudShown[HUD_LOG] = true;
    for(int i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
    {
        cfg.hudUnHide[i] = 1;
    }
    cfg.hudScale = .7f;
    cfg.hudColor[0] = .325f;
    cfg.hudColor[1] = .686f;
    cfg.hudColor[2] = .278f;
    cfg.hudColor[3] = 1;
    cfg.hudIconAlpha = 1;
    cfg.xhairAngle = 0;
    cfg.xhairSize = .5f;
    cfg.xhairVitality = false;
    cfg.xhairColor[0] = 1;
    cfg.xhairColor[1] = 1;
    cfg.xhairColor[2] = 1;
    cfg.xhairColor[3] = 1;
    cfg.filterStrength = .8f;
  //cfg.snd_3D = false;
  //cfg.snd_ReverbFactor = 100;
    cfg.moveCheckZ = true;
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = 1; // IF BETTER
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // Never.
    cfg.slidingCorpses = false;
    //cfg.fastMonsters = false;
    cfg.secretMsg = true;
    cfg.netJumping = true;
    cfg.netEpisode = (char *) "";
    cfg.netMap = 0;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4; // Use the default color by default.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1; // Use map default.
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.automapTitleAtBottom = true;
    cfg.hideIWADAuthor = true;
    cfg.menuTextColors[0][0] = defFontRGB[0];
    cfg.menuTextColors[0][1] = defFontRGB[1];
    cfg.menuTextColors[0][2] = defFontRGB[2];
    cfg.menuTextColors[1][0] = defFontRGB2[0];
    cfg.menuTextColors[1][1] = defFontRGB2[1];
    cfg.menuTextColors[1][2] = defFontRGB2[2];
    cfg.menuTextColors[2][0] = defFontRGB3[0];
    cfg.menuTextColors[2][1] = defFontRGB3[1];
    cfg.menuTextColors[2][2] = defFontRGB3[2];
    cfg.menuTextColors[3][0] = defFontRGB3[0];
    cfg.menuTextColors[3][1] = defFontRGB3[1];
    cfg.menuTextColors[3][2] = defFontRGB3[2];
    cfg.menuSlam = true;
    cfg.menuShortcutsEnabled = true;
    cfg.menuGameSaveSuggestDescription = true;

    cfg.confirmQuickGameSave = true;
    cfg.confirmRebornLoad = true;
    cfg.loadLastSaveOnReborn = false;

    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;
    cfg.fixFloorFire = false;
    cfg.fixPlaneScrollMaterialsEastOnly = true;

    cfg.statusbarOpacity = 1;
    cfg.statusbarCounterAlpha = 1;

    cfg.automapCustomColors = 0; // Never.
    cfg.automapL0[0] = .455f; // Unseen areas.
    cfg.automapL0[1] = .482f;
    cfg.automapL0[2] = .439f;

    cfg.automapL1[0] = .292f; // onesided lines
    cfg.automapL1[1] = .195f;
    cfg.automapL1[2] = .062f;

    cfg.automapL2[0] = .812f; // floor height change lines
    cfg.automapL2[1] = .687f;
    cfg.automapL2[2] = .519f;

    cfg.automapL3[0] = .402f; // ceiling change lines
    cfg.automapL3[1] = .230f;
    cfg.automapL3[2] = .121f;

    cfg.automapMobj[0] = .093f;
    cfg.automapMobj[1] = .093f;
    cfg.automapMobj[2] = .093f;

    cfg.automapBack[0] = 1.0f;
    cfg.automapBack[1] = 1.0f;
    cfg.automapBack[2] = 1.0f;
    cfg.automapOpacity = 1.0f;
    cfg.automapLineAlpha = 1.0f;
    cfg.automapLineWidth = 1.1f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = true;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;

    cfg.hudCheatCounterScale = .7f;
    cfg.hudCheatCounterShowWithAutomap = true;

    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5;
    cfg.msgAlign = 1; // Center.
    cfg.msgBlink = 5;

    cfg.msgColor[0] = defFontRGB3[0];
    cfg.msgColor[1] = defFontRGB3[1];
    cfg.msgColor[2] = defFontRGB3[2];

    cfg.inventoryTimer = 5;
    cfg.inventoryWrap = false;
    cfg.inventoryUseNext = true;
    cfg.inventoryUseImmediate = false;
    cfg.inventorySlotMaxVis = 7;
    cfg.inventorySlotShowEmpty = true;
    cfg.inventorySelectMode = 0; // Cursor select.

    cfg.chatBeep = true;

  //cfg.killMessages = true;
    cfg.bobView = 1;
    cfg.bobWeapon = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;
    cfg.respawnMonstersNightmare = false;

    cfg.weaponOrder[0] = WT_SEVENTH;    // mace \ beak
    cfg.weaponOrder[1] = WT_SIXTH;      // phoenixrod \ beak
    cfg.weaponOrder[2] = WT_FIFTH;      // skullrod \ beak
    cfg.weaponOrder[3] = WT_FOURTH;     // blaster \ beak
    cfg.weaponOrder[4] = WT_THIRD;      // crossbow \ beak
    cfg.weaponOrder[5] = WT_SECOND;     // goldwand \ beak
    cfg.weaponOrder[6] = WT_EIGHTH;     // gauntlets \ beak
    cfg.weaponOrder[7] = WT_FIRST;      // staff \ beak

    cfg.weaponCycleSequential = true;

    cfg.menuEffectFlags = MEF_TEXT_SHADOW;
    cfg.hudFog = 5;

    cfg.ringFilter = 1;
    cfg.tomeCounter = 10;
    cfg.tomeSound = 3;

    // Use the crossfade transition by default.
    Con_SetInteger("con-transition", 0);

    // Heretic's torch light does not attenuate with distance.
    DD_SetInteger(DD_FIXEDCOLORMAP_ATTENUATE, 0);

    // Do the common pre init routine;
    G_CommonPreInit();
}

void H_PostInit()
{
    CommandLine &cmdLine = DENG2_APP->commandLine();

    /// @todo Kludge: Shareware WAD has different border background.
    /// @todo Do this properly!
    ::borderGraphics[0] = (::gameMode == heretic_shareware)? "Flats:FLOOR04" : "Flats:FLAT513";

    G_CommonPostInit();

    P_InitWeaponInfo();
    IN_Init();

    // Game parameters.
    ::monsterInfight = GetDefInt("AI|Infight", 0);

    // Defaults for skill, episode and map.
    ::defaultGameRules.skill = /*startSkill =*/ SM_MEDIUM;

    if(cmdLine.check("-deathmatch"))
    {
        ::cfg.netDeathmatch = true;
    }

    // Apply these game rules.
    ::defaultGameRules.noMonsters      = cmdLine.check("-nomonsters")? true : false;
    ::defaultGameRules.respawnMonsters = cmdLine.check("-respawn")   ? true : false;

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

void H_Shutdown()
{
    P_ShutdownInventory();
    IN_Shutdown();
    G_CommonShutdown();
}
