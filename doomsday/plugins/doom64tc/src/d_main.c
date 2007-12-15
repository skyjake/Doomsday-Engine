/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * d_main.c: DOOM64TC specifc Initialization.
 */

// HEADER FILES ------------------------------------------------------------

#include "doom64tc.h"

#include "m_argv.h"
#include "hu_stuff.h"
#include "hu_msg.h"
#include "p_saveg.h"
#include "p_mapspec.h"
#include "am_map.h"

// MACROS ------------------------------------------------------------------

#define BGCOLOR     7
#define FGCOLOR     8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     verbose;

boolean devparm;                // started game with -devparm
boolean nomonsters;             // checkparm of -nomonsters
boolean respawnparm;            // checkparm of -respawn
boolean fastparm;               // checkparm of -fast
boolean turboparm;              // checkparm of -turbo
float   turbomul;               // multiplier for turbo

skillmode_t startskill;
int     startepisode;
int     startmap;
boolean autostart;
FILE   *debugfile;

gamemode_t gamemode;
int     gamemodebits;
gamemission_t gamemission = GM_DOOM;

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

boolean monsterinfight;

// print title for every printed line
char    title[128];

// Demo loop.
int     demosequence;
int     pagetic;
char   *pagename;

// The patches used in drawing the view border
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
 * @return boolean      (TRUE) if we changed game modes successfully.
 */
boolean D_SetGameMode(gamemode_t mode)
{
    gamemode = mode;

    if(G_GetGameState() == GS_LEVEL)
        return false;

    switch(mode)
    {
    case registered: // DOOM64tc
        gamemodebits = GM_REGISTERED;
        break;

    case indetermined: // Well, no IWAD found.
        gamemodebits = GM_INDETERMINED;
        break;

    default:
        Con_Error("D_SetGameMode: Unknown gamemode %i", mode);
    }

    return true;
}

void D_GetDemoLump(int num, char *out)
{
    sprintf(out, "%cDEMO%i", 'R', num);
}

/**
 * Check which known IWADs are found. The purpose of this routine is to
 * find out which IWADs the user lets us to know about, but we don't
 * decide which one gets loaded or even see if the WADs are actually
 * there. The default location for IWADs is Data\GAMENAMETEXT\.
 */
void DetectIWADs(void)
{
    // The '>' means the paths are affected by the base path.
    char   *paths[] = {
        "}data\\"GAMENAMETEXT"\\",
        "}data\\",
        "}",
        "}iwads\\",
        "",
        0
    };
    int     k;
    char    fn[256];

    // Tell the engine about all the possible IWADs.
    for(k = 0; paths[k]; k++)
    {
        sprintf(fn, "%s%s", paths[k], "doom64.wad");
        DD_AddIWAD(fn);
    }
}

boolean LumpsFound(char **list)
{
    for(; *list; list++)
        if(W_CheckNumForName(*list) == -1)
            return false;
    return true;
}

/*
 * Checks availability of IWAD files by name, to determine whether
 * registered/commercial features  should be executed (notably loading
 * PWAD's).
 */
void D_IdentifyFromData(void)
{
    typedef struct {
        char  **lumps;
        gamemode_t mode;
    } identify_t;

    char   *registered_lumps[] = {
        // List of lumps to detect registered with.
        "e1m01", "e1m02", "e2m03",
        "f_suck", NULL
    };
    identify_t list[] = {
        {registered_lumps, registered},
    };
    int     i, num = sizeof(list) / sizeof(identify_t);

    // Now we must look at the lumps.
    for(i = 0; i < num; ++i)
    {
        // If all the listed lumps are found, selection is made.
        // All found?
        if(LumpsFound(list[i].lumps))
        {
            D_SetGameMode(list[i].mode);
            gamemission = GM_DOOM;
            return;
        }
    }

    // A detection couldn't be made.
    D_SetGameMode(registered);       // Assume the minimum.
    Con_Message("\nIdentifyVersion: DOOM64TC version unknown.\n"
                "** Important data might be missing! **\n\n");
}

/*
 * gamemode, gamemission and the gameModeString are set.
 */
void G_IdentifyVersion(void)
{
    D_IdentifyFromData();

    // The game mode string is returned in DD_Get(DD_GAME_MODE).
    // It is sent out in netgames, and the pcl_hello2 packet contains it.
    // A client can't connect unless the same game mode is used.
    memset(gameModeString, 0, sizeof(gameModeString));

    strcpy(gameModeString, "doom64tc");
}

/*
 *  Pre Engine Initialization routine.
 *    All game-specific actions that should take place at this time go here.
 */
