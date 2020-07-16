/** @file h_main.cpp  Heretic-specific game initialization.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/CommandLine>
#include <de/Function>
#include <de/NumberValue>>
#include "d_netsv.h"
#include "g_defs.h"
#include "gamesession.h"
#include "hereticv13mapstatereader.h"
#include "hu_menu.h"
#include "hud/widgets/automapwidget.h"
#include "m_argv.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_saveg.h"
#include "saveslots.h"

using namespace de;
using namespace common;

gamemode_t gameMode;
int gameModeBits;

const char *ammoName[NUM_AMMO_TYPES] = {
    "Crystal",
    "Arrow",
    "Orb",
    "Rune",
    "FireOrb",
    "MSphere",
};

// Default font colours.
float const defFontRGB[]  = { .425f, .986f, .378f };
float const defFontRGB2[] = { 1, .65f, .275f };
float const defFontRGB3[] = { 1.0f, 1.0f, 1.0f };

// The patches used in drawing the view border.
// Percent-encoded.
const char *borderGraphics[] = {
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

#if defined(HAVE_EARTHQUAKE)
int localQuakeHappening[MAXPLAYERS];
int localQuakeTimeout[MAXPLAYERS];
#endif

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
    cfg.common.playerMoveSpeed = 1;
    cfg.common.povLookAround = true;
    cfg.common.statusbarScale = 1;
    cfg.common.screenBlocks = cfg.common.setBlocks = 10;
    cfg.common.echoMsg = true;
    cfg.common.lookSpeed = 3;
    cfg.common.turnSpeed = 1;
    cfg.common.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.common.menuScale = .9f;
    cfg.common.menuTextGlitter = 0;
    cfg.common.menuShadow = 0;
  //cfg.menuQuitSound = true;
    cfg.common.menuTextFlashColor[0] = .7f;
    cfg.common.menuTextFlashColor[1] = .9f;
    cfg.common.menuTextFlashColor[2] = 1;
    cfg.common.menuTextFlashSpeed = 4;
    cfg.common.menuCursorRotate = false;

    cfg.common.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.common.hudPatchReplaceMode = PRM_ALLOW_TEXT;
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
    cfg.common.hudScale = .7f;
    cfg.common.hudColor[0] = .325f;
    cfg.common.hudColor[1] = .686f;
    cfg.common.hudColor[2] = .278f;
    cfg.common.hudColor[3] = 1;
    cfg.common.hudIconAlpha = 1;
    cfg.common.xhairAngle = 0;
    cfg.common.xhairSize = .5f;
    cfg.common.xhairLineWidth = 1;
    cfg.common.xhairVitality = false;
    cfg.common.xhairColor[0] = 1;
    cfg.common.xhairColor[1] = 1;
    cfg.common.xhairColor[2] = 1;
    cfg.common.xhairColor[3] = 1;
    cfg.common.filterStrength = .8f;
  //cfg.snd_3D = false;
  //cfg.snd_ReverbFactor = 100;
    cfg.moveCheckZ = true;
    cfg.common.jumpPower = 9;
    cfg.common.airborneMovement = 1;
    cfg.common.weaponAutoSwitch = 1; // IF BETTER
    cfg.common.noWeaponAutoSwitchIfFiring = false;
    cfg.common.ammoAutoSwitch = 0; // Never.
    cfg.slidingCorpses = false;
    //cfg.fastMonsters = false;
    cfg.secretMsg = true;
    cfg.common.netJumping = true;
    cfg.common.netEpisode = (char *) "";
    cfg.common.netMap = 0;
    cfg.common.netSkill = SM_MEDIUM;
    cfg.common.netColor = 4; // Use the default color by default.
    cfg.common.netMobDamageModifier = 1;
    cfg.common.netMobHealthModifier = 1;
    cfg.common.netGravity = -1; // Use map default.
    cfg.common.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.common.mapTitle = true;
    cfg.common.automapTitleAtBottom = true;
    cfg.common.hideIWADAuthor = true;
    cfg.common.hideUnknownAuthor = true;
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
    cfg.common.menuSlam = true;
    cfg.common.menuShortcutsEnabled = true;
    cfg.common.menuGameSaveSuggestDescription = true;

    cfg.common.confirmQuickGameSave = true;
    cfg.common.confirmRebornLoad = true;
    cfg.common.loadLastSaveOnReborn = false;

    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;
    cfg.fixFloorFire = false;
    cfg.fixPlaneScrollMaterialsEastOnly = true;

    cfg.common.statusbarOpacity = 1;
    cfg.common.statusbarCounterAlpha = 1;

    cfg.common.automapCustomColors = 0; // Never.
    cfg.common.automapL0[0] = .455f; // Unseen areas.
    cfg.common.automapL0[1] = .482f;
    cfg.common.automapL0[2] = .439f;

    cfg.common.automapL1[0] = .292f; // onesided lines
    cfg.common.automapL1[1] = .195f;
    cfg.common.automapL1[2] = .062f;

    cfg.common.automapL2[0] = .812f; // floor height change lines
    cfg.common.automapL2[1] = .687f;
    cfg.common.automapL2[2] = .519f;

    cfg.common.automapL3[0] = .402f; // ceiling change lines
    cfg.common.automapL3[1] = .230f;
    cfg.common.automapL3[2] = .121f;

    cfg.common.automapMobj[0] = .093f;
    cfg.common.automapMobj[1] = .093f;
    cfg.common.automapMobj[2] = .093f;

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
    cfg.common.automapBabyKeys = true;
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

    cfg.inventoryTimer = 5;
    cfg.inventoryWrap = false;
    cfg.inventoryUseNext = true;
    cfg.inventoryUseImmediate = false;
    cfg.inventorySlotMaxVis = 7;
    cfg.inventorySlotShowEmpty = true;
    cfg.inventorySelectMode = 0; // Cursor select.

    cfg.common.chatBeep = true;

  //cfg.killMessages = true;
    cfg.common.bobView = 1;
    cfg.common.bobWeapon = 1;
    cfg.bobWeaponLower = true;
    cfg.common.cameraNoClip = true;
    cfg.respawnMonstersNightmare = false;

    cfg.common.weaponOrder[0] = WT_SEVENTH;    // mace \ beak
    cfg.common.weaponOrder[1] = WT_SIXTH;      // phoenixrod \ beak
    cfg.common.weaponOrder[2] = WT_FIFTH;      // skullrod \ beak
    cfg.common.weaponOrder[3] = WT_FOURTH;     // blaster \ beak
    cfg.common.weaponOrder[4] = WT_THIRD;      // crossbow \ beak
    cfg.common.weaponOrder[5] = WT_SECOND;     // goldwand \ beak
    cfg.common.weaponOrder[6] = WT_EIGHTH;     // gauntlets \ beak
    cfg.common.weaponOrder[7] = WT_FIRST;      // staff \ beak

    cfg.common.weaponCycleSequential = true;

    cfg.common.menuEffectFlags = MEF_TEXT_SHADOW;
    cfg.common.hudFog = 5;

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

static void initAmmoInfo()
{
    static const int defaultMaxAmmo[NUM_AMMO_TYPES] = {100, 50, 200, 200, 20, 150};

    for (int i = AT_FIRST; i < NUM_AMMO_TYPES; ++i)
    {
        const String name = ammoName[i];
        if (const ded_value_t *value = Defs().getValueById("Player|Max ammo|" + name))
        {
            // Note: `maxAmmo` is a global variable from p_inter.c
            maxAmmo[i] = String(value->text).toInt();
        }
        else
        {
            maxAmmo[i] = defaultMaxAmmo[i];
        }
    }
}

void H_PostInit()
{
    CommandLine &cmdLine = DE_APP->commandLine();

    /// @todo Kludge: Shareware WAD has different border background.
    /// @todo Do this properly!
    ::borderGraphics[0] = (::gameMode == heretic_shareware)? "Flats:FLOOR04" : "Flats:FLAT513";

    G_CommonPostInit();

    initAmmoInfo();
    P_InitWeaponInfo();
    IN_Init();

    // Game parameters.
    ::monsterInfight = 0;
    if (const ded_value_t *infight = Defs().getValueById("AI|Infight"))
    {
        ::monsterInfight = String(infight->text).toInt();
    }

    // Defaults for skill, episode and map.
    gfw_SetDefaultRule(skill, SM_MEDIUM);

    if (cmdLine.check("-deathmatch"))
    {
        ::cfg.common.netDeathmatch = true;
    }

    // Apply these game rules.
    gfw_SetDefaultRule(noMonsters,
                       cmdLine.check("-nomonsters") ||
                           gfw_GameProfile()->optionValue("noMonsters").isTrue());
    gfw_SetDefaultRule(respawnMonsters,
                       cmdLine.check("-respawn") ||
                           gfw_GameProfile()->optionValue("respawn").isTrue());

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
        gfw_SetDefaultRule(skill, skillmode_t(skillNumber > 0 ? skillNumber - 1 : skillNumber));
    }

    G_AutoStartOrBeginTitleLoop();
}

void H_Shutdown()
{
    P_ShutdownInventory();
    IN_Shutdown();
    G_CommonShutdown();
}
