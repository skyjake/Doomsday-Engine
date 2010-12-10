/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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
 * Game initialization - Heretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "jheretic.h"

#include "m_argv.h"
#include "dmu_lib.h"
#include "fi_lib.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_log.h"
#include "hu_lib.h"
#include "p_saveg.h"
#include "d_net.h"
#include "p_mapspec.h"
#include "p_switch.h"
#include "am_map.h"
#include "p_inventory.h"
#include "p_player.h"
#include "g_update.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define GID(v)          (toGameId(v))

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
const float defFontRGB[]  = { .425f, .986f, .378f };
const float defFontRGB2[] = { 1.0f, 1.0f, 1.0f };
const float defFontRGB3[] = { 1, .65f, .275f };

// The patches used in drawing the view border.
char* borderLumps[] = {
    "FLAT513", // Background.
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
game_import_t gi;
game_export_t gx;

// Identifiers given to the games we register during startup.
gameid_t gameIds[NUM_GAME_MODES];

static skillmode_t startSkill;
static uint startEpisode;
static uint startMap;
static boolean autoStart;

// CODE --------------------------------------------------------------------

static __inline gameid_t toGameId(int gamemode)
{
    assert(gamemode >= 0 && gamemode < NUM_GAME_MODES);
    return gameIds[(gamemode_t) gamemode];
}

/**
 * Get a 32-bit integer value.
 */
int G_GetInteger(int id)
{
    switch(id)
    {
    case DD_DMU_VERSION:
        return DMUAPI_VER;

    default:
        break;
    }
    // ID not recognized, return NULL.
    return 0;
}

/**
 * Get a pointer to the value of a variable. Added for 64-bit support.
 */
void* G_GetVariable(int id)
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
        return xgClasses;

    case DD_PSPRITE_BOB_X:
        R_GetWeaponBob(DISPLAYPLAYER, &bob[0], NULL);
        return &bob[0];

    case DD_PSPRITE_BOB_Y:
        R_GetWeaponBob(DISPLAYPLAYER, NULL, &bob[1]);
        return &bob[1];

    default:
        break;
    }

    // ID not recognized, return NULL.
    return 0;
}

