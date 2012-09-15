/**\file g_game.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * Top-level (common) game routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#include "dmu_lib.h"
#include "fi_lib.h"
#include "hu_lib.h"
#include "p_saveg.h"
#include "p_sound.h"
#include "g_controls.h"
#include "g_eventsequence.h"
#include "p_mapsetup.h"
#include "p_user.h"
#include "p_actor.h"
#include "p_tick.h"
#include "am_map.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_log.h"
#include "hu_chat.h"
#include "hu_msg.h"
#include "hu_pspr.h"
#include "g_common.h"
#include "g_update.h"
#include "g_eventsequence.h"
#include "d_net.h"
#include "x_hair.h"
#include "p_player.h"
#include "r_common.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_start.h"
#include "p_inventory.h"
#if __JHERETIC__ || __JHEXEN__
# include "p_inventory.h"
# include "hu_inventory.h"
#endif
#include "d_net.h"

// MACROS ------------------------------------------------------------------

#define BODYQUEUESIZE       (32)

#define UNNAMEDMAP          "Unnamed"
#define NOTAMAPNAME         "N/A"
#define READONLYCVAR        CVF_READ_ONLY|CVF_NO_MAX|CVF_NO_MIN|CVF_NO_ARCHIVE

// TYPES -------------------------------------------------------------------

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
struct missileinfo_s {
    mobjtype_t  type;
    float       speed[2];
}
MonsterMissileInfo[] =
{
#if __JDOOM__ || __JDOOM64__
    {MT_BRUISERSHOT, {15, 20}},
    {MT_HEADSHOT, {10, 20}},
    {MT_TROOPSHOT, {10, 20}},
# if __JDOOM64__
    {MT_BRUISERSHOTRED, {15, 20}},
    {MT_NTROSHOT, {20, 40}},
# endif
#elif __JHERETIC__
    {MT_IMPBALL, {10, 20}},
    {MT_MUMMYFX1, {9, 18}},
    {MT_KNIGHTAXE, {9, 18}},
    {MT_REDAXE, {9, 18}},
    {MT_BEASTBALL, {12, 20}},
    {MT_WIZFX1, {18, 24}},
    {MT_SNAKEPRO_A, {14, 20}},
    {MT_SNAKEPRO_B, {14, 20}},
    {MT_HEADFX1, {13, 20}},
    {MT_HEADFX3, {10, 18}},
    {MT_MNTRFX1, {20, 26}},
    {MT_MNTRFX2, {14, 20}},
    {MT_SRCRFX1, {20, 28}},
    {MT_SOR2FX1, {20, 28}},
#endif
    {-1, {-1, -1}}                  // Terminator
};
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(CycleTextureGamma);
D_CMD(DeleteGameSave);
D_CMD(EndGame);
D_CMD(HelpScreen);
D_CMD(ListMaps);
D_CMD(LoadGame);
D_CMD(OpenLoadMenu);
D_CMD(QuickLoadGame);
D_CMD(QuickSaveGame);
D_CMD(SaveGame);
D_CMD(OpenSaveMenu);
D_CMD(WarpMap);

void    G_PlayerReborn(int player);
void    G_DoReborn(int playernum);

typedef struct {
    Uri* mapUri;
    uint episode;
    uint map;
    boolean revisit;
} loadmap_params_t;
int     G_DoLoadMap(loadmap_params_t* params);

void    G_DoLoadGame(void);
void    G_DoPlayDemo(void);
void    G_DoMapCompleted(void);
void    G_DoVictory(void);
void    G_DoLeaveMap(void);
void    G_DoRestartMap(void);
void    G_DoSaveGame(void);
void    G_DoScreenShot(void);
void    G_DoQuitGame(void);

void    G_StopDemo(void);

/**
 * Updates game status cvars for the specified player.
 */
void G_UpdateGSVarsForPlayer(player_t* pl);

/**
 * Updates game status cvars for the current map.
 */
void G_UpdateGSVarsForMap(void);

void R_LoadVectorGraphics(void);

int Hook_DemoStop(int hookType, int val, void* parameters);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void G_InitNewGame(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

game_config_t cfg; // The global cfg.

int debugSound; // Debug flag for displaying sound info.

skillmode_t dSkill;

skillmode_t gameSkill;
uint gameEpisode;
uint gameMap;
uint gameMapEntryPoint; // Position indicator for reborn.

uint nextMap;
#if __JHEXEN__
uint nextMapEntryPoint;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean secretExit;
#endif

#if __JHEXEN__
uint mapHub = 0;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean respawnMonsters;
#endif
boolean monsterInfight;

boolean paused;
boolean sendPause; // Send a pause event next tic.
boolean userGame = false; // Ok to save / end game.
boolean deathmatch; // Only if started as net death.
player_t players[MAXPLAYERS];

int mapStartTic; // Game tic at map start.
int totalKills, totalItems, totalSecret; // For intermission.

boolean singledemo; // Quit after playing a demo from cmdline.
boolean briefDisabled = false;

boolean precache = true; // If @c true, load all graphics at start.
boolean customPal = false; // If @c true, a non-IWAD palette is in use.

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
wbstartstruct_t wmInfo; // Params for world map / intermission.
#endif

// Game Action Variables:
int gaSaveGameSlot = 0;
boolean gaSaveGameGenerateName = true;
ddstring_t* gaSaveGameName;
int gaLoadGameSlot = 0;

#if __JDOOM__ || __JDOOM64__
mobj_t *bodyQueue[BODYQUEUESIZE];
int bodyQueueSlot;
#endif

// vars used with game status cvars
int gsvInMap = 0;
int gsvCurrentMusic = 0;
int gsvMapMusic = -1;

int gsvArmor = 0;
int gsvHealth = 0;

#if !__JHEXEN__
int gsvKills = 0;
int gsvItems = 0;
int gsvSecrets = 0;
#endif

int gsvCurrentWeapon;
int gsvWeapons[NUM_WEAPON_TYPES];
int gsvKeys[NUM_KEY_TYPES];
int gsvAmmo[NUM_AMMO_TYPES];

char *gsvMapName = NOTAMAPNAME;

int gamePauseWhenFocusLost;
int gameUnpauseWhenFocusGained;

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
int gsvInvItems[NUM_INVENTORYITEM_TYPES];
#endif

#if __JHEXEN__
int gsvWPieces[4];
#endif

static gamestate_t gameState = GS_STARTUP;

cvartemplate_t gamestatusCVars[] = {
   {"game-state", READONLYCVAR, CVT_INT, &gameState, 0, 0},
   {"game-state-map", READONLYCVAR, CVT_INT, &gsvInMap, 0, 0},
   {"game-paused", READONLYCVAR, CVT_INT, &paused, 0, 0},
   {"game-skill", READONLYCVAR, CVT_INT, &gameSkill, 0, 0},

   {"game-pause-focuslost", 0, CVT_INT, &gamePauseWhenFocusLost, 0, 1},
   {"game-unpause-focusgained", 0, CVT_INT, &gameUnpauseWhenFocusGained, 0, 1},

   {"map-id", READONLYCVAR, CVT_INT, &gameMap, 0, 0},
   {"map-name", READONLYCVAR, CVT_CHARPTR, &gsvMapName, 0, 0},
   {"map-episode", READONLYCVAR, CVT_INT, &gameEpisode, 0, 0},
#if __JHEXEN__
   {"map-hub", READONLYCVAR, CVT_INT, &mapHub, 0, 0},
#endif
   {"game-music", READONLYCVAR, CVT_INT, &gsvCurrentMusic, 0, 0},
   {"map-music", READONLYCVAR, CVT_INT, &gsvMapMusic, 0, 0},
#if !__JHEXEN__
   {"game-stats-kills", READONLYCVAR, CVT_INT, &gsvKills, 0, 0},
   {"game-stats-items", READONLYCVAR, CVT_INT, &gsvItems, 0, 0},
   {"game-stats-secrets", READONLYCVAR, CVT_INT, &gsvSecrets, 0, 0},
#endif

   {"player-health", READONLYCVAR, CVT_INT, &gsvHealth, 0, 0},
   {"player-armor", READONLYCVAR, CVT_INT, &gsvArmor, 0, 0},
   {"player-weapon-current", READONLYCVAR, CVT_INT, &gsvCurrentWeapon, 0, 0},

#if __JDOOM__ || __JDOOM64__
   // Ammo
   {"player-ammo-bullets", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CLIP], 0, 0},
   {"player-ammo-shells", READONLYCVAR, CVT_INT, &gsvAmmo[AT_SHELL], 0, 0},
   {"player-ammo-cells", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CELL], 0, 0},
   {"player-ammo-missiles", READONLYCVAR, CVT_INT, &gsvAmmo[AT_MISSILE], 0, 0},
   // Weapons
   {"player-weapon-fist", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-pistol", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-shotgun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-chaingun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   {"player-weapon-mlauncher", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIFTH], 0, 0},
   {"player-weapon-plasmarifle", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SIXTH], 0, 0},
   {"player-weapon-bfg", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SEVENTH], 0, 0},
   {"player-weapon-chainsaw", READONLYCVAR, CVT_INT, &gsvWeapons[WT_EIGHTH], 0, 0},
   {"player-weapon-sshotgun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_NINETH], 0, 0},
   // Keys
   {"player-key-blue", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUECARD], 0, 0},
   {"player-key-yellow", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOWCARD], 0, 0},
   {"player-key-red", READONLYCVAR, CVT_INT, &gsvKeys[KT_REDCARD], 0, 0},
   {"player-key-blueskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUESKULL], 0, 0},
   {"player-key-yellowskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOWSKULL], 0, 0},
   {"player-key-redskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_REDSKULL], 0, 0},
#elif __JHERETIC__
   // Ammo
   {"player-ammo-goldwand", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CRYSTAL], 0, 0},
   {"player-ammo-crossbow", READONLYCVAR, CVT_INT, &gsvAmmo[AT_ARROW], 0, 0},
   {"player-ammo-dragonclaw", READONLYCVAR, CVT_INT, &gsvAmmo[AT_ORB], 0, 0},
   {"player-ammo-hellstaff", READONLYCVAR, CVT_INT, &gsvAmmo[AT_RUNE], 0, 0},
   {"player-ammo-phoenixrod", READONLYCVAR, CVT_INT, &gsvAmmo[AT_FIREORB], 0, 0},
   {"player-ammo-mace", READONLYCVAR, CVT_INT, &gsvAmmo[AT_MSPHERE], 0, 0},
    // Weapons
   {"player-weapon-staff", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-goldwand", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-crossbow", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-dragonclaw", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   {"player-weapon-hellstaff", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIFTH], 0, 0},
   {"player-weapon-phoenixrod", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SIXTH], 0, 0},
   {"player-weapon-mace", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SEVENTH], 0, 0},
   {"player-weapon-gauntlets", READONLYCVAR, CVT_INT, &gsvWeapons[WT_EIGHTH], 0, 0},
   // Keys
   {"player-key-yellow", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOW], 0, 0},
   {"player-key-green", READONLYCVAR, CVT_INT, &gsvKeys[KT_GREEN], 0, 0},
   {"player-key-blue", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUE], 0, 0},
   // Inventory items
   {"player-artifact-ring", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVULNERABILITY], 0, 0},
   {"player-artifact-shadowsphere", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVISIBILITY], 0, 0},
   {"player-artifact-crystalvial", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALTH], 0, 0},
   {"player-artifact-mysticurn", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUPERHEALTH], 0, 0},
   {"player-artifact-tomeofpower", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TOMBOFPOWER], 0, 0},
   {"player-artifact-torch", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TORCH], 0, 0},
   {"player-artifact-firebomb", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FIREBOMB], 0, 0},
   {"player-artifact-egg", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_EGG], 0, 0},
   {"player-artifact-wings", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FLY], 0, 0},
   {"player-artifact-chaosdevice", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORT], 0, 0},
#elif __JHEXEN__
   // Mana
   {"player-mana-blue", READONLYCVAR, CVT_INT, &gsvAmmo[AT_BLUEMANA], 0, 0},
   {"player-mana-green", READONLYCVAR, CVT_INT, &gsvAmmo[AT_GREENMANA], 0, 0},
   // Keys
   {"player-key-steel", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY1], 0, 0},
   {"player-key-cave", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY2], 0, 0},
   {"player-key-axe", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY3], 0, 0},
   {"player-key-fire", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY4], 0, 0},
   {"player-key-emerald", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY5], 0, 0},
   {"player-key-dungeon", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY6], 0, 0},
   {"player-key-silver", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY7], 0, 0},
   {"player-key-rusted", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY8], 0, 0},
   {"player-key-horn", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY9], 0, 0},
   {"player-key-swamp", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEYA], 0, 0},
   {"player-key-castle", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEYB], 0, 0},
   // Weapons
   {"player-weapon-first", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0},
   {"player-weapon-second", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0},
   {"player-weapon-third", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0},
   {"player-weapon-fourth", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0},
   // Weapon Pieces
   {"player-weapon-piece1", READONLYCVAR, CVT_INT, &gsvWPieces[0], 0, 0},
   {"player-weapon-piece2", READONLYCVAR, CVT_INT, &gsvWPieces[1], 0, 0},
   {"player-weapon-piece3", READONLYCVAR, CVT_INT, &gsvWPieces[2], 0, 0},
   {"player-weapon-allpieces", READONLYCVAR, CVT_INT, &gsvWPieces[3], 0, 0},
   // Inventory items
   {"player-artifact-defender", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVULNERABILITY], 0, 0},
   {"player-artifact-quartzflask", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALTH], 0, 0},
   {"player-artifact-mysticurn", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUPERHEALTH], 0, 0},
   {"player-artifact-mysticambit", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALINGRADIUS], 0, 0},
   {"player-artifact-darkservant", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUMMON], 0, 0},
   {"player-artifact-torch", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TORCH], 0, 0},
   {"player-artifact-porkalator", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_EGG], 0, 0},
   {"player-artifact-wings", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FLY], 0, 0},
   {"player-artifact-repulsion", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BLASTRADIUS], 0, 0},
   {"player-artifact-flechette", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_POISONBAG], 0, 0},
   {"player-artifact-banishment", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORTOTHER], 0, 0},
   {"player-artifact-speed", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SPEED], 0, 0},
   {"player-artifact-might", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BOOSTMANA], 0, 0},
   {"player-artifact-bracers", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BOOSTARMOR], 0, 0},
   {"player-artifact-chaosdevice", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORT], 0, 0},
   {"player-artifact-skull", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZSKULL], 0, 0},
   {"player-artifact-heart", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBIG], 0, 0},
   {"player-artifact-ruby", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMRED], 0, 0},
   {"player-artifact-emerald1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMGREEN1], 0, 0},
   {"player-artifact-emerald2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMGREEN2], 0, 0},
   {"player-artifact-sapphire1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBLUE1], 0, 0},
   {"player-artifact-sapphire2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBLUE2], 0, 0},
   {"player-artifact-daemoncodex", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZBOOK1], 0, 0},
   {"player-artifact-liberoscura", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZBOOK2], 0, 0},
   {"player-artifact-flamemask", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZSKULL2], 0, 0},
   {"player-artifact-glaiveseal", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZFWEAPON], 0, 0},
   {"player-artifact-holyrelic", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZCWEAPON], 0, 0},
   {"player-artifact-sigilmagus", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZMWEAPON], 0, 0},
   {"player-artifact-gear1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR1], 0, 0},
   {"player-artifact-gear2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR2], 0, 0},
   {"player-artifact-gear3", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR3], 0, 0},
   {"player-artifact-gear4", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR4], 0, 0},
