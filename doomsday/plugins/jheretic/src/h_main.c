/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * h_main.c: Game initialization - jHeretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "jheretic.h"

#include "m_argv.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_log.h"
#include "p_saveg.h"
#include "d_net.h"
#include "p_mapspec.h"
#include "p_switch.h"
#include "am_map.h"

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
boolean turboParm; // checkparm of -turbo
boolean fastParm; // checkparm of -fast
boolean artiSkipParm; // whether shift-enter skips an artifact

float turboMul; // multiplier for turbo

skillmode_t startSkill;
int startEpisode;
int startMap;
boolean autoStart;

gamemode_t gameMode;
int gameModeBits;

// This is returned in D_Get(DD_GAME_MODE), max 16 chars.
char gameModeString[17];

boolean monsterInfight;

// Default font colours.
const float defFontRGB[] = { .425f, 0.986f, 0.378f};
const float defFontRGB2[] = { 1.0f, 1.0f, 1.0f};

char *borderLumps[] = {
    "FLAT513", // background
    "bordt", // top
    "bordr", // right
    "bordb", // bottom
    "bordl", // left
    "bordtl", // top left
    "bordtr", // top right
    "bordbr", // bottom right
    "bordbl" // bottom left
};

char *baseDefault = "heretic.cfg";

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Attempt to change the current game mode. Can only be done when not
 * actually in a level.
 *
 * \todo Doesn't actually do anything yet other than set the game mode
 * global vars.
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
    case shareware: // shareware, E1, M9
        gameModeBits = GM_SHAREWARE;
        break;

    case registered: // registered episodes
        gameModeBits = GM_REGISTERED;
        break;

    case extended: // episodes 4 and 5 present
        gameModeBits = GM_EXTENDED;
        break;

    case indetermined: // Well, no IWAD found.
        gameModeBits = GM_INDETERMINED;
        break;

    default:
        Con_Error("G_SetGameMode: Unknown gamemode %i", mode);
    }

    return true;
}

static void addFile(char *file)
{
    int             numWADFiles;
    char           *new;

    for(numWADFiles = 0; wadFiles[numWADFiles]; numWADFiles++);

    new = malloc(strlen(file) + 1);
    strcpy(new, file);
    if(strlen(exrnWADs) + strlen(file) < 78)
    {
        if(strlen(exrnWADs))
        {
            strcat(exrnWADs, ", ");
        }
        else
        {
            strcpy(exrnWADs, "External Wadfiles: ");
        }
        strcat(exrnWADs, file);
    }
    else if(strlen(exrnWADs2) + strlen(file) < 79)
    {
        if(strlen(exrnWADs2))
        {
            strcat(exrnWADs2, ", ");
        }
        else
        {
            strcpy(exrnWADs2, "     ");
            strcat(exrnWADs, ",");
        }
        strcat(exrnWADs2, file);
    }

    wadFiles[numWADFiles] = new;
}

/**
 * Check which known IWADs are found. The purpose of this routine is to
 * find out which IWADs the user lets us to know about, but we don't
 * decide which one gets loaded or even see if the WADs are actually
 * there. The default location for IWADs is Data\GAMENAMETEXT\.
 */
void G_DetectIWADs(void)
{
    // Add a couple of probable locations for Heretic.wad.
    DD_AddIWAD("}data\\"GAMENAMETEXT"\\heretic.wad");
    DD_AddIWAD("}data\\heretic.wad");
    DD_AddIWAD("}heretic.wad");
    DD_AddIWAD("heretic.wad");
}

/**
 * gamemode, gamemission and the gameModeString are set.
 */
