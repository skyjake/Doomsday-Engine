/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * d_main.c: Game initialization (jDoom-specific).
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "m_argv.h"
#include "hu_stuff.h"
#include "hu_log.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "p_saveg.h"
#include "p_mapspec.h"
#include "p_switch.h"
#include "am_map.h"
#include "g_defs.h"

// MACROS ------------------------------------------------------------------

#define BGCOLOR                 (7)
#define FGCOLOR                 (8)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int verbose;

boolean devParm; // checkparm of -devparm
boolean noMonstersParm; // checkparm of -noMonstersParm
boolean respawnParm; // checkparm of -respawn
boolean fastParm; // checkparm of -fast
boolean turboParm; // checkparm of -turbo

float turboMul; // multiplier for turbo
boolean monsterInfight;

gamemode_t gameMode;
int gameModeBits;
gamemission_t gameMission = GM_DOOM;

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

// The patches used in drawing the view border.
char *borderLumps[] = {
    "FLOOR7_2",
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

static skillmode_t startSkill;
static int startEpisode;
static int startMap;
static boolean autoStart;

// CODE --------------------------------------------------------------------

/**
 * Attempt to change the current game mode. Can only be done when not
 * actually in a map.
 *
 * \todo Doesn't actually do anything yet other than set the game mode
 * global vars.
 *
 * @param mode          The game mode to change to.
 *
 * @return              @true, if we changed game modes successfully.
 */
boolean G_SetGameMode(gamemode_t mode)
{
    gameMode = mode;

    if(G_GetGameState() == GS_MAP)
        return false;

    switch(mode)
    {
    case shareware: // DOOM 1 shareware, E1, M9
        gameModeBits = GM_SHAREWARE;
        break;

    case registered: // DOOM 1 registered, E3, M27
        gameModeBits = GM_REGISTERED;
        break;

    case commercial: // DOOM 2 retail, E1 M34
        gameModeBits = GM_COMMERCIAL;
        break;

    // DOOM 2 german edition not handled

    case retail: // DOOM 1 retail, E4, M36
        gameModeBits = GM_RETAIL;
        break;

    case indetermined: // Well, no IWAD found.
        gameModeBits = GM_INDETERMINED;
        break;

    default:
        Con_Error("G_SetGameMode: Unknown gameMode %i", mode);
    }

    return true;
}

/**
 * Check which known IWADs are found. The purpose of this routine is to
 * find out which IWADs the user lets us to know about, but we don't decide
 * which one gets loaded or even see if the WADs are actually there.
 *
 * The default location for IWADs is Data\GAMENAMETEXT\.
 */
void G_DetectIWADs(void)
{
    typedef struct {
        char   *file;
        char   *override;
    } fspec_t;

    // The '>' means the paths are affected by the base path.
    char   *paths[] = {
        "}data\\"GAMENAMETEXT"\\",
        "}data\\",
        "}",
        "}iwads\\",
        "",
        0
    };
    fspec_t iwads[] = {
        {"tnt.wad", "-tnt"},
        {"plutonia.wad", "-plutonia"},
        {"doom2.wad", "-doom2"},
        {"doom1.wad", "-sdoom"},
        {"doom.wad", "-doom"},
        {"doom.wad", "-ultimate"},
        {"doomu.wad", "-udoom"},
    {"freedoom.wad", "-freedoom"},
        {0, 0}
    };
    int                 i, k;
    boolean             overridden = false;
    char                fn[256];

    // First check if an overriding command line option is being used.
    for(i = 0; iwads[i].file; ++i)
        if(ArgExists(iwads[i].override))
        {
            overridden = true;
            break;
        }

    // Tell the engine about all the possible IWADs.
    for(k = 0; paths[k]; k++)
        for(i = 0; iwads[i].file; ++i)
        {
            // Are we allowed to use this?
            if(overridden && !ArgExists(iwads[i].override))
                continue;
            sprintf(fn, "%s%s", paths[k], iwads[i].file);
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
        char  **lumps;
        gamemode_t mode;
    } identify_t;

    char   *shareware_lumps[] = {
        // List of lumps to detect shareware with.
        "e1m1", "e1m2", "e1m3", "e1m4", "e1m5", "e1m6",
        "e1m7", "e1m8", "e1m9",
        "d_e1m1", "floor4_8", "floor7_2", NULL
    };
    char   *registered_lumps[] = {
        // List of lumps to detect registered with.
        "e2m1", "e2m2", "e2m3", "e2m4", "e2m5", "e2m6",
        "e2m7", "e2m8", "e2m9",
        "e3m1", "e3m2", "e3m3", "e3m4", "e3m5", "e3m6",
        "e3m7", "e3m8", "e3m9",
        "cybre1", "cybrd8", "floor7_2", NULL
    };
    char   *retail_lumps[] = {
        // List of lumps to detect Ultimate Doom with.
        "e4m1", "e4m2", "e4m3", "e4m4", "e4m5", "e4m6",
        "e4m7", "e4m8", "e4m9",
        "m_epi4", NULL
    };
    char   *commercial_lumps[] = {
        // List of lumps to detect Doom II with.
        "map01", "map02", "map03", "map04", "map10", "map20",
        "map25", "map30",
        "vilen1", "vileo1", "vileq1", "grnrock", NULL
    };
    char   *plutonia_lumps[] = {
        "_deutex_", "mc5", "mc11", "mc16", "mc20", NULL
    };
    char   *tnt_lumps[] = {
        "cavern5", "cavern7", "stonew1", NULL
    };
    identify_t list[] = {
        {commercial_lumps, commercial}, // Doom2 is easiest to detect.
        {retail_lumps, retail}, // Ultimate Doom is obvious.
        {registered_lumps, registered},
        {shareware_lumps, shareware}
    };
    int         i, num = sizeof(list) / sizeof(identify_t);

    // First check the command line.
    if(ArgCheck("-sdoom"))
    {
        // Shareware DOOM.
        G_SetGameMode(shareware);
        return;
    }

    if(ArgCheck("-doom"))
    {
        // Registered DOOM.
        G_SetGameMode(registered);
        return;
    }

    if(ArgCheck("-doom2") || ArgCheck("-plutonia") || ArgCheck("-tnt") || ArgCheck("-freedoom"))
    {
        // DOOM 2.
        G_SetGameMode(commercial);
        gameMission = GM_DOOM2;
        if(ArgCheck("-plutonia"))
            gameMission = GM_PLUT;
        if(ArgCheck("-tnt"))
            gameMission = GM_TNT;
        return;
    }

    if(ArgCheck("-ultimate") || ArgCheck("-udoom"))
    {
        // Retail DOOM 1: Ultimate DOOM.
        G_SetGameMode(retail);
        return;
    }

    // Now we must look at the lumps.
    for(i = 0; i < num; ++i)
    {
        // If all the listed lumps are found, selection is made.
        // All found?
        if(lumpsFound(list[i].lumps))
        {
            G_SetGameMode(list[i].mode);
            // Check the mission packs.
            if(lumpsFound(plutonia_lumps))
                gameMission = GM_PLUT;
            else if(lumpsFound(tnt_lumps))
                gameMission = GM_TNT;
            else if(gameMode == commercial)
                gameMission = GM_DOOM2;
            else
                gameMission = GM_DOOM;
            return;
        }
    }

    // A detection couldn't be made.
    G_SetGameMode(shareware);       // Assume the minimum.
    Con_Message("\nIdentifyVersion: DOOM version unknown.\n"
                "** Important data might be missing! **\n\n");
}

/**
 * gameMode, gameMission and the gameModeString are set.
 */
void G_IdentifyVersion(void)
{
    identifyFromData();

    // The game mode string is returned in DD_Get(DD_GAME_MODE).
    // It is sent out in netgames, and the PCL_HELLO2 packet contains it.
    // A client can't connect unless the same game mode is used.
    memset(gameModeString, 0, sizeof(gameModeString));

    strcpy(gameModeString,
           gameMode == shareware ? "doom1-share" :
           gameMode == registered ? "doom1" :
           gameMode == retail ? "doom1-ultimate" :
           gameMode == commercial ?
                (gameMission == GM_PLUT ? "doom2-plut" :
                 gameMission == GM_TNT ? "doom2-tnt" : "doom2") :
           "-");
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

    pf->hud.keysCombine = false;
    pf->hud.shown[HUD_HEALTH] = true;
    pf->hud.shown[HUD_ARMOR] = true;
    pf->hud.shown[HUD_AMMO] = true;
    pf->hud.shown[HUD_KEYS] = true;
    pf->hud.shown[HUD_FRAGS] = true;
    pf->hud.shown[HUD_FACE] = false;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
        pf->hud.unHide[i] = 1;
    pf->hud.scale = .6f;
    pf->hud.color[CR] = 1;
    pf->hud.color[CG] = 0;
    pf->hud.color[CB] = 0;
    pf->hud.color[CA] = 1;
    pf->hud.iconAlpha = 1;
    pf->hud.counterCheatScale = .7f;

    pf->xhair.size = .5f;
    pf->xhair.vitality = false;
    pf->xhair.color[CR] = 1;
    pf->xhair.color[CG] = 1;
    pf->xhair.color[CB] = 1;
    pf->xhair.color[CA] = 1;

    pf->camera.offsetZ = 41;
    pf->camera.bob = 1;
    pf->camera.povLookAround = true;

    pf->inventory.weaponAutoSwitch = 1; // if better
    pf->inventory.noWeaponAutoSwitchIfFiring = false;
    pf->inventory.ammoAutoSwitch = 0; // never
    pf->inventory.weaponOrder[0] = WT_SIXTH;
    pf->inventory.weaponOrder[1] = WT_NINETH;
    pf->inventory.weaponOrder[2] = WT_FOURTH;
    pf->inventory.weaponOrder[3] = WT_THIRD;
    pf->inventory.weaponOrder[4] = WT_SECOND;
    pf->inventory.weaponOrder[5] = WT_EIGHTH;
    pf->inventory.weaponOrder[6] = WT_FIFTH;
    pf->inventory.weaponOrder[7] = WT_SEVENTH;
    pf->inventory.weaponOrder[8] = WT_FIRST;
    pf->inventory.berserkAutoSwitch = true;

    pf->statusbar.fixOuchFace = true;
    pf->statusbar.scale = 20; // Full size.
    pf->statusbar.opacity = 1;
    pf->statusbar.counterAlpha = 1;

    pf->automap.customColors = 0; // Never.
    pf->automap.line0[CR] = .4f; // Unseen areas
    pf->automap.line0[CG] = .4f;
    pf->automap.line0[CB] = .4f;
    pf->automap.line1[CR] = 1.f; // onesided lines
    pf->automap.line1[CG] = 0.f;
    pf->automap.line1[CB] = 0.f;
    pf->automap.line2[CR] = .77f; // floor height change lines
    pf->automap.line2[CG] = .6f;
    pf->automap.line2[CB] = .325f;
    pf->automap.line3[CR] = 1.f; // ceiling change lines
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
    pf->msgLog.count = 4;
    pf->msgLog.scale = .8f;
    pf->msgLog.upTime = 5 * TICSPERSEC;
    pf->msgLog.align = ALIGN_LEFT;
    pf->msgLog.blink = 5;
    pf->msgLog.color[CR] = 1;
    pf->msgLog.color[CG] = 0;
    pf->msgLog.color[CB] = 0;

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
    gr->freeAimBFG = 0; // allow free-aim 0=none 1=not BFG 2=All
    gr->mobDamageModifier = 1;
    gr->mobHealthModifier = 1;
    gr->gravityModifier = -1; // use map default
    gr->maxSkulls = true;
    gr->allowSkullsInWalls = false;
    gr->anyBossDeath = false;
    gr->monstersStuckInDoors = false;
    gr->avoidDropoffs = true;
    gr->moveBlock = false;
    gr->fallOff = true;
    gr->announceFrags = true;
    gr->cameraNoClip = true;
    gr->respawnMonstersNightmare = true;
}

/**
 * Pre Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    G_SetGameMode(indetermined);

    memset(gs.players, 0, sizeof(gs.players));
    gs.netEpisode = 1;
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
    gs.cfg.turningSkull = true;
    gs.cfg.hudFog = 1;
    gs.cfg.mapTitle = true;
    gs.cfg.hideAuthorIdSoft = true;
    gs.cfg.menuColor[0] = 1;
    gs.cfg.menuColor2[0] = 1;
    gs.cfg.menuSlam = false;
    gs.cfg.menuHotkeys = true;
    gs.cfg.askQuickSaveLoad = true;

    G_InitGameRules(&GAMERULES);
    G_InitPlayerProfile(&PLRPROFILE);

    // Do the common pre init routine;
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

    // Border background changes depending on mission.
    if(gameMission == GM_DOOM2 || gameMission == GM_PLUT ||
       gameMission == GM_TNT)
        borderLumps[0] = "GRNROCK";

    // Common post init routine
    G_CommonPostInit();

    // Initialize ammo info.
    P_InitAmmoInfo();

    // Initialize weapon info.
    P_InitWeaponInfo();

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                gameMode == retail ? "The Ultimate DOOM Startup\n" :
                gameMode == shareware ? "DOOM Shareware Startup\n" :
                gameMode == registered ? "DOOM Registered Startup\n" :
                gameMode == commercial ?
                    (gameMission == GM_PLUT ? "Final DOOM: The Plutonia Experiment\n" :
                     gameMission == GM_TNT ? "Final DOOM: TNT: Evilution\n" :
                     "DOOM 2: Hell on Earth\n") :
                "Public DOOM\n");

    Con_FPrintf(CBLF_RULER, "");

    // Game parameters.
    monsterInfight = GetDefInt("AI|Infight", 0);

    // Get skill / episode / map from parms.
    gameSkill = startSkill = SM_NOITEMS;
    startEpisode = 1;
    startMap = 1;
    autoStart = false;

    // Game mode specific settings.
    // Plutonia and TNT automatically turn on the full sky.
    if(gameMode == commercial &&
       (gameMission == GM_PLUT || gameMission == GM_TNT))
    {
        Con_SetInteger("rend-sky-full", 1, true);
    }

    // Command line options.
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

    p = ArgCheck("-episode");
    if(p && p < myargc - 1)
    {
        startEpisode = Argv(p + 1)[0] - '0';
        startMap = 1;
        autoStart = true;
    }

    p = ArgCheck("-timer");
    if(p && p < myargc - 1 && deathmatch)
    {
        int                 time;

        time = atoi(Argv(p + 1));
        Con_Message("Maps will end after %d minute", time);
        if(time > 1)
            Con_Message("s");
        Con_Message(".\n");
    }

    p = ArgCheck("-warp");
    if(p && p < myargc - 1)
    {
        if(gameMode == commercial)
        {
            startMap = atoi(Argv(p + 1));
            autoStart = true;
        }
        else if(p < myargc - 2)
        {
            startEpisode = Argv(p + 1)[0] - '0';
            startMap = Argv(p + 2)[0] - '0';
            autoStart = true;
        }
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
        if(gameMode == commercial)
            Con_Message("Warp to Map %d, Skill %d\n", startMap,
                        startSkill + 1);
        else
            Con_Message("Warp to Episode %d, Map %d, Skill %d\n",
                        startEpisode, startMap, startSkill + 1);
    }

    // Load a saved game?
    p = ArgCheck("-loadgame");
    if(p && p < myargc - 1)
    {
        SV_GetSaveGameFileName(Argv(p + 1)[0] - '0', file);
        G_LoadGame(file);
    }

    // Check valid episode and map
    if(autoStart || IS_NETGAME)
    {
        if(gameMode == commercial)
            sprintf(mapStr, "MAP%2.2d", startMap);
        else
            sprintf(mapStr, "E%d%d", startEpisode, startMap);

        if(!W_CheckNumForName(mapStr))
        {
            startEpisode = 1;
            startMap = 1;
        }
    }

    // Print a string showing the state of the game parameters
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
