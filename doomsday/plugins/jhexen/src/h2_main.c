/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
#include "d_net.h"
#include "g_update.h"
#include "g_common.h"
#include "p_mapspec.h"
#include "am_map.h"
#include "p_switch.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct execopt_s {
    char           *name;
    void          (*func) (char **args, int tag);
    int             requiredArgs;
    int             tag;
} execopt_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void X_CreateLUTs(void);
extern void X_DestroyLUTs(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void handleArgs();
static void execOptionScripts(char **args, int tag);
static void execOptionSkill(char **args, int tag);
static void execOptionPlayDemo(char **args, int tag);
static void warpCheck(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int verbose;

boolean useScriptsDir = false;
filename_t scriptsDir;

boolean noMonstersParm; // checkparm of -nomonsters
boolean respawnParm; // checkparm of -respawn
boolean turboParm; // checkparm of -turbo
boolean randomClassParm; // checkparm of -randclass
boolean devParm; // checkparm of -devparm
boolean artiSkipParm; // Whether shift-enter skips an artifact.

float turboMul; // Multiplier for turbo.
boolean netCheatParm; // Allow cheating in netgames (-netcheat)

gamemode_t gameMode;
int gameModeBits;

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

// Default font colours.
const float defFontRGB[] = { .9f, 0.0f, 0.0f};
const float defFontRGB2[] = { .9f, .9f, .9f};

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

static boolean autoStart;
static skillmode_t startSkill;
static int startEpisode;
static int startMap;
static int warpMap;

static execopt_t execOptions[] = {
    {"-scripts", execOptionScripts, 1, 0},
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

void G_InitPlayerProfile(playerprofile_t* pf)
{
    int                 i;

    if(!pf)
        return;

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(pf, 0, sizeof(playerprofile_t));
    pf->color = 8;
    pf->pClass = PCLASS_FIGHTER;

    pf->ctrl.moveSpeed = 1;
    pf->ctrl.dclickUse = false;
    pf->ctrl.lookSpeed = 3;
    pf->ctrl.turnSpeed = 1;
    pf->ctrl.airborneMovement = 1;
    pf->ctrl.useAutoAim = true;

    pf->screen.blocks = pf->screen.setBlocks = 10;

    pf->camera.offsetZ = 48;
    pf->camera.bob = 1;
    pf->camera.povLookAround = true;

    pf->psprite.bob = 1;

    pf->xhair.size = .5f;
    pf->xhair.vitality = false;
    pf->xhair.color[CR] = 1;
    pf->xhair.color[CG] = 1;
    pf->xhair.color[CB] = 1;
    pf->xhair.color[CA] = 1;

    pf->inventory.weaponAutoSwitch = 1; // IF BETTER
    pf->inventory.noWeaponAutoSwitchIfFiring = false;
    pf->inventory.ammoAutoSwitch = 0; // never
    pf->inventory.timer = 5;
    pf->inventory.nextOnNoUse = true;
    pf->inventory.weaponOrder[0] = WT_FOURTH;
    pf->inventory.weaponOrder[1] = WT_THIRD;
    pf->inventory.weaponOrder[2] = WT_SECOND;
    pf->inventory.weaponOrder[3] = WT_FIRST;

    pf->hud.scale = .7f;
    pf->hud.color[CR] = defFontRGB[CR];    // use the default colour by default.
    pf->hud.color[CG] = defFontRGB[CG];
    pf->hud.color[CB] = defFontRGB[CB];
    pf->hud.color[CA] = 1;
    pf->hud.iconAlpha = 1;
    pf->hud.shown[HUD_MANA] = true;
    pf->hud.shown[HUD_HEALTH] = true;
    pf->hud.shown[HUD_ARTI] = true;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // When the hud/statusbar unhides.
        pf->hud.unHide[i] = 1;

    pf->statusbar.scale = 20;
    pf->statusbar.opacity = 1;
    pf->statusbar.counterAlpha = 1;

    pf->automap.customColors = 0; // Never.
    pf->automap.line0[CR] = .42f; // Unseen areas
    pf->automap.line0[CG] = .42f;
    pf->automap.line0[CB] = .42f;

    pf->automap.line1[CR] = .41f; // onesided lines
    pf->automap.line1[CG] = .30f;
    pf->automap.line1[CB] = .15f;

    pf->automap.line2[CR] = .82f; // floor height change lines
    pf->automap.line2[CG] = .70f;
    pf->automap.line2[CB] = .52f;

    pf->automap.line3[CR] = .47f; // ceiling change lines
    pf->automap.line3[CG] = .30f;
    pf->automap.line3[CB] = .16f;

    pf->automap.mobj[CR] = 1.f;
    pf->automap.mobj[CG] = 1.f;
    pf->automap.mobj[CB] = 1.f;

    pf->automap.background[CR] = 1.0f;
    pf->automap.background[CG] = 1.0f;
    pf->automap.background[CB] = 1.0f;
    pf->automap.opacity = 1.0f;
    pf->automap.lineAlpha = 1.0f;
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
    pf->msgLog.align = ALIGN_CENTER;
    pf->msgLog.blink = 5;
    pf->msgLog.color[CR] = defFontRGB2[CR];
    pf->msgLog.color[CG] = defFontRGB2[CG];
    pf->msgLog.color[CB] = defFontRGB2[CB];

    pf->chat.playBeep = 1;
}

void G_InitGameRules(gamerules_t* gr)
{
    if(!gr)
        return;

    memset(gr, 0, sizeof(gamerules_t));

    gr->jumpAllow = true; // True by default in Hexen.
    gr->jumpPower = 9;
    gr->fastMonsters = false;
    gr->mobDamageModifier = 1;
    gr->mobHealthModifier = 1;
    gr->gravityModifier = -1; // Use map default.
    gr->cameraNoClip = true;
}

/**
 * Pre Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    // Calculate the various LUTs used by the playsim.
    X_CreateLUTs();

    useScriptsDir = false;
    memset(scriptsDir, 0, sizeof(scriptsDir));

    G_SetGameMode(indetermined);

    memset(gs.players, 0, sizeof(gs.players));
    gs.netMap = 1;
    gs.netSkill = SM_MEDIUM;

    // Config defaults. The real settings are read from the .cfg files
    // but these will be used no such files are found.
    memset(&gs.cfg, 0, sizeof(gs.cfg));
    gs.cfg.mapTitle = true;
    gs.cfg.menuScale = .75f;
    gs.cfg.menuColor[0] = defFontRGB[0];   // use the default colour by default.
    gs.cfg.menuColor[1] = defFontRGB[1];
    gs.cfg.menuColor[2] = defFontRGB[2];
    gs.cfg.menuColor2[0] = defFontRGB2[0]; // use the default colour by default.
    gs.cfg.menuColor2[1] = defFontRGB2[1];
    gs.cfg.menuColor2[2] = defFontRGB2[2];
    gs.cfg.menuEffects = 0;
    gs.cfg.menuHotkeys = true;
    gs.cfg.hudFog = 5;
    gs.cfg.menuSlam = true;
    gs.cfg.flashColor[0] = 1.0f;
    gs.cfg.flashColor[1] = .5f;
    gs.cfg.flashColor[2] = .5f;
    gs.cfg.flashSpeed = 4;
    gs.cfg.turningSkull = false;
    gs.cfg.usePatchReplacement = 2; // Use built-in replacements if available.

    G_InitGameRules(&GAMERULES);
    G_InitPlayerProfile(&PLRPROFILE);

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

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                gameMode == shareware? "*** Hexen 4-map Beta Demo ***\n"
                    : "Hexen\n");
    Con_FPrintf(CBLF_RULER, "");

    // Game parameters.
    /* None */

    // Get skill / episode / map from parms.
    startEpisode = 1;
    startSkill = SM_MEDIUM;
    startMap = 1;

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
    gs.players[CONSOLEPLAYER].pClass = pClass;

    P_InitMapMusicInfo(); // Init music fields in mapinfo.

    Con_Message("Parsing SNDINFO...\n");
    S_ParseSndInfoLump();

    Con_Message("SN_InitSequenceScript: Registering sound sequences.\n");
    SN_InitSequenceScript();

    // Check for command line warping. Follows P_Init() because the
    // MAPINFO.TXT script must be already processed.
    warpCheck();

    // Are we autostarting?
    if(autoStart)
    {
        Con_Message("Warp to Map %d (\"%s\":%d), Skill %d\n", warpMap,
                    P_GetMapName(startMap), startMap, startSkill + 1);
    }

    // Load a saved game?
    if((p = ArgCheckWith("-loadgame", 1)) != 0)
    {
        G_LoadGame(atoi(Argv(p + 1)));
    }

    // Check valid episode and map.
    if((autoStart || IS_NETGAME))
    {
        sprintf(mapStr,"MAP%2.2d", startMap);
        if(!W_CheckNumForName(mapStr))
        {
            startEpisode = 1;
            startMap = 1;
        }
    }

    if(G_GetGameAction() != GA_LOADGAME)
    {
        if(autoStart || IS_NETGAME)
        {
            G_StartNewInit();
            G_InitNew(startSkill, startEpisode, startMap);
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
    artiSkipParm = ArgExists("-artiskip");
    netCheatParm = ArgExists("-netcheat");

    GAMERULES.deathmatch = ArgExists("-deathmatch");

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

static void warpCheck(void)
{
    int                     p, map;

    p = ArgCheck("-warp");
    if(p && p < Argc() - 1)
    {
        warpMap = atoi(Argv(p + 1));
        map = P_TranslateMap(warpMap);
        if(map == -1)
        {   // Couldn't find real map number.
            startMap = 1;
            Con_Message("-WARP: Invalid map number.\n");
        }
        else
        {   // Found a valid startmap.
            startMap = map;
            autoStart = true;
        }
    }
    else
    {
        warpMap = 1;
        startMap = P_TranslateMap(1);
        if(startMap == -1)
        {
            startMap = 1;
        }
    }
}

static void execOptionSkill(char **args, int tag)
{
    startSkill = args[1][0] - '1';
    autoStart = true;
}

static void execOptionPlayDemo(char **args, int tag)
{
    char                    file[256];

    sprintf(file, "%s.lmp", args[1]);
    DD_AddStartupWAD(file);
    Con_Message("Playing demo %s.lmp.\n", args[1]);
}

static void execOptionScripts(char **args, int tag)
{
    useScriptsDir = true;
    snprintf(scriptsDir, FILENAME_T_MAXLEN, "%s", args[1]);
}

void G_Shutdown(void)
{
    uint                    i;

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
    X_DestroyLUTs();
}

void G_EndFrame(void)
{
    // Nothing to do.
}
