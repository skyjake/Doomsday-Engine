/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
int startEpisode;
int startMap;
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

void G_InitPlayerProfile(playerprofile_t* pf)
{
    int                 i;

    if(!pf)
        return;

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(pf, 0, sizeof(playerprofile_t));
    pf->color = 4;

    pf->ctrl.moveSpeed = 1;
    pf->ctrl.dclickUse = false;
    pf->ctrl.lookSpeed = 3;
    pf->ctrl.turnSpeed = 1;
    pf->ctrl.airborneMovement = 1;
    pf->ctrl.useAutoAim = true;

    pf->screen.blocks = pf->screen.setBlocks = 10;

    pf->hud.shown[HUD_HEALTH] = true;
    pf->hud.shown[HUD_ARMOR] = true;
    pf->hud.shown[HUD_AMMO] = true;
    pf->hud.shown[HUD_KEYS] = true;
    pf->hud.shown[HUD_FRAGS] = true;
    pf->hud.shown[HUD_POWER] = false; // They will be visible when the automap is.
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
        pf->hud.unHide[i] = 1;
    pf->hud.scale = .6f;
    pf->hud.color[CR] = 1;
    pf->hud.color[CG] = 0;
    pf->hud.color[CB] = 0;
    pf->hud.color[CA] = 0.75f;
    pf->hud.iconAlpha = 0.5f;
    pf->hud.counterCheatScale = .7f; // From jHeretic.

    pf->xhair.size = .5f;
    pf->xhair.vitality = false;
    pf->xhair.color[CR] = 1;
    pf->xhair.color[CG] = 1;
    pf->xhair.color[CB] = 1;
    pf->xhair.color[CA] = 1;

    pf->camera.offsetZ = 54;
    pf->camera.bob = 1;
    pf->camera.povLookAround = true;

    pf->inventory.weaponAutoSwitch = 1; // "If better" mode.
    pf->inventory.noWeaponAutoSwitchIfFiring = false;
    pf->inventory.ammoAutoSwitch = 0; // Never.
    pf->inventory.weaponOrder[0] = WT_TENTH;
    pf->inventory.weaponOrder[1] = WT_SIXTH;
    pf->inventory.weaponOrder[2] = WT_NINETH;
    pf->inventory.weaponOrder[3] = WT_FOURTH;
    pf->inventory.weaponOrder[4] = WT_THIRD;
    pf->inventory.weaponOrder[5] = WT_SECOND;
    pf->inventory.weaponOrder[6] = WT_EIGHTH;
    pf->inventory.weaponOrder[7] = WT_FIFTH;
    pf->inventory.weaponOrder[8] = WT_SEVENTH;
    pf->inventory.weaponOrder[9] = WT_FIRST;
    pf->inventory.berserkAutoSwitch = true;

    pf->automap.customColors = 0; // Never.
    pf->automap.line0[CR] = .4f; // Unseen areas.
    pf->automap.line0[CG] = .4f;
    pf->automap.line0[CB] = .4f;
    pf->automap.line1[CR] = 1.f; // Onesided lines.
    pf->automap.line1[CG] = 0.f;
    pf->automap.line1[CB] = 0.f;
    pf->automap.line2[CR] = .77f; // Floor height change lines.
    pf->automap.line2[CG] = .6f;
    pf->automap.line2[CB] = .325f;
    pf->automap.line3[CR] = 1.f; // Ceiling change lines.
    pf->automap.line3[CG] = .95f;
    pf->automap.line3[CB] = 0.f;
    pf->automap.mobj[CR] = 0.f;
    pf->automap.mobj[CG] = 1.f;
    pf->automap.mobj[CB] = 0.f;
    pf->automap.background[CR] = 0.f;
    pf->automap.background[CG] = 0.f;
    pf->automap.background[CB] = 0.f;
    pf->automap.opacity = .7f;
    pf->automap.lineAlpha = .7f;
    pf->automap.showDoors = true;
    pf->automap.doorGlow = 8;
    pf->automap.hudDisplay = 2;
    pf->automap.rotate = true;
    pf->automap.babyKeys = false;
    pf->automap.zoomSpeed = .1f;
    pf->automap.panSpeed = .5f;
    pf->automap.panResetOnOpen = true;
    pf->automap.openSeconds = AUTOMAP_OPEN_SECONDS;

    pf->msgLog.show = true;
    pf->msgLog.count = 1;
    pf->msgLog.scale = .8f;
    pf->msgLog.upTime = 5 * TICSPERSEC;
    pf->msgLog.align = ALIGN_LEFT;
    pf->msgLog.blink = 5;
    pf->msgLog.color[CR] = 1;
    pf->msgLog.color[CG] = 1;
    pf->msgLog.color[CB] = 1;

    pf->chat.playBeep = 1;

    pf->psprite.bob = 1;
    pf->psprite.bobLower = true;
}

