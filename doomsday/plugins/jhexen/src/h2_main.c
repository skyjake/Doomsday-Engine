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
 * h2_main.c: Hexen specifc Initialization.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "jhexen.h"

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

typedef struct execopt_s {
    char*           name;
    void          (*func) (const char** args, int tag);
    int             requiredArgs;
    int             tag;
} execopt_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void X_CreateLUTs(void);
extern void X_DestroyLUTs(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void handleArgs();
static void execOptionScripts(const char** args, int tag);
static void execOptionDevMaps(const char** args, int tag);
static void execOptionSkill(const char** args, int tag);
static void execOptionPlayDemo(const char** args, int tag);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int verbose;

boolean DevMaps; // true = map development mode.
char* DevMapsDir = ""; // Development maps directory.

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
const float defFontRGB[] = { .9f, 0.0f, 0.0f};
const float defFontRGB2[] = { .9f, .9f, .9f};

// Network games parameters.

boolean autoStart;

FILE   *debugFile;

char   *borderLumps[] = {
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

static int warpMap;

static execopt_t execOptions[] = {
    {"-scripts", execOptionScripts, 1, 0},
    {"-devmaps", execOptionDevMaps, 1, 0},
    {"-skill", execOptionSkill, 1, 0},
    {"-playdemo", execOptionPlayDemo, 1, 0},
    {"-timedemo", execOptionPlayDemo, 1, 0},
    {NULL, NULL, 0, 0} // Terminator.
};

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

    if(W_CheckNumForName("MAP05") >= 0)
    {
        // Normal Hexen.
        strcpy(gameModeString, "hexen");
        G_SetGameMode(registered);
    }

    // This is not a very accurate test...
    if(W_CheckNumForName("MAP59") >= 0 && W_CheckNumForName("MAP60") >= 0)
    {
        // It must be Deathkings!
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
    // The startup WADs.
    DD_AddIWAD("}data\\jhexen\\hexen.wad");
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
    int                     i;

    // Calculate the various LUTs used by the playsim.
    X_CreateLUTs();

    G_SetGameMode(indetermined);

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
    cfg.menuColor[0] = defFontRGB[0];   // use the default colour by default.
    cfg.menuColor[1] = defFontRGB[1];
    cfg.menuColor[2] = defFontRGB[2];
    cfg.menuColor2[0] = defFontRGB2[0]; // use the default colour by default.
    cfg.menuColor2[1] = defFontRGB2[1];
    cfg.menuColor2[2] = defFontRGB2[2];
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
    cfg.msgAlign = ALIGN_CENTER;
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

    // Hexen has a nifty "Ethereal Travel" screen, so don't show the
    // console during map setup.
    Con_SetInteger("con-show-during-setup", 0, true);

    // Do the common pre init routine.
    G_CommonPreInit();
}

/**
 * Post Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PostInit(void)
{
    int                     p;
    int                     pClass;
    char                    mapStr[6];

    // Do this early as other systems need to know.
    P_InitPlayerClassInfo();

    // Common post init routine.
    G_CommonPostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                gameMode == shareware? "*** Hexen 4-map Beta Demo ***\n"
                    : "Hexen\n");
    Con_FPrintf(CBLF_RULER, "");

    // Game parameters.
    /* None */

    // Get skill / episode / map from parms.
    startEpisode = 0;
    startSkill = SM_MEDIUM;
    startMap = 0;

    // Game mode specific settings.
    /* None */

    // Command line options.
    handleArgs();

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
    if(autoStart || IS_NETGAME)
    {
        sprintf(mapStr,"MAP%2.2d", startMap+1);

        if(!W_CheckNumForName(mapStr))
        {
            startMap = 0;
        }
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

static void handleArgs(void)
{
    int                     p;
    execopt_t              *opt;

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
        int                     scale = 200;

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

    // Process command line options.
    for(opt = execOptions; opt->name != NULL; opt++)
    {
        p = ArgCheck(opt->name);
        if(p && p < Argc() - opt->requiredArgs)
        {
            opt->func(ArgvPtr(p), opt->tag);
        }
    }
}

static void execOptionSkill(const char** args, int tag)
{
    startSkill = args[1][0] - '1';
    autoStart = true;
}

static void execOptionPlayDemo(const char** args, int tag)
{
    char                file[256];

    sprintf(file, "%s.lmp", args[1]);
    DD_AddStartupWAD(file);
    Con_Message("Playing demo %s.lmp.\n", args[1]);
}

static void execOptionScripts(const char** args, int tag)
{
    sc_FileScripts = true;
    sc_ScriptsDir = args[1];
}

static void execOptionDevMaps(const char** args, int tag)
{
    char*               str;

    DevMaps = true;
    Con_Message("Map development mode enabled:\n");
    Con_Message("[config    ] = %s\n", args[1]);
    SC_OpenFileCLib(args[1]);
    SC_MustGetStringName("mapsdir");
    SC_MustGetString();
    Con_Message("[mapsdir   ] = %s\n", sc_String);
    DevMapsDir = malloc(strlen(sc_String) + 1);
    strcpy(DevMapsDir, sc_String);
    SC_MustGetStringName("scriptsdir");
    SC_MustGetString();
    Con_Message("[scriptsdir] = %s\n", sc_String);
    sc_FileScripts = true;
    str = malloc(strlen(sc_String) + 1);
    strcpy(str, sc_String);
    sc_ScriptsDir = str;

    while(SC_GetString())
    {
        if(SC_Compare("file"))
        {
            SC_MustGetString();
            DD_AddStartupWAD(sc_String);
        }
        else
        {
            SC_ScriptError(NULL);
        }
    }
    SC_Close();
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
    R_ShutdownVectorGraphics();
    X_DestroyLUTs();
    P_FreeWeaponSlots();
    GUI_Shutdown();
}

void G_EndFrame(void)
{
    SN_UpdateActiveSequences();
}
