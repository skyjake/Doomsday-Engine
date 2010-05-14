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
 * hconsole.c: Console stuff - jHexen specific.
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

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdCheat);
DEFCC(CCmdCheatGod);
DEFCC(CCmdCheatNoClip);
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
DEFCC(CCmdSetViewMode);

DEFCC(CCmdCycleSpy);

DEFCC(CCmdSetDemoMode);

DEFCC(CCmdSpawnMobj);

DEFCC(CCmdPrintPlayerCoords);

DEFCC(CCmdScriptInfo);
DEFCC(CCmdTest);
DEFCC(CCmdMovePlane);

void G_UpdateEyeHeight(cvar_t* unused);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

DEFCC(CCmdScreenShot);
DEFCC(CCmdHexenFont);
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
    {"view-size", 0, CVT_INT, &cfg.setBlocks, 3, 13},
    {"hud-title", 0, CVT_BYTE, &cfg.mapTitle, 0, 1},
    {"hud-title-author-noiwad", 0, CVT_BYTE, &cfg.hideIWADAuthor, 0, 1},

    {"view-bob-height", 0, CVT_FLOAT, &cfg.bobView, 0, 1},
    {"view-bob-weapon", 0, CVT_FLOAT, &cfg.bobWeapon, 0, 1},
    {"view-filter-strength", 0, CVT_FLOAT, &cfg.filterStrength, 0, 1},

// Server-side options
    // Game state
    {"server-game-skill", 0, CVT_BYTE, &cfg.netSkill, 0, 4},
    {"server-game-map", CVF_NO_MAX, CVT_BYTE, &cfg.netMap, 0, 0},
    {"server-game-deathmatch", 0, CVT_BYTE,
        &cfg.netDeathmatch, 0, 1}, /* jHexen only has one deathmatch mode */

    // Modifiers
    {"server-game-mod-damage", 0, CVT_BYTE, &cfg.netMobDamageModifier, 1, 100},
    {"server-game-mod-health", 0, CVT_BYTE, &cfg.netMobHealthModifier, 1, 20},
    {"server-game-mod-gravity", 0, CVT_INT, &cfg.netGravity, -1, 100},

    // Gameplay options
    {"server-game-jump", 0, CVT_BYTE, &cfg.netJumping, 0, 1},
    {"server-game-nomonsters", 0, CVT_BYTE, &cfg.netNoMonsters, 0, 1},
    {"server-game-randclass", 0, CVT_BYTE, &cfg.netRandomClass, 0, 1},
    {"server-game-radiusattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZRadiusAttack, 0, 1},
    {"server-game-monster-meleeattack-nomaxz", 0, CVT_BYTE,
        &cfg.netNoMaxZMonsterMeleeAttack, 0, 1},

    // Misc
    {"msg-hub-override", 0, CVT_BYTE, &cfg.overrideHubMsg, 0, 2},

// Player
    // Player data
    {"player-color", 0, CVT_BYTE, &cfg.netColor, 0, 8},
    {"player-eyeheight", 0, CVT_INT, &cfg.plrViewHeight, 41, 54, G_UpdateEyeHeight},
    {"player-class", 0, CVT_BYTE, &cfg.netClass, 0, 2},

    // Movment
    {"player-move-speed", 0, CVT_FLOAT, &cfg.playerMoveSpeed, 0, 1},
    {"player-jump", 0, CVT_INT, &cfg.jumpEnabled, 0, 1},
    {"player-jump-power", 0, CVT_FLOAT, &cfg.jumpPower, 0, 100},
    {"player-air-movement", 0, CVT_BYTE, &cfg.airborneMovement, 0, 32},

    // Weapon switch preferences
    {"player-autoswitch", 0, CVT_BYTE, &cfg.weaponAutoSwitch, 0, 2},
    {"player-autoswitch-ammo", 0, CVT_BYTE, &cfg.ammoAutoSwitch, 0, 2},
    {"player-autoswitch-notfiring", 0, CVT_BYTE,
        &cfg.noWeaponAutoSwitchIfFiring, 0, 1},

    // Weapon Order preferences
    {"player-weapon-order0", 0, CVT_INT, &cfg.weaponOrder[0], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order1", 0, CVT_INT, &cfg.weaponOrder[1], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order2", 0, CVT_INT, &cfg.weaponOrder[2], 0, NUM_WEAPON_TYPES},
    {"player-weapon-order3", 0, CVT_INT, &cfg.weaponOrder[3], 0, NUM_WEAPON_TYPES},

    {"player-weapon-nextmode", 0, CVT_BYTE, &cfg.weaponNextMode, 0, 1},

    // Misc
    {"player-camera-noclip", 0, CVT_INT, &cfg.cameraNoClip, 0, 1},

// Compatibility options
    {"game-icecorpse", 0, CVT_INT, &cfg.translucentIceCorpse, 0, 1},

// Game state
    {"game-fastmonsters", 0, CVT_BYTE, &cfg.fastMonsters, 0, 1},

// Gameplay
    {"game-maulator-time", CVF_NO_MAX, CVT_INT, &maulatorSeconds, 1, 0},

// Misc
    {"msg-echo", 0, CVT_BYTE, &cfg.echoMsg, 0, 1},
    {NULL}
};