#endif
   {NULL}
};

ccmdtemplate_t gameCmds[] = {
    { "deletegamesave", "ss",   CCmdDeleteGameSave },
    { "deletegamesave", "s",    CCmdDeleteGameSave },
    { "endgame",        "",     CCmdEndGame },
    { "helpscreen",     "",     CCmdHelpScreen },
    { "listmaps",       "",     CCmdListMaps },
    { "loadgame",       "ss",   CCmdLoadGame },
    { "loadgame",       "s",    CCmdLoadGame },
    { "loadgame",       "",     CCmdOpenLoadMenu },
    { "quickload",      "",     CCmdQuickLoadGame },
    { "quicksave",      "",     CCmdQuickSaveGame },
    { "savegame",       "sss",  CCmdSaveGame },
    { "savegame",       "ss",   CCmdSaveGame },
    { "savegame",       "s",    CCmdSaveGame },
    { "savegame",       "",     CCmdOpenSaveMenu },
    { "togglegamma",    "",     CCmdCycleTextureGamma },
    { NULL }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint dEpisode;
static uint dMap;
static uint dMapEntryPoint;

static gameaction_t gameAction;
static boolean quitInProgress;

// CODE --------------------------------------------------------------------

void G_Register(void)
{
    int i;

    // Default values (overridden by values from .cfg files).
    gamePauseWhenFocusLost = true;
    gameUnpauseWhenFocusGained = false;

    for(i = 0; gamestatusCVars[i].path; ++i)
        Con_AddVariable(gamestatusCVars + i);

    for(i = 0; gameCmds[i].name; ++i)
        Con_AddCommand(gameCmds + i);

    C_CMD("warp", "i", WarpMap);
#if __JDOOM__ || __JHERETIC__
# if __JDOOM__
    if(!(gameModeBits & GM_ANY_DOOM2))
# endif
    {
        C_CMD("warp", "ii", WarpMap);
    }
#endif
}

boolean G_QuitInProgress(void)
{
    return quitInProgress;
}

void G_SetGameAction(gameaction_t action)
{
    if(G_QuitInProgress()) return;

    if(gameAction != action)
        gameAction = action;
}

gameaction_t G_GameAction(void)
{
    return gameAction;
}

/**
 * Common Pre Game Initialization routine.
 * Game-specfic pre init actions should be placed in eg D_PreInit() (for jDoom)
 */
void G_CommonPreInit(void)
{
    int i, j;

    quitInProgress = false;
    verbose = CommandLine_Exists("-verbose");

    // Register hooks.
    Plug_AddHook(HOOK_DEMO_STOP, Hook_DemoStop);

    // Setup the players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* pl = players + i;

        pl->plr = DD_GetPlayer(i);
        pl->plr->extraData = (void*) &players[i];

        /// \todo Only necessary because the engine does not yet unload game
        /// plugins when they are not in use; thus a game change may leave
        /// these pointers dangling.
        for(j = 0; j < NUMPSPRITES; ++j)
        {
            pl->pSprites[j].state = NULL;
            pl->plr->pSprites[j].statePtr = NULL;
        }
    }

    G_RegisterBindClasses();
    G_RegisterPlayerControls();
    P_RegisterMapObjs();

    R_LoadVectorGraphics();
    R_LoadColorPalettes();

    P_InitPicAnims();

    // Add our cvars and ccmds to the console databases.
    G_ConsoleRegistration();    // Main command list.
    D_NetConsoleRegistration(); // For network.
    G_Register();               // Read-only game status cvars (for playsim).
    G_ControlRegister();        // For controls/input.
    SV_Register();              // Game-save system.
    Hu_MenuRegister();          // For the menu.
    GUI_Register();             // For the UI library.
    Hu_MsgRegister();           // For the game messages.
    ST_Register();              // For the hud/statusbar.
    WI_Register();              // For the interlude/intermission.
    X_Register();               // For the crosshair.
    FI_StackRegister();         // For the InFine lib.
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_Register();
#endif

    Con_SetString2("map-name", NOTAMAPNAME, SVF_WRITE_OVERRIDE);
}

#if __JHEXEN__
/**
 * \todo all this swapping colors around is rather silly, why not simply
 * reorder the translation tables at load time?
 */
void R_GetTranslation(int plrClass, int plrColor, int* tclass, int* tmap)
{
    int mapped;

    if(plrClass == PCLASS_PIG)
    {
        // A pig is never translated.
        *tclass = *tmap = 0;
        return;
    }

    if(gameMode == hexen_v10)
    {
        const int mapping[3][4] = {
            /* Fighter */ { 1, 2, 0, 3 },
            /* Cleric */  { 1, 0, 2, 3 },
            /* Mage */    { 1, 0, 2, 3 }
        };
        mapped = mapping[plrClass][plrColor];
    }
    else
    {
        const int mapping[3][8] = {
            /* Fighter */ { 1, 2, 0, 3, 4, 5, 6, 7 },
            /* Cleric */  { 1, 0, 2, 3, 4, 5, 6, 7 },
            /* Mage */    { 1, 0, 2, 3, 4, 5, 6, 7 }
        };
        mapped = mapping[plrClass][plrColor];
    }

    *tclass = (mapped? plrClass : 0);
    *tmap   = mapped;
}

void Mobj_UpdateTranslationClassAndMap(mobj_t* mo)
{
    if(mo->player)
    {
        int plrColor = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
        R_GetTranslation(mo->player->class_, plrColor, &mo->tclass, &mo->tmap);
    }
    else if(mo->flags & MF_TRANSLATION)
    {
        mo->tclass = mo->special1;
        mo->tmap = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
    }
    else
    {
        // No translation.
        mo->tmap = mo->tclass = 0;
    }
}
#endif // __JHEXEN__

