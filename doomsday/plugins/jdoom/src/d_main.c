/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * Game initialization (jDoom-specific).
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>

#include "jdoom.h"

#include "d_netsv.h"
#include "d_net.h"
#include "m_argv.h"
#include "dmu_lib.h"
#include "fi_lib.h"
#include "hu_stuff.h"
#include "hu_log.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_lib.h"
#include "p_saveg.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_switch.h"
#include "am_map.h"
#include "g_defs.h"
#include "p_player.h"
#include "g_update.h"

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
boolean fastParm; // checkparm of -fast
boolean turboParm; // checkparm of -turbo
//boolean randomClassParm; // checkparm of -randclass

float turboMul; // Multiplier for turbo.

gamemode_t gameMode;
int gameModeBits;

// Default font colours.
const float defFontRGB2[] = { .85f, 0, 0 };
const float defFontRGB3[] = { 1, .9f, .4f };

// The patches used in drawing the view border.
char* borderLumps[] = {
    "FLOOR7_2", // Background.
    "BRDR_T", // Top.
    "BRDR_R", // Right.
    "BRDR_B", // Bottom.
    "BRDR_L", // Left.
    "BRDR_TL", // Top left.
    "BRDR_TR", // Top right.
    "BRDR_BR", // Bottom right.
    "BRDR_BL" // Bottom left.
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// The interface to the Doomsday engine.
game_import_t gi;
game_export_t gx;

// Identifiers given to the games we register during startup.
gameid_t gameIds[NUM_GAME_MODES];

static skillmode_t startSkill;
static uint startEpisode;
static uint startMap;
static boolean autoStart;

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
        return xgClasses;

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
#define MAINDEF         GAMENAMETEXT ".ded"
#define MAINCONFIG      GAMENAMETEXT ".cfg"
#define STARTUPPK3      GAMENAMETEXT ".pk3"

    /* HacX */
    gameIds[doom2_hacx] = DD_AddGame("hacx", DATAPATH, DEFSPATH, MAINDEF, MAINCONFIG, "HACX - Twitch 'n Kill", "Banjo Software", "hacx", 0);
    DD_AddGameResource(gameIds[doom2_hacx], RC_PACKAGE, "hacx.wad", "HACX-R");
    DD_AddGameResource(gameIds[doom2_hacx], RC_PACKAGE, STARTUPPK3, 0);

    /* DOOM2 (TNT) */
    gameIds[doom2_tnt] = DD_AddGame("doom2-tnt", DATAPATH, DEFSPATH, MAINDEF, MAINCONFIG, "Final DOOM: TNT: Evilution", "Team TNT", "tnt", 0);
    DD_AddGameResource(gameIds[doom2_tnt], RC_PACKAGE, "tnt.wad", "CAVERN5;CAVERN7;STONEW1");
    DD_AddGameResource(gameIds[doom2_tnt], RC_PACKAGE, STARTUPPK3, 0);

    /* DOOM2 (Plutonia) */
    gameIds[doom2_plut] = DD_AddGame("doom2-plut", DATAPATH, DEFSPATH, MAINDEF, MAINCONFIG, "Final DOOM: The Plutonia Experiment", "Dario Casali and Milo Casali", "plutonia", "plut");
    DD_AddGameResource(gameIds[doom2_plut], RC_PACKAGE, "plutonia.wad", "_DEUTEX_;MC5;MC11;MC16;MC20");
    DD_AddGameResource(gameIds[doom2_plut], RC_PACKAGE, STARTUPPK3, 0);

    /* DOOM2 */
    gameIds[doom2] = DD_AddGame("doom2", DATAPATH, DEFSPATH, MAINDEF, MAINCONFIG, "DOOM 2: Hell on Earth", "id Software", "doom2", 0);
    DD_AddGameResource(gameIds[doom2], RC_PACKAGE, "doom2f.wad;doom2.wad", "MAP01;MAP02;MAP03;MAP04;MAP10;MAP20;MAP25;MAP30;VILEN1;VILEO1;VILEQ1;GRNROCK");
    DD_AddGameResource(gameIds[doom2], RC_PACKAGE, STARTUPPK3, 0);

    /* DOOM (Ultimate) */
    gameIds[doom_ultimate] = DD_AddGame("doom1-ultimate", DATAPATH, DEFSPATH, MAINDEF, MAINCONFIG, "The Ultimate DOOM", "id Software", "ultimatedoom", "udoom");
    DD_AddGameResource(gameIds[doom_ultimate], RC_PACKAGE, "doomu.wad;doom.wad", "E4M1;E4M2;E4M3;E4M4;E4M5;E4M6;E4M7;E4M8;E4M9;M_EPI4");
    DD_AddGameResource(gameIds[doom_ultimate], RC_PACKAGE, STARTUPPK3, 0);

    /* DOOM */
    gameIds[doom] = DD_AddGame("doom1", DATAPATH, DEFSPATH, MAINDEF, MAINCONFIG, "DOOM Registered", "id Software", "doom", 0);
    DD_AddGameResource(gameIds[doom], RC_PACKAGE, "doom.wad", "E2M1;E2M2;E2M3;E2M4;E2M5;E2M6;E2M7;E2M8;E2M9;E3M1;E3M2;E3M3;E3M4;E3M5;E3M6;E3M7;E3M8;E3M9;CYBRE1;CYBRD8;FLOOR7_2");
    DD_AddGameResource(gameIds[doom], RC_PACKAGE, STARTUPPK3, 0);

    /* DOOM (Shareware) */
    gameIds[doom_shareware] = DD_AddGame("doom1-share", DATAPATH, DEFSPATH, MAINDEF, MAINCONFIG, "DOOM Shareware", "id Software", "sdoom", 0);
    DD_AddGameResource(gameIds[doom_shareware], RC_PACKAGE, "doom1.wad", "E1M1;E1M2;E1M3;E1M4;E1M5;E1M6;E1M7;E1M8;E1M9;D_E1M1;FLOOR4_8;FLOOR7_2");
    DD_AddGameResource(gameIds[doom_shareware], RC_PACKAGE, STARTUPPK3, 0);
    return true;

#undef STARTUPPK3
#undef MAINCONFIG
#undef MAINDEF
#undef DEFSPATH
#undef DATAPATH
}

