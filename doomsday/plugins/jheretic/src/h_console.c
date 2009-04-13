/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * h_console.c: Console stuff - jHeretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

#include "hu_stuff.h"
#include "f_infine.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatClip);
DEFCC(CCmdCheatWarp);
DEFCC(CCmdCheatReveal);
DEFCC(CCmdCheatGive);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdCheatWhere);
DEFCC(CCmdCheatPig);
DEFCC(CCmdCheatLeaveMap);
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

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdViewSize);
DEFCC(CCmdHereticFont);
DEFCC(CCmdConBackground);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

material_t* consoleBG = NULL;
float consoleZoom = 1;

// Console variables.
cvar_t gameCVars[] = {
// Console
    {"con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f},

// View/Refresh
    {"view-size", CVF_PROTECTED, CVT_INT, &PLRPROFILE.screen.blocks, 3, 13},
    {"hud-title", 0, CVT_BYTE, &gs.cfg.mapTitle, 0, 1},

    {"view-bob-height", 0, CVT_FLOAT, &PLRPROFILE.camera.bob, 0, 1},
    {"view-bob-weapon", 0, CVT_FLOAT, &PLRPROFILE.psprite.bob, 0, 1},
    {"view-bob-weapon-switch-lower", 0, CVT_BYTE, &PLRPROFILE.psprite.bobLower, 0, 1},

    {"view-ringfilter", 0, CVT_INT, &PLRPROFILE.screen.ringFilter, 1, 2},

// Server-side options
    // Game state
    {"server-game-deathmatch", 0, CVT_BYTE, &GAMERULES.deathmatch, 0, 1}, /* jHeretic only has one deathmatch mode */

    // Modifiers
    {"server-game-mod-damage", 0, CVT_BYTE, &GAMERULES.mobDamageModifier, 1, 100},
    {"server-game-mod-health", 0, CVT_BYTE, &GAMERULES.mobHealthModifier, 1, 20},
    {"server-game-mod-gravity", 0, CVT_INT, &GAMERULES.gravityModifier, -1, 100},

    // Gameplay options
    {"server-game-jump", 0, CVT_BYTE, &GAMERULES.jumpAllow, 0, 1},
    {"server-game-nomonsters", 0, CVT_BYTE, &GAMERULES.noMonsters, 0, 1},
    {"server-game-respawn", 0, CVT_BYTE, &GAMERULES.respawn, 0, 1},
    {"server-game-radiusattack-nomaxz", 0, CVT_BYTE, &GAMERULES.noMaxZRadiusAttack, 0, 1},
    {"server-game-monster-meleeattack-nomaxz", 0, CVT_BYTE, &GAMERULES.noMaxZMonsterMeleeAttack, 0, 1},
    {"server-game-coop-nodamage", 0, CVT_BYTE, &GAMERULES.noCoopDamage, 0, 1},
    {"server-game-noteamdamage", 0, CVT_BYTE, &GAMERULES.noTeamDamage, 0, 1},

// Player
    // Player data
    {"player-color", 0, CVT_BYTE, &PLRPROFILE.color, 0, 4},
    {"player-eyeheight", 0, CVT_INT, &PLRPROFILE.camera.offsetZ, 41, 54},

    // Movment
    {"player-move-speed", 0, CVT_FLOAT, &PLRPROFILE.ctrl.moveSpeed, 0, 1},
    {"player-jump-power", 0, CVT_FLOAT, &GAMERULES.jumpPower, 0, 100},
    {"player-air-movement", 0, CVT_BYTE, &PLRPROFILE.ctrl.airborneMovement, 0, 32},

    // Weapon switch preferences
    {"player-autoswitch", 0, CVT_BYTE, &PLRPROFILE.inventory.weaponAutoSwitch, 0, 2},
    {"player-autoswitch-ammo", 0, CVT_BYTE, &PLRPROFILE.inventory.ammoAutoSwitch, 0, 2},
    {"player-autoswitch-notfiring", 0, CVT_BYTE, &PLRPROFILE.inventory.noWeaponAutoSwitchIfFiring, 0, 1},

    // Weapon Order preferences
    {"player-weapon-order0", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[0], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order1", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[1], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order2", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[2], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order3", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[3], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order4", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[4], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order5", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[5], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order6", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[6], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order7", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[7], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order8", 0, CVT_INT, &PLRPROFILE.inventory.weaponOrder[8], 0, NUM_WEAPON_TYPES},

    {"player-weapon-nextmode", 0, CVT_BYTE, &PLRPROFILE.inventory.weaponNextMode, 0, 1},

    // Misc
    {"player-camera-noclip", 0, CVT_BYTE, &GAMERULES.cameraNoClip, 0, 1},

// Compatibility options
    {"game-monsters-stuckindoors", 0, CVT_BYTE, &GAMERULES.monstersStuckInDoors, 0, 1},
    {"game-objects-neverhangoverledges", 0, CVT_BYTE, &GAMERULES.avoidDropoffs, 0, 1},
    {"game-objects-clipping", 0, CVT_BYTE, &GAMERULES.moveBlock, 0, 1},
    {"game-player-wallrun-northonly", 0, CVT_BYTE, &GAMERULES.wallRunNorthOnly, 0, 1},
    {"game-objects-falloff", 0, CVT_BYTE, &GAMERULES.fallOff, 0, 1},
    {"game-zclip", 0, CVT_BYTE, &GAMERULES.moveCheckZ, 0, 1},
    {"game-corpse-sliding", 0, CVT_BYTE, &GAMERULES.slidingCorpses, 0, 1},

// Game state
    {"game-fastmonsters", 0, CVT_BYTE, &GAMERULES.fastMonsters, 0, 1},

// Misc
    {"game-corpse-time", CVF_NO_MAX, CVT_INT, &PLRPROFILE.corpseTime, 0, 0},
    {NULL}
};

//  Console commands
ccmd_t gameCCmds[] = {
    {"spy",        "",      CCmdCycleSpy},
    {"screenshot", "",      CCmdScreenShot},
    {"viewsize",   "s",     CCmdViewSize},

    // $cheats
    {"cheat",       "s",    CCmdCheat},
    {"god",         "",     CCmdCheatGod},
    {"noclip",      "",     CCmdCheatClip},
    {"warp",        NULL,   CCmdCheatWarp},
    {"reveal",      "i",    CCmdCheatReveal},
    {"give",        NULL,   CCmdCheatGive},
    {"kill",        "",     CCmdCheatMassacre},
    {"leavemap",    "",     CCmdCheatLeaveMap},
    {"suicide",     "",     CCmdCheatSuicide},
    {"where",       "",     CCmdCheatWhere},

    {"hereticfont", "",     CCmdHereticFont},
    {"conbg",       "s",    CCmdConBackground},

    // $infine
    {"startinf",    "s",    CCmdStartInFine},
    {"stopinf",     "",     CCmdStopInFine},
    {"stopfinale",  "",     CCmdStopInFine},

    {"spawnmobj",   NULL,   CCmdSpawnMobj},
    {"coord",       "",     CCmdPrintPlayerCoords},

    // $democam
    {"makelocp",    "i",    CCmdMakeLocal},
    {"makecam",     "i",    CCmdSetCamera},
    {"setlock",     NULL,   CCmdSetViewLock},
    {"lockmode",    "i",    CCmdSetViewLock},
    {"viewmode",    NULL,   CCmdSetViewMode},

    // Heretic specific
    {"invleft",     NULL,   CCmdInventory},
    {"invright",    NULL,   CCmdInventory},
    {"chicken",     "",     CCmdCheatPig},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Add the console variables and commands.
 */
void G_ConsoleRegistration(void)
{
    int                 i;

    for(i = 0; gameCVars[i].name; ++i)
        Con_AddVariable(gameCVars + i);
    for(i = 0; gameCCmds[i].name; ++i)
        Con_AddCommand(gameCCmds + i);
}

/**
 * Settings for console background drawing.
 * Called EVERY FRAME by the console drawer.
 */
void H_ConsoleBg(int* width, int* height)
{
    if(consoleBG)
    {
        DGL_SetMaterial(consoleBG);
        *width = (int) (64 * consoleZoom);
        *height = (int) (64 * consoleZoom);
    }
    else
    {
        DGL_SetNoMaterial();
        *width = *height = 0;
    }
}

/**
 * Draw (char *) text in the game's font.
 * Called by the console drawer.
 */
int ConTextOut(const char* text, int x, int y)
{
    int                 old = typeInTime;

    typeInTime = 0xffffff;
    M_WriteText2(x, y, text, huFontA, -1, -1, -1, -1);
    typeInTime = old;
    return 0;
}

/**
 * Get the visual width of (char*) text in the game's font.
 */
int ConTextWidth(const char* text)
{
    return M_StringWidth(text, huFontA);
}

/**
 * Custom filter when drawing text in the game's font.
 */
void ConTextFilter(char* text)
{
    strupr(text);
}

/**
 * Console command to take a screenshot (duh).
 */
DEFCC(CCmdScreenShot)
{
    G_ScreenShot();
    return true;
}

/**
 * Console command to change the size of the view window.
 */
DEFCC(CCmdViewSize)
{
    int                 min = 3, max = 13, *val = &PLRPROFILE.screen.blocks;

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
    R_SetViewSize(CONSOLEPLAYER, PLRPROFILE.screen.blocks);
    return true;
}

/**
 * Configure the console to use the game's font.
 */
DEFCC(CCmdHereticFont)
{
    ddfont_t    cfont;

    cfont.flags = DDFONT_WHITE;
    cfont.height = 9;
    cfont.sizeX = 1.2f;
    cfont.sizeY = 2;
    cfont.drawText = ConTextOut;
    cfont.getWidth = ConTextWidth;
    cfont.filterText = ConTextFilter;
    Con_SetFont(&cfont);
    return true;
}

/**
 * Configure the console background.
 */
DEFCC(CCmdConBackground)
{
    material_t*         mat;

    if(!stricmp(argv[1], "off") || !stricmp(argv[1], "none"))
    {
        consoleBG = NULL;
        return true;
    }

    if((mat = P_ToPtr(DMU_MATERIAL,
            P_MaterialCheckNumForName(argv[1], MN_ANY))))
        consoleBG = mat;

    return true;
}