void R_LoadColorPalettes(void)
{
#define PALLUMPNAME         "PLAYPAL"
#define PALENTRIES          (256)
#define PALID               (0)

    lumpnum_t lumpNum = W_GetLumpNumForName(PALLUMPNAME);
    uint8_t data[PALENTRIES*3];

    // Record whether we are using a custom palette.
    customPal = W_LumpIsCustom(lumpNum);

    W_ReadLumpSection(lumpNum, data, 0 + PALID * (PALENTRIES * 3), PALENTRIES * 3);
    R_CreateColorPalette("R8G8B8", PALLUMPNAME, data, PALENTRIES);

    /**
     * Create the translation tables to map the green color ramp to gray,
     * brown, red.
     *
     * \note Assumes a given structure of the PLAYPAL. Could be read from a
     * lump instead?
     */
#if __JDOOM__ || __JDOOM64__
    {
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    int i;

    // Translate just the 16 green colors.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 0x70 && i <= 0x7f)
        {
            // Map green ramp to gray, brown, red.
            translationtables[i] = 0x60 + (i & 0xf);
            translationtables[i + 256] = 0x40 + (i & 0xf);
            translationtables[i + 512] = 0x20 + (i & 0xf);
        }
        else
        {
            // Keep all other colors as is.
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
    }
#elif __JHERETIC__
    {
    int i;
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);

    // Fill out the translation tables.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225); // yellow
            translationtables[i + 256] = 145 + (i - 225); // red
            translationtables[i + 512] = 190 + (i - 225); // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
    }
#else // __JHEXEN__
    {
    int i, cl, idx;
    uint8_t* translationtables = (uint8_t*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    lumpnum_t lumpNum;
    char name[9];
    int numPerClass = (gameMode == hexen_v10? 3 : 7);

    // In v1.0, the color translations are a bit different. There are only
    // three translation maps per class, whereas Doomsday assumes seven maps
    // per class. Thus we'll need to account for the difference.

    idx = 0;
    for(cl = 0; cl < 3; ++cl)
        for(i = 0; i < 7; ++i)
        {
            if(i == numPerClass) break; // Not present.
            strcpy(name, "TRANTBL0");
            if(idx < 10)
                name[7] += idx;
            else
                name[7] = 'A' + idx - 10;
            idx++;
#ifdef _DEBUG
            Con_Message("Reading translation table '%s' as tclass=%i tmap=%i.\n", name, cl, i);
#endif
            if(-1 != (lumpNum = W_CheckLumpNumForName(name)))
            {
                W_ReadLumpSection(lumpNum, &translationtables[(7*cl + i) * 256], 0, 256);
            }
        }
    }
#endif

#undef PALID
#undef PALENTRIES
#undef PALLUMPNAME
}

/**
 * @todo Read this information from a definition (ideally with more user
 *       friendly mnemonics...).
 */
void R_LoadVectorGraphics(void)
{
#define R                          (1.0f)
#define NUMITEMS(x)                (sizeof(x)/sizeof((x)[0]))

    const Point2Rawf keyPoints[] = {
        {-3 * R / 4, 0}, {-3 * R / 4, -R / 4}, // Mid tooth.
        {    0,      0}, {   -R,      0}, {   -R, -R / 2}, // Shaft and end tooth.

        {    0,      0}, {R / 4, -R / 2}, // Bow.
        {R / 2, -R / 2}, {R / 2,  R / 2},
        {R / 4,  R / 2}, {    0,      0},
    };
    const def_svgline_t key[] = {
        { 2, &keyPoints[ 0] },
        { 3, &keyPoints[ 2] },
        { 6, &keyPoints[ 5] }
    };
    const Point2Rawf thintrianglePoints[] = {
        {-R / 2,  R - R / 2},
        {     R,          0}, // `
        {-R / 2, -R + R / 2}, // /
        {-R / 2,  R - R / 2} // |>
    };
    const def_svgline_t thintriangle[] = {
        { 4, thintrianglePoints },
    };
#if __JDOOM__ || __JDOOM64__
    const Point2Rawf arrowPoints[] = {
        {    -R + R / 8, 0},  {             R, 0}, // -----
        { R - R / 2, -R / 4}, {             R, 0}, { R - R / 2,  R / 4}, // ----->
        {-R - R / 8, -R / 4}, {    -R + R / 8, 0}, {-R - R / 8,  R / 4}, // >---->
        {-R + R / 8, -R / 4}, {-R + 3 * R / 8, 0}, {-R + R / 8,  R / 4}, // >>--->
    };
    const def_svgline_t arrow[] = {
        { 2, &arrowPoints[ 0] },
        { 3, &arrowPoints[ 2] },
        { 3, &arrowPoints[ 5] },
        { 3, &arrowPoints[ 8] }
    };
#elif __JHERETIC__ || __JHEXEN__
    const Point2Rawf arrowPoints[] = {
        {-R + R / 4,      0}, {         0,      0}, // center line.
        {-R + R / 4,  R / 8}, {         R,      0}, {-R + R / 4, -R / 8}, // blade

        {-R + R / 8, -R / 4}, {-R + R / 4, -R / 4}, // guard
        {-R + R / 4,  R / 4}, {-R + R / 8,  R / 4},
        {-R + R / 8, -R / 4},

        {-R + R / 8, -R / 8}, {-R - R / 4, -R / 8}, // hilt
        {-R - R / 4,  R / 8}, {-R + R / 8,  R / 8},
    };
    const def_svgline_t arrow[] = {
        { 2, &arrowPoints[ 0] },
        { 3, &arrowPoints[ 2] },
        { 5, &arrowPoints[ 5] },
        { 4, &arrowPoints[10] }
    };
#endif
#if __JDOOM__
    const Point2Rawf cheatarrowPoints[] = {
        {    -R + R / 8, 0},  {             R, 0}, // -----
        { R - R / 2, -R / 4}, {             R, 0}, { R - R / 2,  R / 4}, // ----->
        {-R - R / 8, -R / 4}, {    -R + R / 8, 0}, {-R - R / 8,  R / 4}, // >---->
        {-R + R / 8, -R / 4}, {-R + 3 * R / 8, 0}, {-R + R / 8,  R / 4}, // >>--->

        {        -R / 2,      0}, {        -R / 2, -R / 6}, // >>-d--->
        {-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6,  R / 4},

        {        -R / 6,      0}, {        -R / 6, -R / 6}, // >>-dd-->
        {             0, -R / 6}, {             0,  R / 4},

        {         R / 6,  R / 4}, {         R / 6, -R / 7}, // >>-ddt->
        {R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7}
    };
    const def_svgline_t cheatarrow[] = {
        { 2, &cheatarrowPoints[ 0] },
        { 3, &cheatarrowPoints[ 2] },
        { 3, &cheatarrowPoints[ 5] },
        { 3, &cheatarrowPoints[ 8] },
        { 4, &cheatarrowPoints[11] },
        { 4, &cheatarrowPoints[15] },
        { 4, &cheatarrowPoints[19] }
    };
#endif

    const Point2Rawf crossPoints[] = { // + (open center)
        {-R,  0}, {-R / 5 * 2,          0},
        { 0, -R}, {         0, -R / 5 * 2},
        { R,  0}, { R / 5 * 2,          0},
        { 0,  R}, {         0,  R / 5 * 2}
    };
    const def_svgline_t cross[] = {
        { 2, &crossPoints[0] },
        { 2, &crossPoints[2] },
        { 2, &crossPoints[4] },
        { 2, &crossPoints[6] }
    };
    const Point2Rawf twinanglesPoints[] = { // > <
        {-R, -R * 10 / 14}, {-(R - (R * 10 / 14)), 0}, {-R,  R * 10 / 14}, // >
        { R, -R * 10 / 14}, {  R - (R * 10 / 14) , 0}, { R,  R * 10 / 14}, // <
    };
    const def_svgline_t twinangles[] = {
        { 3, &twinanglesPoints[0] },
        { 3, &twinanglesPoints[3] }
    };
    const Point2Rawf squarePoints[] = { // square
        {-R, -R}, {-R,  R},
        { R,  R}, { R, -R},
        {-R, -R}
    };
    const def_svgline_t square[] = {
        { 5, squarePoints },
    };
    const Point2Rawf squarecornersPoints[] = { // square (open center)
        {   -R, -R / 2}, {-R, -R}, {-R / 2,      -R}, // topleft
        {R / 2,     -R}, { R, -R}, {     R,  -R / 2}, // topright
        {   -R,  R / 2}, {-R,  R}, {-R / 2,       R}, // bottomleft
        {R / 2,      R}, { R,  R}, {     R,   R / 2}, // bottomright
    };
    const def_svgline_t squarecorners[] = {
        { 3, &squarecornersPoints[ 0] },
        { 3, &squarecornersPoints[ 3] },
        { 3, &squarecornersPoints[ 6] },
        { 3, &squarecornersPoints[ 9] }
    };
    const Point2Rawf anglePoints[] = { // v
        {-R, -R}, { 0,  0}, { R, -R}
    };
    const def_svgline_t angle[] = {
        { 3, anglePoints }
    };

    R_NewSvg(VG_KEY, key, NUMITEMS(key));
    R_NewSvg(VG_TRIANGLE, thintriangle, NUMITEMS(thintriangle));
    R_NewSvg(VG_ARROW, arrow, NUMITEMS(arrow));
#if __JDOOM__
    R_NewSvg(VG_CHEATARROW, cheatarrow, NUMITEMS(cheatarrow));
#endif
    R_NewSvg(VG_XHAIR1, cross, NUMITEMS(cross));
    R_NewSvg(VG_XHAIR2, twinangles, NUMITEMS(twinangles));
    R_NewSvg(VG_XHAIR3, square, NUMITEMS(square));
    R_NewSvg(VG_XHAIR4, squarecorners, NUMITEMS(squarecorners));
    R_NewSvg(VG_XHAIR5, angle, NUMITEMS(angle));

#undef NUMITEMS
#undef R
}

/**
 * @param name  Name of the font to lookup.
 * @return  Unique id of the found font.
 */
fontid_t R_MustFindFontForName(const char* name)
{
    Uri* uri = Uri_NewWithPath2(name, RC_NULL);
    fontid_t fontId = Fonts_ResolveUri(uri);
    Uri_Delete(uri);
    if(fontId) return fontId;
    Con_Error("Failed loading font \"%s\".", name);
    exit(1); // Unreachable.
}

void R_InitRefresh(void)
{
    if(IS_DEDICATED) return;

    VERBOSE( Con_Message("R_InitRefresh: Loading data for referesh.\n") );

    // Setup the view border.
    cfg.screenBlocks = cfg.setBlocks;
    { Uri* paths[9];
    uint i;
    for(i = 0; i < 9; ++i)
        paths[i] = ((borderGraphics[i] && borderGraphics[i][0])? Uri_NewWithPath2(borderGraphics[i], RC_NULL) : 0);
    R_SetBorderGfx((const Uri**)paths);
    for(i = 0; i < 9; ++i)
        if(paths[i])
            Uri_Delete(paths[i]);
    }
    R_ResizeViewWindow(RWF_FORCE|RWF_NO_LERP);

    // Locate our fonts.
    fonts[GF_FONTA]    = R_MustFindFontForName("a");
    fonts[GF_FONTB]    = R_MustFindFontForName("b");
    fonts[GF_STATUS]   = R_MustFindFontForName("status");
#if __JDOOM__
    fonts[GF_INDEX]    = R_MustFindFontForName("index");
#endif
#if __JDOOM__ || __JDOOM64__
    fonts[GF_SMALL]    = R_MustFindFontForName("small");
#endif
#if __JHERETIC__ || __JHEXEN__
    fonts[GF_SMALLIN]  = R_MustFindFontForName("smallin");
#endif
    fonts[GF_MAPPOINT] = R_MustFindFontForName("mappoint");

    { float mul = 1.4f;
    DD_SetVariable(DD_PSPRITE_LIGHTLEVEL_MULTIPLIER, &mul);
    }
}

void R_InitHud(void)
{
    Hu_LoadData();

#if __JHERETIC__ || __JHEXEN__
    VERBOSE( Con_Message("Initializing inventory...\n") )
    Hu_InventoryInit();
#endif

    VERBOSE2( Con_Message("Initializing statusbar...\n") )
    ST_Init();

    VERBOSE2( Con_Message("Initializing menu...\n") )
    Hu_MenuInit();

    VERBOSE2( Con_Message("Initializing status-message/question system...\n") )
    Hu_MsgInit();
}

/**
 * Common Post Game Initialization routine.
 * Game-specific post init actions should be placed in eg D_PostInit()
 * (for jDoom) and NOT here.
 */
void G_CommonPostInit(void)
{
    R_InitRefresh();
    FI_StackInit();
    GUI_Init();

    // Init the save system and create the game save directory.
    SV_Init();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_ReadTypes();
#endif

    VERBOSE( Con_Message("Initializing playsim...\n") )
    P_Init();

    VERBOSE( Con_Message("Initializing head-up displays...\n") )
    R_InitHud();

    G_InitEventSequences();
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    G_RegisterCheats();
#endif

    // From this point on, the shortcuts are always active.
    DD_Execute(true, "activatebcontext shortcut");

    // Display a breakdown of the available maps.
    DD_Execute(true, "listmaps");
}

/**
 * Common game shutdown routine.
 * \note Game-specific actions should be placed in G_Shutdown rather than here.
 */
void G_CommonShutdown(void)
{
    Plug_RemoveHook(HOOK_DEMO_STOP, Hook_DemoStop);

    Hu_MsgShutdown();
    Hu_UnloadData();
    D_NetClearBuffer();

    SV_Shutdown();
    P_Shutdown();
    G_ShutdownEventSequences();

    FI_StackShutdown();
    Hu_MenuShutdown();
    ST_Shutdown();
    GUI_Shutdown();
}

/**
 * Retrieve the current game state.
 *
 * @return              The current game state.
 */
gamestate_t G_GameState(void)
{
    return gameState;
}

#if _DEBUG
static const char* getGameStateStr(gamestate_t state)
{
    struct statename_s {
        gamestate_t     state;
        const char*     name;
    } stateNames[] =
    {
        {GS_MAP, "GS_MAP"},
        {GS_INTERMISSION, "GS_INTERMISSION"},
        {GS_FINALE, "GS_FINALE"},
        {GS_STARTUP, "GS_STARTUP"},
        {GS_WAITING, "GS_WAITING"},
        {GS_INFINE, "GS_INFINE"},
        {-1, NULL}
    };
    uint                i;

    for(i = 0; stateNames[i].name; ++i)
        if(stateNames[i].state == state)
            return stateNames[i].name;

    return NULL;
}
#endif

/**
 * Called when the gameui binding context is active. Triggers the menu.
 */
int G_UIResponder(event_t* ev)
{
    // Handle "Press any key to continue" messages.
    if(Hu_MsgResponder(ev))
        return true;

    if(ev->state != EVS_DOWN)
        return false;
    if(!(ev->type == EV_KEY || ev->type == EV_MOUSE_BUTTON || ev->type == EV_JOY_BUTTON))
        return false;

    if(!Hu_MenuIsActive() && !DD_GetInteger(DD_SHIFT_DOWN))
    {
        // Any key/button down pops up menu if in demos.
        if((G_GameAction() == GA_NONE && !singledemo && Get(DD_PLAYBACK)) ||
           (G_GameState() == GS_INFINE && FI_IsMenuTrigger()))
        {
            Hu_MenuCommand(MCMD_OPEN);
            return true;
        }
    }

    return false;
}

/**
 * Change the game's state.
 *
 * @param state         The state to change to.
 */
void G_ChangeGameState(gamestate_t state)
{
    boolean gameUIActive = false;
    boolean gameActive = true;

    if(G_QuitInProgress()) return;

    if(state < 0 || state >= NUM_GAME_STATES)
        Con_Error("G_ChangeGameState: Invalid state %i.\n", (int) state);

    if(gameState != state)
    {
#if _DEBUG
        // Log gamestate changes in debug builds, with verbose.
        Con_Message("G_ChangeGameState: New state %s.\n", getGameStateStr(state));
#endif
        gameState = state;
    }

    // Update the state of the gameui binding context.
    switch(gameState)
    {
    case GS_FINALE:
    case GS_STARTUP:
    case GS_WAITING:
    case GS_INFINE:
        gameActive = false;
    case GS_INTERMISSION:
        gameUIActive = true;
        break;
    default:
        break;
    }

    if(gameUIActive)
    {
        DD_Execute(true, "activatebcontext gameui");
        B_SetContextFallback("gameui", G_UIResponder);
    }

    DD_Executef(true, "%sactivatebcontext game", gameActive? "" : "de");
}

boolean G_StartFinale(const char* script, int flags, finale_mode_t mode, const char* defId)
{
    assert(script && script[0]);
    { uint i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        // Clear the message queue for all local players.
        ST_LogEmpty(i);

        // Close the automap for all local players.
        ST_AutomapOpen(i, false, true);

#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }}
    G_SetGameAction(GA_NONE);
    FI_StackExecuteWithId(script, flags, mode, defId);
    return true;
}

void G_StartTitle(void)
{
    ddfinale_t fin;

    G_StopDemo();
    userGame = false;

    // The title script must always be defined.
    if(!Def_Get(DD_DEF_FINALE, "title", &fin))
        Con_Error("G_StartTitle: A title script must be defined.");

    G_StartFinale(fin.script, FF_LOCAL, FIMODE_NORMAL, "title");
}

void G_StartHelp(void)
{
    ddfinale_t fin;
    if(G_QuitInProgress()) return;

    if(Def_Get(DD_DEF_FINALE, "help", &fin))
    {
        Hu_MenuCommand(MCMD_CLOSEFAST);
        G_StartFinale(fin.script, FF_LOCAL, FIMODE_NORMAL, "help");
        return;
    }
    Con_Message("Warning: InFine script 'help' not defined, ignoring.\n");
}

/**
 * Prints a banner to the console containing information pertinent to the
 * current map (e.g., map name, author...).
 */
static void printMapBanner(void)
{
    const char* name = P_GetMapNiceName();

    Con_Printf("\n");
    if(name)
    {
        char buf[64];
#if __JHEXEN__
        dd_snprintf(buf, 64, "Map %u (%u): %s", P_GetMapWarpTrans(gameMap)+1, gameMap+1, name);
#else
        dd_snprintf(buf, 64, "Map %u: %s", gameMap+1, name);
#endif
        Con_FPrintf(CPF_LIGHT|CPF_BLUE, "%s\n", buf);
    }

#if !__JHEXEN__
    {
    static const char* unknownAuthorStr = "Unknown";
    Uri* uri = G_ComposeMapUri(gameEpisode, gameMap);
    AutoStr* path = Uri_Compose(uri);
    const char* lauthor;

    lauthor = P_GetMapAuthor(P_MapIsCustom(Str_Text(path)));
    if(!lauthor)
        lauthor = unknownAuthorStr;

    Con_FPrintf(CPF_LIGHT|CPF_BLUE, "Author: %s\n", lauthor);

    Uri_Delete(uri);
    }
#endif
    Con_Printf("\n");
}

void G_BeginMap(void)
{
    G_ChangeGameState(GS_MAP);

    R_SetViewPortPlayer(CONSOLEPLAYER, CONSOLEPLAYER); // View the guy you are playing.
    R_ResizeViewWindow(RWF_FORCE|RWF_NO_LERP);

    G_ControlReset(-1); // Clear all controls for all local players.
    G_UpdateGSVarsForMap();

    // Time can now progress in this map.
    mapStartTic = (int) GAMETIC;
    mapTime = actualMapTime = 0;

    printMapBanner();

    // The music may have been paused for the briefing; unpause.
    S_PauseMusic(false);
}

int G_EndGameResponse(msgresponse_t response, int userValue, void* userPointer)
{
    if(response == MSG_YES)
    {
        if(IS_CLIENT)
        {
            DD_Executef(false, "net disconnect");
        }
        else
        {
            G_StartTitle();
        }
    }
    return true;
}

void G_EndGame(void)
{
    if(G_QuitInProgress()) return;

    if(!userGame)
    {
        Hu_MsgStart(MSG_ANYKEY, ENDNOGAME, NULL, 0, NULL);
        return;
    }

    /*
    if(IS_NETGAME)
    {
        Hu_MsgStart(MSG_ANYKEY, NETEND, NULL, 0, NULL);
        return;
    }
    */

    Hu_MsgStart(MSG_YESNO, IS_CLIENT? GET_TXT(TXT_DISCONNECT) : ENDGAME, G_EndGameResponse, 0, NULL);
}

/// @param mapInfo  Can be @c NULL.
static void initFogForMap(ddmapinfo_t* mapInfo)
{
#if __JHEXEN__
    int fadeTable;
#endif

    if(!mapInfo || !(mapInfo->flags & MIF_FOG))
    {
        R_SetupFogDefaults();
    }
    else
    {
        R_SetupFog(mapInfo->fogStart, mapInfo->fogEnd, mapInfo->fogDensity, mapInfo->fogColor);
    }

#if __JHEXEN__
    fadeTable = P_GetMapFadeTable(gameMap);
    if(fadeTable == W_GetLumpNumForName("COLORMAP"))
    {
        // We don't want fog in this case.
        GL_UseFog(false);
    }
    else
    {
        // Probably fog ... don't use fullbright sprites
        if(fadeTable == W_GetLumpNumForName("FOGMAP"))
        {
            // Tell the renderer to turn on the fog.
            GL_UseFog(true);
        }
    }
#endif
}

int G_DoLoadMap(loadmap_params_t* p)
{
    boolean hasMapInfo = false;
    ddmapinfo_t mapInfo;

    DENG_ASSERT(p);

    // Is MapInfo data available for this map?
    { AutoStr* mapUriStr = Uri_Compose(p->mapUri);
    if(mapUriStr)
    {
        hasMapInfo = Def_Get(DD_DEF_MAP_INFO, Str_Text(mapUriStr), &mapInfo);
    }}

    P_SetupMap(p->mapUri, p->episode, p->map);
    initFogForMap(hasMapInfo? &mapInfo : 0);

#if __JHEXEN__
    if(p->revisit)
    {
        // We've been here before; deserialize this map's save state.
        SV_HxLoadClusterMap();
    }
#endif

    /// @todo Fixme: Do not assume!
    return 0; // Assume success.
}

static int G_DoLoadMapWorker(void* params)
{
    loadmap_params_t* p = (loadmap_params_t*) params;
    int result = G_DoLoadMap(p);
    BusyMode_WorkerEnd();
    return result;
}

int G_DoLoadMapAndMaybeStartBriefing(loadmap_params_t* p)
{
    ddfinale_t fin;
    boolean hasBrief;

    DENG_ASSERT(p);

    hasBrief = G_BriefingEnabled(p->episode, p->map, &fin);

    G_DoLoadMap(p);

    // Start a briefing, if there is one.
    if(hasBrief)
    {
        G_StartFinale(fin.script, 0, FIMODE_BEFORE, 0);
    }
    return hasBrief;
}

static int G_DoLoadMapAndMaybeStartBriefingWorker(void* parameters)
{
    loadmap_params_t* p = (loadmap_params_t*)parameters;
    int result = G_DoLoadMapAndMaybeStartBriefing(p);
    BusyMode_WorkerEnd();
    return result;
}

int G_Responder(event_t* ev)
{
    DENG_ASSERT(ev);

    // Eat all events once shutdown has begun.
    if(G_QuitInProgress()) return true;

    if(G_GameState() == GS_MAP)
    {
        if(ev->type == EV_FOCUS)
        {
            if(gamePauseWhenFocusLost && !ev->data1)
            {
                G_SetPause(true);
            }
            else if(gameUnpauseWhenFocusGained && ev->data1)
            {
                G_SetPause(false);
            }
            return false; // others might be interested
        }

        // With the menu active, none of these should respond to input events.
        if(!Hu_MenuIsActive() && !Hu_IsMessageActive())
        {
            if(ST_Responder(ev))
                return true;

            if(G_EventSequenceResponder(ev))
                return true;
        }
    }

    return Hu_MenuResponder(ev);
}

int G_PrivilegedResponder(event_t* ev)
{
    // Ignore all events once shutdown has begun.
    if(G_QuitInProgress()) return false;

    if(Hu_MenuPrivilegedResponder(ev))
        return true;

    // Process the screen shot key right away.
    if(devParm && ev->type == EV_KEY && ev->data1 == DDKEY_F1)
    {
        if(ev->state == EVS_DOWN)
            G_ScreenShot();
        return true; // All F1 events are eaten.
    }

    return false; // Not eaten.
}

void G_UpdateGSVarsForPlayer(player_t* pl)
{
    int i, plrnum;
    gamestate_t gameState;

    if(!pl) return;

    plrnum = pl - players;
    gameState = G_GameState();

    gsvHealth = pl->health;
#if !__JHEXEN__
    // Map stats
    gsvKills = pl->killCount;
    gsvItems = pl->itemCount;
    gsvSecrets = pl->secretCount;
#endif
        // armor
#if __JHEXEN__
    gsvArmor = FixedDiv(PCLASS_INFO(pl->class_)->autoArmorSave +
                        pl->armorPoints[ARMOR_ARMOR] +
                        pl->armorPoints[ARMOR_SHIELD] +
                        pl->armorPoints[ARMOR_HELMET] +
                        pl->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
#else
    gsvArmor = pl->armorPoints;
#endif
    // Owned keys
    for(i = 0; i < NUM_KEY_TYPES; ++i)
#if __JHEXEN__
        gsvKeys[i] = (pl->keys & (1 << i))? 1 : 0;
#else
        gsvKeys[i] = pl->keys[i];
#endif
    // current weapon
    gsvCurrentWeapon = pl->readyWeapon;

    // owned weapons
    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        gsvWeapons[i] = pl->weapons[i].owned;

#if __JHEXEN__
    // weapon pieces
    gsvWPieces[0] = (pl->pieces & WPIECE1)? 1 : 0;
    gsvWPieces[1] = (pl->pieces & WPIECE2)? 1 : 0;
    gsvWPieces[2] = (pl->pieces & WPIECE3)? 1 : 0;
    gsvWPieces[3] = (pl->pieces == 7)? 1 : 0;
#endif
    // Current ammo amounts.
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        gsvAmmo[i] = pl->ammo[i].owned;

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    // Inventory items.
    for(i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        if(pl->plr->inGame && gameState == GS_MAP)
            gsvInvItems[i] = P_InventoryCount(plrnum, IIT_FIRST + i);
        else
            gsvInvItems[i] = 0;
    }
#endif
}

void G_UpdateGSVarsForMap(void)
{
    char* name;

    // First check for a MapInfo defined map name.
    name = (char*) DD_GetVariable(DD_MAP_NAME);
    if(name)
    {
        char* ch = strchr(name, ':'); // Skip the E#M# or Map #.
        if(ch)
        {
            name = ch + 1;
            while(*name && isspace(*name)) { name++; }
        }
    }

#if __JHEXEN__
    // In Hexen the MAPINFO lump may contain a map name.
    if(!name)
    {
        name = P_GetMapName(gameMap);
    }
#endif

    // If still no name, define it as "unnamed".
    if(!name)
    {
        name = UNNAMEDMAP;
    }

    Con_SetString2("map-name", name, SVF_WRITE_OVERRIDE);
}

void G_DoQuitGame(void)
{
#define QUITWAIT_MILLISECONDS 1500

    static uint quitTime = 0;

    if(!quitInProgress)
    {
        quitInProgress = true;
        quitTime = Sys_GetRealTime();

        Hu_MenuCommand(MCMD_CLOSEFAST);

        if(!IS_NETGAME)
        {
#if __JDOOM__ || __JDOOM64__
            // Play an exit sound if it is enabled.
            if(cfg.menuQuitSound)
            {
# if __JDOOM64__
                static int quitsounds[8] = {
                    SFX_VILACT,
                    SFX_GETPOW,
                    SFX_PEPAIN,
                    SFX_SLOP,
                    SFX_SKESWG,
                    SFX_KNTDTH,
                    SFX_BSPACT,
                    SFX_SGTATK
                };
# else
                static int quitsounds[8] = {
                    SFX_PLDETH,
                    SFX_DMPAIN,
                    SFX_POPAIN,
                    SFX_SLOP,
                    SFX_TELEPT,
                    SFX_POSIT1,
                    SFX_POSIT3,
                    SFX_SGTATK
                };
                static int quitsounds2[8] = {
                    SFX_VILACT,
                    SFX_GETPOW,
                    SFX_BOSCUB,
                    SFX_SLOP,
                    SFX_SKESWG,
                    SFX_KNTDTH,
                    SFX_BSPACT,
                    SFX_SGTATK
                };

                if(gameModeBits & GM_ANY_DOOM2)
                    S_LocalSound(quitsounds2[P_Random() & 7], 0);
                else
# endif
                    S_LocalSound(quitsounds[P_Random() & 7], 0);
            }
#endif
            DD_Executef(true, "activatebcontext deui");
        }
    }

    if(Sys_GetRealTime() > quitTime + QUITWAIT_MILLISECONDS)
    {
        Sys_Quit();
    }
    else
    {
        float t = (Sys_GetRealTime() - quitTime) / (float) QUITWAIT_MILLISECONDS;
        quitDarkenOpacity = t*t*t;
    }

#undef QUITWAIT_MILLISECONDS
}

void G_QueMapMusic(uint episode, uint map)
{
#if __JHEXEN__
    /**
     * @note Kludge: Due to the way music is managed with Hexen, unless
     * we explicitly stop the current playing track the engine will not
     * change tracks. This is due to the use of the runtime-updated
     * "currentmap" definition (the engine thinks music has not changed
     * because the current Music definition is the same).
     *
     * It only worked previously was because the waiting-for-map-load
     * song was started prior to map loading.
     *
     * @todo Rethink the Music definition stuff with regard to Hexen.
     * Why not create definitions during startup by parsing MAPINFO?
     */
    S_StopMusic();
    //S_StartMusic("chess", true); // Waiting-for-map-load song
#endif
    S_MapMusic(episode, map);
    S_PauseMusic(true);
}

static void runGameAction(void)
{
    gameaction_t currentAction;

    // Do things to change the game state.
    while((currentAction = G_GameAction()) != GA_NONE)
    {
        switch(currentAction)
        {
        case GA_NEWGAME:
            G_InitNewGame();
            G_NewGame(dSkill, dEpisode, dMap, dMapEntryPoint);
            G_SetGameAction(GA_NONE);
            break;

        case GA_LOADGAME:
            G_DoLoadGame();
            break;

        case GA_SAVEGAME:
            G_DoSaveGame();
            break;

        case GA_QUIT:
            G_DoQuitGame();
            // No further game state changes occur once we have begun to quit.
            return;

        case GA_SCREENSHOT:
            G_DoScreenShot();
            G_SetGameAction(GA_NONE);
            break;

        case GA_LEAVEMAP:
            G_DoLeaveMap();
            G_SetGameAction(GA_NONE);
            break;

        case GA_RESTARTMAP:
            G_DoRestartMap();
            G_SetGameAction(GA_NONE);
            break;

        case GA_MAPCOMPLETED:
            G_DoMapCompleted();
            break;

#if __JHEXEN__
        case GA_SETMAP:
            SV_HxInitBaseSlot();
            G_NewGame(dSkill, dEpisode, dMap, dMapEntryPoint);
            G_SetGameAction(GA_NONE);
            break;
#endif

        case GA_VICTORY:
            G_SetGameAction(GA_NONE);
            break;

        default: break;
        }
    }
}

/**
 * Do needed reborns for any fallen players.
 */
static void rebornPlayers(void)
{
    int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];
        ddplayer_t* ddplr = plr->plr;
        mobj_t* mo = ddplr->mo;

        if(ddplr->inGame && plr->playerState == PST_REBORN && !P_MobjIsCamera(mo))
        {
            G_DoReborn(i);
        }

        // Player has left?
        if(plr->playerState == PST_GONE)
        {
            plr->playerState = PST_REBORN;
            if(mo)
            {
                if(!IS_CLIENT)
                {
                    P_SpawnTeleFog(mo->origin[VX], mo->origin[VY], mo->angle + ANG180);
                }

                // Let's get rid of the mobj.
#ifdef _DEBUG
                Con_Message("rebornPlayers: Removing player %i's mobj.\n", i);
#endif
                P_MobjRemove(mo, true);
                ddplr->mo = NULL;
            }
        }
    }
}

/**
 * The core of the timing loop. Game state, game actions etc occur here.
 *
 * @param ticLength     How long this tick is, in seconds.
 */
void G_Ticker(timespan_t ticLength)
{
    static gamestate_t oldGameState = -1;

    // Always tic:
    Hu_FogEffectTicker(ticLength);
    Hu_MenuTicker(ticLength);
    Hu_MsgTicker();

    if(IS_CLIENT && !Get(DD_GAME_READY)) return;

    runGameAction();

    if(!G_QuitInProgress())
    {
        // Do player reborns if needed.
        rebornPlayers();

        // Update the viewer's look angle
        //G_LookAround(CONSOLEPLAYER);

        if(!IS_CLIENT)
        {
            // Enable/disable sending of frames (delta sets) to clients.
            Set(DD_ALLOW_FRAMES, G_GameState() == GS_MAP);

            // Tell Doomsday when the game is paused (clients can't pause
            // the game.)
            Set(DD_CLIENT_PAUSED, P_IsPaused());
        }

        // Must be called on every tick.
        P_RunPlayers(ticLength);
    }
    else
    {
        if(!IS_CLIENT)
        {
            // Disable sending of frames (delta sets) to clients.
            Set(DD_ALLOW_FRAMES, false);
        }
    }

    if(G_GameState() == GS_MAP && !IS_DEDICATED)
    {
        ST_Ticker(ticLength);
    }

    // Track view window changes.
    R_ResizeViewWindow(0);

    // The following is restricted to fixed 35 Hz ticks.
    if(DD_IsSharpTick())
    {
        // Do main actions.
        switch(G_GameState())
        {
        case GS_MAP:
            // Update in-map game status cvar.
            if(oldGameState != GS_MAP)
                gsvInMap = 1;

            P_DoTick();
            HU_UpdatePsprites();

            // Active briefings once again (they were disabled when loading
            // a saved game).
            briefDisabled = false;

            if(IS_DEDICATED)
                break;

            Hu_Ticker();
            break;

        case GS_INTERMISSION:
#if __JDOOM__ || __JDOOM64__
            WI_Ticker();
#else
            IN_Ticker();
#endif
            break;

        default:
            if(oldGameState != G_GameState())
            {
                // Update game status cvars.
                gsvInMap = 0;
                Con_SetString2("map-name", NOTAMAPNAME, SVF_WRITE_OVERRIDE);
                gsvMapMusic = -1;
            }
            break;
        }

        // Update the game status cvars for player data.
        G_UpdateGSVarsForPlayer(&players[CONSOLEPLAYER]);

        // Servers will have to update player information and do such stuff.
        if(!IS_CLIENT)
            NetSv_Ticker();
    }

    oldGameState = gameState;
}

/**
 * Called when a player leaves a map.
 *
 * Jobs include; striping keys, inventory and powers from the player
 * and configuring other player-specific properties ready for the next
 * map.
 *
 * @param player        Id of the player to configure.
 */
void G_PlayerLeaveMap(int player)
{
#if __JHERETIC__ || __JHEXEN__
    uint i;
    int flightPower;
#endif
    player_t* p = &players[player];
    boolean newCluster;

#if __JHEXEN__
    newCluster = (P_GetMapCluster(gameMap) != P_GetMapCluster(nextMap));
#else
    newCluster = true;
#endif

#if __JHERETIC__ || __JHEXEN__
    // Remember if flying.
    flightPower = p->powers[PT_FLIGHT];
#endif

#if __JHERETIC__
    // Empty the inventory of excess items
    for(i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        inventoryitemtype_t type = IIT_FIRST + i;
        uint count = P_InventoryCount(player, type);

        if(count)
        {
            uint j;

            if(type != IIT_FLY)
                count--;

            for(j = 0; j < count; ++j)
                P_InventoryTake(player, type, true);
        }
    }
#endif

#if __JHEXEN__
    if(newCluster)
    {
        uint count = P_InventoryCount(player, IIT_FLY);

        for(i = 0; i < count; ++i)
            P_InventoryTake(player, IIT_FLY, true);
    }
#endif

    // Remove their powers.
    p->update |= PSF_POWERS;
    memset(p->powers, 0, sizeof(p->powers));

#if __JHEXEN__
    if(!newCluster && !deathmatch)
        p->powers[PT_FLIGHT] = flightPower; // Restore flight.
#endif

    // Remove their keys.
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    p->update |= PSF_KEYS;
    memset(p->keys, 0, sizeof(p->keys));
#else
    if(!deathmatch && newCluster)
        p->keys = 0;
#endif

    // Misc
#if __JHERETIC__
    p->rain1 = NULL;
    p->rain2 = NULL;
#endif

    // Un-morph?
#if __JHERETIC__ || __JHEXEN__
    p->update |= PSF_MORPH_TIME;
    if(p->morphTics)
    {
        p->readyWeapon = p->plr->mo->special1; // Restore weapon.
        p->morphTics = 0;
    }
#endif

    p->plr->lookDir = 0;
    p->plr->mo->flags &= ~MF_SHADOW; // Cancel invisibility.
    p->plr->extraLight = 0; // Cancel gun flashes.
    p->plr->fixedColorMap = 0; // Cancel IR goggles.

    // Clear filter.
    p->plr->flags &= ~DDPF_VIEW_FILTER;
    //p->plr->flags |= DDPF_FILTER; // Server: Send the change to the client.
    p->damageCount = 0; // No palette changes.
    p->bonusCount = 0;

#if __JHEXEN__
    p->poisonCount = 0;
#endif

    ST_LogEmpty(p - players);
}

/**
 * Safely clears the player data structures.
 */
void ClearPlayer(player_t *p)
{
    ddplayer_t *ddplayer = p->plr;
    int         playeringame = ddplayer->inGame;
    int         flags = ddplayer->flags;
    int         start = p->startSpot;
    fixcounters_t counter, acked;

    // Restore counters.
    counter = ddplayer->fixCounter;
    acked = ddplayer->fixAcked;

    memset(p, 0, sizeof(*p));
    // Restore the pointer to ddplayer.
    p->plr = ddplayer;
#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    P_InventoryEmpty(p - players);
    P_InventorySetReadyItem(p - players, IIT_NONE);
#endif
    // Also clear ddplayer.
    memset(ddplayer, 0, sizeof(*ddplayer));
    // Restore the pointer to this player.
    ddplayer->extraData = p;
    // Restore the playeringame data.
    ddplayer->inGame = playeringame;
    ddplayer->flags = flags & ~(DDPF_INTERYAW | DDPF_INTERPITCH);
    // Don't clear the start spot.
    p->startSpot = start;
    // Restore counters.
    ddplayer->fixCounter = counter;
    ddplayer->fixAcked = acked;

    ddplayer->fixCounter.angles++;
    ddplayer->fixCounter.origin++;
    ddplayer->fixCounter.mom++;
}

/**
 * Called after a player dies (almost everything is cleared and then
 * re-initialized).
 */
void G_PlayerReborn(int player)
{
    player_t       *p;
    int             frags[MAXPLAYERS];
    int             killcount, itemcount, secretcount;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    int             i;
#endif
#if __JHERETIC__
    boolean         secret = false;
    int             spot;
#elif __JHEXEN__
    uint            worldTimer;
#endif

    if(player < 0 || player >= MAXPLAYERS)
        return; // Wha?

    p = &players[player];

    assert(sizeof(p->frags) == sizeof(frags));
    memcpy(frags, p->frags, sizeof(frags));
    killcount = p->killCount;
    itemcount = p->itemCount;
    secretcount = p->secretCount;
#if __JHEXEN__
    worldTimer = p->worldTimer;
#endif

#if __JHERETIC__
    if(p->didSecret)
        secret = true;
    spot = p->startSpot;
#endif

    // Clears (almost) everything.
    ClearPlayer(p);

#if __JHERETIC__
    p->startSpot = spot;
#endif

    memcpy(p->frags, frags, sizeof(p->frags));
    p->killCount = killcount;
    p->itemCount = itemcount;
    p->secretCount = secretcount;
#if __JHEXEN__
    p->worldTimer = worldTimer;
#endif
    p->colorMap = cfg.playerColor[player];
    p->class_ = P_ClassForPlayerWhenRespawning(player, false);
#if __JHEXEN__
    if(p->class_ == PCLASS_FIGHTER && !IS_NETGAME)
    {
        // In Hexen single-player, the Fighter's default color is Yellow.
        p->colorMap = 2;
    }
#endif
    p->useDown = p->attackDown = true; // Don't do anything immediately.
    p->playerState = PST_LIVE;
    p->health = maxHealth;
    p->brain.changeWeapon = WT_NOCHANGE;

#if __JDOOM__ || __JDOOM64__
    p->readyWeapon = p->pendingWeapon = WT_SECOND;
    p->weapons[WT_FIRST].owned = true;
    p->weapons[WT_SECOND].owned = true;

    // Initalize the player's ammo counts.
    memset(p->ammo, 0, sizeof(p->ammo));
    p->ammo[AT_CLIP].owned = 50;

    // See if the Values specify anything.
    P_InitPlayerValues(p);

#elif __JHERETIC__
    p->readyWeapon = p->pendingWeapon = WT_SECOND;
    p->weapons[WT_FIRST].owned = true;
    p->weapons[WT_SECOND].owned = true;
    p->ammo[AT_CRYSTAL].owned = 50;

    if(gameMap == 8 || secret)
    {
        p->didSecret = true;
    }

#ifdef _DEBUG
    {
        int k;
        for(k = 0; k < NUM_WEAPON_TYPES; ++k)
        {
            Con_Message("Player %i owns wpn %i: %i\n", player, k, p->weapons[k].owned);
        }
    }
#endif

#else
    p->readyWeapon = p->pendingWeapon = WT_FIRST;
    p->weapons[WT_FIRST].owned = true;
    localQuakeHappening[player] = false;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Reset maxammo.
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
        p->ammo[i].max = maxAmmo[i];
#endif

    // Reset viewheight.
    p->viewHeight = cfg.plrViewHeight;
    p->viewHeightDelta = 0;

    // We'll need to update almost everything.
#if __JHERETIC__
    p->update |= PSF_VIEW_HEIGHT |
        PSF_STATE | PSF_HEALTH | PSF_ARMOR_TYPE | PSF_ARMOR_POINTS |
        PSF_INVENTORY | PSF_POWERS | PSF_KEYS | PSF_OWNED_WEAPONS | PSF_AMMO |
        PSF_MAX_AMMO | PSF_PENDING_WEAPON | PSF_READY_WEAPON | PSF_MORPH_TIME;
#else
    p->update |= PSF_REBORN;
#endif

    p->plr->flags &= ~DDPF_DEAD;
}

#if __JDOOM__ || __JDOOM64__
void G_QueueBody(mobj_t* mo)
{
    if(!mo)
        return;

    // Flush an old corpse if needed.
    if(bodyQueueSlot >= BODYQUEUESIZE)
        P_MobjRemove(bodyQueue[bodyQueueSlot % BODYQUEUESIZE], false);

    bodyQueue[bodyQueueSlot % BODYQUEUESIZE] = mo;
    bodyQueueSlot++;
}
#endif

int rebornLoadConfirmResponse(msgresponse_t response, int userValue, void* userPointer)
{
    DENG_UNUSED(userPointer);
    if(response == MSG_YES)
    {
        gaLoadGameSlot = userValue;
        G_SetGameAction(GA_LOADGAME);
    }
    else
    {
#if __JHEXEN__
        // Load the last autosave? (Not optional in Hexen).
        if(SV_IsSlotUsed(AUTO_SLOT))
        {
            gaLoadGameSlot = AUTO_SLOT;
            G_SetGameAction(GA_LOADGAME);
        }
        else
#endif
        {
            // Restart the current map, discarding all items obtained by players.
            G_SetGameAction(GA_RESTARTMAP);
        }
    }
    return true;
}

void G_DoReborn(int plrNum)
{
    // Are we still awaiting a response to a previous reborn confirmation?
    if(Hu_IsMessageActiveWithCallback(rebornLoadConfirmResponse)) return;

    if(plrNum < 0 || plrNum >= MAXPLAYERS)
        return; // Wha?

    if(IS_NETGAME)
    {
        P_RebornPlayer(plrNum);
        return;
    }

    if(G_IsLoadGamePossible())
    {
#if !__JHEXEN__
        int autoSlot = -1;
#endif
        int lastSlot = -1;

        // First ensure we have up-to-date info.
        SV_UpdateAllSaveInfo();

        // Use the latest save?
        if(cfg.loadLastSaveOnReborn)
        {
            lastSlot = Con_GetInteger("game-save-last-slot");
            if(!SV_IsSlotUsed(lastSlot)) lastSlot = -1;
        }

        // Use the latest autosave? (Not optional in Hexen).
#if !__JHEXEN__
        if(cfg.loadAutoSaveOnReborn)
        {
            autoSlot = AUTO_SLOT;
            if(!SV_IsSlotUsed(autoSlot)) autoSlot = -1;
        }
#endif

        // Have we chosen a save state to load?
        if(lastSlot >= 0
#if !__JHEXEN__
           || autoSlot >= 0
#endif
           )
        {
            // Everything appears to be in order - schedule the game-save load!
#if !__JHEXEN__
            const int chosenSlot = (lastSlot >= 0? lastSlot : autoSlot);
#else
            const int chosenSlot = lastSlot;
#endif
            if(!cfg.confirmRebornLoad)
            {
                gaLoadGameSlot = chosenSlot;
                G_SetGameAction(GA_LOADGAME);
            }
            else
            {
                // Compose the confirmation message.
                SaveInfo* info = SV_SaveInfoForSlot(chosenSlot);
                AutoStr* msg = Str_Appendf(AutoStr_NewStd(), REBORNLOAD_CONFIRM, Str_Text(SaveInfo_Name(info)));
                S_LocalSound(SFX_REBORNLOAD_CONFIRM, NULL);
                Hu_MsgStart(MSG_YESNO, Str_Text(msg), rebornLoadConfirmResponse, chosenSlot, 0);
            }
            return;
        }

        // Autosave loading cannot be disabled in Hexen.
#if __JHEXEN__
        if(SV_IsSlotUsed(AUTO_SLOT))
        {
            gaLoadGameSlot = AUTO_SLOT;
            G_SetGameAction(GA_LOADGAME);
            return;
        }
#endif
    }

    // Restart the current map, discarding all items obtained by players.
    G_SetGameAction(GA_RESTARTMAP);
}

static void G_InitNewGame(void)
{
#if __JHEXEN__
    SV_HxInitBaseSlot();
#endif

    /// @todo Do not clear this save slot. Instead we should set a game state
    ///       flag to signal when a new game should be started instead of loading
    ///       the autosave slot.
    SV_ClearSlot(AUTO_SLOT);

#if __JHEXEN__
    P_ACSInitNewGame();
#endif
}

#if __JDOOM__ || __JDOOM64__
static void G_ApplyGameRuleFastMonsters(boolean fast)
{
    static boolean oldFast = false;
    int i;

    // Only modify when the rule changes state.
    if(fast == oldFast) return;
    oldFast = fast;

    /// @fixme Kludge: Assumes the original values speed values haven't been modified!
    for(i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
        STATES[i].tics = fast? 1 : 2;
    for(i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
        STATES[i].tics = fast? 4 : 8;
    for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
        STATES[i].tics = fast? 1 : 2;
    // Kludge end.
}
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
static void G_ApplyGameRuleFastMissiles(boolean fast)
{
    static boolean oldFast = false;
    int i;

    // Only modify when the rule changes state.
    if(fast == oldFast) return;
    oldFast = fast;

    /// @fixme Kludge: Assumes the original values speed values haven't been modified!
    for(i = 0; MonsterMissileInfo[i].type != -1; ++i)
    {
        MOBJINFO[MonsterMissileInfo[i].type].speed =
            MonsterMissileInfo[i].speed[fast? 1 : 0];
    }
    // Kludge end.
}
#endif

static void G_ApplyGameRules(skillmode_t skill)
{
    if(skill < SM_BABY)
        skill = SM_BABY;
    if(skill > NUM_SKILL_MODES - 1)
        skill = NUM_SKILL_MODES - 1;
    gameSkill = skill;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(!IS_NETGAME)
    {
        deathmatch = false;
        respawnMonsters = false;
        noMonstersParm = CommandLine_Exists("-nomonsters")? true : false;
    }
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    respawnMonsters = respawnParm;
#endif

#if __JDOOM__ || __JHERETIC__
    // Is respawning enabled at all in nightmare skill?
    if(skill == SM_NIGHTMARE)
        respawnMonsters = cfg.respawnMonstersNightmare;
#endif

    // Fast monsters?
#if __JDOOM__ || __JDOOM64__
    {
        boolean fastMonsters = fastParm;
# if __JDOOM__
        if(gameSkill == SM_NIGHTMARE) fastMonsters = true;
# endif
        G_ApplyGameRuleFastMonsters(fastMonsters);
    }
#endif

    // Fast missiles?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {
        boolean fastMissiles = fastParm;
# if !__JDOOM64__
        if(gameSkill == SM_NIGHTMARE) fastMissiles = true;
# endif
        G_ApplyGameRuleFastMissiles(fastMissiles);
    }
#endif
}

void G_LeaveMap(uint newMap, uint _entryPoint, boolean _secretExit)
{
    if(IS_CLIENT || (cyclingMaps && mapCycleNoExit)) return;

#if __JHEXEN__
    if((gameMode == hexen_betademo || gameMode == hexen_demo) && newMap != DDMAXINT && newMap > 3)
    {   // Not possible in the 4-map demo.
        P_SetMessage(&players[CONSOLEPLAYER], "PORTAL INACTIVE -- DEMO", false);
        return;
    }
#endif

#if __JHEXEN__
    nextMap = newMap;
    nextMapEntryPoint = _entryPoint;
#else
    secretExit = _secretExit;
# if __JDOOM__
    // If no Wolf3D maps, no secret exit!
    if(secretExit && (gameModeBits & GM_ANY_DOOM2))
    {
        Uri* mapUri = G_ComposeMapUri(0, 30);
        AutoStr* mapPath = Uri_Compose(mapUri);
        if(!P_MapExists(Str_Text(mapPath)))
            secretExit = false;
        Uri_Delete(mapUri);
    }
# endif
#endif

    G_SetGameAction(GA_MAPCOMPLETED);
}

/**
 * @return              @c true, if the game has been completed.
 */
boolean G_IfVictory(void)
{
#if __JDOOM64__
    if(gameMap == 27)
    {
        return true;
    }
#elif __JDOOM__
    if(gameMode == doom_chex)
    {
        if(gameMap == 4)
            return true;
    }
    else if((gameModeBits & GM_ANY_DOOM) && gameMap == 7)
        return true;
#elif __JHERETIC__
    if(gameMap == 7)
    {
        return true;
    }

#elif __JHEXEN__
    if(nextMap == DDMAXINT && nextMapEntryPoint == DDMAXINT)
    {
        return true;
    }
#endif
    return false;
}

static int prepareIntermission(void* paramaters)
{
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    wmInfo.episode = gameEpisode;
    wmInfo.currentMap = gameMap;
    wmInfo.nextMap = nextMap;
    wmInfo.didSecret = players[CONSOLEPLAYER].didSecret;

# if __JDOOM__ || __JDOOM64__
    wmInfo.maxKills = totalKills;
    wmInfo.maxItems = totalItems;
    wmInfo.maxSecret = totalSecret;

    G_PrepareWIData();
# endif
#endif

#if __JDOOM__ || __JDOOM64__
    WI_Init(&wmInfo);
#elif __JHERETIC__
    IN_Init(&wmInfo);
#else /* __JHEXEN__ */
    IN_Init();
#endif
    G_ChangeGameState(GS_INTERMISSION);

    BusyMode_WorkerEnd();
    return 0;
}

void G_DoMapCompleted(void)
{
    int i;

    G_SetGameAction(GA_NONE);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            ST_AutomapOpen(i, false, true);

#if __JHERETIC__ || __JHEXEN__
            Hu_InventoryOpen(i, false);
#endif

            G_PlayerLeaveMap(i); // take away cards and stuff

            // Update this client's stats.
            NetSv_SendPlayerState(i, DDSP_ALL_PLAYERS,
                                  PSF_FRAGS | PSF_COUNTERS, true);
        }
    }

    GL_SetFilter(false);

#if __JHEXEN__
    SN_StopAllSequences();
#endif

    // Go to an intermission?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    {
    ddmapinfo_t minfo;
    Uri* mapUri = G_ComposeMapUri(gameEpisode, gameMap);
    AutoStr* mapPath = Uri_Compose(mapUri);
    if(Def_Get(DD_DEF_MAP_INFO, Str_Text(mapPath), &minfo) && (minfo.flags & MIF_NO_INTERMISSION))
    {
        Uri_Delete(mapUri);
        G_WorldDone();
        return;
    }
    Uri_Delete(mapUri);
    }