//  Console commands
ccmd_t  gameCCmds[] = {
    {"spy",         "",     CCmdCycleSpy},
    {"screenshot",  "",     CCmdScreenShot},

    // $cheats
    {"cheat",       "s",    CCmdCheat},
    {"god",         NULL,   CCmdCheatGod},
    {"noclip",      NULL,   CCmdCheatNoClip},
    {"warp",        "i",    CCmdCheatWarp},
    {"reveal",      "i",    CCmdCheatReveal},
    {"give",        NULL,   CCmdCheatGive},
    {"kill",        "",     CCmdCheatMassacre},
    {"suicide",     NULL,   CCmdCheatSuicide},
    {"where",       "",     CCmdCheatWhere},

    {"hexenfont",   "",     CCmdHexenFont},
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

    // jHexen specific
    {"pig",         "",     CCmdCheatPig},
    {"runscript",   "i",    CCmdCheatRunScript},
    {"scriptinfo",  NULL,   CCmdScriptInfo},
    {"class",       "i",    CCmdCheatShadowcaster},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Add the console variables and commands.
 */
void G_ConsoleRegistration(void)
{
    uint                    i;

    for(i = 0; gameCVars[i].name; ++i)
        Con_AddVariable(&gameCVars[i]);
    for(i = 0; gameCCmds[i].name; ++i)
        Con_AddCommand(&gameCCmds[i]);
}

/**
 * Settings for console background drawing.
 * Called EVERY FRAME by the console drawer.
 */
void G_ConsoleBg(int *width, int *height)
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
 * Called when the player-eyeheight cvar is changed.
 */
void G_UpdateEyeHeight(cvar_t* unused)
{
    player_t* plr = &players[CONSOLEPLAYER];
    if(!(plr->plr->flags & DDPF_CAMERA))
        plr->viewHeight = (float) cfg.plrViewHeight;
}

/**
 * Draw (char *) text in the game's font.
 * Called by the console drawer.
 */
int ConTextOut(const char* string, int x, int y)
{
    M_WriteText3(string, x, y, GF_FONTA, -1, -1, -1, -1, false, false, 0);
    return 0;
}

/**
 * Get the visual width of (char*) text in the game's font.
 */
int ConTextWidth(const char* string)
{
    return M_StringWidth(string, GF_FONTA);
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
 * Configure the console to use the game's font.
 */
DEFCC(CCmdHexenFont)
{
    ddfont_t            cfont;

    cfont.flags = DDFONT_WHITE;
    cfont.height = 9;
    cfont.sizeX = 1.2f;
    cfont.sizeY = 2;
    cfont.drawText = ConTextOut;
    cfont.getWidth = ConTextWidth;
    cfont.filterText = NULL;

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