void G_InitGameRules(gamerules_t* gr)
{
    if(!gr)
        return;

    memset(gr, 0, sizeof(gamerules_t));

    gr->moveCheckZ = true;
    gr->jumpPower = 9;
    gr->announceSecrets = true;
    gr->slidingCorpses = false;
    gr->fastMonsters = false;
    gr->jumpAllow = true;
    gr->freeAimBFG = 0; // Allow free-aim 0=none 1=not BFG 2=All.
    gr->mobDamageModifier = 1;
    gr->mobHealthModifier = 1;
    gr->gravityModifier = -1; // Use map default.
    gr->maxSkulls = true;
    gr->allowSkullsInWalls = false;
    gr->anyBossDeath = false;
    gr->monstersStuckInDoors = false;
    gr->avoidDropoffs = true;
    gr->moveBlock = false;
    gr->fallOff = true;
    gr->announceFrags = true;
    gr->cameraNoClip = true;
    gr->weaponRecoil = true;
}

/**
 * Pre Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    G_SetGameMode(indetermined);

    memset(gs.players, 0, sizeof(gs.players));
    gs.netMap = 1;
    gs.netSkill = SM_MEDIUM;
    gs.netSlot = 0;

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&gs.cfg, 0, sizeof(gs.cfg));
    gs.cfg.echoMsg = true;
    gs.cfg.usePatchReplacement = 2; // Use built-in replacements if available.
    gs.cfg.menuScale = .9f;
    gs.cfg.menuGlitter = .5f;
    gs.cfg.menuShadow = 0.33f;
    gs.cfg.menuQuitSound = true;
    gs.cfg.flashColor[0] = .7f;
    gs.cfg.flashColor[1] = .9f;
    gs.cfg.flashColor[2] = 1;
    gs.cfg.flashSpeed = 4;
    gs.cfg.turningSkull = false;
    gs.cfg.mapTitle = true;
    gs.cfg.hideAuthorMidway = true;
    gs.cfg.menuColor[0] = 1;
    gs.cfg.menuColor2[0] = 1;
    gs.cfg.menuSlam = false;
    gs.cfg.menuHotkeys = true;
    gs.cfg.hudFog = 1;
    gs.cfg.askQuickSaveLoad = true;

    G_InitGameRules(&GAMERULES);
    G_InitPlayerProfile(&PLRPROFILE);

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
    char                file[256];
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
    startEpisode = 1;
    startMap = 1;
    autoStart = false;

    // Game mode specific settings
    // None.

    // Command line options
    noMonstersParm = ArgCheck("-nomonsters");
    respawnParm = ArgCheck("-respawn");
    fastParm = ArgCheck("-fast");
    devParm = ArgCheck("-devparm");

    if(ArgCheck("-altdeath"))
        GAMERULES.deathmatch = 2;
    else if(ArgCheck("-deathmatch"))
        GAMERULES.deathmatch = 1;

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
        startMap = atoi(Argv(p + 1));
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
        Con_Message("Warp to Episode %d, Map %d, Skill %d\n", startEpisode,
                    startMap, startSkill + 1);
    }

    // Load a saved game?
    p = ArgCheck("-loadgame");
    if(p && p < myargc - 1)
    {
        SV_GetSaveGameFileName(Argv(p + 1)[0] - '0', file);
        G_LoadGame(file);
    }

    // Check valid episode and map.
    if((autoStart || IS_NETGAME))
    {
        sprintf(mapStr, "MAP%2.2d", startMap);

        if(!W_CheckNumForName(mapStr))
        {
            startEpisode = 1;
            startMap = 1;
        }
    }

    // Print a string showing the state of the game parameters.
    Con_Message("Game state parameters:%s%s%s%s%s\n",
                noMonstersParm? " nomonsters" : "",
                respawnParm? " respawn" : "",
                fastParm? " fast" : "",
                turboParm? " turbo" : "",
                (GAMERULES.deathmatch ==1)? " deathmatch" :
                    (GAMERULES.deathmatch ==2)? " altdeath" : "");

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
    uint                i;

    Hu_MsgShutdown();
    Hu_UnloadData();

    for(i = 0; i < MAXPLAYERS; ++i)
        HUMsg_ClearMessages(i);

    P_DestroyIterList(spechit);
    P_DestroyIterList(linespecials);
    P_DestroyLineTagLists();
    P_DestroySectorTagLists();
    P_FreeButtons();
    AM_Shutdown();
}

void G_EndFrame(void)
{
    // Nothing to do.
}