void D_PreInit(void)
{
    int     i;

    D_SetGameMode(indetermined);

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&cfg, 0, sizeof(cfg));
    cfg.playerMoveSpeed = 1;
    cfg.dclickuse = false;
    cfg.povLookAround = true;
    cfg.screenblocks = cfg.setblocks = 10;
    cfg.echoMsg = true;
    cfg.lookSpeed = 3;
    cfg.turnSpeed = 1;
    cfg.usePatchReplacement = 2; // Use built-in replacements if available.
    cfg.menuScale = .9f;
    cfg.menuGlitter = .5f;
    cfg.menuShadow = 0.33f;
    cfg.menuQuitSound = true;
    cfg.flashcolor[0] = .7f;
    cfg.flashcolor[1] = .9f;
    cfg.flashcolor[2] = 1;
    cfg.flashspeed = 4;
    cfg.turningSkull = false;
    cfg.hudShown[HUD_HEALTH] = true;
    cfg.hudShown[HUD_ARMOR] = true;
    cfg.hudShown[HUD_AMMO] = true;
    cfg.hudShown[HUD_KEYS] = true;
    cfg.hudShown[HUD_FRAGS] = true;
    cfg.hudShown[HUD_POWER] = true;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
    cfg.hudScale = .6f;
    cfg.hudColor[0] = 1;
    cfg.hudColor[1] = cfg.hudColor[2] = 0;
    cfg.hudColor[3] = 0.75f;
    cfg.hudIconAlpha = 0.5f;
    cfg.xhairSize = 1;
    for(i = 0; i < 4; ++i)
        cfg.xhairColor[i] = 255;
    cfg.moveCheckZ = true;
    cfg.jumpPower = 9;
    cfg.airborneMovement = 1;
    cfg.weaponAutoSwitch = 1; // IF BETTER
    cfg.noWeaponAutoSwitchIfFiring = false;
    cfg.ammoAutoSwitch = 0; // never
    cfg.secretMsg = true;
    cfg.netJumping = true;
    cfg.netEpisode = 1;
    cfg.netMap = 1;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4;
    cfg.netBFGFreeLook = 0;    // allow free-aim 0=none 1=not BFG 2=All
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = 54;
    cfg.levelTitle = true;
    cfg.hideAuthorIdSoft = true;
    cfg.menuColor[0] = 1;
    cfg.menuColor2[0] = 1;
    cfg.menuSlam = false;
    cfg.askQuickSaveLoad = true;

    cfg.maxskulls = true;
    cfg.allowskullsinwalls = false;
    cfg.anybossdeath = false;
    cfg.monstersStuckInDoors = false;
    cfg.avoidDropoffs = true;
    cfg.moveBlock = false;
    cfg.fallOff = true;

/*    cfg.automapPos = 5;
    cfg.automapWidth = 1.0f;
    cfg.automapHeight = 1.0f;*/

    cfg.automapL0[0] = 0.4f;    // Unseen areas
    cfg.automapL0[1] = 0.4f;
    cfg.automapL0[2] = 0.4f;

    cfg.automapL1[0] = 1.0f;    // onesided lines
    cfg.automapL1[1] = 0.0f;
    cfg.automapL1[2] = 0.0f;

    cfg.automapL2[0] = 0.77f;   // floor height change lines
    cfg.automapL2[1] = 0.6f;
    cfg.automapL2[2] = 0.325f;

    cfg.automapL3[0] = 1.0f;    // ceiling change lines
    cfg.automapL3[1] = 0.95f;
    cfg.automapL3[2] = 0.0f;

    cfg.automapBack[0] = 0.0f;
    cfg.automapBack[1] = 0.0f;
    cfg.automapBack[2] = 0.0f;
    cfg.automapBack[3] = 0.7f;
    cfg.automapLineAlpha = .7f;
    cfg.automapShowDoors = true;
    cfg.automapDoorGlow = 8;
    cfg.automapHudDisplay = 2;
    cfg.automapRotate = true;
    cfg.automapBabyKeys = false;
    cfg.automapZoomSpeed = .1f;
    cfg.automapPanSpeed = .5f;
    cfg.automapPanResetOnOpen = true;
    cfg.counterCheatScale = .7f; //From jHeretic

    cfg.msgShow = true;
    cfg.msgCount = 1;
    cfg.msgScale = .8f;
    cfg.msgUptime = 5 * TICSPERSEC;
    cfg.msgAlign = ALIGN_LEFT;
    cfg.msgBlink = 5;

    cfg.msgColor[0] = cfg.msgColor[1] = cfg.msgColor[2] = 1;

    cfg.chatBeep = 1;

    cfg.killMessages = true;
    cfg.bobWeapon = 1;
    cfg.bobView = 1;
    cfg.bobWeaponLower = true;
    cfg.cameraNoClip = true;
    cfg.respawnMonstersNightmare = true;

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

    // Do the common pre init routine;
    G_PreInit();
}

