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
 * H_Console.c: jHeretic specific console stuff
 */

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/h_stat.h"
#include "jHeretic/h_config.h"
#include "jHeretic/G_game.h"
#include "jHeretic/Sounds.h"
#include "Common/hu_stuff.h"
#include "jHeretic/Mn_def.h"
#include "Common/f_infine.h"

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
DEFCC(CCmdCheatExitLevel);
DEFCC(CCmdCheatSuicide);

DEFCC(CCmdMakeLocal);
DEFCC(CCmdSetCamera);
DEFCC(CCmdSetViewLock);

DEFCC(CCmdCycleSpy);

DEFCC(CCmdPlayDemo);
DEFCC(CCmdRecordDemo);
DEFCC(CCmdStopDemo);

DEFCC(CCmdSpawnMobj);

DEFCC(CCmdPrintPlayerCoords);

DEFCC(CCmdInventory);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdViewSize);
DEFCC(CCmdPause);
DEFCC(CCmdHereticFont);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     consoleFlat = 6;
float   consoleZoom = 1;

// Console variables.
cvar_t  gameCVars[] = {
// Console
    {"con-flat", CVF_NO_MAX, CVT_INT, &consoleFlat, 0, 0,
        "The number of the flat to use for the console background."},
    {"con-zoom", 0, CVT_FLOAT, &consoleZoom, 0.1f, 100.0f,
        "Zoom factor for the console background."},

// View/Refresh
    {"view-size", CVF_PROTECTED, CVT_INT, &cfg.screenblocks, 3, 13,
        "View window size (3-13)."},
    {"hud-title", 0, CVT_BYTE, &cfg.levelTitle, 0, 1,
        "1=Show level title and author in the beginning."},

    {"view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1,
        "Scale for viewheight bobbing."},
    {"view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1,
        "Scale for player weapon bobbing."},
    {"view-bob-weapon-switch-lower", 0, CVT_BYTE, &cfg.bobWeaponLower, 0, 1,
        "HUD weapon lowered during weapon switching."},

    {"view-ringfilter", 0, CVT_INT, &cfg.ringFilter, 1, 2,
        "Ring effect filter. 1=Brownish, 2=Blue."},

// Server-side options
    // Game state
    {"server-game-skill", 0, CVT_BYTE,
        &cfg.netSkill, 0, 4,
        "Skill level in multiplayer games."},
    {"server-game-map", 0, CVT_BYTE,
        &cfg.netMap, 1, 31,
        "Map to use in multiplayer games."},
    {"server-game-episode", 0, CVT_BYTE,
        &cfg.netEpisode, 1, 6,
        "Episode to use in multiplayer games."},
    {"server-game-deathmatch", 0, CVT_BYTE,
        &cfg.netDeathmatch, 0, 1, /* jHeretic only has one deathmatch mode */
        "Start multiplayers games as deathmatch."},

    // Modifiers
    {"server-game-mod-damage", 0, CVT_BYTE,
        &cfg.netMobDamageModifier, 1, 100,
        "Enemy (mob) damage modifier, multiplayer (1..100)."},
    {"server-game-mod-health", 0, CVT_BYTE,
        &cfg.netMobHealthModifier, 1, 20,
        "Enemy (mob) health modifier, multiplayer (1..20)."},

    // Gameplay options
    {"server-game-jump", 0, CVT_BYTE,
        &cfg.netJumping, 0, 1,
        "1=Allow jumping in multiplayer games."},
    {"server-game-nomonsters", 0, CVT_BYTE,
        &cfg.netNomonsters, 0, 1,
        "1=No monsters."},
    {"server-game-respawn", 0, CVT_BYTE,
        &cfg.netRespawn, 0, 1,
        "1= -respawn was used."},
    {"server-game-respawn-monsters-nightmare", 0, CVT_BYTE,
        &cfg.respawnMonstersNightmare, 0, 1,
        "1=Monster respawning in Nightmare difficulty enabled."},
    {"server-game-radiusattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZRadiusAttack, 0, 1,
        "1=ALL radius attacks are infinitely tall."},
    {"server-game-monster-meleeattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZMonsterMeleeAttack, 0, 1,
        "1=Monster melee attacks are infinitely tall."},

// Player
    // Player data
    {"player-color", 0, CVT_BYTE, &cfg.netColor, 0, 4,
        "Player color: 0=green, 1=yellow, 2=red, 3=blue, 4=default."},
    {"player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54,
        "Player eye height. The original is 41."},

    // Movment
    {"player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1,
        "Player movement speed modifier."},
    {"player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1,
        "1=Allow jumping."},
    {"player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100,
        "Jump power."},
    {"player-air-movement", 0, CVT_BYTE, &cfg.airborneMovement, 0, 32,
        "Player movement speed while airborne."},

    // Weapon switch preferences
    {"player-autoswitch", 0, CVT_BYTE,
        &cfg.weaponAutoSwitch, 0, 2,
        "Change weapon automatically when picking one up. 1=If better 2=Always"},

    // Weapon Order preferences
    {"player-weapon-order0", 0, CVT_INT,
        &cfg.weaponOrder[0], 0, NUMWEAPONS,
        "Weapon change order, slot 0."},
    {"player-weapon-order1", 0, CVT_INT,
        &cfg.weaponOrder[1], 0, NUMWEAPONS,
        "Weapon change order, slot 1."},
    {"player-weapon-order2", 0, CVT_INT,
        &cfg.weaponOrder[2], 0, NUMWEAPONS,
        "Weapon change order, slot 2."},
    {"player-weapon-order3", 0, CVT_INT,
        &cfg.weaponOrder[3], 0, NUMWEAPONS,
        "Weapon change order, slot 3."},
    {"player-weapon-order4", 0, CVT_INT,
        &cfg.weaponOrder[4], 0, NUMWEAPONS,
        "Weapon change order, slot 4."},
    {"player-weapon-order5", 0, CVT_INT,
        &cfg.weaponOrder[5], 0, NUMWEAPONS,
        "Weapon change order, slot 5."},
    {"player-weapon-order6", 0, CVT_INT,
        &cfg.weaponOrder[6], 0, NUMWEAPONS,
        "Weapon change order, slot 6."},
    {"player-weapon-order7", 0, CVT_INT,
        &cfg.weaponOrder[7], 0, NUMWEAPONS,
        "Weapon change order, slot 7."},
    {"player-weapon-order8", 0, CVT_INT,
        &cfg.weaponOrder[8], 0, NUMWEAPONS,
        "Weapon change order, slot 8."},

    {"player-weapon-nextmode", 0, CVT_BYTE,
        &cfg.weaponNextMode, 0, 1,
        "1= Use custom weapon order with Next/Previous weapon."},

    // Misc
    {"player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1,
        "1=Camera players have no movement clipping."},

// Compatibility options
    {"game-monsters-stuckindoors", 0, CVT_BYTE, &cfg.monstersStuckInDoors, 0, 1,
        "1=Monsters can get stuck in doortracks (disables DOOM bug fix)."},
    {"game-objects-hangoverledges", 0, CVT_BYTE, &cfg.avoidDropoffs, 0, 1,
        "1=Only some objects can hang over tall ledges (enables DOOM bug fix)."},
    {"game-objects-clipping", 0, CVT_BYTE, &cfg.moveBlock, 0, 1,
        "1=Use EXACTLY DOOM's clipping code (disables DOOM bug fix)."},
    {"game-player-wallrun-northonly", 0, CVT_BYTE, &cfg.wallRunNorthOnly, 0, 1,
        "1=Players can only wallrun North (disables DOOM bug fix)."},
    {"game-objects-falloff", 0, CVT_BYTE, &cfg.fallOff, 0, 1,
        "1=Objects fall under their own weight (enables DOOM bug fix)."},
    {"game-zclip", 0, CVT_BYTE, &cfg.moveCheckZ, 0, 1,
        "1=Allow mobjs to move under/over each other (enables DOOM bug fix)."},
    {"game-corpse-sliding", 0, CVT_BYTE, &cfg.slidingCorpses, 0, 1,
        "1=Corpses slide down stairs and ledges (enables enhanced BOOM behaviour)."},

// Game state
    {"game-fastmonsters", 0, CVT_BYTE, &cfg.fastMonsters, 0, 1,
        "1=Fast monsters in non-demo single player."},

// Gameplay
    {"game-corpse-time", CVF_NO_MAX, CVT_INT, &cfg.corpseTime, 0, 0,
        "Corpse vanish time in seconds, 0=disabled."},

// Misc
    {NULL}
};

