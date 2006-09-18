/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *
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

/*
 * d_console.c: jDoom specific console stuff
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "hu_stuff.h"
#include "f_infine.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatNoClip);
DEFCC(CCmdCheatWarp);
DEFCC(CCmdCheatReveal);
DEFCC(CCmdCheatGive);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdCheatWhere);
DEFCC(CCmdCheatExitLevel);
DEFCC(CCmdCheatSuicide);

DEFCC(CCmdMakeLocal);
DEFCC(CCmdSetCamera);
DEFCC(CCmdSetViewLock);
DEFCC(CCmdSetViewMode);

DEFCC(CCmdCycleSpy);

DEFCC(CCmdPlayDemo);
DEFCC(CCmdRecordDemo);
DEFCC(CCmdStopDemo);

DEFCC(CCmdSpawnMobj);

DEFCC(CCmdPrintPlayerCoords);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdViewSize);
DEFCC(CCmdPause);
DEFCC(CCmdDoomFont);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     consoleFlat = 0;
float   consoleZoom = 1;

// Console variables.
cvar_t  gameCVars[] = {
// Console
    {"con-flat", CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0},
    {"con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f},

// View/Refresh
    {"view-size", CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13},
    {"hud-title", 0, CVT_BYTE, &cfg.levelTitle, 0, 1},
    {"hud-title-noidsoft", 0, CVT_BYTE, &cfg.hideAuthorIdSoft, 0, 1},

    {"view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1},
    {"view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1},
    {"view-bob-weapon-switch-lower", 0, CVT_BYTE, &cfg.bobWeaponLower, 0, 1},

// Server-side options
    // Game state
    {"server-game-skill", 0, CVT_BYTE, &cfg.netSkill, 0, 4},
    {"server-game-map", 0, CVT_BYTE, &cfg.netMap, 1, 31},
    {"server-game-episode", 0, CVT_BYTE, &cfg.netEpisode, 1, 6},
    {"server-game-deathmatch", 0, CVT_BYTE, &cfg.netDeathmatch, 0, 2},

    // Modifiers
    {"server-game-mod-damage", 0, CVT_BYTE,
        &cfg.netMobDamageModifier, 1, 100},
    {"server-game-mod-health", 0, CVT_BYTE,
        &cfg.netMobHealthModifier, 1, 20},

    {"server-game-mod-gravity", 0, CVT_INT, &cfg.netGravity, -1, 100},

    // Items
    {"server-game-nobfg", 0, CVT_BYTE, &cfg.noNetBFG, 0, 1},
    {"server-game-coop-nothing", 0, CVT_BYTE, &cfg.noCoopAnything, 0, 1},
    {"server-game-coop-respawn-items", 0, CVT_BYTE,
        &cfg.coopRespawnItems, 0, 1},
    {"server-game-coop-noweapons", 0, CVT_BYTE, &cfg.noCoopWeapons, 0, 1},

    // Gameplay options
    {"server-game-jump", 0, CVT_BYTE, &cfg.netJumping, 0, 1},
    {"server-game-bfg-freeaim", 0, CVT_BYTE, &cfg.netBFGFreeLook, 0, 1},
    {"server-game-nomonsters", 0, CVT_BYTE, &cfg.netNomonsters, 0, 1 },
    {"server-game-respawn", 0, CVT_BYTE, &cfg.netRespawn, 0, 1},
    {"server-game-respawn-monsters-nightmare", 0, CVT_BYTE,
        &cfg.respawnMonstersNightmare, 0, 1},
    {"server-game-radiusattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZRadiusAttack, 0, 1},
    {"server-game-monster-meleeattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZMonsterMeleeAttack, 0, 1},

    {"server-game-coop-nodamage", 0, CVT_BYTE, &cfg.noCoopDamage, 0, 1},
    {"server-game-noteamdamage", 0, CVT_BYTE, &cfg.noTeamDamage, 0, 1},

    // Misc
    {"server-game-deathmatch-killmsg", 0, CVT_BYTE, &cfg.killMessages, 0, 1},

// Player
    // Player data
    {"player-color", 0, CVT_BYTE, &cfg.netColor, 0, 3},
    {"player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54},

    // Movment
    {"player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1},
    {"player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1},
    {"player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100},
    {"player-air-movement", 0, CVT_BYTE, &cfg.airborneMovement, 0, 32},

    // Weapon switch preferences
    {"player-autoswitch", 0, CVT_BYTE, &cfg.weaponAutoSwitch, 0, 2},
    {"player-autoswitch-ammo", 0, CVT_BYTE, &cfg.ammoAutoSwitch, 0, 2},
    {"player-autoswitch-berserk", 0, CVT_BYTE, &cfg.berserkAutoSwitch, 0, 1},

    // Weapon Order preferences
    {"player-weapon-order0", 0, CVT_INT, &cfg.weaponOrder[0], 0, NUMWEAPONS},
    {"player-weapon-order1", 0, CVT_INT, &cfg.weaponOrder[1], 0, NUMWEAPONS},
    {"player-weapon-order2", 0, CVT_INT, &cfg.weaponOrder[2], 0, NUMWEAPONS},
    {"player-weapon-order3", 0, CVT_INT, &cfg.weaponOrder[3], 0, NUMWEAPONS},
    {"player-weapon-order4", 0, CVT_INT, &cfg.weaponOrder[4], 0, NUMWEAPONS},
    {"player-weapon-order5", 0, CVT_INT, &cfg.weaponOrder[5], 0, NUMWEAPONS},
    {"player-weapon-order6", 0, CVT_INT, &cfg.weaponOrder[6], 0, NUMWEAPONS},
    {"player-weapon-order7", 0, CVT_INT, &cfg.weaponOrder[7], 0, NUMWEAPONS},
    {"player-weapon-order8", 0, CVT_INT, &cfg.weaponOrder[8], 0, NUMWEAPONS},

    {"player-weapon-nextmode", 0, CVT_BYTE, &cfg.weaponNextMode, 0, 1},

    // Misc
    {"player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1},
    {"player-death-lookup", 0, CVT_BYTE, &cfg.deathLookUp, 0, 1},

// Compatibility options
    {"game-raiseghosts", 0, CVT_BYTE, &cfg.raiseghosts, 0, 1},
    {"game-maxskulls", 0, CVT_BYTE, &cfg.maxskulls, 0, 1},
    {"game-skullsinwalls", 0, CVT_BYTE, &cfg.allowskullsinwalls, 0, 1},
    {"game-anybossdeath666", 0, CVT_BYTE, &cfg.anybossdeath, 0, 1},
    {"game-monsters-stuckindoors", 0, CVT_BYTE, &cfg.monstersStuckInDoors, 0, 1},
    {"game-objects-hangoverledges", 0, CVT_BYTE, &cfg.avoidDropoffs, 0, 1},
    {"game-objects-clipping", 0, CVT_BYTE, &cfg.moveBlock, 0, 1},
    {"game-zombiescanexit", 0, CVT_BYTE, &cfg.zombiesCanExit, 0, 1},
    {"game-player-wallrun-northonly", 0, CVT_BYTE, &cfg.wallRunNorthOnly, 0, 1},
    {"game-objects-falloff", 0, CVT_BYTE, &cfg.fallOff, 0, 1},
    {"game-zclip", 0, CVT_BYTE, &cfg.moveCheckZ, 0, 1},
    {"game-corpse-sliding", 0, CVT_BYTE, &cfg.slidingCorpses, 0, 1},

// Game state
    {"game-fastmonsters", 0, CVT_BYTE, &fastparm, 0, 1},

// Gameplay
    {"game-corpse-time", CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0},

// Misc
    {NULL}
};

//  Console commands
ccmd_t  gameCCmds[] = {
    {"spy",         CCmdCycleSpy},
    {"screenshot",  CCmdScreenShot},
    {"viewsize",    CCmdViewSize},
    {"pause",       CCmdPause},

    // $cheats
    {"cheat",       CCmdCheat},
    {"god",         CCmdCheatGod},
    {"noclip",      CCmdCheatNoClip},
    {"warp",        CCmdCheatWarp},
    {"reveal",      CCmdCheatReveal},
    {"give",        CCmdCheatGive},
    {"kill",        CCmdCheatMassacre},
    {"exitlevel",   CCmdCheatExitLevel},
    {"suicide",     CCmdCheatSuicide},
    {"where",       CCmdCheatWhere},

    {"doomfont",    CCmdDoomFont},

    // $infine
    {"startinf",    CCmdStartInFine},
    {"stopinf",     CCmdStopInFine},
    {"stopfinale",  CCmdStopInFine},

    {"spawnmobj",   CCmdSpawnMobj},
    {"coord",       CCmdPrintPlayerCoords},

    // $democam
    {"makelocp",    CCmdMakeLocal},
    {"makecam",     CCmdSetCamera},
    {"setlock",     CCmdSetViewLock},
    {"lockmode",    CCmdSetViewLock},
    {"viewmode",    CCmdSetViewMode},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Add the console variables and commands.
 */