#elif __JHEXEN__
    if(!deathmatch)
    {
        G_WorldDone();
        return;
    }
#endif

    // Has the player completed the game?
    if(G_IfVictory())
    {   // Victorious!
        G_SetGameAction(GA_VICTORY);
        return;
    }

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
# if __JDOOM__
    if((gameModeBits & (GM_DOOM|GM_DOOM_SHAREWARE|GM_DOOM_ULTIMATE)) && gameMap == 8)
    {
        int i;
        for(i = 0; i < MAXPLAYERS; ++i)
            players[i].didSecret = true;
    }
# endif

    // Determine the next map.
    nextMap = G_GetNextMap(gameEpisode, gameMap, secretExit);
#endif

    // Time for an intermission.
#if __JDOOM64__
    S_StartMusic("dm2int", true);
#elif __JDOOM__
    S_StartMusic((gameModeBits & GM_ANY_DOOM2)? "dm2int" : "inter", true);
#elif __JHERETIC__
    S_StartMusic("intr", true);
#elif __JHEXEN__
    S_StartMusic("hub", true);
#endif
    S_PauseMusic(true);

    BusyMode_RunNewTask(BUSYF_TRANSITION, prepareIntermission, NULL);

#if __JHERETIC__
    // @todo is this necessary at this time?
    NetSv_SendGameState(0, DDSP_ALL_PLAYERS);
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    NetSv_Intermission(IMF_BEGIN, 0, 0);
#else /* __JHEXEN__ */
    NetSv_Intermission(IMF_BEGIN, (int) nextMap, (int) nextMapEntryPoint);
