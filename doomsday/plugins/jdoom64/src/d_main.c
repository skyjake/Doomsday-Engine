/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * Initialization - DOOM64 specifc.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "jdoom64.h"

#include "m_argv.h"
#include "dmu_lib.h"
#include "fi_lib.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_log.h"
#include "hu_msg.h"
#include "hu_lib.h"
#include "p_saveg.h"
#include "p_mapspec.h"
#include "p_switch.h"
#include "am_map.h"
#include "g_defs.h"
#include "p_inventory.h"
#include "p_player.h"
#include "d_net.h"
#include "g_update.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    char** lumps;
    gamemode_t mode;
} identify_t;

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

gamemode_t gameMode = indetermined;
int gameModeBits = 0;

// Default font colours.
const float defFontRGB2[] = { .85f, 0, 0 };

// The patches used in drawing the view border.
char* borderLumps[] = {
    "FTILEABC", // Background.
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

/**
 * Attempt to change the current game mode. Can only be done when not
 * actually in a map.
 *
 * \todo Doesn't actually do anything yet other than set the game mode
 * global vars.
 *
 * @param mode          GameMode to change to.
 *
 * @return              @true, if we changed game modes successfully.
 */
boolean G_SetGameMode(int/*gamemode_t*/ mode)
{
    if(G_GetGameState() == GS_MAP)
        return false;
    gameMode = mode;
    gameModeBits = mode > 0? 1 << (mode-1) : 0;
    return true;
}

int G_RegisterGames(int hookType, int parm, void* data)
{
#define DATAPATH        DD_BASEPATH_DATA GAMENAMETEXT "\\"
#define DEFSPATH        DD_BASEPATH_DEFS GAMENAMETEXT "\\"
#define STARTUPPK3      GAMENAMETEXT ".pk3"
#define STARTUPDED      GAMENAMETEXT ".ded"
#define NUMELEMENTS(v)  (sizeof(v)/sizeof((v)[0]))

    const char* lumps[] = { "map01", "map02", "map38", "f_suck" };
    gameid_t gameId = DD_AddGame(doom64, "doom64", DATAPATH, DEFSPATH, STARTUPDED, "Doom 64", "Midway Software", "doom64", 0, lumps, NUMELEMENTS(lumps));
    DD_AddGameResource(gameId, RT_PACKAGE, DDRC_WAD, "doom64.wad");
    DD_AddGameResource(gameId, RT_PACKAGE, DDRC_ZIP, STARTUPPK3);
    return true;

#undef NUMELEMENTS
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
    cfg.menuNoStretch = false;
    cfg.flashColor[0] = .7f;
    cfg.flashColor[1] = .9f;
    cfg.flashColor[2] = 1;
    cfg.flashSpeed = 4;
    cfg.turningSkull = false;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_FRAGS] = true;
    cfg.hudShown[HUD_INVENTORY] = false; // They will be visible when the automap is.
    cfg.hudShown[HUD_LOG] = true;
    { int i;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    }
    cfg.hudScale = .6f;
    cfg.hudWideOffset = 1;
    cfg.hudColor[0] = 1;
    cfg.hudColor[1] = cfg.hudColor[2] = 0;
    cfg.hudColor[3] = 0.75f;
    cfg.hudFog = 1;
    cfg.hudIconAlpha = 0.5f;
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
    cfg.fastMonsters = false;
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
    cfg.hideIWADAuthor = true;
    cfg.menuColors[0][CR] = 1;
    cfg.menuColors[0][CG] = 0;
    cfg.menuColors[0][CB] = 0;
    cfg.menuColors[1][CR] = 1;
    cfg.menuColors[1][CG] = 0;
    cfg.menuColors[1][CB] = 0;
    cfg.menuColors[2][CR] = 1;
    cfg.menuColors[2][CG] = 0;
    cfg.menuColors[2][CB] = 0;
    cfg.menuSlam = false;
    cfg.menuHotkeys = true;
    cfg.askQuickSaveLoad = true;

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
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;
    cfg.counterCheatScale = .7f; // From jHeretic.

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

    // Do the common pre init routine.
    G_CommonPreInit();
}

/**
 * Post Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PostInit(int mode)
{
    filename_t file;
    int p;

    G_SetGameMode((gamemode_t)mode);

    // Common post init routine.
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

    // Game mode specific settings
    // None.

    // Command line options
    noMonstersParm = ArgCheck("-nomonsters");
    respawnParm = ArgCheck("-respawn");
    fastParm = ArgCheck("-fast");
    devParm = ArgCheck("-devparm");

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

    p = ArgCheck("-timer");
    if(p && p < myargc - 1 && deathmatch)
    {
        int time = atoi(Argv(p + 1));
        Con_Message("Levels will end after %d minute", time);
        if(time > 1)
            Con_Message("s");
        Con_Message(".\n");
    }

    p = ArgCheck("-warp");
    if(p && p < myargc - 1)
    {
        startMap = atoi(Argv(p + 1)) - '1';
        autoStart = true;
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
        Con_Message("Warp to Episode %d, Map %d, Skill %d\n", startEpisode+1,
                    startMap+1, startSkill + 1);
    }

    // Load a saved game?
    p = ArgCheck("-loadgame");
    if(p && p < myargc - 1)
    {
        SV_GetSaveGameFileName(file, Argv(p + 1)[0] - '0', FILENAME_T_MAXLEN);
        G_LoadGame(file);
    }

    // Check valid episode and map.
    if((autoStart || IS_NETGAME) && !P_MapExists(0, startMap))
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
    P_ShutdownInventory();
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