/**
 * Pre Game Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.dclickUse = false;
    cfg.povLookAround = true;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.usePatchReplacement = 2; // Use built-in replacements if available.
    cfg.menuScale = .9f;
    cfg.menuGlitter = .5f;
    cfg.menuShadow = 0.33f;
    cfg.menuQuitSound = true;
    cfg.menuEffects = 1; // Do type-in effect.
    cfg.flashColor[0] = .7f;
    cfg.flashColor[1] = .9f;
    cfg.flashColor[2] = 1;
    cfg.flashSpeed = 4;
    cfg.turningSkull = true;
    cfg.hudKeysCombine = false;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_FRAGS] = true;
    cfg.hudShown[HUD_FACE] = false;
    cfg.hudShown[HUD_LOG] = true;
    { int i;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    }
    cfg.hudScale = .6f;
    cfg.hudWideOffset = 1;
    cfg.hudColor[0] = .85f;
    cfg.hudColor[1] = cfg.hudColor[2] = 0;
    cfg.hudColor[3] = 1;
    cfg.hudFog = 1;
    cfg.hudIconAlpha = 1;
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
    cfg.weaponAutoSwitch = 1; // if better
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // never
    cfg.secretMsg = true;
    cfg.slidingCorpses = false;
    cfg.fastMonsters = false;
    cfg.netJumping = true;
    cfg.netEpisode = 0;
    cfg.netMap = 0;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4;
    cfg.netBFGFreeLook = 0;    // allow free-aim 0=none 1=not BFG 2=All
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.hideIWADAuthor = true;
    cfg.menuColors[0][CR] = .85f;
    cfg.menuColors[0][CG] = 0;
    cfg.menuColors[0][CB] = 0;
    cfg.menuColors[1][CR] = 1;
    cfg.menuColors[1][CG] = .7f;
    cfg.menuColors[1][CB] = .3f;
    cfg.menuColors[2][CR] = 1;
    cfg.menuColors[2][CG] = 1;
    cfg.menuColors[2][CB] = 1;
    cfg.menuSlam = false;
    cfg.menuHotkeys = true;
    cfg.menuNoStretch = false;
    cfg.askQuickSaveLoad = true;

    cfg.maxSkulls = true;
    cfg.allowSkullsInWalls = false;
    cfg.anyBossDeath = false;
    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;
    cfg.fixOuchFace = true;
    cfg.fixStatusbarOwnedWeapons = true;

    cfg.statusbarScale = 1;
    cfg.statusbarOpacity = 1;
    cfg.statusbarCounterAlpha = 1;

    cfg.automapCustomColors = 0; // Never.
    cfg.automapL0[0] = .4f; // Unseen areas
    cfg.automapL0[1] = .4f;
    cfg.automapL0[2] = .4f;

    cfg.automapL1[0] = 1.f; // onesided lines
    cfg.automapL1[1] = 0.f;
    cfg.automapL1[2] = 0.f;

    cfg.automapL2[0] = .77f; // floor height change lines
    cfg.automapL2[1] = .6f;
    cfg.automapL2[2] = .325f;

    cfg.automapL3[0] = 1.f; // ceiling change lines
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
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;

    cfg.counterCheatScale = .7f;

    cfg.msgCount = 4;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5;
    cfg.msgAlign = 0; // Left.
    cfg.msgBlink = 5;

    cfg.msgColor[0] = .85f;
    cfg.msgColor[1] = cfg.msgColor[2] = 0;

    cfg.chatBeep = true;

    cfg.killMessages = true;
    cfg.bobWeapon = 1;
    cfg.bobView = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;
    cfg.respawnMonstersNightmare = true;

    cfg.weaponOrder[0] = WT_SIXTH;
    cfg.weaponOrder[1] = WT_NINETH;
    cfg.weaponOrder[2] = WT_FOURTH;
    cfg.weaponOrder[3] = WT_THIRD;
    cfg.weaponOrder[4] = WT_SECOND;
    cfg.weaponOrder[5] = WT_EIGHTH;
    cfg.weaponOrder[6] = WT_FIFTH;
    cfg.weaponOrder[7] = WT_SEVENTH;
    cfg.weaponOrder[8] = WT_FIRST;

    cfg.weaponCycleSequential = true;
    cfg.berserkAutoSwitch = true;

    // Use the DOOM transition by default.
    Con_SetInteger("con-transition", 1, 0);

    // Do the common pre init routine;
    G_CommonPreInit();
}

/**
 * Post Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PostInit(gameid_t gameId)
{
    filename_t file;
    int p;

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

    // Border background is different in DOOM2.
    if(gameModeBits & GM_ANY_DOOM2)
        borderLumps[0] = "GRNROCK";

    // Common post init routine
    G_CommonPostInit();

    // Initialize ammo info.
    P_InitAmmoInfo();

    // Initialize weapon info.
    P_InitWeaponInfo();

    // Game parameters.
    monsterInfight = GetDefInt("AI|Infight", 0);

    // Get skill / episode / map from parms.
    gameSkill = startSkill = SM_NOITEMS;
    startEpisode = 0;
    startMap = 0;
    autoStart = false;

    // Command line options.
    noMonstersParm = ArgCheck("-nomonsters")? true : false;
    respawnParm = ArgCheck("-respawn")? true : false;
    fastParm = ArgCheck("-fast")? true : false;
    devParm = ArgCheck("-devparm")? true : false;

    if(ArgCheck("-altdeath"))
        cfg.netDeathmatch = 2;
    else if(ArgCheck("-deathmatch"))
        cfg.netDeathmatch = 1;

    p = ArgCheck("-skill");
    if(p && p < myargc - 1)
    {
        startSkill = Argv(p + 1)[0] - '1';
        autoStart = true;
    }

    p = ArgCheck("-episode");
    if(p && p < myargc - 1)
    {
        startEpisode = Argv(p + 1)[0] - '1';
        startMap = 0;
        autoStart = true;
    }

    p = ArgCheck("-timer");
    if(p && p < myargc - 1 && deathmatch)
    {
        int time = atoi(Argv(p + 1));
        Con_Message("Maps will end after %d minute", time);
        if(time > 1)
            Con_Message("s");
        Con_Message(".\n");
    }

    p = ArgCheck("-warp");
    if(p && p < myargc - 1)
    {
        if(gameModeBits & GM_ANY_DOOM2)
        {
            startMap = atoi(Argv(p + 1)) - 1;
            autoStart = true;
        }
        else if(p < myargc - 2)
        {
            startEpisode = Argv(p + 1)[0] - '1';
            startMap = Argv(p + 2)[0] - '1';
            autoStart = true;
        }
    }

    // Turbo option.
    p = ArgCheck("-turbo");
    turboMul = 1.0f;
    if(p)
    {
        int scale = 200;

        turboParm = true;
        if(p < myargc - 1)
            scale = atoi(Argv(p + 1));
        if(scale < 10)
            scale = 10;
        if(scale > 400)
            scale = 400;

        Con_Message("turbo scale: %i%%\n", scale);
        turboMul = scale / 100.f;
    }

    // Are we autostarting?
    if(autoStart)
    {
        if(gameModeBits & GM_ANY_DOOM2)
            Con_Message("Warp to Map %d, Skill %d\n", startMap+1, startSkill + 1);
        else
            Con_Message("Warp to Episode %d, Map %d, Skill %d\n", startEpisode+1, startMap+1, startSkill + 1);
    }

    // Load a saved game?
    p = ArgCheck("-loadgame");
    if(p && p < myargc - 1)
    {
        SV_GetSaveGameFileName(file, Argv(p + 1)[0] - '0', FILENAME_T_MAXLEN);
        G_LoadGame(file);
    }

    // Check valid episode and map
    if((autoStart || IS_NETGAME) && !P_MapExists((gameModeBits & GM_ANY_DOOM)? startEpisode : 0, startMap))
    {
        startEpisode = 0;
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
            G_StartTitle(); // Start up intro loop.
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
    AM_Shutdown();
    P_FreeWeaponSlots();
    FI_StackShutdown();
    GUI_Shutdown();
}

/**
 * Takes a copy of the engine's entry points and exported data. Returns
 * a pointer to the structure that contains our entry points and exports.
 */
game_export_t* GetGameAPI(game_import_t* imports)
{
    // Make sure this plugin isn't newer than Doomsday...
    if(imports->version < DOOMSDAY_VERSION)
        Con_Error(GAME_NICENAME " requires at least Doomsday " DOOMSDAY_VERSION_TEXT "!\n");

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
    gx.G_Drawer = D_Display;
    gx.G_Drawer2 = D_Display2;
    gx.PrivilegedResponder = (boolean (*)(event_t*)) G_PrivilegedResponder;
    gx.FinaleResponder = FI_Responder;
    gx.G_Responder = G_Responder;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (float (*)(void *)) P_MobjGetFriction;
    gx.ConsoleBackground = D_ConsoleBg;
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