void G_IdentifyVersion(void)
{
    // The game mode string is used in netgames.
    strcpy(gameModeString, "heretic");

    if(W_CheckNumForName("E2M1") == -1)
    {
        // Can't find episode 2 maps, must be the shareware WAD
        strcpy(gameModeString, "heretic-share");
    }
    else if(W_CheckNumForName("EXTENDED") != -1)
    {
        // Found extended lump, must be the extended WAD
        strcpy(gameModeString, "heretic-ext");
    }
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

    pf->statusbar.scale = 20;         // Full size.
    pf->statusbar.opacity = 1;
    pf->statusbar.counterAlpha = 1;

    pf->screen.blocks = pf->screen.setBlocks = 10;
    pf->screen.ringFilter = 1;

    pf->hud.shown[HUD_AMMO] = true;
    pf->hud.shown[HUD_ARMOR] = true;
    pf->hud.shown[HUD_KEYS] = true;
    pf->hud.shown[HUD_HEALTH] = true;
    pf->hud.shown[HUD_ARTI] = true;
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
        pf->hud.unHide[i] = 1;
    pf->hud.scale = .7f;
    pf->hud.color[CR] = .325f;
    pf->hud.color[CG] = .686f;
    pf->hud.color[CB] = .278f;
    pf->hud.color[CA] = 1;
    pf->hud.iconAlpha = 1;
    pf->hud.counterCheatScale = .7f;
    pf->hud.tomeCounter = 10;
    pf->hud.tomeSound = 3;

    pf->xhair.size = .5f;
    pf->xhair.vitality = false;
    pf->xhair.color[CR] = 1;
    pf->xhair.color[CG] = 1;
    pf->xhair.color[CB] = 1;
    pf->xhair.color[CA] = 1;

    pf->inventory.nextOnNoUse = true;
    pf->inventory.weaponAutoSwitch = 1; // IF BETTER
    pf->inventory.noWeaponAutoSwitchIfFiring = false;
    pf->inventory.ammoAutoSwitch = 0; // never
    pf->inventory.timer = 5;

    pf->camera.offsetZ = 41;
    pf->camera.bob = 1;
    pf->camera.povLookAround = true;

    pf->automap.customColors = 0; // Never.
    pf->automap.line0[CR] = .455f; // Unseen areas
    pf->automap.line0[CG] = .482f;
    pf->automap.line0[CB] = .439f;
    pf->automap.line1[CR] = .294f; // onesided lines
    pf->automap.line1[CG] = .196f;
    pf->automap.line1[CB] = .063f;
    pf->automap.line2[CR] = .184f; // floor height change lines
    pf->automap.line2[CG] = .094f;
    pf->automap.line2[CB] = .002f;
    pf->automap.line3[CR] = .592f; // ceiling change lines
    pf->automap.line3[CG] = .388f;
    pf->automap.line3[CB] = .231f;
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
    pf->automap.babyKeys = true;
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

    pf->psprite.bob = 1;
    pf->psprite.bobLower = true;

    pf->inventory.weaponOrder[0] = WT_SEVENTH;    // mace \ beak
    pf->inventory.weaponOrder[1] = WT_SIXTH;      // phoenixrod \ beak
    pf->inventory.weaponOrder[2] = WT_FIFTH;      // skullrod \ beak
    pf->inventory.weaponOrder[3] = WT_FOURTH;     // blaster \ beak
    pf->inventory.weaponOrder[4] = WT_THIRD;      // crossbow \ beak
    pf->inventory.weaponOrder[5] = WT_SECOND;     // goldwand \ beak
    pf->inventory.weaponOrder[6] = WT_EIGHTH;     // gauntlets \ beak
    pf->inventory.weaponOrder[7] = WT_FIRST;      // staff \ beak
}

void G_InitGameRules(gamerules_t* gr)
{
    if(!gr)
        return;

    memset(gr, 0, sizeof(gamerules_t));

    gr->moveCheckZ = true;
    gr->jumpPower = 9;
    gr->slidingCorpses = false;
    gr->fastMonsters = false;
    gr->announceSecrets = true;
    gr->jumpAllow = true;
    gr->mobDamageModifier = 1;
    gr->mobHealthModifier = 1;
    gr->gravityModifier = -1;        // use map default
    gr->monstersStuckInDoors = false;
    gr->avoidDropoffs = true;
    gr->moveBlock = false;
    gr->fallOff = true;
    gr->cameraNoClip = true;
    gr->respawnMonstersNightmare = false;
}