#endif

    S_PauseMusic(false);
}

#if __JDOOM__ || __JDOOM64__
void G_PrepareWIData(void)
{
    Uri* mapUri = G_ComposeMapUri(gameEpisode, gameMap);
    AutoStr* mapPath = Uri_Compose(mapUri);
    wbstartstruct_t* info = &wmInfo;
    ddmapinfo_t minfo;
    int i;

    info->maxFrags = 0;

    // See if there is a par time definition.
    if(Def_Get(DD_DEF_MAP_INFO, Str_Text(mapPath), &minfo) && minfo.parTime > 0)
        info->parTime = TICRATE * (int) minfo.parTime;
    else
        info->parTime = -1; // Unknown.

    info->pNum = CONSOLEPLAYER;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* p = &players[i];
        wbplayerstruct_t* pStats = &info->plyr[i];

        pStats->inGame = p->plr->inGame;
        pStats->kills = p->killCount;
        pStats->items = p->itemCount;
        pStats->secret = p->secretCount;
        pStats->time = mapTime;
        memcpy(pStats->frags, p->frags, sizeof(pStats->frags));
    }

    Uri_Delete(mapUri);
}
#endif

void G_WorldDone(void)
{
    ddfinale_t fin;

#if __JDOOM__ || __JDOOM64__
    if(secretExit)
        players[CONSOLEPLAYER].didSecret = true;
#endif

    // Clear the currently playing script, if any.
    FI_StackClear();

    if(G_DebriefingEnabled(gameEpisode, gameMap, &fin) &&
       G_StartFinale(fin.script, 0, FIMODE_AFTER, 0))
    {
        return;
    }

    // We have either just returned from a debriefing or there wasn't one.
    briefDisabled = false;

    G_SetGameAction(GA_LEAVEMAP);
}