void G_ConsoleRegistration()
{
    int     i;

    for(i = 0; gameCVars[i].name; i++)
        Con_AddVariable(gameCVars + i);
    for(i = 0; gameCCmds[i].name; i++)
        Con_AddCommand(gameCCmds + i);
}

/*
 * Settings for console background drawing.
 * Called EVERY FRAME by the console drawer.
 */
void D_ConsoleBg(int *width, int *height)
{
    extern int consoleFlat;
    extern float consoleZoom;

    if(consoleFlat)
    {
        GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
        *width = (int) (64 * consoleZoom);
        *height = (int) (64 * consoleZoom);
    }
    else
    {
        GL_SetNoTexture();
        *width = *height = 0;
    }
}

/*
 * Draw (char *) text in the game's font.
 * Called by the console drawer.
 */
int ConTextOut(char *text, int x, int y)
{
    extern int typein_time;
    int     old = typein_time;

    typein_time = 0xffffff;

    M_WriteText2(x, y, text, hu_font_a, -1, -1, -1, -1);
    typein_time = old;
    return 0;
}

/*
 * Get the visual width of (char*) text in the game's font.
 */
int ConTextWidth(char *text)
{
    return M_StringWidth(text, hu_font_a);
}

/*
 * Custom filter when drawing text in the game's font.
 */
