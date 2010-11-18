/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * Hexen specifc Initialization.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "jhexen.h"

#include "dmu_lib.h"
#include "fi_lib.h"
#include "hu_log.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_lib.h"
#include "d_net.h"
#include "g_update.h"
#include "g_common.h"
#include "p_mapspec.h"
#include "am_map.h"
#include "p_switch.h"
#include "p_player.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void X_CreateLUTs(void);
extern void X_DestroyLUTs(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int verbose;

boolean noMonstersParm; // checkparm of -nomonsters
boolean respawnParm; // checkparm of -respawn
boolean turboParm; // checkparm of -turbo
boolean randomClassParm; // checkparm of -randclass
boolean devParm; // checkparm of -devparm

float turboMul; // Multiplier for turbo.
boolean netCheatParm; // Allow cheating in netgames (-netcheat)

skillmode_t startSkill;
int startEpisode;
int startMap;

gamemode_t gameMode;
int gameModeBits;

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

// Default font colours.
const float defFontRGB[] = { .9f, .0f, .0f };
const float defFontRGB2[] = { .9f, .9f, .9f };
const float defFontRGB3[] = { 1, .65f, .275f };

boolean autoStart;

char* borderLumps[] = {
    "F_022", // Background.
    "bordt", // Top.
    "bordr", // Right.
    "bordb", // Bottom.
    "bordl", // Left.
    "bordtl", // Top left.
    "bordtr", // Top right.
    "bordbr", // Bottom right.
    "bordbl" // Bottom left.
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Attempt to change the current game mode. Can only be done when not
 * actually in a map.
 *
 * \todo Doesn't actually do anything yet other than set the game mode
 *  global vars.
 *
 * @param mode          The game mode to change to.
 *
 * @return              @c true, if we changed game modes successfully.
 */
boolean G_SetGameMode(gamemode_t mode)
{
    gameMode = mode;

    if(G_GetGameState() == GS_MAP)
        return false;

    switch(mode)
    {
    case shareware: // Shareware (4-map demo)
        gameModeBits = GM_SHAREWARE;
        break;

    case registered: // HEXEN registered
        gameModeBits = GM_REGISTERED;
        break;

    case extended: // Deathkings
        gameModeBits = GM_REGISTERED|GM_EXTENDED;
        break;

    case indetermined: // Well, no IWAD found.
        gameModeBits = GM_INDETERMINED;
        break;

    default:
        Con_Error("G_SetGameMode: Unknown gamemode %i", mode);
    }

    return true;
}

/**
 * Set the game mode string.
 */
void G_IdentifyVersion(void)
{
    // Determine the game mode. Assume demo mode.
    strcpy(gameModeString, "hexen-demo");
    G_SetGameMode(shareware);

    if(P_MapExists(0, 4))
    {   // Normal Hexen.
        strcpy(gameModeString, "hexen");
        G_SetGameMode(registered);
    }

    // This is not a very accurate test...
    if(P_MapExists(0, 58) && P_MapExists(0, 59))
    {   // It must be Deathkings!
        strcpy(gameModeString, "hexen-dk");
        G_SetGameMode(extended);
    }
}

/**
 * Check which known IWADs are found. The purpose of this routine is to
 * find out which IWADs the user lets us to know about, but we don't
 * decide which one gets loaded or even see if the WADs are actually
 * there. The default location for IWADs is Data\GAMENAMETEXT\.
 */
void G_DetectIWADs(void)
{
    DD_AddIWAD("}data\\"GAMENAMETEXT"\\hexen.wad");
    DD_AddIWAD("}data\\hexen.wad");
    DD_AddIWAD("}hexen.wad");
    DD_AddIWAD("hexen.wad");
}

/**
 * Pre Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    int i;

    G_SetGameMode(indetermined);

    // Calculate the various LUTs used by the playsim.
    X_CreateLUTs();

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.statusbarScale = 1;
    cfg.dclickUse = false;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.hudShown[HUD_MANA] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_CURRENTITEM] = true;
    cfg.hudShown[HUD_LOG] = true;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
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
    cfg.netColor = 8;           // Use the default color by default.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.hideIWADAuthor = true;
    cfg.menuScale = .75f;
    cfg.menuColors[0][0] = defFontRGB[0];   // use the default colour by default.
    cfg.menuColors[0][1] = defFontRGB[1];
    cfg.menuColors[0][2] = defFontRGB[2];
    cfg.menuColors[1][0] = defFontRGB2[0]; // use the default colour by default.
    cfg.menuColors[1][1] = defFontRGB2[1];
    cfg.menuColors[1][2] = defFontRGB2[2];
    cfg.menuColors[2][0] = defFontRGB3[0];
    cfg.menuColors[2][1] = defFontRGB3[1];
    cfg.menuColors[2][2] = defFontRGB3[2];
    cfg.menuEffects = 0;
    cfg.menuHotkeys = true;
    cfg.menuNoStretch = false;
    cfg.askQuickSaveLoad = true;
    cfg.hudFog = 5;
    cfg.menuSlam = true;
    cfg.flashColor[0] = 1.0f;
    cfg.flashColor[1] = .5f;
    cfg.flashColor[2] = .5f;
    cfg.flashSpeed = 4;
    cfg.turningSkull = false;
    cfg.hudScale = .7f;
    cfg.hudWideOffset = 1;
    cfg.hudColor[0] = defFontRGB[0];    // use the default colour by default.
    cfg.hudColor[1] = defFontRGB[1];
    cfg.hudColor[2] = defFontRGB[2];
    cfg.hudColor[3] = 1;
    cfg.hudIconAlpha = 1;
    cfg.usePatchReplacement = 2; // Use built-in replacements if available.
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
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;
    cfg.counterCheatScale = .7f; //From jHeretic

    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5;
    cfg.msgAlign = 1; // Center.
    cfg.msgBlink = 5;
    cfg.msgColor[0] = defFontRGB2[0];
    cfg.msgColor[1] = defFontRGB2[1];
    cfg.msgColor[2] = defFontRGB2[2];

    cfg.inventoryTimer = 5;
    cfg.inventoryWrap = false;
    cfg.inventoryUseNext = false;
    cfg.inventoryUseImmediate = false;
    cfg.inventorySlotMaxVis = 7;
    cfg.inventorySlotShowEmpty = true;
    cfg.inventorySelectMode = 0; // Cursor select.

    cfg.chatBeep = 1;

    cfg.weaponOrder[0] = WT_FOURTH;
    cfg.weaponOrder[1] = WT_THIRD;
    cfg.weaponOrder[2] = WT_SECOND;
    cfg.weaponOrder[3] = WT_FIRST;

    cfg.weaponCycleSequential = true;

    // Do the common pre init routine.
    G_CommonPreInit();
}

/**
 * Post Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PostInit(void)
{
    int p, pClass, warpMap;

    // Do this early as other systems need to know.
    P_InitPlayerClassInfo();

    // Common post init routine.
    G_CommonPostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Game parameters.
    /* None */

    // Get skill / episode / map from parms.
    startEpisode = 0;
    startSkill = SM_MEDIUM;
    startMap = 0;

    // Game mode specific settings.
    /* None */

    // Command line options.
    noMonstersParm = ArgExists("-nomonsters");
    respawnParm = ArgExists("-respawn");
    randomClassParm = ArgExists("-randclass");
    devParm = ArgExists("-devparm");
    netCheatParm = ArgExists("-netcheat");

    cfg.netDeathmatch = ArgExists("-deathmatch");

    // Turbo movement option.
    p = ArgCheck("-turbo");
    turboMul = 1.0f;
    if(p)
    {
        int scale = 200;

        turboParm = true;
        if(p < Argc() - 1)
            scale = atoi(Argv(p + 1));
        if(scale < 10)
            scale = 10;
        if(scale > 400)
            scale = 400;

        Con_Message("turbo scale: %i%%\n", scale);
        turboMul = scale / 100.f;
    }

    if((p = ArgCheckWith("-scripts", 1)) != 0)
    {
        sc_FileScripts = true;
        sc_ScriptsDir = Argv(p + 1);
    }

    if((p = ArgCheckWith("-skill", 1)) != 0)
    {
        startSkill = (skillmode_t)(Argv(p + 1)[0] - '1');
        autoStart = true;
    }

    // Check the -class argument.
    pClass = PCLASS_FIGHTER;
    if((p = ArgCheck("-class")) != 0)
    {
        classinfo_t*            pClassInfo;

        pClass = atoi(Argv(p + 1));
        if(pClass < 0 || pClass > NUM_PLAYER_CLASSES)
        {
            Con_Error("Invalid player class: %d\n", pClass);
        }
        pClassInfo = PCLASS_INFO(pClass);

        if(pClassInfo->userSelectable)
        {
            Con_Error("Player class '%s' is not user-selectable.\n",
                      pClassInfo->niceName);
        }

        Con_Message("\nPlayer Class: '%s'\n", pClassInfo->niceName);
    }
    cfg.playerClass[CONSOLEPLAYER] = pClass;

    P_InitMapMusicInfo(); // Init music fields in mapinfo.

    Con_Message("Parsing SNDINFO...\n");
    S_ParseSndInfoLump();

    Con_Message("SN_InitSequenceScript: Registering sound sequences.\n");
    SN_InitSequenceScript();

    // Check for command line warping. Follows P_Init() because the
    // MAPINFO.TXT script must be already processed.
    p = ArgCheck("-warp");
    if(p && p < Argc() - 1)
    {
        warpMap = atoi(Argv(p + 1)) - 1;
        startMap = P_TranslateMap(warpMap);
        autoStart = true;
    }
    else
    {
        warpMap = 0;
        startMap = P_TranslateMap(0);
    }

    // Are we autostarting?
    if(autoStart)
    {
        Con_Message("Warp to Map %d (\"%s\":%d), Skill %d\n", warpMap+1,
                    P_GetMapName(startMap), startMap+1, startSkill + 1);
    }

    // Load a saved game?
    if((p = ArgCheckWith("-loadgame", 1)) != 0)
    {
        G_LoadGame(atoi(Argv(p + 1)));
    }

    // Check valid episode and map.
    if((autoStart || IS_NETGAME) && !P_MapExists(0, startMap))
    {
        startMap = 0;
    }

    if(G_GetGameAction() != GA_LOADGAME)
    {
        if(autoStart || IS_NETGAME)
        {
            G_DeferedInitNew(startSkill, startEpisode, startMap);
        }
        else
        {
            // Start up intro loop.
            G_StartTitle();
        }
    }
}

void G_Shutdown(void)
{
    Hu_MsgShutdown();
    Hu_UnloadData();
    Hu_LogShutdown();

    P_DestroyIterList(spechit);
    P_DestroyIterList(linespecials);
    P_DestroyLineTagLists();
    P_DestroySectorTagLists();
    P_ShutdownInventory();
    AM_Shutdown();
    X_DestroyLUTs();
    P_FreeWeaponSlots();
    FI_StackShutdown();
    GUI_Shutdown();
}

void G_EndFrame(void)
{
    SN_UpdateActiveSequences();
}
