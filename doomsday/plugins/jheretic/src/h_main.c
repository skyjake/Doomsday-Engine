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
#include "p_inventory.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define MAXWADFILES         20

// MAPDIR should be defined as the directory that holds development maps
// for the -wart # # command
#define MAPDIR              "\\data\\"

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

char *wadFiles[MAXWADFILES] = {
    "heretic.wad",
    "texture1.lmp",
    "texture2.lmp",
    "pnames.lmp"
};

char *baseDefault = "heretic.cfg";

char exrnWADs[80];
char exrnWADs2[80];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean devMap;

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
    for(i = 0; i < NUMHUDUNHIDEEVENTS; ++i) // when the hud/statusbar unhides.
        cfg.hudUnHide[i] = 1;
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
    cfg.ammoAutoSwitch = 0; // never
    cfg.slidingCorpses = false;
    cfg.fastMonsters = false;
    cfg.secretMsg = true;
    cfg.netJumping = true;
    cfg.netEpisode = 0;
    cfg.netMap = 1;
    cfg.netSkill = SM_MEDIUM;
    cfg.netColor = 4;           // Use the default color by default.
    cfg.netMobDamageModifier = 1;
    cfg.netMobHealthModifier = 1;
    cfg.netGravity = -1;        // use map default
    cfg.plrViewHeight = DEFAULT_PLAYER_VIEWHEIGHT;
    cfg.mapTitle = true;
    cfg.hideIWADAuthor = true;
    cfg.menuColor[0] = defFontRGB[0]; // use the default colour by default.
    cfg.menuColor[1] = defFontRGB[1];
    cfg.menuColor[2] = defFontRGB[2];
    cfg.menuColor2[0] = defFontRGB2[0]; // use the default colour by default.
    cfg.menuColor2[1] = defFontRGB2[1];
    cfg.menuColor2[2] = defFontRGB2[2];
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
    cfg.automapL0[0] = .455f; // Unseen areas
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
    cfg.msgAlign = ALIGN_CENTER;
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

    cfg.chatBeep = 1;

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

    cfg.menuEffects = 0;
    cfg.hudFog = 5;

    cfg.ringFilter = 1;
    cfg.tomeCounter = 10;
    cfg.tomeSound = 3;

    // Shareware WAD has different border background
    if(W_CheckNumForName("E2M1") == -1)
        borderLumps[0] = "FLOOR04";

    // Do the common pre init routine;
    G_CommonPreInit();
}

/**
 * Post Engine Initialization routine.
 * All game-specific actions that should take place at this time go here.
 */
void G_PostInit(void)
{
    int                 e, m, p;
    filename_t          file;
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

    // -DEVMAP <episode> <map>
    // Adds a map wad from the development directory to the wad list,
    // and sets the start episode and the start map.
    devMap = false;
    p = ArgCheck("-devmap");
    if(p && p < myargc - 2)
    {
        e = Argv(p + 1)[0] - 1;
        m = Argv(p + 2)[0] - 1;
        sprintf(file, MAPDIR "E%cM%c.wad", e+1, m+1);
        addFile(file);
        printf("DEVMAP: Episode %c, Map %c.\n", e+1, m+1);
        startEpisode = e;
        startMap = m;
        autoStart = true;
        devMap = true;
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

    // Check valid episode and map
    if(autoStart || IS_NETGAME && !devMap)
    {
        sprintf(mapStr, "E%d%d", startEpisode+1, startMap+1);

        if(!W_CheckNumForName(mapStr))
        {
            startEpisode = 0;
            startMap = 0;
        }
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
    R_ShutdownVectorGraphics();
    P_FreeWeaponSlots();
    GUI_Shutdown();
}

void G_EndFrame(void)
{
    // Nothing to do.
}
