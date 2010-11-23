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

#include <string.h>

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
#include "p_mapsetup.h"

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
const float defFontRGB[]  = { .9f, .0f, .0f };
const float defFontRGB2[] = { .9f, .9f, .9f };
const float defFontRGB3[] = { 1, .65f, .275f };

// The patches used in drawing the view border.
char* borderLumps[] = {
    "F_022", // Background.
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

// The interface to the Doomsday engine.
game_export_t gx;
game_import_t gi;

// Identifiers given to the games we register during startup.
gameid_t gameIds[NUM_GAME_MODES];

static boolean autoStart = false;
static uint startEpisode = 0;
static uint startMap = 0;
static playerclass_t startPlayerClass = PCLASS_NONE;
static skillmode_t startSkill = SM_MEDIUM;

// CODE --------------------------------------------------------------------

/**
 * Get a 32-bit integer value.
 */
int G_GetInteger(int id)
{
    switch(id)
    {
    case DD_GAME_DMUAPI_VER:
        return DMUAPI_VER;

    default:
        break;
    }
    // ID not recognized, return NULL.
    return 0;
}

/**
 * Get a pointer to the value of a named variable/constant.
 */
void* G_GetVariable(int id)
{
    static float bob[2];

    switch(id)
    {
    case DD_GAME_NAME:
        return GAMENAMETEXT;

    case DD_GAME_NICENAME:
        return GAME_NICENAME;

    case DD_GAME_ID:
        return GAMENAMETEXT " " GAME_VERSION_TEXT;

    case DD_GAME_VERSION_SHORT:
        return GAME_VERSION_TEXT;

    case DD_GAME_VERSION_LONG:
        return GAME_VERSION_TEXTLONG "\n" GAME_DETAILS;

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

    default:
        break;
    }
    return 0;
}

int G_RegisterGames(int hookType, int parm, void* data)
{
#define DATAPATH        DD_BASEPATH_DATA GAMENAMETEXT "\\"
#define DEFSPATH        DD_BASEPATH_DEFS GAMENAMETEXT "\\"
#define STARTUPPK3      GAMENAMETEXT ".pk3"
#define STARTUPDED      GAMENAMETEXT ".ded"

    /* Hexen (Death Kings) */
    gameIds[hexen_deathkings] = DD_AddGame("hexen-dk", DATAPATH, DEFSPATH, STARTUPDED, "Hexen (Deathkings of the Dark Citadel)", "Raven Software", "deathkings", "dk");
    DD_AddGameResource(gameIds[hexen_deathkings], RT_PACKAGE, "hexen.wad", "MAP08;MAP22;TINTTAB;FOGMAP;TRANTBLA;DARTA1;ARTIPORK;SKYFOG;TALLYTOP;GROVER");
    DD_AddGameResource(gameIds[hexen_deathkings], RT_PACKAGE, "hexdd.wad", "MAP59;MAP60");
    DD_AddGameResource(gameIds[hexen_deathkings], RT_PACKAGE, STARTUPPK3, 0);

    /* Hexen */
    gameIds[hexen] = DD_AddGame("hexen", DATAPATH, DEFSPATH, STARTUPDED, "Hexen", "Raven Software", "hexen", 0);
    DD_AddGameResource(gameIds[hexen], RT_PACKAGE, "hexen.wad", "MAP08;MAP22;TINTTAB;FOGMAP;TRANTBLA;DARTA1;ARTIPORK;SKYFOG;TALLYTOP;GROVER");
    DD_AddGameResource(gameIds[hexen], RT_PACKAGE, STARTUPPK3, 0);

    /* Hexen (Demo) */
    gameIds[hexen_demo] = DD_AddGame("hexen-demo", DATAPATH, DEFSPATH, STARTUPDED, "Hexen 4-map Beta Demo", "Raven Software", "dhexen", 0);
    DD_AddGameResource(gameIds[hexen_demo], RT_PACKAGE, "hexen.wad", "MAP01;MAP04;TINTTAB;FOGMAP;TRANTBLA;DARTA1;ARTIPORK;SKYFOG;TALLYTOP;GROVER");
    DD_AddGameResource(gameIds[hexen_demo], RT_PACKAGE, STARTUPPK3, 0);
    return true;

#undef STARTUPDED
#undef STARTUPPK3
#undef DEFSPATH
#undef DATAPATH
}

/**
 * Pre Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    // Calculate the various LUTs used by the playsim.
    X_CreateLUTs();

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    { int i;
    for(i = 0; i < MAXPLAYERS; ++i)
        cfg.playerClass[i] = PCLASS_FIGHTER;
    }
    cfg.playerMoveSpeed = 1;
    cfg.statusbarScale = 1;
    cfg.dclickUse = false;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.hudShown[HUD_MANA] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_CURRENTITEM] = true;
    cfg.hudShown[HUD_LOG] = true;
    { int i;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    }
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

    cfg.chatBeep = true;

    cfg.weaponOrder[0] = WT_FOURTH;
    cfg.weaponOrder[1] = WT_THIRD;
    cfg.weaponOrder[2] = WT_SECOND;
    cfg.weaponOrder[3] = WT_FIRST;

    cfg.weaponCycleSequential = true;

    // Do the common pre init routine.
    G_CommonPreInit();
}

/**
 * Post Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PostInit(gameid_t gameId)
{
    int p, warpMap;

    /// \todo Refactor me away.
    { size_t i;
    for(i = 0; i < NUM_GAME_MODES; ++i)
        if(gameIds[i] == gameId)
        {
            gameMode = (gamemode_t) i;
            gameModeBits = 1 << gameMode;
            break;
        }
    if(i == NUM_GAME_MODES)
        Con_Error("Failed gamemode lookup for id %i.", (int)gameId);
    }

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
    noMonstersParm = ArgExists("-nomonsters");
    respawnParm = ArgExists("-respawn");
    randomClassParm = ArgExists("-randclass");
    devParm = ArgExists("-devparm");

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

        Con_Message("Turbo scale: %i%%\n", scale);
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

    if((p = ArgCheck("-class")) != 0)
    {
        playerclass_t pClass = (playerclass_t)atoi(Argv(p + 1));
        if(!VALID_PLAYER_CLASS(pClass))
            Con_Message("Warning, ignoring invalid player class id=%d specified with -class\n", (int)pClass);
        else if(!PCLASS_INFO(pClass)->userSelectable)
            Con_Message("Warning, ignoring non-user-selectable player class id=%d specified with -class.\n", (int)pClass);
        else
        {
            startPlayerClass = pClass;
        }
    }

    if(startPlayerClass != PCLASS_NONE)
    {
        Con_Message("Player Class: '%s'\n", PCLASS_INFO(startPlayerClass)->niceName);
        cfg.playerClass[CONSOLEPLAYER] = startPlayerClass;
    }

    P_InitMapMusicInfo(); // Init music fields in mapinfo.

    Con_Message("Parsing SNDINFO...\n");
    S_ParseSndInfoLump();

    Con_Message("SN_InitSequenceScript: Registering sound sequences.\n");
    SN_InitSequenceScript();

    // Check for command line warping.
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
        Con_Message("Warp to Map %d (\"%s\":%d), Skill %d\n", warpMap+1, P_GetMapName(startMap), startMap+1, startSkill + 1);
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

/**
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
game_export_t *GetGameAPI(game_import_t *imports)
{
    // Take a copy of the imports, but only copy as much data as is
    // allowed and legal.
    memset(&gi, 0, sizeof(gi));
    memcpy(&gi, imports, MIN_OF(sizeof(game_import_t), imports->apiSize));

    // Clear all of our exports.
    memset(&gx, 0, sizeof(gx));

    // Fill in the data for the exports.
    gx.apiSize = sizeof(gx);
    gx.PreInit = G_PreInit;
    gx.PostInit = G_PostInit;
    gx.Shutdown = G_Shutdown;
    gx.Ticker = G_Ticker;
    gx.G_Drawer = G_Display;
    gx.G_Drawer2 = G_Display2;
    gx.PrivilegedResponder = (boolean (*)(event_t *)) G_PrivilegedResponder;
    gx.FallbackResponder = NULL; //Hu_MenuResponder;
    gx.FinaleResponder = FI_Responder;
    gx.G_Responder = G_Responder;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (float (*)(void *)) P_MobjGetFriction;
    gx.EndFrame = G_EndFrame;
    gx.ConsoleBackground = G_ConsoleBg;
    gx.UpdateState = G_UpdateState;
#undef Get
    gx.GetInteger = G_GetInteger;
    gx.GetVariable = G_GetVariable;

    gx.NetServerStart = D_NetServerStarted;
    gx.NetServerStop = D_NetServerClose;
    gx.NetConnect = D_NetConnect;
    gx.NetDisconnect = D_NetDisconnect;
    gx.NetPlayerEvent = D_NetPlayerEvent;
    gx.NetWorldEvent = D_NetWorldEvent;
    gx.HandlePacket = D_HandlePacket;
    gx.NetWriteCommands = D_NetWriteCommands;
    gx.NetReadCommands = D_NetReadCommands;

    // Data structure sizes.
    gx.ticcmdSize = sizeof(ticcmd_t);
    gx.mobjSize = sizeof(mobj_t);
    gx.polyobjSize = sizeof(polyobj_t);

    gx.SetupForMapData = P_SetupForMapData;

    // These really need better names. Ideas?
    gx.HandleMapDataPropertyValue = P_HandleMapDataPropertyValue;
    gx.HandleMapObjectStatusReport = P_HandleMapObjectStatusReport;

    return &gx;
}
