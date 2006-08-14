/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * HConsole.c: jHexen specific console stuff
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "d_net.h"
#include "hu_stuff.h"
#include "f_infine.h"
#include "g_common.h"
#include "g_controls.h"
#include "p_inventory.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    S_InitScript();
void    SN_InitSequenceScript(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatClip);
DEFCC(CCmdCheatGive);
DEFCC(CCmdCheatWarp);
DEFCC(CCmdCheatPig);
DEFCC(CCmdCheatMassacre);
DEFCC(CCmdCheatShadowcaster);
DEFCC(CCmdCheatWhere);
DEFCC(CCmdCheatRunScript);
DEFCC(CCmdCheatReveal);
DEFCC(CCmdCheatSuicide);

DEFCC(CCmdMakeLocal);
DEFCC(CCmdSetCamera);
DEFCC(CCmdSetViewLock);

DEFCC(CCmdCycleSpy);

DEFCC(CCmdSetDemoMode);

DEFCC(CCmdSpawnMobj);

DEFCC(CCmdPrintPlayerCoords);

DEFCC(CCmdScriptInfo);
DEFCC(CCmdTest);
DEFCC(CCmdMovePlane);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdViewSize);
DEFCC(CCmdPause);
DEFCC(CCmdHexenFont);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern ccmd_t netCCmds[];

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

    {"view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1},
    {"view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1},

// Server-side options
    // Game state
    {"server-game-skill", 0, CVT_BYTE, &cfg.netSkill, 0, 4},
    {"server-game-map", 0, CVT_BYTE, &cfg.netMap, 1, 99},
    {"server-game-deathmatch", 0, CVT_BYTE,
        &cfg.netDeathmatch, 0, 1}, /* jHexen only has one deathmatch mode */

    // Modifiers
    {"server-game-mod-damage", 0, CVT_BYTE, &cfg.netMobDamageModifier, 1, 100},
    {"server-game-mod-health", 0, CVT_BYTE, &cfg.netMobHealthModifier, 1, 20},
    {"server-game-mod-gravity", 0, CVT_INT, &cfg.netGravity, -1, 100},

    // Gameplay options
    {"server-game-jump", 0, CVT_BYTE, &cfg.netJumping, 0, 1},
    {"server-game-nomonsters", 0, CVT_BYTE, &cfg.netNomonsters, 0, 1},
    {"server-game-randclass", 0, CVT_BYTE, &cfg.netRandomclass, 0, 1},
    {"server-game-radiusattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZRadiusAttack, 0, 1},
    {"server-game-monster-meleeattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZMonsterMeleeAttack, 0, 1},

// Player
    // Player data
    {"player-color", 0, CVT_BYTE, &cfg.netColor, 0, 8},
    {"player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54},
    {"player-class", 0, CVT_BYTE, &cfg.netClass, 0, 2},

    // Movment
    {"player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1},
    {"player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1},
    {"player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100},
    {"player-air-movement", 0, CVT_BYTE, &cfg.airborneMovement, 0, 32},

    // Weapon switch preferences
    {"player-autoswitch", 0, CVT_BYTE, &cfg.weaponAutoSwitch, 0, 2},

    // Weapon Order preferences
    {"player-weapon-order0", 0, CVT_INT, &cfg.weaponOrder[0], 0, NUMWEAPONS},
    {"player-weapon-order1", 0, CVT_INT, &cfg.weaponOrder[1], 0, NUMWEAPONS},
    {"player-weapon-order2", 0, CVT_INT, &cfg.weaponOrder[2], 0, NUMWEAPONS},
    {"player-weapon-order3", 0, CVT_INT, &cfg.weaponOrder[3], 0, NUMWEAPONS},

    {"player-weapon-nextmode", 0, CVT_BYTE, &cfg.weaponNextMode, 0, 1},

    // Misc
    {"player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1},

// Game state
    {"game-fastmonsters", 0, CVT_BYTE, &cfg.fastMonsters, 0, 1},

// Gameplay
    {"game-maulator-time", CVF_NO_MAX, CVT_INT, &MaulatorSeconds, 1, 0},

// Game options (non-gameplay affecting)
    {"game-icecorpse", 0, CVT_INT, &cfg.translucentIceCorpse, 0, 1},

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
    {"noclip",      CCmdCheatClip},
    {"warp",        CCmdCheatWarp},
    {"reveal",      CCmdCheatReveal},
    {"give",        CCmdCheatGive},
    {"kill",        CCmdCheatMassacre},
    {"suicide",     CCmdCheatSuicide},

    {"hexenfont",   CCmdHexenFont},

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

    // jHexen specific
    {"invleft",     CCmdInventory},
    {"invright",    CCmdInventory},
    {"pig",         CCmdCheatPig},
    {"runscript",   CCmdCheatRunScript},
    {"scriptinfo",  CCmdScriptInfo},
    {"where",       CCmdCheatWhere},
    {"class",       CCmdCheatShadowcaster},
#ifdef DEMOCAM
    {"demomode",    CCmdSetDemoMode},
#endif
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
void H2_ConsoleBg(int *width, int *height)
{
    extern int consoleFlat;
    extern float consoleZoom;

    GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
    *width = 64 * consoleZoom;
    *height = 64 * consoleZoom;
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
DEFCC(CCmdHexenFont)
{
    ddfont_t cfont;

    cfont.flags = DDFONT_WHITE;
    cfont.height = 9;
    cfont.sizeX = 1.2f;
    cfont.sizeY = 2;
    cfont.TextOut = ConTextOut;
    cfont.Width = ConTextWidth;
    cfont.Filter = ConTextFilter;
    Con_SetFont(&cfont);
    return true;
}