/**
 * Pre Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PreInit(void)
{
    G_SetGameMode(indetermined);

    // Shareware WAD has different border background
    if(W_CheckNumForName("E2M1") == -1)
        borderLumps[0] = "FLOOR04";

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
    gs.cfg.menuGlitter = 0;
    gs.cfg.menuShadow = 0;
  //gs.cfg.menuQuitSound = true;
    gs.cfg.flashColor[0] = .7f;
    gs.cfg.flashColor[1] = .9f;
    gs.cfg.flashColor[2] = 1;
    gs.cfg.flashSpeed = 4;
    gs.cfg.turningSkull = false;
    gs.cfg.mapTitle = true;
  //gs.cfg.hideAuthorIdSoft = true;
    gs.cfg.menuColor[0] = defFontRGB[0];   // use the default colour by default.
    gs.cfg.menuColor[1] = defFontRGB[1];
    gs.cfg.menuColor[2] = defFontRGB[2];
    gs.cfg.menuColor2[0] = defFontRGB2[0]; // use the default colour by default.
    gs.cfg.menuColor2[1] = defFontRGB2[1];
    gs.cfg.menuColor2[2] = defFontRGB2[2];
    gs.cfg.menuSlam = true;
    gs.cfg.menuHotkeys = true;
    gs.cfg.askQuickSaveLoad = true;
    gs.cfg.menuEffects = 0;
    gs.cfg.hudFog = 5;

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

    if(W_CheckNumForName("E2M1") == -1)
        // Can't find episode 2 maps, must be the shareware WAD.
        G_SetGameMode(shareware);
    else if(W_CheckNumForName("EXTENDED") != -1)
        // Found extended lump, must be the extended WAD.
        G_SetGameMode(extended);
    else
        G_SetGameMode(registered);

    // Common post init routine.
    G_CommonPostInit();

    // Initialize weapon info using definitions.
    P_InitWeaponInfo();

    // Print a game mode banner with rulers.
    Con_FPrintf(CBLF_RULER | CBLF_WHITE | CBLF_CENTER,
                gameMode == shareware? "Heretic Shareware Startup\n" :
                gameMode == registered? "Heretic Registered Startup\n" :
                gameMode == extended? "Heretic: Shadow of the Serpent Riders Startup\n" :
                "Public Heretic\n");
    Con_FPrintf(CBLF_RULER, "");

    // Game parameters.
    monsterInfight = GetDefInt("AI|Infight", 0);

    // Defaults for skill, episode and map.
    startSkill = SM_MEDIUM;
    startEpisode = 1;
    startMap = 1;
    autoStart = false;

    // Game mode specific settings.
    /* None */

    // Command line options.
    noMonstersParm = ArgCheck("-nomonsters");
    respawnParm = ArgCheck("-respawn");
    devParm = ArgCheck("-devparm");
    artiSkipParm = !(ArgCheck("-noartiskip"));

    if(ArgCheck("-deathmatch"))
    {
        GAMERULES.deathmatch = true;
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
        startEpisode = Argv(p + 1)[0] - '0';
        startMap = 1;
        autoStart = true;
    }

    p = ArgCheck("-warp");
    if(p && p < myargc - 2)
    {
        startEpisode = Argv(p + 1)[0] - '0';
        startMap = Argv(p + 2)[0] - '0';
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

    // Check valid episode and map
    if(autoStart || IS_NETGAME)
    {
        sprintf(mapStr, "E%d%d", startEpisode, startMap);
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
            G_InitNew(startSkill, startEpisode, startMap);
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
