/**\file h2_main.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 1999 Activision
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
 * Game initialization - Hexen specifc.
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <string.h>

#include "jhexen.h"

#include "am_map.h"
#include "d_netsv.h"
#include "g_common.h"
#include "g_defs.h"
#include "m_argv.h"
#include "p_inventory.h"
#include "p_map.h"
#include "p_player.h"
#include "p_saveg.h"
#include "p_sound.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int verbose;

boolean devParm; // checkparm of -devparm
boolean noMonstersParm; // checkparm of -nomonsters
boolean respawnParm; // checkparm of -respawn
//boolean fastParm; // checkparm of -fast
boolean turboParm; // checkparm of -turbo
boolean randomClassParm; // checkparm of -randclass

float turboMul; // Multiplier for turbo.

gamemode_t gameMode;
int gameModeBits;

// Default font colours.
const float defFontRGB[]   = {  .9f, .0f,  .0f };
const float defFontRGB2[]  = { 1.f,  .65f, .275f };
const float defFontRGB3[] = {  .9f, .9f,  .9f };

// The patches used in drawing the view border.
// Percent-encoded.
char* borderGraphics[] = {
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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean autoStart = false;
static uint startEpisode = 0;
static uint startMap = 0;
static playerclass_t startPlayerClass = PCLASS_NONE;
static skillmode_t startSkill = SM_MEDIUM;

// CODE --------------------------------------------------------------------

/**
 * Get a 32-bit integer value.
 */
int X_GetInteger(int id)
{
    return Common_GetInteger(id);
}

/**
 * Get a pointer to the value of a named variable/constant.
 */