typedef struct {
    const char* name;
    int slot;
} savestateworker_params_t;

static int G_SaveStateWorker(void* parameters)
{
    savestateworker_params_t* p = (savestateworker_params_t*)parameters;
    int result = SV_SaveGame(p->slot, p->name);
    BusyMode_WorkerEnd();
    return result;
}

void G_DoLeaveMap(void)
{
#if __JHEXEN__
    playerbackup_t playerBackup[MAXPLAYERS];
    boolean oldRandomClassParm;
#endif
    loadmap_params_t p;
    boolean revisit = false;
    boolean hasBrief;

    // Unpause the current game.
    sendPause = paused = false;

    // If there are any InFine scripts running, they must be stopped.
    FI_StackClear();

    // Delete raw images to conserve texture memory.
    DD_Executef(true, "texreset raw");

    // Ensure that the episode and map indices are good.
    G_ValidateMap(&gameEpisode, &nextMap);

#if __JHEXEN__
    /**
     * First, determine whether we've been to this map previously and if so,
     * whether we need to load the archived map state.
     */
    revisit = SV_HxHaveMapSaveForSlot(BASE_SLOT, nextMap);
    if(deathmatch) revisit = false;

    // Same cluster?
    if(P_GetMapCluster(gameMap) == P_GetMapCluster(nextMap))
    {
        if(!deathmatch)
        {
            // Save current map.
            SV_HxSaveClusterMap();
        }
    }
    else // Entering new cluster.
    {
        if(!deathmatch)
        {
            SV_ClearSlot(BASE_SLOT);
        }

        // Re-apply the game rules.
        /// @todo Necessary?
        G_ApplyGameRules(gameSkill);
    }

    // Take a copy of the player objects (they will be cleared in the process
    // of calling P_SetupMap() and we need to restore them after).
    SV_HxBackupPlayersInCluster(playerBackup);

    // Disable class randomization (all players must spawn as their existing class).
    oldRandomClassParm = randomClassParm;
    randomClassParm = false;

    // We don't want to see a briefing if we've already visited this map.
    if(revisit) briefDisabled = true;

    /// @todo Necessary?
    { uint i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = players + i;
        ddplayer_t* ddplr = plr->plr;

        if(!ddplr->inGame) continue;

        if(!IS_CLIENT)
        {
            // Force players to be initialized upon first map load.
            plr->playerState = PST_REBORN;
            plr->worldTimer = 0;
        }

        ST_AutomapOpen(i, false, true);
        Hu_InventoryOpen(i, false);
    }}
    //<- todo end.

    // In Hexen the RNG is re-seeded each time the map changes.
    M_ResetRandom();
#endif

#if __JHEXEN__
    gameMapEntryPoint = nextMapEntryPoint;
#else
    gameMapEntryPoint = 0;
#endif

    p.mapUri     = G_ComposeMapUri(gameEpisode, nextMap);
    p.episode    = gameEpisode;
    p.map        = nextMap;
    p.revisit    = revisit;

    hasBrief = G_BriefingEnabled(p.episode, p.map, 0);
    if(!hasBrief)
    {
        G_QueMapMusic(p.episode, p.map);
    }

    gameMap = p.map;

#if __JHEXEN__
    /// @todo It should not be necessary for the server to re-transmit the game
    ///       config at this time (needed because of G_ApplyGameRules() ?).
    NetSv_UpdateGameConfig();
#endif

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

    /// @todo Use progress bar mode and update progress during the setup.
    BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ BUSYF_TRANSITION | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                G_DoLoadMapAndMaybeStartBriefingWorker, &p, "Loading map...");
    Uri_Delete(p.mapUri);

    if(!hasBrief)
    {
        // No briefing; begin the map.
        HU_WakeWidgets(-1/* all players */);
        G_BeginMap();
    }

#if __JHEXEN__
    if(!revisit)
    {
        // First visit; destroy all freshly spawned players (??).
        P_RemoveAllPlayerMobjs();
    }

    SV_HxRestorePlayersInCluster(playerBackup, nextMapEntryPoint);

    // Restore the random class option.
    randomClassParm = oldRandomClassParm;

    // Launch waiting scripts.
    if(!deathmatch)
    {
        P_CheckACSStore(gameMap);
    }
#endif

    // In a non-network, non-deathmatch game, save immediately into the autosave slot.
    if(!IS_NETGAME && !deathmatch)
    {
        AutoStr* name = G_GenerateSaveGameName();
        savestateworker_params_t p;

        p.name = Str_Text(name);
        p.slot = AUTO_SLOT;

        /// @todo Use progress bar mode and update progress during the setup.
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    G_SaveStateWorker, &p, "Auto-Saving game...");
    }
}

void G_DoRestartMap(void)
{
#if __JHEXEN__
    // This is a restart, so we won't brief again.
    briefDisabled = true;

    // Restart the game session entirely.
    G_InitNewGame();
    G_NewGame(dSkill, dEpisode, dMap, dMapEntryPoint);
#else
    loadmap_params_t p;

    G_StopDemo();

    // Unpause the current game.
    sendPause = paused = false;

    // Delete raw images to conserve texture memory.
    DD_Executef(true, "texreset raw");

    p.mapUri     = G_ComposeMapUri(gameEpisode, gameMap);
    p.episode    = gameEpisode;
    p.map        = gameMap;
    p.revisit    = false; // Don't reload save state.

    // This is a restart, so we won't brief again.
    G_QueMapMusic(gameEpisode, gameMap);

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

    /**
     * Load the map.
     */
    if(!BusyMode_Active())
    {
        /// @todo Use progress bar mode and update progress during the setup.
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ BUSYF_TRANSITION | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    G_DoLoadMapWorker, &p, "Loading map...");
    }
    else
    {
        G_DoLoadMap(&p);
    }

    // No briefing; begin the map.
    HU_WakeWidgets(-1 /* all players */);
    G_BeginMap();

    Z_CheckHeap();
    Uri_Delete(p.mapUri);
#endif
}

#if __JHEXEN__
void G_DeferredSetMap(skillmode_t skill, uint episode, uint map, uint mapEntryPoint)
{
    dSkill = skill;
    dEpisode = episode;
    dMap = map;
    dMapEntryPoint = mapEntryPoint;

    G_SetGameAction(GA_SETMAP);
}
#endif

boolean G_IsLoadGamePossible(void)
{
    return !(IS_CLIENT && !Get(DD_PLAYBACK));
}

boolean G_LoadGame(int slot)
{
    if(!G_IsLoadGamePossible()) return false;

    // Check whether this slot is in use. We do this here also because we
    // need to provide our caller with instant feedback. Naturally this is
    // no guarantee that the game-save will be accessible come load time.

    // First ensure we have up-to-date info.
    SV_UpdateAllSaveInfo();

    if(!SV_IsSlotUsed(slot))
    {
        Con_Message("Warning:G_LoadGame: Save slot #%i is not in use, aborting load.\n", slot);
        return false;
    }

    // Everything appears to be in order - schedule the game-save load!
    gaLoadGameSlot = slot;
    G_SetGameAction(GA_LOADGAME);
    return true;
}

/**
 * Called by G_Ticker based on gameaction.
 */
void G_DoLoadGame(void)
{
#if __JHEXEN__
    boolean mustCopyBaseToAutoSlot = (gaLoadGameSlot != AUTO_SLOT);
#endif

    G_SetGameAction(GA_NONE);
    if(!SV_LoadGame(gaLoadGameSlot)) return;

#if __JHEXEN__
    if(!mustCopyBaseToAutoSlot) return;
    if(IS_NETGAME) return;

    // Copy the base slot to the autosave slot.
    SV_CopySlot(BASE_SLOT, AUTO_SLOT);
#endif
}

boolean G_IsSaveGamePossible(void)
{
    player_t* player;

    if(IS_CLIENT || Get(DD_PLAYBACK)) return false;
    if(GS_MAP != G_GameState()) return false;

    player = &players[CONSOLEPLAYER];
    if(PST_DEAD == player->playerState) return false;

    return true;
}

boolean G_SaveGame2(int slot, const char* name)
{
    if(0 > slot || slot >= NUMSAVESLOTS) return false;
    if(!G_IsSaveGamePossible()) return false;

    gaSaveGameSlot = slot;
    if(!gaSaveGameName)
        gaSaveGameName = Str_New();
    if(name && name[0])
    {
        // A new name.
        gaSaveGameGenerateName = false;
        Str_Set(gaSaveGameName, name);
    }
    else
    {
        // Reusing the current name or generating a new one.
        gaSaveGameGenerateName = (name && !name[0]);
        Str_Clear(gaSaveGameName);
    }
    G_SetGameAction(GA_SAVEGAME);
    return true;
}

boolean G_SaveGame(int slot)
{
    return G_SaveGame2(slot, NULL);
}

AutoStr* G_GenerateSaveGameName(void)
{
    AutoStr* str = AutoStr_New();
    int time = mapTime / TICRATE, hours, seconds, minutes;
    const char* baseName, *mapName;
    char baseNameBuf[256];
    AutoStr* mapPath;
    Uri* mapUri;

    hours   = time / 3600; time -= hours * 3600;
    minutes = time / 60;   time -= minutes * 60;
    seconds = time;

    mapUri = G_ComposeMapUri(gameEpisode, gameMap);
    mapPath = Uri_Compose(mapUri);

    mapName = P_GetMapNiceName();
#if __JHEXEN__
    // No map name? Try MAPINFO.
    if(!mapName)
    {
        mapName = P_GetMapName(gameMap);
    }
#endif
    // Still no map name? Use the identifier.
    // Some tricksy modders provide us with an empty map name...
    // \todo Move this logic engine-side.
    if(!mapName || !mapName[0] || mapName[0] == ' ')
    {
        mapName = Str_Text(mapPath);
    }

    baseName = NULL;
    if(P_MapIsCustom(Str_Text(mapPath)))
    {
        F_ExtractFileBase(baseNameBuf, P_MapSourceFile(Str_Text(mapPath)), 256);
        baseName = baseNameBuf;
    }

    Str_Appendf(str, "%s%s%s %02i:%02i:%02i", (baseName? baseName : ""),
        (baseName? ":" : ""), mapName, hours, minutes, seconds);

    Uri_Delete(mapUri);
    return str;
}

/**
 * Called by G_Ticker based on gameaction.
 */
void G_DoSaveGame(void)
{
    savestateworker_params_t p;
    const char* name;
    boolean didSave;

    if(gaSaveGameName && !Str_IsEmpty(gaSaveGameName))
    {
        name = Str_Text(gaSaveGameName);
    }
    else
    {
        // No name specified.
        SaveInfo* info = SV_SaveInfoForSlot(gaSaveGameSlot);
        if(!gaSaveGameGenerateName && !Str_IsEmpty(SaveInfo_Name(info)))
        {
            // Slot already in use; reuse the existing name.
            name = Str_Text(SaveInfo_Name(info));
        }
        else
        {
            name = Str_Text(G_GenerateSaveGameName());
        }
    }

    /**
     * Try to make a new game-save.
     */
    p.name = name;
    p.slot = gaSaveGameSlot;
    /// @todo Use progress bar mode and update progress during the setup.
    didSave = (BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                           G_SaveStateWorker, &p, "Saving game...") != 0);
    if(didSave)
    {
        //Hu_MenuUpdateGameSaveWidgets();
        P_SetMessage(&players[CONSOLEPLAYER], TXT_GAMESAVED, false);

        // Notify the engine that the game was saved.
        /// @todo After the engine has the primary responsibility of
        /// saving the game, this notification is unnecessary.
        Game_Notify(DD_NOTIFY_GAME_SAVED, NULL);
    }
    G_SetGameAction(GA_NONE);
}

