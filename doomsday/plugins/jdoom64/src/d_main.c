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
 * d_main.c: Initialization - jDoom64 specifc.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "jdoom64.h"

#include "m_argv.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_log.h"
#include "hu_msg.h"
#include "p_saveg.h"
#include "p_mapspec.h"
#include "p_switch.h"
#include "am_map.h"
#include "g_defs.h"
#include "p_inventory.h"
#include "p_player.h"

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

float turboMul; // multiplier for turbo
boolean monsterInfight;

skillmode_t startSkill;
uint startEpisode;
uint startMap;
boolean autoStart;
FILE *debugFile;

gamemode_t gameMode;
int gameModeBits;

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

// Print title for every printed line.
char title[128];

// Demo loop.
int demoSequence;
int pageTic;
char *pageName;

// The patches used in drawing the view border.
char *borderLumps[] = {
    "FTILEABC",
    "brdr_t",
    "brdr_r",
    "brdr_b",
    "brdr_l",
    "brdr_tl",
    "brdr_tr",
    "brdr_br",
    "brdr_bl"
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Attempt to change the current game mode. Can only be done when not
 * actually in a level.
 *
 * \todo Doesn't actually do anything yet other than set the game mode
 *  global vars.
 *
 * @param mode          The game mode to change to.
 *
 * @return boolean      @c true, if we changed game modes successfully.
 */
boolean G_SetGameMode(gamemode_t mode)
{
    gameMode = mode;

    if(G_GetGameState() == GS_MAP)
        return false;

    switch(mode)
    {
    case commercial:
        gameModeBits = GM_COMMERCIAL;
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
 * Check which known IWADs are found. The purpose of this routine is to
 * find out which IWADs the user lets us to know about, but we don't
 * decide which one gets loaded or even see if the WADs are actually
 * there. The default location for IWADs is Data\GAMENAMETEXT\.
 */
void G_DetectIWADs(void)
{
    int                 k;
    char                fn[256];
    // The '}' means the paths are affected by the base path.
    char               *paths[] = {
        "}data\\"GAMENAMETEXT"\\",
        "}data\\",
        "}",
        "}iwads\\",
        "",
        0
    };

    // Tell the engine about all the possible IWADs.
    for(k = 0; paths[k]; ++k)
    {
        sprintf(fn, "%s%s", paths[k], "doom64.wad");
        DD_AddIWAD(fn);
    }
}

static boolean lumpsFound(char **list)
{
    for(; *list; list++)
        if(W_CheckNumForName(*list) == -1)
            return false;

    return true;
}

/**
 * Checks availability of IWAD files by name, to determine whether
 * registered/commercial features  should be executed (notably loading
 * PWAD's).
 */
static void identifyFromData(void)
{
    typedef struct {
        char          **lumps;
        gamemode_t      mode;
    } identify_t;
    char               *commercialLumps[] = {
        // List of lumps to detect registered with.
        "map01", "map02", "map38",
        "f_suck", NULL
    };
    identify_t          list[] = {
        {commercialLumps, commercial},
    };
    int                 i, num = sizeof(list) / sizeof(identify_t);

    // Now we must look at the lumps.
    for(i = 0; i < num; ++i)
    {
        // If all the listed lumps are found, selection is made.
        // All found?
        if(lumpsFound(list[i].lumps))
        {
            G_SetGameMode(list[i].mode);
            return;
        }
    }

    // A detection couldn't be made.
    G_SetGameMode(commercial); // Assume the minimum.
    Con_Message("\nIdentifyVersion: DOOM64 version unknown.\n"
                "** Important data might be missing! **\n\n");
}

/**
 * gameMode, gameMission and the gameModeString are set.
 */
void G_IdentifyVersion(void)
{
    identifyFromData();

    // The game mode string is returned in DD_Get(DD_GAME_MODE).
    // It is sent out in netgames, and the pcl_hello2 packet contains it.
    // A client can't connect unless the same game mode is used.
    memset(gameModeString, 0, sizeof(gameModeString));

    strcpy(gameModeString, "doom64");
}

/**
 * Pre Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    int                 i;

    G_SetGameMode(indetermined);

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
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
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
    cfg.menuColor[0] = 1;
    cfg.menuColor2[0] = 1;
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
    cfg.msgAlign = ALIGN_LEFT;
    cfg.msgBlink = 5;

    cfg.msgColor[0] = cfg.msgColor[1] = cfg.msgColor[2] = 1;

    cfg.chatBeep = 1;

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

    cfg.berserkAutoSwitch = true;

    // Do the common pre init routine.
    G_CommonPreInit();
}

/**
 * Post Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PostInit(void)
{
    int                 p;
    filename_t          file;
    char                mapStr[6];

    // Common post init routine.
    G_CommonPostInit();

    // Initialize ammo info.
    P_InitAmmoInfo();

    // Initialize weapon info.
    P_InitWeaponInfo();

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                "Doom64 Startup\n");
    Con_FPrintf(CBLF_RULER, "");

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
        int                 time;

        time = atoi(Argv(p + 1));
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
        int                 scale = 200;

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
        SV_GetSaveGameFileName(file, Argv(p + 1)[0] - '0',
                               FILENAME_T_MAXLEN);
        G_LoadGame(file);
    }

    // Check valid episode and map.
    if((autoStart || IS_NETGAME))
    {
        sprintf(mapStr, "MAP%2.2d", startMap+1);

        if(!W_CheckNumForName(mapStr))
        {
            startEpisode = 0;
            startMap = 0;
        }
    }

    // Print a string showing the state of the game parameters.
    Con_Message("Game state parameters:%s%s%s%s%s\n",
                noMonstersParm? " nomonsters" : "",
                respawnParm? " respawn" : "",
                fastParm? " fast" : "",
                turboParm? " turbo" : "",
                (cfg.netDeathmatch ==1)? " deathmatch" :
                    (cfg.netDeathmatch ==2)? " altdeath" : "");

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
    R_ShutdownVectorGraphics();
    P_FreeWeaponSlots();
    GUI_Shutdown();
}

void G_EndFrame(void)
{
    // Nothing to do.
}