int G_RegisterGames(int hookType, int parm, void* data)
{
#define DATAPATH        DD_BASEPATH_DATA PLUGIN_NAMETEXT "\\"
#define DEFSPATH        DD_BASEPATH_DEFS PLUGIN_NAMETEXT "\\"
#define MAINCONFIG      PLUGIN_NAMETEXT ".cfg"
#define STARTUPPK3      PLUGIN_NAMETEXT ".pk3"

    /* Heretic (Extended) */
    gameIds[heretic_extended] = DD_AddGame("heretic-ext", DATAPATH, DEFSPATH, MAINCONFIG, "Heretic: Shadow of the Serpent Riders", "Raven Software", "hereticext", "xheretic");
    DD_AddGameResource(GID(heretic_extended), RC_PACKAGE, RF_STARTUP, "heretic.wad", "EXTENDED;E5M2;E5M7;E6M2;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    DD_AddGameResource(GID(heretic_extended), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(heretic_extended), RC_DEFINITION, 0, "heretic-ext.ded", 0);

    /* Heretic */
    gameIds[heretic] = DD_AddGame("heretic", DATAPATH, DEFSPATH, MAINCONFIG, "Heretic Registered", "Raven Software", "heretic", 0);
    DD_AddGameResource(GID(heretic), RC_PACKAGE, RF_STARTUP, "heretic.wad", "E2M2;E3M6;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    DD_AddGameResource(GID(heretic), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(heretic), RC_DEFINITION, 0, "heretic.ded", 0);

    /* Heretic (Shareware) */
    gameIds[heretic_shareware] = DD_AddGame("heretic-share", DATAPATH, DEFSPATH, MAINCONFIG, "Heretic Shareware", "Raven Software", "sheretic", 0);
    DD_AddGameResource(GID(heretic_shareware), RC_PACKAGE, RF_STARTUP, "heretic1.wad", "E1M1;MUMSIT;WIZACT;MUS_CPTD;CHKNC5;SPAXA1A5");
    DD_AddGameResource(GID(heretic_shareware), RC_PACKAGE, RF_STARTUP, STARTUPPK3, 0);
    DD_AddGameResource(GID(heretic_shareware), RC_DEFINITION, 0, "heretic-share.ded", 0);
    return true;

#undef STARTUPPK3
#undef MAINCONFIG
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
    cfg.statusbarScale = 1;
    cfg.screenBlocks = cfg.setBlocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.usePatchReplacement = 2; // Use built-in replacements if available.
    cfg.menuScale = .9f;
    cfg.menuGlitter = 0;
    cfg.menuShadow = 0;
    cfg.menuNoStretch = false;
  //cfg.menuQuitSound = true;
    cfg.flashColor[0] = .7f;
    cfg.flashColor[1] = .9f;
    cfg.flashColor[2] = 1;
    cfg.flashSpeed = 4;
    cfg.turningSkull = false;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_CURRENTITEM] = true;
    cfg.hudShown[HUD_LOG] = true;
    { int i;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    }
    cfg.hudScale = .7f;
    cfg.hudWideOffset = 1;
    cfg.hudColor[0] = .325f;
    cfg.hudColor[1] = .686f;
    cfg.hudColor[2] = .278f;
    cfg.hudColor[3] = 1;
    cfg.hudIconAlpha = 1;
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
    cfg.fastMonsters = false;
    cfg.secretMsg = true;
    cfg.netJumping = true;
    cfg.netEpisode = 0;
    cfg.netMap = 1;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4; // Use the default color by default.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1; // Use map default.
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.hideIWADAuthor = true;
    cfg.menuColors[0][0] = defFontRGB[0];
    cfg.menuColors[0][1] = defFontRGB[1];
    cfg.menuColors[0][2] = defFontRGB[2];
    cfg.menuColors[1][0] = defFontRGB2[0];
    cfg.menuColors[1][1] = defFontRGB2[1];
    cfg.menuColors[1][2] = defFontRGB2[2];
    cfg.menuColors[2][0] = defFontRGB3[0];
    cfg.menuColors[2][1] = defFontRGB3[1];
    cfg.menuColors[2][2] = defFontRGB3[2];
    cfg.menuSlam = true;
    cfg.menuHotkeys = true;
    cfg.askQuickSaveLoad = true;

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

    cfg.automapL1[0] = .294f; // onesided lines
    cfg.automapL1[1] = .196f;
    cfg.automapL1[2] = .063f;

    cfg.automapL2[0] = .184f; // floor height change lines
    cfg.automapL2[1] = .094f;
    cfg.automapL2[2] = .002f;

    cfg.automapL3[0] = .592f; // ceiling change lines
    cfg.automapL3[1] = .388f;
    cfg.automapL3[2] = .231f;

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
    cfg.automapBabyKeys = true;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.automapOpenSeconds = AUTOMAP_OPEN_SECONDS;
    cfg.counterCheatScale = .7f;

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

    cfg.menuEffects = 0;
    cfg.hudFog = 5;

    cfg.ringFilter = 1;
    cfg.tomeCounter = 10;
    cfg.tomeSound = 3;

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

    // Shareware WAD has different border background
    if(gameMode == heretic_shareware)
        borderLumps[0] = "FLOOR04";

    // Common post init routine.
    G_CommonPostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Game parameters.
    monsterInfight = GetDefInt("AI|Infight", 0);

    // Defaults for skill, episode and map.
    startSkill = SM_MEDIUM;
    startEpisode = 0;
    startMap = 0;
    autoStart = false;

    // Game mode specific settings.
    /* None */

    // Command line options.
    noMonstersParm = ArgCheck("-nomonsters");
    respawnParm = ArgCheck("-respawn");
    devParm = ArgCheck("-devparm");

    if(ArgCheck("-deathmatch"))
    {
        cfg.netDeathmatch = true;
    }

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

    p = ArgCheck("-warp");
    if(p && p < myargc - 2)
    {
        startEpisode = Argv(p + 1)[0] - '1';
        startMap = Argv(p + 2)[0] - '1';
        autoStart = true;
    }

    // turbo option.
    p = ArgCheck("-turbo");
    turboMul = 1.0f;
    if(p)
    {
        int             scale = 200;

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

    // Check valid episode and map
    if((autoStart || IS_NETGAME) && !P_MapExists(startEpisode, startMap))
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
    // Make sure this plugin isn't newer than Doomsday...
    if(imports->version < DOOMSDAY_VERSION)
        Con_Error(PLUGIN_NICENAME " requires at least " DOOMSDAY_NICENAME " " DOOMSDAY_VERSION_TEXT "!\n");

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
    gx.G_Drawer = H_Display;
    gx.G_Drawer2 = H_Display2;
    gx.PrivilegedResponder = (boolean (*)(event_t *)) G_PrivilegedResponder;
    gx.FallbackResponder = NULL; //Hu_MenuResponder;
    gx.FinaleResponder = FI_Responder;
    gx.G_Responder = G_Responder;
    gx.MobjThinker = P_MobjThinker;
    gx.MobjFriction = (float (*)(void *)) P_MobjGetFriction;
    gx.ConsoleBackground = H_ConsoleBg;
    gx.UpdateState = G_UpdateState;

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