void* X_GetVariable(int id)
{
    static float bob[2];

    switch(id)
    {
    case DD_PLUGIN_NAME:
        return PLUGIN_NAMETEXT;

    case DD_PLUGIN_NICENAME:
        return PLUGIN_NICENAME;

    case DD_PLUGIN_VERSION_SHORT:
        return PLUGIN_VERSION_TEXT;

    case DD_PLUGIN_VERSION_LONG:
        return PLUGIN_VERSION_TEXTLONG "\n" PLUGIN_DETAILS;

    case DD_PLUGIN_HOMEURL:
        return PLUGIN_HOMEURL;

    case DD_PLUGIN_DOCSURL:
        return PLUGIN_DOCSURL;

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

/**
 * Pre Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void X_PreInit(void)
{
    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
        cfg.playerClass[i] = PCLASS_FIGHTER;
    }
    cfg.playerMoveSpeed = 1;
    cfg.statusbarScale = 1;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.hudShown[HUD_MANA] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_READYITEM] = true;
    cfg.hudShown[HUD_LOG] = true;
    { int i;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    }
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.xhairAngle = 0;
    cfg.xhairSize = .5f;
    cfg.xhairVitality = false;
    cfg.xhairColor[0] = 1;
    cfg.xhairColor[1] = 1;
    cfg.xhairColor[2] = 1;
    cfg.xhairColor[3] = 1;
    cfg.filterStrength = .8f;
    cfg.jumpEnabled = cfg.netJumping = true; // true by default in Hexen
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = 1; // IF BETTER
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // never
    cfg.fastMonsters = false;
    cfg.netMap = 0;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 8; // Use the default color by default.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.hideIWADAuthor = true;
    cfg.menuPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.menuScale = .75f;
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
    cfg.menuEffectFlags = MEF_TEXT_SHADOW;
    cfg.menuShortcutsEnabled = true;

    cfg.inludePatchReplaceMode = PRM_ALLOW_TEXT;

    cfg.confirmQuickGameSave = true;
    cfg.confirmRebornLoad = true;
    cfg.loadLastSaveOnReborn = false;

    cfg.hudFog = 5;
    cfg.menuSlam = true;
    cfg.menuGameSaveSuggestName = true;
    cfg.menuTextFlashColor[0] = 1.0f;
    cfg.menuTextFlashColor[1] = .5f;
    cfg.menuTextFlashColor[2] = .5f;
    cfg.menuTextFlashSpeed = 4;
    cfg.menuCursorRotate = false;

    cfg.hudPatchReplaceMode = PRM_ALLOW_TEXT;
    cfg.hudScale = .7f;
    cfg.hudColor[0] = defFontRGB[0];
    cfg.hudColor[1] = defFontRGB[1];
    cfg.hudColor[2] = defFontRGB[2];
    cfg.hudColor[3] = 1;
    cfg.hudIconAlpha = 1;
    cfg.cameraNoClip = true;
    cfg.bobView = cfg.bobWeapon = 1;

    cfg.statusbarOpacity = 1;
    cfg.statusbarCounterAlpha = 1;
    cfg.inventoryTimer = 5;

    cfg.automapCustomColors = 0; // Never.
    cfg.automapL0[0] = .42f; // Unseen areas
    cfg.automapL0[1] = .42f;
    cfg.automapL0[2] = .42f;

    cfg.automapL1[0] = .41f; // onesided lines
    cfg.automapL1[1] = .30f;
    cfg.automapL1[2] = .15f;

    cfg.automapL2[0] = .82f; // floor height change lines
    cfg.automapL2[1] = .70f;
    cfg.automapL2[2] = .52f;

    cfg.automapL3[0] = .47f; // ceiling change lines
    cfg.automapL3[1] = .30f;
    cfg.automapL3[2] = .16f;

    cfg.automapMobj[0] = 1.f;
    cfg.automapMobj[1] = 1.f;
    cfg.automapMobj[2] = 1.f;

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
    cfg.automapBabyKeys = false;
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
    cfg.inventoryUseNext = false;
    cfg.inventoryUseImmediate = false;
    cfg.inventorySlotMaxVis = 7;
    cfg.inventorySlotShowEmpty = true;
    cfg.inventorySelectMode = 0; // Cursor select.

    cfg.chatBeep = true;

    cfg.weaponOrder[0] = WT_FOURTH;
    cfg.weaponOrder[1] = WT_THIRD;
    cfg.weaponOrder[2] = WT_SECOND;
    cfg.weaponOrder[3] = WT_FIRST;

    cfg.weaponCycleSequential = true;

    // Use the crossfade transition by default.
    Con_SetInteger("con-transition", 0);

    // Do the common pre init routine.
    G_CommonPreInit();
}

/**
 * Post Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void X_PostInit(void)
{
    AutoStr* path;
    int p, warpMap;
    Uri* uri;

    // Do this early as other systems need to know.
    P_InitPlayerClassInfo();

    // Common post init routine.
    G_CommonPostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Game parameters.
    /* None */

    // Game mode specific settings.
    /* None */

    // Command line options.
    noMonstersParm = CommandLine_Exists("-nomonsters");
    respawnParm = CommandLine_Exists("-respawn");
    randomClassParm = CommandLine_Exists("-randclass");
    devParm = CommandLine_Exists("-devparm");

    cfg.netDeathmatch = CommandLine_Exists("-deathmatch");

    // Turbo movement option.
    p = CommandLine_Check("-turbo");
    turboMul = 1.0f;
    if(p)
    {
        int scale = 200;

        turboParm = true;
        if(p < CommandLine_Count() - 1)
            scale = atoi(CommandLine_At(p + 1));
        if(scale < 10)
            scale = 10;
        if(scale > 400)
            scale = 400;

        Con_Message("Turbo scale: %i%%\n", scale);
        turboMul = scale / 100.f;
    }

    if((p = CommandLine_CheckWith("-scripts", 1)) != 0)
    {
        sc_FileScripts = true;
        sc_ScriptsDir = CommandLine_At(p + 1);
    }

    P_InitMapMusicInfo(); // Init music fields in mapinfo.

    Con_Message("Parsing SNDINFO...\n");
    S_ParseSndInfoLump();

    Con_Message("SN_InitSequenceScript: Registering sound sequences.\n");
    SN_InitSequenceScript();

    // Load a saved game?
    p = CommandLine_CheckWith("-loadgame", 1);
    if(p != 0)
    {
        const int saveSlot = SV_ParseSlotIdentifier(CommandLine_At(p + 1));
        if(SV_IsUserWritableSlot(saveSlot) && G_LoadGame(saveSlot))
        {
            // No further initialization is to be done.
            return;
        }
    }

    if((p = CommandLine_CheckWith("-skill", 1)) != 0)
    {
        startSkill = (skillmode_t)(CommandLine_At(p + 1)[0] - '1');
        autoStart = true;
    }

    if((p = CommandLine_Check("-class")) != 0)
    {
        playerclass_t pClass = (playerclass_t)atoi(CommandLine_At(p + 1));
        if(!VALID_PLAYER_CLASS(pClass))
        {
            Con_Message("Warning: Invalid player class id=%d specified with -class, ignoring.\n", (int)pClass);
        }
        else if(!PCLASS_INFO(pClass)->userSelectable)
        {
            Con_Message("Warning: Non-user-selectable player class id=%d specified with -class, ignoring.\n", (int)pClass);
        }
        else
        {
            startPlayerClass = pClass;
        }
    }

    if(startPlayerClass != PCLASS_NONE)
    {
        Con_Message("Player Class: '%s'\n", PCLASS_INFO(startPlayerClass)->niceName);
        cfg.playerClass[CONSOLEPLAYER] = startPlayerClass;
        autoStart = true;
    }

    // Check for command line warping.
    p = CommandLine_Check("-warp");
    if(p && p < CommandLine_Count() - 1)
    {
        warpMap = atoi(CommandLine_At(p + 1)) - 1;
        startMap = P_TranslateMap(warpMap);
        autoStart = true;
    }
    else
    {
        warpMap = 0;
        startMap = P_TranslateMap(warpMap);
    }

    // Are we autostarting?
    if(autoStart)
    {
        Con_Message("Warp to Map %d (\"%s\":%d), Skill %d\n", warpMap+1, P_GetMapName(startMap), startMap+1, startSkill + 1);
    }

    // Validate episode and map.
    uri = G_ComposeMapUri(0, startMap);
    path = Uri_Compose(uri);
    if((autoStart || IS_NETGAME) && !P_MapExists(Str_Text(path)))
    {
        startMap = 0;
    }
    Uri_Delete(uri);

    if(autoStart || IS_NETGAME)
    {
        G_DeferredNewGame(startSkill, startEpisode, startMap, 0/* default */);
    }
    else
    {
        // Start up intro loop.
        G_StartTitle();
    }
}

void X_Shutdown(void)
{
    P_ShutdownInventory();
    X_DestroyLUTs();
    G_CommonShutdown();
}