/*
 *  Post Engine Initialization routine.
 *    All game-specific actions that should take place at this time go here.
 */
void D_PostInit(void)
{
    int     p;
    char    file[256];
    char    mapstr[6];

    // Common post init routine
    G_PostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                "DOOM64TC: Absolution");
    Con_FPrintf(CBLF_RULER, "");

    // Game parameters.
    monsterinfight = GetDefInt("AI|Infight", 0);

    // get skill / episode / map from parms
    gameskill = startskill = SM_NOITEMS;
    startepisode = 1;
    startmap = 1;
    autostart = false;

    // Game mode specific settings
    // Plutonia and TNT automatically turn on the full sky.
    if(gamemode == commercial &&
       (gamemission == GM_PLUT || gamemission == GM_TNT))
    {
        Con_SetInteger("rend-sky-full", 1, true);
    }

    // Command line options
    nomonsters = ArgCheck("-nomonsters");
    respawnparm = ArgCheck("-respawn");
    fastparm = ArgCheck("-fast");
    devparm = ArgCheck("-devparm");

    if(ArgCheck("-altdeath"))
        cfg.netDeathmatch = 2;
    else if(ArgCheck("-deathmatch"))
        cfg.netDeathmatch = 1;

    p = ArgCheck("-skill");
    if(p && p < myargc - 1)
    {
        startskill = Argv(p + 1)[0] - '1';
        autostart = true;
    }

    p = ArgCheck("-episode");
    if(p && p < myargc - 1)
    {
        startepisode = Argv(p + 1)[0] - '0';
        startmap = 1;
        autostart = true;
    }

    p = ArgCheck("-timer");
    if(p && p < myargc - 1 && deathmatch)
    {
        int     time;

        time = atoi(Argv(p + 1));
        Con_Message("Levels will end after %d minute", time);
        if(time > 1)
            Con_Message("s");
        Con_Message(".\n");
    }

    p = ArgCheck("-warp");
    if(p && p < myargc - 1)
    {
        if(gamemode == commercial)
        {
            startmap = atoi(Argv(p + 1));
            autostart = true;
        }
        else if(p < myargc - 2)
        {
            startepisode = Argv(p + 1)[0] - '0';
            startmap = Argv(p + 2)[0] - '0';
            autostart = true;
        }
    }

    // turbo option
    p = ArgCheck("-turbo");
    turbomul = 1.0f;
    if(p)
    {
        int     scale = 200;

        turboparm = true;
        if(p < myargc - 1)
            scale = atoi(Argv(p + 1));
        if(scale < 10)
            scale = 10;
        if(scale > 400)
            scale = 400;

        Con_Message("turbo scale: %i%%\n", scale);
        turbomul = scale / 100.f;
    }

    // Are we autostarting?
    if(autostart)
    {
        Con_Message("Warp to Episode %d, Map %d, Skill %d\n", startepisode,
                    startmap, startskill + 1);
    }

    // Load a saved game?
    p = ArgCheck("-loadgame");
    if(p && p < myargc - 1)
    {
        SV_GetSaveGameFileName(Argv(p + 1)[0] - '0', file);
        G_LoadGame(file);
    }

    // Check valid episode and map
    if((autostart || IS_NETGAME))
    {
        sprintf(mapstr,"E%d%d",startepisode, startmap);

        if(!W_CheckNumForName(mapstr))
        {
            startepisode = 1;
            startmap = 1;
        }
    }

    // Print a string showing the state of the game parameters
    Con_Message("Game state parameters:%s%s%s%s%s\n",
                 nomonsters? " nomonsters" : "",
                 respawnparm? " respawn" : "",
                 fastparm? " fast" : "",
                 turboparm? " turbo" : "",
                 (cfg.netDeathmatch ==1)? " deathmatch" :
                    (cfg.netDeathmatch ==2)? " altdeath" : "");

    if(G_GetGameAction() != GA_LOADGAME)
    {
        if(autostart || IS_NETGAME)
        {
            G_DeferedInitNew(startskill, startepisode, startmap);
        }
        else
        {
            G_StartTitle();     // start up intro loop
        }
    }
}

void D_Shutdown(void)
{
    uint        i;

    HU_UnloadData();

    for(i = 0; i < MAXPLAYERS; ++i)
        HUMsg_ClearMessages(&players[i]);

    P_DestroyIterList(spechit);
    P_DestroyIterList(linespecials);
    P_DestroyLineTagLists();
    P_DestroySectorTagLists();
    P_FreeButtons();
    AM_Shutdown();
}

void D_Ticker(timespan_t ticLength)
{
    Hu_MenuTicker(ticLength);
    G_Ticker(ticLength);
}

void D_EndFrame(void)
{
}