//  Console commands
ccmd_t  gameCCmds[] = {
    {"spy",        CCmdCycleSpy,       "Change the viewplayer when not in deathmatch.", 0 },
    {"screenshot", CCmdScreenShot,     "Take a screenshot.", 0 },
    {"viewsize",   CCmdViewSize,       "Set the view size.", 0 },
    {"pause",      CCmdPause,          "Pause the game (same as pressing the pause key).", 0 },

    // $cheats
    {"cheat",      CCmdCheat,          "Issue a cheat code using the original Hexen cheats.", 0 },
    {"god",        CCmdCheatGod,       "I don't think He needs any help...", 0 },
    {"noclip",     CCmdCheatClip,      "Movement clipping on/off.", 0 },
    {"warp",       CCmdCheatWarp,      "Warp to a map.", 0 },
    {"reveal",     CCmdCheatReveal,    "Map cheat.", 0 },
    {"give",       CCmdCheatGive,      "Cheat command to give you various kinds of things.", 0 },
    {"kill",       CCmdCheatMassacre,  "Kill all the monsters on the level.", 0 },
    {"exitlevel",  CCmdCheatExitLevel, "Exit the current level.", 0 },
    {"suicide",    CCmdCheatSuicide,   "Kill yourself. What did you think?", 0 },

    {"hereticfont",CCmdHereticFont,    "Use the Heretic font.", 0 },

    // $infine
    {"startinf",   CCmdStartInFine,    "Start an InFine script.", 0 },
    {"stopinf",    CCmdStopInFine,     "Stop the currently playing interlude/finale.", 0 },
    {"stopfinale", CCmdStopInFine,     "Stop the currently playing interlude/finale.", 0 },

    {"spawnmobj",  CCmdSpawnMobj,      "Spawn a new mobj.", 0 },
    {"coord",      CCmdPrintPlayerCoords,   "Print the coordinates of the consoleplayer.", 0 },

    // $democam
    {"makelocp",   CCmdMakeLocal,      "Make local player.", 0 },
    {"makecam",    CCmdSetCamera,      "Toggle camera mode.", 0 },
    {"setlock",    CCmdSetViewLock,    "Set camera viewlock.", 0 },
    {"lockmode",   CCmdSetViewLock,    "Set camera viewlock mode.", 0 },

    // $moveplane
    {"movefloor",   CCmdMovePlane,      "Move a sector's floor plane.", 0 },
    {"moveceil",    CCmdMovePlane,      "Move a sector's ceiling plane.", 0 },
    {"movesec",     CCmdMovePlane,      "Move a sector's both planes.", 0 },

    // Heretic specific
    {"invleft",    CCmdInventory,      "Move inventory cursor to the left.", 0 },
    {"invright",   CCmdInventory,      "Move inventory cursor to the right.", 0 },
    {"chicken",    CCmdCheatPig,       "Turn yourself into a chicken. Go ahead.", 0 },
    {"where",      CCmdCheatWhere,     "Prints your map number and exact location.", 0 },
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
void H_ConsoleBg(int *width, int *height)
{
    extern int consoleFlat;
    extern float consoleZoom;

    GL_SetFlat(consoleFlat + W_CheckNumForName("F_START") + 1);
    *width = (int) (64 * consoleZoom);
    *height = (int) (64 * consoleZoom);
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
DEFCC(CCmdHereticFont)
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