void G_DeferredNewGame(skillmode_t skill, uint episode, uint map, uint mapEntryPoint)
{
    dSkill = skill;
    dEpisode = episode;
    dMap = map;
    dMapEntryPoint = mapEntryPoint;

    G_SetGameAction(GA_NEWGAME);
}

void G_NewGame(skillmode_t skill, uint episode, uint map, uint mapEntryPoint)
{
    uint i;

    G_StopDemo();

    // Clear the menu if open.
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // If there are any InFine scripts running, they must be stopped.
    FI_StackClear();

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = players + i;
        ddplayer_t* ddplr = plr->plr;

        if(!ddplr->inGame) continue;

        if(!IS_CLIENT)
        {
            // Force players to be initialized upon first map load.
            plr->playerState = PST_REBORN;
#if __JHEXEN__
            plr->worldTimer = 0;
#else
            plr->didSecret = false;
#endif
        }

        ST_AutomapOpen(i, false, true);
#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }

    userGame = true; // Will be set false if a demo.

    // Unpause the current game.
    sendPause = paused = false;

    // Delete raw images to conserve texture memory.
    DD_Executef(true, "texreset raw");

    // Make sure that the episode and map numbers are good.
    G_ValidateMap(&episode, &map);

    gameSkill = skill;
    gameEpisode = episode;
    gameMap = map;
    gameMapEntryPoint = mapEntryPoint;

    G_ApplyGameRules(skill);
    M_ResetRandom();

    NetSv_UpdateGameConfig();

    { loadmap_params_t p;
    boolean hasBrief;

    p.mapUri     = G_ComposeMapUri(gameEpisode, gameMap);
    p.episode    = gameEpisode;
    p.map        = gameMap;
    p.revisit    = false;

    hasBrief = G_BriefingEnabled(gameEpisode, gameMap, 0);
    if(!hasBrief)
    {
        G_QueMapMusic(gameEpisode, gameMap);
    }

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

    if(!BusyMode_Active())
    {
        /// @todo Use progress bar mode and update progress during the setup.
        BusyMode_RunNewTaskWithName(BUSYF_ACTIVITY | /*BUSYF_PROGRESS_BAR |*/ BUSYF_TRANSITION | (verbose? BUSYF_CONSOLE_OUTPUT : 0),
                                    G_DoLoadMapAndMaybeStartBriefingWorker, &p, "Loading map...");
    }
    else
    {
        G_DoLoadMapAndMaybeStartBriefing(&p);
    }

    if(!hasBrief)
    {
        // No briefing; begin the map.
        HU_WakeWidgets(-1 /* all players */);
        G_BeginMap();
    }

    Z_CheckHeap();
    Uri_Delete(p.mapUri);
    }
}

int G_QuitGameResponse(msgresponse_t response, int userValue, void* userPointer)
{
    if(response == MSG_YES)
    {
        G_SetGameAction(GA_QUIT);
    }
    return true;
}

void G_QuitGame(void)
{
    const char* endString;
    if(G_QuitInProgress()) return;

    if(Hu_IsMessageActiveWithCallback(G_QuitGameResponse))
    {
        // User has re-tried to quit with "quit" when the question is already on
        // the screen. Apparently we should quit...
        DD_Execute(true, "quit!");
        return;
    }

#if __JDOOM__ || __JDOOM64__
    endString = endmsg[((int) GAMETIC % (NUM_QUITMESSAGES + 1))];
#else
    endString = GET_TXT(TXT_QUITMSG);
#endif

#if __JDOOM__ || __JDOOM64__
    S_LocalSound(SFX_SWTCHN, NULL);
#elif __JHERETIC__
    S_LocalSound(SFX_SWITCH, NULL);
#elif __JHEXEN__
    S_LocalSound(SFX_PICKUP_KEY, NULL);
#endif

    Con_Open(false);
    Hu_MsgStart(MSG_YESNO, endString, G_QuitGameResponse, 0, NULL);
}

const char* P_GetGameModeName(void)
{
    static const char* dm   = "deathmatch";
    static const char* coop = "cooperative";
    static const char* sp   = "singleplayer";
    if(IS_NETGAME)
    {
        if(deathmatch) return dm;
        return coop;
    }
    return sp;
}

uint G_GetMapNumber(uint episode, uint map)
{
#if __JHEXEN__
    return P_TranslateMap(map);
#elif __JDOOM64__
    return map;
#else
# if __JDOOM__
    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
        return map;
    else
# endif
    {
        return map + episode * 9; // maps per episode.
    }
#endif
}

Uri* G_ComposeMapUri(uint episode, uint map)
{
    lumpname_t mapId;
#if __JDOOM64__
    dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "MAP%02u", map+1);
#elif __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "MAP%02u", map+1);
    else
        dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "E%uM%u", episode+1, map+1);
#elif  __JHERETIC__
    dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "E%uM%u", episode+1, map+1);
#else
    dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "MAP%02u", map+1);
#endif
    return Uri_NewWithPath2(mapId, RC_NULL);
}

boolean G_ValidateMap(uint* episode, uint* map)
{
    boolean ok = true;
    AutoStr* path;
    Uri* uri;

#if __JDOOM64__
    if(*map > 98)
    {
        *map = 98;
        ok = false;
    }
#elif __JDOOM__
    if(gameModeBits & (GM_DOOM_SHAREWARE|GM_DOOM_CHEX))
    {
        if(*episode != 0)
        {
            *episode = 0;
            ok = false;
        }
    }
    else
    {
        if(*episode > 8)
        {
            *episode = 8;
            ok = false;
        }
    }

    if(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX))
    {
        if(*map > 98)
        {
            *map = 98;
            ok = false;
        }
    }
    else
    {
        if(*map > 8)
        {
            *map = 8;
            ok = false;
        }
    }

#elif __JHERETIC__
    //  Allow episodes 0-8.
    if(*episode > 8)
    {
        *episode = 8;
        ok = false;
    }

    if(*map > 8)
    {
        *map = 8;
        ok = false;
    }

    if(gameMode == heretic_shareware)
    {
        if(*episode != 0)
        {
            *episode = 0;
            ok = false;
        }
    }
    else if(gameMode == heretic_extended)
    {
        if(*episode == 5)
        {
            if(*map > 2)
            {
                *map = 2;
                ok = false;
            }
        }
        else if(*episode > 4)
        {
            *episode = 4;
            ok = false;
        }
    }
    else // Registered version checks
    {
        if(*episode == 3)
        {
            if(*map != 0)
            {
                *map = 0;
                ok = false;
            }
        }
        else if(*episode > 2)
        {
            *episode = 2;
            ok = false;
        }
    }
#elif __JHEXEN__
    if(*map > 98)
    {
        *map = 98;
        ok = false;
    }
#endif

    // Check that the map truly exists.
    uri = G_ComposeMapUri(*episode, *map);
    path = Uri_Compose(uri);
    if(!P_MapExists(Str_Text(path)))
    {
        // (0,0) should exist always?
        *episode = 0;
        *map = 0;
        ok = false;
    }
    Uri_Delete(uri);

    return ok;
}

uint G_GetNextMap(uint episode, uint map, boolean secretExit)
{
#if __JHEXEN__
    return G_GetMapNumber(episode, P_GetMapNextMap(map));
#elif __JDOOM64__
    if(secretExit)
    {
        switch(map)
        {
        case 0: return 31;
        case 3: return 28;
        case 11: return 29;
        case 17: return 30;
        case 31: return 0;
        default:
            Con_Message("G_NextMap: Warning - No secret exit on map %u!", map+1);
            break;
        }
    }

    switch(map)
    {
    case 23: return 27;
    case 31: return 0;
    case 28: return 4;
    case 29: return 12;
    case 30: return 18;
    case 24: return 0;
    case 25: return 0;
    case 26: return 0;
    default:
        return map + 1;
    }
#elif __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
    {
        if(secretExit)
        {
            switch(map)
            {
            case 14: return 30;
            case 30: return 31;
            default:
               Con_Message("G_NextMap: Warning - No secret exit on map %u!", map+1);
               break;
            }
        }

        switch(map)
        {
        case 30:
        case 31: return 15;
        default:
            return map + 1;
        }
    }
    else if(gameMode == doom_chex)
    {
        return map + 1; // Go to next map.
    }
    else
    {
        if(secretExit && map != 8)
            return 8; // Go to secret map.

        switch(map)
        {
        case 8: // Returning from secret map.
            switch(episode)
            {
            case 0: return 3;
            case 1: return 5;
            case 2: return 6;
            case 3: return 2;
            default:
                Con_Error("G_NextMap: Invalid episode num #%u!", episode);
            }
            return 0; // Unreachable
        default:
            return map + 1; // Go to next map.
        }
    }
#elif __JHERETIC__
    if(secretExit && map != 8)
        return 8; // Go to secret map.

    switch(map)
    {
    case 8: // Returning from secret map.
        switch(episode)
        {
        case 0: return 6;
        case 1: return 4;
        case 2: return 4;
        case 3: return 4;
        case 4: return 3;
        default:
            Con_Error("G_NextMap: Invalid episode num #%u!", episode);
        }
        return 0; // Unreachable
    default:
        return map + 1; // Go to next map.
    }
#endif
}

#if __JHERETIC__
const char* P_GetShortMapName(uint episode, uint map)
{
    const char* name = P_GetMapName(episode, map);
    const char* ptr;

    // Skip over the "ExMx:" from the beginning.
    ptr = strchr(name, ':');
    if(!ptr)
        return name;

    name = ptr + 1;
    while(*name && isspace(*name))
        name++; // Skip any number of spaces.

    return name;
}

const char* P_GetMapName(uint episode, uint map)
{
    Uri* mapUri = G_ComposeMapUri(episode, map);
    AutoStr* mapPath = Uri_Compose(mapUri);
    ddmapinfo_t info;
    void* ptr;

    // Get the map info definition.
    if(!Def_Get(DD_DEF_MAP_INFO, Str_Text(mapPath), &info))
    {
        // There is no map information for this map...
        Uri_Delete(mapUri);
        return "";
    }
    Uri_Delete(mapUri);

    if(Def_Get(DD_DEF_TEXT, info.name, &ptr) != -1)
        return ptr;

    return info.name;
}
#endif

/**
 * Print a list of maps and the WAD files where they are from.
 */
void G_PrintFormattedMapList(uint episode, const char** files, uint count)
{
    const char* current = NULL;
    uint i, k, rangeStart = 0, len;

    for(i = 0; i < count; ++i)
    {
        if(!current && files[i])
        {
            current = files[i];
            rangeStart = i;
        }
        else if(current && (!files[i] || stricmp(current, files[i])))
        {
            // Print a range.
            len = i - rangeStart;
            Con_Printf("  "); // Indentation.
            if(len <= 2)
            {
                for(k = rangeStart; k < i; ++k)
                {
                    Uri* mapUri = G_ComposeMapUri(episode, k);
                    AutoStr* path = Uri_ToString(mapUri);
                    Con_Printf("%s%s", Str_Text(path), (k != i-1) ? "," : "");
                    Uri_Delete(mapUri);
                }
            }
            else
            {
                Uri* mapUri = G_ComposeMapUri(episode, rangeStart);
                AutoStr* path = Uri_ToString(mapUri);

                Con_Printf("%s-", Str_Text(path));
                Uri_Delete(mapUri);

                mapUri = G_ComposeMapUri(episode, i-1);
                path = Uri_ToString(mapUri);
                Con_Printf("%s", Str_Text(path));
                Uri_Delete(mapUri);
            }
            Con_Printf(": %s\n", F_PrettyPath(current));

            // Moving on to a different file.
            current = files[i];
            rangeStart = i;
        }
    }
}

/**
 * Print a list of loaded maps and which WAD files are they located in.
 * The maps are identified using the "ExMy" and "MAPnn" markers.
 */
void G_PrintMapList(void)
{
    uint episode, map, numEpisodes, maxMapsPerEpisode;
    const char* sourceList[100];

#if __JDOOM__
    if(gameMode == doom_ultimate)
    {
        numEpisodes = 4;
        maxMapsPerEpisode = 9;
    }
    else if(gameMode == doom)
    {
        numEpisodes = 3;
        maxMapsPerEpisode = 9;
    }
    else
    {
        numEpisodes = 1;
        maxMapsPerEpisode = gameMode == doom_chex? 5 : 99;
    }
#elif __JHERETIC__
    if(gameMode == heretic_extended)
        numEpisodes = 6;
    else if(gameMode == heretic)
        numEpisodes = 3;
    else
        numEpisodes = 1;
    maxMapsPerEpisode = 9;
#else
    numEpisodes = 1;
    maxMapsPerEpisode = 99;
#endif

    for(episode = 0; episode < numEpisodes; ++episode)
    {
        memset((void*) sourceList, 0, sizeof(sourceList));

        // Find the name of each map (not all may exist).
        for(map = 0; map < maxMapsPerEpisode; ++map)
        {
            Uri* uri = G_ComposeMapUri(episode, map);
            AutoStr* path = Uri_Compose(uri);
            sourceList[map] = P_MapSourceFile(Str_Text(path));
            Uri_Delete(uri);
        }
        G_PrintFormattedMapList(episode, sourceList, 99);
    }
}

/**
 * Check if there is a finale before the map.
 * Returns true if a finale was found.
 */
int G_BriefingEnabled(uint episode, uint map, ddfinale_t* fin)
{
    AutoStr* mapPath;
    Uri* mapUri;
    int result;

    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled || G_GameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
        return false;

    // Is there such a finale definition?
    mapUri = G_ComposeMapUri(episode, map);
    mapPath = Uri_Compose(mapUri);
    result = Def_Get(DD_DEF_FINALE_BEFORE, Str_Text(mapPath), fin);
    Uri_Delete(mapUri);
    return result;
}

/**
 * Check if there is a finale after the map.
 * Returns true if a finale was found.
 */
int G_DebriefingEnabled(uint episode, uint map, ddfinale_t* fin)
{
    AutoStr* mapPath;
    Uri* mapUri;
    int result;

    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled)
        return false;
#if __JHEXEN__
    if(cfg.overrideHubMsg && G_GameState() == GS_MAP &&
       !(nextMap == DDMAXINT && nextMapEntryPoint == DDMAXINT) &&
       P_GetMapCluster(map) != P_GetMapCluster(nextMap))
        return false;