void ConTextFilter(char *text)
{
    strupr(text);
}

/*
 * Console command to take a screenshot (duh).
 */
DEFCC(CCmdScreenShot)
{
    G_ScreenShot();
    return true;
}

/*
 * Console command to change the size of the view window.
 */
DEFCC(CCmdViewSize)
{
    int     min = 3, max = 13, *val = &cfg.screenblocks;

    if(argc != 2)
    {
        Con_Printf("Usage: %s (size)\n", argv[0]);
        Con_Printf("Size can be: +, -, (num).\n");
        return true;
    }

    // Adjust/set the value
    if(!stricmp(argv[1], "+"))
        (*val)++;
    else if(!stricmp(argv[1], "-"))
        (*val)--;
    else
        *val = strtol(argv[1], NULL, 0);

    // Clamp it
    if(*val < min)
        *val = min;
    if(*val > max)
        *val = max;

    // Update the view size if necessary.
    R_SetViewSize(cfg.screenblocks, 0);
    return true;
}

/*
 * Console command to pause the game (when not in the menu).
 */
DEFCC(CCmdPause)
{
    extern boolean sendpause;

    if(!menuactive)
        sendpause = true;

    return true;
}

/*
 * Configure the console to use the game's font.
 */
DEFCC(CCmdDoomFont)
{
    ddfont_t cfont;

    cfont.flags = DDFONT_WHITE;
    cfont.height = 8;
    cfont.sizeX = 1.5f;
    cfont.sizeY = 2;
    cfont.TextOut = ConTextOut;
    cfont.Width = ConTextWidth;
    cfont.Filter = ConTextFilter;

    Con_SetFont(&cfont);
    return true;
}