#endif
    if(G_GameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
        return false;

    // Is there such a finale definition?
    mapUri = G_ComposeMapUri(episode, map);
    mapPath = Uri_Compose(mapUri);
    result = Def_Get(DD_DEF_FINALE_AFTER, Str_Text(mapPath), fin);
    Uri_Delete(mapUri);
    return result;
}

/**
 * Stops both playback and a recording. Called at critical points like
 * starting a new game, or ending the game in the menu.
 */
void G_StopDemo(void)
{
    DD_Execute(true, "stopdemo");
}

int Hook_DemoStop(int hookType, int val, void* paramaters)
{
    boolean aborted = val != 0;

    G_ChangeGameState(GS_WAITING);

    if(!aborted && singledemo)
    {   // Playback ended normally.
        G_SetGameAction(GA_QUIT);
        return true;
    }

    G_SetGameAction(GA_NONE);

    if(IS_NETGAME && IS_CLIENT)
    {
        // Restore normal game state?
        deathmatch = false;
        noMonstersParm = false;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        respawnMonsters = false;
#endif
#if __JHEXEN__
        randomClassParm = false;
#endif
    }

    {int i;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        ST_AutomapOpen(i, false, true);
#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }}
    return true;
}

void G_ScreenShot(void)
{
    G_SetGameAction(GA_SCREENSHOT);
}

/**
 * Find an unused screenshot file name. Uses the game's identity key as the
 * file name base.
 * @return  Composed file name. Must be released with Str_Delete().
 */
static ddstring_t* composeScreenshotFileName(void)
{
    GameInfo gameInfo;
    ddstring_t* name;
    int numPos;

    if(!DD_GameInfo(&gameInfo))
    {
        Con_Error("composeScreenshotFileName: Failed retrieving Game.");
        return NULL; // Unreachable.
    }

    name = Str_Appendf(Str_New(), "%s-", gameInfo.identityKey);
    numPos = Str_Length(name);
    { int i;
    for(i = 0; i < 1e6; ++i) // Stop eventually...
    {
        Str_Appendf(name, "%03i.png", i);
        if(!F_FileExists(Str_Text(name)))
            break;
        Str_Truncate(name, numPos);
    }}
    return name;
}

void G_DoScreenShot(void)
{
    ddstring_t* name = composeScreenshotFileName();
    if(!name)
    {
        Con_Message("G_DoScreenShot: Failed composing file name, screenshot not saved.\n");
        return;
    }

    if(M_ScreenShot(Str_Text(name), 24))
    {
        Con_Message("Wrote screenshot: %s\n", F_PrettyPath(Str_Text(name)));
    }
    else
    {
        Con_Message("Failed to write screenshot \"%s\".\n", F_PrettyPath(Str_Text(name)));
    }
    Str_Delete(name);
}

static void openLoadMenu(void)
{
    Hu_MenuCommand(MCMD_OPEN);
    /// @todo This should be called automatically when opening the page
    /// thus making this function redundant.
    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("LoadGame"));
}

static void openSaveMenu(void)
{
    Hu_MenuCommand(MCMD_OPEN);
    /// @todo This should be called automatically when opening the page
    /// thus making this function redundant.
    Hu_MenuUpdateGameSaveWidgets();
    Hu_MenuSetActivePage(Hu_MenuFindPageByName("SaveGame"));
}

D_CMD(OpenLoadMenu)
{
    if(!G_IsLoadGamePossible()) return false;
    openLoadMenu();
    return true;
}

D_CMD(OpenSaveMenu)
{
    if(!G_IsSaveGamePossible()) return false;
    openSaveMenu();
    return true;
}

int loadGameConfirmResponse(msgresponse_t response, int userValue, void* userPointer)
{
    if(response == MSG_YES)
    {
        const int slot = userValue;
        G_LoadGame(slot);
    }
    return true;
}

D_CMD(LoadGame)
{
    const boolean confirm = (argc == 3 && !stricmp(argv[2], "confirm"));
    int slot;

    if(G_QuitInProgress()) return false;
    if(!G_IsLoadGamePossible()) return false;

    if(IS_NETGAME)
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QLOADNET, NULL, 0, NULL);
        return false;
    }

    // Ensure we have up-to-date info.
    SV_UpdateAllSaveInfo();

    slot = SV_ParseSlotIdentifier(argv[1]);
    if(SV_IsSlotUsed(slot))
    {
        // A known used slot identifier.
        SaveInfo* info;
        AutoStr* msg;

        if(confirm || !cfg.confirmQuickGameSave)
        {
            // Try to schedule a GA_LOADGAME action.
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            return G_LoadGame(slot);
        }

        info = SV_SaveInfoForSlot(slot);
        // Compose the confirmation message.
        msg = Str_Appendf(AutoStr_NewStd(), QLPROMPT, Str_Text(SaveInfo_Name(info)));

        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_YESNO, Str_Text(msg), loadGameConfirmResponse, slot, 0);
        return true;
    }
    else if(!stricmp(argv[1], "quick") || !stricmp(argv[1], "<quick>"))
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QSAVESPOT, NULL, 0, NULL);
        return true;
    }

    // Clearly the caller needs some assistance...
    Con_Message("Failed to determine game-save slot from \"%s\"\n", argv[1]);

    // We'll open the load menu if caller is the console.
    // Reasoning: User attempted to load a named game-save however the name
    // specified didn't match anything known. Opening the load menu allows
    // the user to see the names of the known game-saves.
    if(CMDS_CONSOLE == src)
    {
        Con_Message("Opening game-save load menu...\n");
        openLoadMenu();
        return true;
    }

    // No action means the command failed.
    return false;
}

D_CMD(QuickLoadGame)
{
    /// @todo Implement console command scripts?
    return DD_Execute(true, "loadgame quick");
}

int saveGameConfirmResponse(msgresponse_t response, int userValue, void* userPointer)
{
    if(response == MSG_YES)
    {
        const int slot = userValue;
        ddstring_t* name = (ddstring_t*)userPointer;
        G_SaveGame2(slot, Str_Text(name));
        // We're done with the name.
        Str_Delete(name);
    }
    return true;
}

D_CMD(SaveGame)
{
    const boolean confirm = (argc >= 3 && !stricmp(argv[argc-1], "confirm"));
    player_t* player = &players[CONSOLEPLAYER];
    int slot;

    if(G_QuitInProgress()) return false;

    if(IS_CLIENT || IS_NETWORK_SERVER)
    {
        Con_Message("Network savegames are not supported at the moment.\n");
        return false;
    }

    if(player->playerState == PST_DEAD || Get(DD_PLAYBACK))
    {
        S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, SAVEDEAD, NULL, 0, NULL);
        return true;
    }

    if(G_GameState() != GS_MAP)
    {
        S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, SAVEOUTMAP, NULL, 0, NULL);
        return true;
    }

    // Ensure we have up-to-date info.
    SV_UpdateAllSaveInfo();

    slot = SV_ParseSlotIdentifier(argv[1]);
    if(SV_IsUserWritableSlot(slot))
    {
        // A known slot identifier.
        const boolean slotIsUsed = SV_IsSlotUsed(slot);
        SaveInfo* info = SV_SaveInfoForSlot(slot);
        ddstring_t localName, *name;
        AutoStr* msg;

        Str_InitStatic(&localName, (argc >= 3 && stricmp(argv[2], "confirm"))? argv[2] : "");
        if(!slotIsUsed || confirm || !cfg.confirmQuickGameSave)
        {
            // Try to schedule a GA_LOADGAME action.
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            return G_SaveGame2(slot, Str_Text(&localName));
        }

        // Compose the confirmation message.
        msg = Str_Appendf(AutoStr_NewStd(), QSPROMPT, Str_Text(SaveInfo_Name(info)));

        // Make a copy of the name.
        name = Str_Copy(Str_New(), &localName);

        S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
        Hu_MsgStart(MSG_YESNO, Str_Text(msg), saveGameConfirmResponse, slot, (void*)name);
        return true;
    }
    else if(!stricmp(argv[1], "quick") || !stricmp(argv[1], "<quick>"))
    {
        // No quick-save slot has been nominated - allow doing so now.
        Hu_MenuCommand(MCMD_OPEN);
        Hu_MenuUpdateGameSaveWidgets();
        Hu_MenuSetActivePage(Hu_MenuFindPageByName("SaveGame"));
        menuNominatingQuickSaveSlot = true;
        return true;
    }

    // Clearly the caller needs some assistance...
    if(!SV_IsValidSlot(slot))
        Con_Message("Failed to determine game-save slot from \"%s\".\n", argv[1]);
    else
        Con_Message("Game-save slot #%i is non-user-writable.\n", slot);

    // No action means the command failed.
    return false;
}

D_CMD(QuickSaveGame)
{
    /// @todo Implement console command scripts?
    return DD_Execute(true, "savegame quick");
}

boolean G_DeleteSaveGame(int slot)
{
    SaveInfo* info;

    if(!SV_IsUserWritableSlot(slot) || !SV_IsSlotUsed(slot)) return false;

    // A known slot identifier.
    info = SV_SaveInfoForSlot(slot);
    DENG_ASSERT(info);
    SV_ClearSlot(slot);

    if(Hu_MenuIsActive())
    {
        mn_page_t* activePage = Hu_MenuActivePage();
        if(activePage == Hu_MenuFindPageByName("LoadGame") ||
           activePage == Hu_MenuFindPageByName("SaveGame"))
        {
            // Re-open the current menu page.
            Hu_MenuUpdateGameSaveWidgets();
            Hu_MenuSetActivePage2(activePage, true);
        }
    }
    return true;
}

int deleteSaveGameConfirmResponse(msgresponse_t response, int userValue, void* userPointer)
{
    DENG_UNUSED(userPointer);
    if(response == MSG_YES)
    {
        const int slot = userValue;
        G_DeleteSaveGame(slot);
    }
    return true;
}

D_CMD(DeleteGameSave)
{
    const boolean confirm = (argc >= 3 && !stricmp(argv[argc-1], "confirm"));
    player_t* player = &players[CONSOLEPLAYER];
    int slot;

    if(G_QuitInProgress()) return false;

    // Ensure we have up-to-date info.
    SV_UpdateAllSaveInfo();

    slot = SV_ParseSlotIdentifier(argv[1]);
    if(SV_IsUserWritableSlot(slot) && SV_IsSlotUsed(slot))
    {
        // A known slot identifier.
        if(confirm)
        {
            return G_DeleteSaveGame(slot);
        }
        else
        {
            // Compose the confirmation message.
            SaveInfo* info = SV_SaveInfoForSlot(slot);
            AutoStr* msg = Str_Appendf(AutoStr_NewStd(), DELETESAVEGAME_CONFIRM, Str_Text(SaveInfo_Name(info)));
            S_LocalSound(SFX_DELETESAVEGAME_CONFIRM, NULL);
            Hu_MsgStart(MSG_YESNO, Str_Text(msg), deleteSaveGameConfirmResponse, slot, 0);
        }
        return true;
    }

    // Clearly the caller needs some assistance...
    if(!SV_IsValidSlot(slot))
        Con_Message("Failed to determine game-save slot from \"%s\".\n", argv[1]);
    else
        Con_Message("Game-save slot #%i is non-user-writable.\n", slot);

    // No action means the command failed.
    return false;
}

D_CMD(HelpScreen)
{
    G_StartHelp();
    return true;
}

D_CMD(EndGame)
{
    G_EndGame();
    return true;
}

D_CMD(CycleTextureGamma)
{
    R_CycleGammaLevel();
    return true;
}

D_CMD(ListMaps)
{
    Con_Message("Available maps:\n");
    G_PrintMapList();
    return true;
}

D_CMD(WarpMap)
{
    uint epsd, map, i;

    // Only server operators can warp maps in network games.
    /// @todo Implement vote or similar mechanics.
    if(IS_NETGAME && !IS_NETWORK_SERVER) return false;

#if __JDOOM__ || __JDOOM64__ || __JHEXEN__
# if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
# endif
    {
        // "warp M":
        epsd = 0;
        map = MAX_OF(0, atoi(argv[1]));
    }
#endif
#if __JDOOM__
    else
#endif
#if __JDOOM__ || __JHERETIC__
        if(argc == 2)
    {
        // "warp EM" or "warp M":
        int num = atoi(argv[1]);
        epsd = MAX_OF(0, num / 10);
        map  = MAX_OF(0, num % 10);
    }
    else // (argc == 3)
    {
        // "warp E M":
        epsd = MAX_OF(0, atoi(argv[1]));
        map  = MAX_OF(0, atoi(argv[2]));
    }
#endif

    // Internally epsiode and map numbers are zero-based.
    if(epsd != 0) epsd -= 1;
    if(map != 0)  map  -= 1;

    // Catch invalid maps.
#if __JHEXEN__
    // Hexen map numbers require translation.
    map = P_TranslateMapIfExists(map);
#endif
    if(!G_ValidateMap(&epsd, &map))
    {
        const char* fmtString = argc == 3? "Unknown map \"%s, %s\"." : "Unknown map \"%s%s\".";
        AutoStr* msg = Str_Appendf(AutoStr_NewStd(), fmtString, argv[1], argc == 3? argv[2] : "");
        P_SetMessage(players + CONSOLEPLAYER, Str_Text(msg), true);
        return false;
    }

#if __JHEXEN__
    // Hexen does not allow warping to the current map.
    if(userGame && map == gameMap)
    {
        P_SetMessage(players + CONSOLEPLAYER, "Cannot warp to the current map.", true);
        return false;
    }
#endif

    // Close any left open UIs.
    /// @todo Still necessary here?
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = players + i;
        ddplayer_t* ddplr = plr->plr;
        if(!ddplr->inGame) continue;

        ST_AutomapOpen(i, false, true);
#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // So be it.
#if __JHEXEN__
    if(userGame)
    {
        nextMap = map;
        nextMapEntryPoint = 0;
        briefDisabled = true;
        G_SetGameAction(GA_LEAVEMAP);
    }
    else
    {
        G_DeferredNewGame(dSkill, epsd, map, 0/*default*/);
    }
#else
    briefDisabled = true;
    G_DeferredNewGame(gameSkill, epsd, map, 0/*default*/);
#endif

    // If the command src was "us" the game library then it was probably in response to
    // the local player entering a cheat event sequence, so set the "CHANGING MAP" message.
    // Somewhat of a kludge...
    if(src == CMDS_GAME && !(IS_NETGAME && IS_SERVER))
    {
#if __JHEXEN__
        const char* msg = TXT_CHEATWARP;
        int soundId     = SFX_PLATFORM_STOP;
#elif __JHERETIC__
        const char* msg = TXT_CHEATWARP;
        int soundId     = SFX_DORCLS;
#else //__JDOOM__ || __JDOOM64__
        const char* msg = STSTR_CLEV;
        int soundId     = SFX_NONE;
#endif
        P_SetMessage(players + CONSOLEPLAYER, msg, true);
        S_LocalSound(soundId, NULL);
    }
    return true;
}
