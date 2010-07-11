/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 1999-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * g_game.c: Top-level (common) game routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <string.h>
#include <math.h>

#if __JDOOM__
#  include <stdlib.h>
#  include "jdoom.h"
#elif __JDOOM64__
#  include <stdlib.h>
#  include "jdoom64.h"
#elif __JHERETIC__
#  include <stdio.h>
#  include "jheretic.h"
#  include "p_inventory.h"
#elif __JHEXEN__
#  include "jhexen.h"
#  include "p_inventory.h"
#endif

#include "p_saveg.h"
#include "g_controls.h"
#include "g_eventsequence.h"
#include "p_mapsetup.h"
#include "p_user.h"
#include "p_actor.h"
#include "p_tick.h"
#include "am_map.h"
#include "fi_lib.h"
#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_log.h"
#include "hu_msg.h"
#include "hu_pspr.h"
#include "hu_lib.h"
#include "g_common.h"
#include "g_update.h"
#include "d_net.h"
#include "x_hair.h"
#include "p_player.h"
#include "r_common.h"
#include "p_mapspec.h"
#include "p_start.h"
#include "p_inventory.h"
#if __JHERETIC__ || __JHEXEN__
# include "hu_inventory.h"
#endif

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

DEFCC(CCmdListMaps);

void    G_PlayerReborn(int player);
void    G_InitNew(skillmode_t skill, uint episode, uint map);
void    G_DoInitNew(void);
void    G_DoReborn(int playernum);
void    G_DoLoadMap(void);
void    G_DoNewGame(void);
void    G_DoLoadGame(void);
void    G_DoPlayDemo(void);
void    G_DoMapCompleted(void);
void    G_DoVictory(void);
void    G_DoWorldDone(void);
void    G_DoSaveGame(void);
void    G_DoScreenShot(void);
boolean G_ValidateMap(uint *episode, uint *map);

#if __JHEXEN__
void    G_DoSingleReborn(void);
void    H2_PageTicker(void);
void    H2_AdvanceDemo(void);
#endif

void    G_StopDemo(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

game_config_t cfg; // The global cfg.

int debugSound; // Debug flag for displaying sound info.

skillmode_t dSkill;

skillmode_t gameSkill;
uint gameEpisode;
uint gameMap;

uint nextMap;
#if __JHEXEN__
uint nextMapEntryPoint;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean secretExit;
#endif

#if __JHEXEN__
// Position indicator for cooperative net-play reborn
uint rebornPosition;
#endif
#if __JHEXEN__
uint mapHub = 0;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
boolean respawnMonsters;
#endif

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

int saveGameSlot;
char saveDescription[MNDATA_EDIT_TEXT_MAX_LENGTH];

#if __JDOOM__ || __JDOOM64__
mobj_t *bodyQueue[BODYQUEUESIZE];
int bodyQueueSlot;
#endif

filename_t saveName;

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

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
int gsvInvItems[NUM_INVENTORYITEM_TYPES];
#endif

#if __JHEXEN__
int gsvWPieces[4];
#endif

static gamestate_t gameState = GS_STARTUP;

cvar_t gamestatusCVars[] = {
   {"game-state", READONLYCVAR, CVT_INT, &gameState, 0, 0},
   {"game-state-map", READONLYCVAR, CVT_INT, &gsvInMap, 0, 0},
   {"game-paused", READONLYCVAR, CVT_INT, &paused, 0, 0},
   {"game-skill", READONLYCVAR, CVT_INT, &gameSkill, 0, 0},

   {"map-id", READONLYCVAR, CVT_INT, &gameMap, 0, 0},
   {"map-name", READONLYCVAR, CVT_CHARPTR, &gsvMapName, 0, 0},
   {"map-episode", READONLYCVAR, CVT_INT, &gameEpisode, 0, 0},
#if __JDOOM__
   {"map-mission", READONLYCVAR, CVT_INT, &gameMission, 0, 0},
#endif
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

ccmd_t gameCmds[] = {
    { "listmaps",    "",     CCmdListMaps },
    { NULL }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint dEpisode;
static uint dMap;

#if __JHEXEN__
static int gameLoadSlot;
#endif

static gameaction_t gameAction;

// CODE --------------------------------------------------------------------

void G_Register(void)
{
    int i;

    for(i = 0; gamestatusCVars[i].name; ++i)
        Con_AddVariable(gamestatusCVars + i);

    for(i = 0; gameCmds[i].name; ++i)
        Con_AddCommand(gameCmds + i);
}

void G_SetGameAction(gameaction_t action)
{
    if(gameAction == GA_QUIT)
        return;

    if(gameAction != action)
        gameAction = action;
}

gameaction_t G_GetGameAction(void)
{
    return gameAction;
}

/**
 * Common Pre Engine Initialization routine.
 * Game-specfic pre init actions should be placed in eg D_PreInit() (for jDoom)
 */
void G_CommonPreInit(void)
{
    filename_t file;
    int i;

    // Make sure game.dll isn't newer than Doomsday...
    if(gi.version < DOOMSDAY_VERSION)
        Con_Error(GAME_NICENAME " requires at least Doomsday " DOOMSDAY_VERSION_TEXT
                  "!\n");
#ifdef TIC_DEBUG
    rndDebugfile = fopen("rndtrace.txt", "wt");
#endif

    verbose = ArgExists("-verbose");

    // Setup the players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].plr = DD_GetPlayer(i);
        players[i].plr->extraData = (void *) &players[i];
    }

    dd_snprintf(file, FILENAME_T_MAXLEN, CONFIGFILE);
    DD_SetConfigFile(file);

    dd_snprintf(file, FILENAME_T_MAXLEN, DEFSFILE);
    DD_SetDefsFile(file);

    R_SetDataPath( DATAPATH );

    Con_SetString("map-name", NOTAMAPNAME, 1);

    G_RegisterBindClasses();
    G_RegisterPlayerControls();
    P_RegisterMapObjs();

    // Add the cvars and ccmds to the console databases.
    G_ConsoleRegistration();    // Main command list.
    D_NetConsoleRegistration(); // For network.
    G_Register();               // Read-only game status cvars (for playsim).
    G_ControlRegister();        // For controls/input.
    AM_Register();              // For the automap.
    Hu_MenuRegister();          // For the menu.
    Hu_LogRegister();           // For the player message logs.
    Chat_Register();
    Hu_MsgRegister();           // For the game messages.
    ST_Register();              // For the hud/statusbar.
    X_Register();               // For the crosshair.

    DD_AddStartupWAD( STARTUPPK3 );
    G_DetectIWADs();
}

#if __JHEXEN__
/**
 * \todo all this swapping colors around is rather silly, why not simply
 * reorder the translation tables at load time?
 */
void R_GetTranslation(int plrClass, int plrColor, int* tclass, int* tmap)
{
    *tclass = 1;

    if(plrColor == 0)
        *tmap = 1;
    else if(plrColor == 1)
        *tmap = 0;
    else
        *tmap = plrColor;

    // Fighter's colors are a bit different.
    if(plrClass == PCLASS_FIGHTER && *tmap > 1)
        *tclass = 0;
}

void R_SetTranslation(mobj_t* mo)
{
    if(!(mo->flags & MF_TRANSLATION))
    {   // No translation.
        mo->tmap = mo->tclass = 0;
    }
    else
    {
        int tclass, tmap;

        tmap = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;

        if(mo->player)
        {
            tclass = 1;

            if(mo->player->class == PCLASS_FIGHTER)
            {   // Fighter's colors are a bit different.
                if(tmap == 0)
                    tmap = 2;
                else if(tmap == 2)
                    tmap = 0;
                else
                    tclass = 0;
            }

            mo->tclass = tclass;
        }
        else
            tclass = mo->special1;

        mo->tmap = tmap;
        mo->tclass = tclass;
    }
}
#endif

void R_LoadColorPalettes(void)
{
#define PALLUMPNAME         "PLAYPAL"
#define PALENTRIES          (256)
#define PALID               (0)

    lumpnum_t lump = W_GetNumForName(PALLUMPNAME);
    byte data[PALENTRIES*3];

    // Record whether we are using a custom palette.
    customPal = !W_IsFromIWAD(lump);

    W_ReadLumpSection(lump, data, 0 + PALID * (PALENTRIES * 3), PALENTRIES * 3);
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
    int i;
    byte* translationtables = (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);

    for(i = 0; i < 3 * 7; ++i)
    {
        char name[9];
        lumpnum_t lump;

        dd_snprintf(name, 9, "TRANTBL%X", i);

        if((lump = W_CheckNumForName(name)) != -1)
        {
            W_ReadLumpSection(lump, &translationtables[i * 256], 0, 256);
        }
    }
    }
#endif

#undef PALID
#undef PALENTRIES
#undef PALLUMPNAME
}

/**
 * \todo Read this information from a definition (ideally with more user
 * friendly mnemonics).
 */
void R_LoadCompositeFonts(void)
{
#if __JDOOM__
    const fontpatch_t fontIndex[] = {
        { 48, "STYSNUM0" }, // 0
        { 49, "STYSNUM1" }, // 1
        { 50, "STYSNUM2" }, // 2
        { 51, "STYSNUM3" }, // 3
        { 52, "STYSNUM4" }, // 4
        { 53, "STYSNUM5" }, // 5
        { 54, "STYSNUM6" }, // 6
        { 55, "STYSNUM7" }, // 7
        { 56, "STYSNUM8" }, // 8
        { 57, "STYSNUM9" } // 9
    };
#endif
#if __JDOOM__ || __JDOOM64__
    const fontpatch_t fontSmall[] = {
        { 37, "WIPCNT" }, // %
        { 45, "WIMINUS" }, // -
        { 48, "WINUM0" }, // 0
        { 49, "WINUM1" }, // 1
        { 50, "WINUM2" }, // 2
        { 51, "WINUM3" }, // 3
        { 52, "WINUM4" }, // 4
        { 53, "WINUM5" }, // 5
        { 54, "WINUM6" }, // 6
        { 55, "WINUM7" }, // 7
        { 56, "WINUM8" }, // 8
        { 57, "WINUM9" }, // 9
        { 58, "WICOLON" }, // :
    };
    const fontpatch_t fontStatus[] = {
        { 37, "STTPRCNT" }, // %
        { 45, "STTMINUS" }, // -
        { 48, "STTNUM0" }, // 0
        { 49, "STTNUM1" }, // 1
        { 50, "STTNUM2" }, // 2
        { 51, "STTNUM3" }, // 3
        { 52, "STTNUM4" }, // 4
        { 53, "STTNUM5" }, // 5
        { 54, "STTNUM6" }, // 6
        { 55, "STTNUM7" }, // 7
        { 56, "STTNUM8" }, // 8
        { 57, "STTNUM9" } // 9
    };
    const fontpatch_t fontA[] = {
        { 32, "STCFN032" }, // ' '
        { 33, "STCFN033" }, // !
        { 34, "STCFN034" }, // "
        { 35, "STCFN035" }, // #
        { 36, "STCFN036" }, // $
        { 37, "STCFN037" }, // %
        { 38, "STCFN038" }, // &
        { 39, "STCFN039" }, // '
        { 40, "STCFN040" }, // (
        { 41, "STCFN041" }, // )
        { 42, "STCFN042" }, // *
        { 43, "STCFN043" }, // +
        { 44, "STCFN044" }, // ,
        { 45, "STCFN045" }, // -
        { 46, "STCFN046" }, // .
        { 47, "STCFN047" }, // /
        { 48, "STCFN048" }, // 0
        { 49, "STCFN049" }, // 1
        { 50, "STCFN050" }, // 2
        { 51, "STCFN051" }, // 3
        { 52, "STCFN052" }, // 4
        { 53, "STCFN053" }, // 5
        { 54, "STCFN054" }, // 6
        { 55, "STCFN055" }, // 7
        { 56, "STCFN056" }, // 8
        { 57, "STCFN057" }, // 9
        { 58, "STCFN058" }, // :
        { 59, "STCFN059" }, // ;
        { 60, "STCFN060" }, // <
        { 61, "STCFN061" }, // =
        { 62, "STCFN062" }, // >
        { 63, "STCFN063" }, // ?
        { 64, "STCFN064" }, // @
        { 65, "STCFN065" }, // A
        { 66, "STCFN066" }, // B
        { 67, "STCFN067" }, // C
        { 68, "STCFN068" }, // D
        { 69, "STCFN069" }, // E
        { 70, "STCFN070" }, // F
        { 71, "STCFN071" }, // G
        { 72, "STCFN072" }, // H
        { 73, "STCFN073" }, // I
        { 74, "STCFN074" }, // J
        { 75, "STCFN075" }, // K
        { 76, "STCFN076" }, // L
        { 77, "STCFN077" }, // M
        { 78, "STCFN078" }, // N
        { 79, "STCFN079" }, // O
        { 80, "STCFN080" }, // P
        { 81, "STCFN081" }, // Q
        { 82, "STCFN082" }, // R
        { 83, "STCFN083" }, // S
        { 84, "STCFN084" }, // T
        { 85, "STCFN085" }, // U
        { 86, "STCFN086" }, // V
        { 87, "STCFN087" }, // W
        { 88, "STCFN088" }, // X
        { 89, "STCFN089" }, // Y
        { 90, "STCFN090" }, // Z
        { 91, "STCFN091" }, // [
        { 92, "STCFN092" }, // '\'
        { 93, "STCFN093" }, // ]
        { 94, "STCFN094" }, // ^
        { 95, "STCFN095" }, // _
        { 96, "STCFN121" }, // `
        { 97, "STCFN065" }, // a
        { 98, "STCFN066" }, // b
        { 99, "STCFN067" }, // c
        { 100, "STCFN068" }, // d
        { 101, "STCFN069" }, // e
        { 102, "STCFN070" }, // f
        { 103, "STCFN071" }, // g
        { 104, "STCFN072" }, // h
        { 105, "STCFN073" }, // i
        { 106, "STCFN074" }, // j
        { 107, "STCFN075" }, // k
        { 108, "STCFN076" }, // l
        { 109, "STCFN077" }, // m
        { 110, "STCFN078" }, // n
        { 111, "STCFN079" }, // o
        { 112, "STCFN080" }, // p
        { 113, "STCFN081" }, // q
        { 114, "STCFN082" }, // r
        { 115, "STCFN083" }, // s
        { 116, "STCFN084" }, // t
        { 117, "STCFN085" }, // u
        { 118, "STCFN086" }, // v
        { 119, "STCFN087" }, // w
        { 120, "STCFN088" }, // x
        { 121, "STCFN089" }, // y
        { 122, "STCFN090" } // z
    };
    const fontpatch_t fontB[] = {
        { 32, "FONTB032" }, // ' '
        { 33, "FONTB033" }, // !
        { 34, "FONTB034" }, // "
        { 35, "FONTB035" }, // #
        { 36, "FONTB036" }, // $
        { 37, "FONTB037" }, // %
        { 38, "FONTB038" }, // &
        { 39, "FONTB039" }, // '
        { 40, "FONTB040" }, // (
        { 41, "FONTB041" }, // )
        { 42, "FONTB042" }, // *
        { 43, "FONTB043" }, // +
        { 44, "FONTB044" }, // ,
        { 45, "FONTB045" }, // -
        { 46, "FONTB046" }, // .
        { 47, "FONTB047" }, // /
        { 48, "FONTB048" }, // 0
        { 49, "FONTB049" }, // 1
        { 50, "FONTB050" }, // 2
        { 51, "FONTB051" }, // 3
        { 52, "FONTB052" }, // 4
        { 53, "FONTB053" }, // 5
        { 54, "FONTB054" }, // 6
        { 55, "FONTB055" }, // 7
        { 56, "FONTB056" }, // 8
        { 57, "FONTB057" }, // 9
        { 58, "FONTB058" }, // :
        { 59, "FONTB059" }, // ;
        { 60, "FONTB060" }, // <
        { 61, "FONTB061" }, // =
        { 62, "FONTB062" }, // >
        { 63, "FONTB063" }, // ?
        { 64, "FONTB064" }, // @
        { 65, "FONTB065" }, // A
        { 66, "FONTB066" }, // B
        { 67, "FONTB067" }, // C
        { 68, "FONTB068" }, // D
        { 69, "FONTB069" }, // E
        { 70, "FONTB070" }, // F
        { 71, "FONTB071" }, // G
        { 72, "FONTB072" }, // H
        { 73, "FONTB073" }, // I
        { 74, "FONTB074" }, // J
        { 75, "FONTB075" }, // K
        { 76, "FONTB076" }, // L
        { 77, "FONTB077" }, // M
        { 78, "FONTB078" }, // N
        { 79, "FONTB079" }, // O
        { 80, "FONTB080" }, // P
        { 81, "FONTB081" }, // Q
        { 82, "FONTB082" }, // R
        { 83, "FONTB083" }, // S
        { 84, "FONTB084" }, // T
        { 85, "FONTB085" }, // U
        { 86, "FONTB086" }, // V
        { 87, "FONTB087" }, // W
        { 88, "FONTB088" }, // X
        { 89, "FONTB089" }, // Y
        { 90, "FONTB090" }, // Z
        { 97, "FONTB065" }, // a
        { 98, "FONTB066" }, // b
        { 99, "FONTB067" }, // c
        { 100, "FONTB068" }, // d
        { 101, "FONTB069" }, // e
        { 102, "FONTB070" }, // f
        { 103, "FONTB071" }, // g
        { 104, "FONTB072" }, // h
        { 105, "FONTB073" }, // i
        { 106, "FONTB074" }, // j
        { 107, "FONTB075" }, // k
        { 108, "FONTB076" }, // l
        { 109, "FONTB077" }, // m
        { 110, "FONTB078" }, // n
        { 111, "FONTB079" }, // o
        { 112, "FONTB080" }, // p
        { 113, "FONTB081" }, // q
        { 114, "FONTB082" }, // r
        { 115, "FONTB083" }, // s
        { 116, "FONTB084" }, // t
        { 117, "FONTB085" }, // u
        { 118, "FONTB086" }, // v
        { 119, "FONTB087" }, // w
        { 120, "FONTB088" }, // x
        { 121, "FONTB089" }, // y
        { 122, "FONTB090" } // z
    };
#else // __JHERETIC__ || __JHEXEN__
    const fontpatch_t fontStatus[] = {
        { 45, "NEGNUM" }, // -
        { 48, "IN0" }, // 0
        { 49, "IN1" }, // 1
        { 50, "IN2" }, // 2
        { 51, "IN3" }, // 3
        { 52, "IN4" }, // 4
        { 53, "IN5" }, // 5
        { 54, "IN6" }, // 6
        { 55, "IN7" }, // 7
        { 56, "IN8" }, // 8
        { 57, "IN9" }, // 9
    };
    // Heretic/Hexen don't use ASCII numbered font patches
    // plus they don't even have a full set eg '!' = 1 '_'= 58 !!!
    const fontpatch_t fontA[] = {
        { 32, "FONTA00" }, // ' '
        { 33, "FONTA01" }, // !
        { 34, "FONTA02" }, // "
        { 35, "FONTA03" }, // #
        { 36, "FONTA04" }, // $
        { 37, "FONTA05" }, // %
        { 38, "FONTA06" }, // &
        { 39, "FONTA07" }, // '
        { 40, "FONTA08" }, // (
        { 41, "FONTA09" }, // )
        { 42, "FONTA10" }, // *
        { 43, "FONTA11" }, // +
        { 44, "FONTA12" }, // ,
        { 45, "FONTA13" }, // -
        { 46, "FONTA14" }, // .
        { 47, "FONTA15" }, // /
        { 48, "FONTA16" }, // 0
        { 49, "FONTA17" }, // 1
        { 50, "FONTA18" }, // 2
        { 51, "FONTA19" }, // 3
        { 52, "FONTA20" }, // 4
        { 53, "FONTA21" }, // 5
        { 54, "FONTA22" }, // 6
        { 55, "FONTA23" }, // 7
        { 56, "FONTA24" }, // 8
        { 57, "FONTA25" }, // 9
        { 58, "FONTA26" }, // :
        { 59, "FONTA27" }, // ;
        { 60, "FONTA28" }, // <
        { 61, "FONTA29" }, // =
        { 62, "FONTA30" }, // >
        { 63, "FONTA31" }, // ?
        { 64, "FONTA32" }, // @
        { 65, "FONTA33" }, // A
        { 66, "FONTA34" }, // B
        { 67, "FONTA35" }, // C
        { 68, "FONTA36" }, // D
        { 69, "FONTA37" }, // E
        { 70, "FONTA38" }, // F
        { 71, "FONTA39" }, // G
        { 72, "FONTA40" }, // H
        { 73, "FONTA41" }, // I
        { 74, "FONTA42" }, // J
        { 75, "FONTA43" }, // K
        { 76, "FONTA44" }, // L
        { 77, "FONTA45" }, // M
        { 78, "FONTA46" }, // N
        { 79, "FONTA47" }, // O
        { 80, "FONTA48" }, // P
        { 81, "FONTA49" }, // Q
        { 82, "FONTA50" }, // R
        { 83, "FONTA51" }, // S
        { 84, "FONTA52" }, // T
        { 85, "FONTA53" }, // U
        { 86, "FONTA54" }, // V
        { 87, "FONTA55" }, // W
        { 88, "FONTA56" }, // X
        { 89, "FONTA57" }, // Y
        { 90, "FONTA58" }, // Z
        { 91, "FONTA63" }, // [
        { 92, "FONTA60" }, // '\'
        { 93, "FONTA61" }, // ]
        { 94, "FONTA62" }, // ^
        { 95, "FONTA59" }, // _
        { 97, "FONTA33" }, // a
        { 98, "FONTA34" }, // b
        { 99, "FONTA35" }, // c
        { 100, "FONTA36" }, // d
        { 101, "FONTA37" }, // e
        { 102, "FONTA38" }, // f
        { 103, "FONTA39" }, // g
        { 104, "FONTA40" }, // h
        { 105, "FONTA41" }, // i
        { 106, "FONTA42" }, // j
        { 107, "FONTA43" }, // k
        { 108, "FONTA44" }, // l
        { 109, "FONTA45" }, // m
        { 110, "FONTA46" }, // n
        { 111, "FONTA47" }, // o
        { 112, "FONTA48" }, // p
        { 113, "FONTA49" }, // q
        { 114, "FONTA50" }, // r
        { 115, "FONTA51" }, // s
        { 116, "FONTA52" }, // t
        { 117, "FONTA53" }, // u
        { 118, "FONTA54" }, // v
        { 119, "FONTA55" }, // w
        { 120, "FONTA56" }, // x
        { 121, "FONTA57" }, // y
        { 122, "FONTA58" } // z
    };
    const fontpatch_t fontB[] = {
        { 32, "FONTB00" }, // ' '
        { 33, "FONTB01" }, // !
        { 34, "FONTB02" }, // "
        { 35, "FONTB03" }, // #
        { 36, "FONTB04" }, // $
        { 37, "FONTB05" }, // %
        { 38, "FONTB06" }, // &
        { 39, "FONTB07" }, // '
        { 40, "FONTB08" }, // (
        { 41, "FONTB09" }, // )
        { 42, "FONTB10" }, // *
        { 43, "FONTB11" }, // +
        { 44, "FONTB12" }, // ,
        { 45, "FONTB13" }, // -
        { 46, "FONTB14" }, // .
        { 47, "FONTB15" }, // /
        { 48, "FONTB16" }, // 0
        { 49, "FONTB17" }, // 1
        { 50, "FONTB18" }, // 2
        { 51, "FONTB19" }, // 3
        { 52, "FONTB20" }, // 4
        { 53, "FONTB21" }, // 5
        { 54, "FONTB22" }, // 6
        { 55, "FONTB23" }, // 7
        { 56, "FONTB24" }, // 8
        { 57, "FONTB25" }, // 9
        { 58, "FONTB26" }, // :
        { 59, "FONTB27" }, // ;
        { 60, "FONTB28" }, // <
        { 61, "FONTB29" }, // =
        { 62, "FONTB30" }, // >
        { 63, "FONTB31" }, // ?
        { 64, "FONTB32" }, // @
        { 65, "FONTB33" }, // A
        { 66, "FONTB34" }, // B
        { 67, "FONTB35" }, // C
        { 68, "FONTB36" }, // D
        { 69, "FONTB37" }, // E
        { 70, "FONTB38" }, // F
        { 71, "FONTB39" }, // G
        { 72, "FONTB40" }, // H
        { 73, "FONTB41" }, // I
        { 74, "FONTB42" }, // J
        { 75, "FONTB43" }, // K
        { 76, "FONTB44" }, // L
        { 77, "FONTB45" }, // M
        { 78, "FONTB46" }, // N
        { 79, "FONTB47" }, // O
        { 80, "FONTB48" }, // P
        { 81, "FONTB49" }, // Q
        { 82, "FONTB50" }, // R
        { 83, "FONTB51" }, // S
        { 84, "FONTB52" }, // T
        { 85, "FONTB53" }, // U
        { 86, "FONTB54" }, // V
        { 87, "FONTB55" }, // W
        { 88, "FONTB56" }, // X
        { 89, "FONTB57" }, // Y
        { 90, "FONTB58" }, // Z
        { 91, "FONTB59" }, // [
        { 92, "FONTB60" }, // '\'
        { 93, "FONTB61" }, // ]
        { 94, "FONTB62" }, // ^
        { 95, "FONTB63" }, // _
        { 97, "FONTB33" }, // a
        { 98, "FONTB34" }, // b
        { 99, "FONTB35" }, // c
        { 100, "FONTB36" }, // d
        { 101, "FONTB37" }, // e
        { 102, "FONTB38" }, // f
        { 103, "FONTB39" }, // g
        { 104, "FONTB40" }, // h
        { 105, "FONTB41" }, // i
        { 106, "FONTB42" }, // j
        { 107, "FONTB43" }, // k
        { 108, "FONTB44" }, // l
        { 109, "FONTB45" }, // m
        { 110, "FONTB46" }, // n
        { 111, "FONTB47" }, // o
        { 112, "FONTB48" }, // p
        { 113, "FONTB49" }, // q
        { 114, "FONTB50" }, // r
        { 115, "FONTB51" }, // s
        { 116, "FONTB52" }, // t
        { 117, "FONTB53" }, // u
        { 118, "FONTB54" }, // v
        { 119, "FONTB55" }, // w
        { 120, "FONTB56" }, // x
        { 121, "FONTB57" }, // y
        { 122, "FONTB58" } // z
    };
    const fontpatch_t fontSmallIn[] = {
        { 48, "SMALLIN0" }, // 0
        { 49, "SMALLIN1" }, // 1
        { 50, "SMALLIN2" }, // 2
        { 51, "SMALLIN3" }, // 3
        { 52, "SMALLIN4" }, // 4
        { 53, "SMALLIN5" }, // 5
        { 54, "SMALLIN6" }, // 6
        { 55, "SMALLIN7" }, // 7
        { 56, "SMALLIN8" }, // 8
        { 57, "SMALLIN9" }, // 9
    };
#endif

    R_NewCompositeFont(GF_FONTA, "a", fontA, sizeof(fontA) / sizeof(fontA[0]));
    R_NewCompositeFont(GF_FONTB, "b", fontB, sizeof(fontB) / sizeof(fontB[0]));
    R_NewCompositeFont(GF_STATUS, "status", fontStatus, sizeof(fontStatus) / sizeof(fontStatus[0]));
#if __JDOOM__
    R_NewCompositeFont(GF_INDEX, "index", fontIndex, sizeof(fontIndex) / sizeof(fontIndex[0]));
#endif
#if __JDOOM__ || __JDOOM64__
    R_NewCompositeFont(GF_SMALL, "small", fontSmall, sizeof(fontSmall) / sizeof(fontSmall[0]));
#endif
#if __JHERETIC__ || __JHEXEN__
    R_NewCompositeFont(GF_SMALLIN, "smallin", fontSmallIn, sizeof(fontSmallIn) / sizeof(fontSmallIn[0]));
#endif
}

/**
 * \todo Read this information from a definition (ideally with more user
 * friendly mnemonics).
 */
void R_LoadVectorGraphics(void)
{
#define R (1.0f)
    const vgline_t keysquare[] = {
        { {0, 0}, {R / 4, -R / 2} },
        { {R / 4, -R / 2}, {R / 2, -R / 2} },
        { {R / 2, -R / 2}, {R / 2, R / 2} },
        { {R / 2, R / 2}, {R / 4, R / 2} },
        { {R / 4, R / 2}, {0, 0} }, // Handle part type thing.
        { {0, 0}, {-R, 0} }, // Stem.
        { {-R, 0}, {-R, -R / 2} }, // End lockpick part.
        { {-3 * R / 4, 0}, {-3 * R / 4, -R / 4} }
    };
    const vgline_t thintriangle_guy[] = {
        { {-R / 2, R - R / 2}, {R, 0} }, // >
        { {R, 0}, {-R / 2, -R + R / 2} },
        { {-R / 2, -R + R / 2}, {-R / 2, R - R / 2} } // |>
    };
#if __JDOOM__ || __JDOOM64__
    const vgline_t player_arrow[] = {
        { {-R + R / 8, 0}, {R, 0} }, // -----
        { {R, 0}, {R - R / 2, R / 4} }, // ----->
        { {R, 0}, {R - R / 2, -R / 4} },
        { {-R + R / 8, 0}, {-R - R / 8, R / 4} }, // >---->
        { {-R + R / 8, 0}, {-R - R / 8, -R / 4} },
        { {-R + 3 * R / 8, 0}, {-R + R / 8, R / 4} }, // >>--->
        { {-R + 3 * R / 8, 0}, {-R + R / 8, -R / 4} }
    };
    const vgline_t cheat_player_arrow[] = {
        { {-R + R / 8, 0}, {R, 0} }, // -----
        { {R, 0}, {R - R / 2, R / 6} }, // ----->
        { {R, 0}, {R - R / 2, -R / 6} },
        { {-R + R / 8, 0}, {-R - R / 8, R / 6} }, // >----->
        { {-R + R / 8, 0}, {-R - R / 8, -R / 6} },
        { {-R + 3 * R / 8, 0}, {-R + R / 8, R / 6} }, // >>----->
        { {-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6} },
        { {-R / 2, 0}, {-R / 2, -R / 6} }, // >>-d--->
        { {-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6} },
        { {-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4} },
        { {-R / 6, 0}, {-R / 6, -R / 6} }, // >>-dd-->
        { {-R / 6, -R / 6}, {0, -R / 6} },
        { {0, -R / 6}, {0, R / 4} },
        { {R / 6, R / 4}, {R / 6, -R / 7} }, // >>-ddt->
        { {R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32} },
        { {R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7} }
    };
#elif __JHERETIC__
    const vgline_t player_arrow[] = {
        { {-R + R / 4, 0}, {0, 0} }, // center line.
        { {-R + R / 4, R / 8}, {R, 0} }, // blade
        { {-R + R / 4, -R / 8}, {R, 0} },
        { {-R + R / 4, -R / 4}, {-R + R / 4, R / 4} }, // crosspiece
        { {-R + R / 8, -R / 4}, {-R + R / 8, R / 4} },
        { {-R + R / 8, -R / 4}, {-R + R / 4, -R / 4} }, //crosspiece connectors
        { {-R + R / 8, R / 4}, {-R + R / 4, R / 4} },
        { {-R - R / 4, R / 8}, {-R - R / 4, -R / 8} }, // pommel
        { {-R - R / 4, R / 8}, {-R + R / 8, R / 8} },
        { {-R - R / 4, -R / 8}, {-R + R / 8, -R / 8} }
    };
    const vgline_t cheat_player_arrow[] = {
        { {-R + R / 8, 0}, {R, 0} }, // -----
        { {R, 0}, {R - R / 2, R / 6} }, // ----->
        { {R, 0}, {R - R / 2, -R / 6} },
        { {-R + R / 8, 0}, {-R - R / 8, R / 6} }, // >----->
        { {-R + R / 8, 0}, {-R - R / 8, -R / 6} },
        { {-R + 3 * R / 8, 0}, {-R + R / 8, R / 6} }, // >>----->
        { {-R + 3 * R / 8, 0}, {-R + R / 8, -R / 6} },
        { {-R / 2, 0}, {-R / 2, -R / 6} }, // >>-d--->
        { {-R / 2, -R / 6}, {-R / 2 + R / 6, -R / 6} },
        { {-R / 2 + R / 6, -R / 6}, {-R / 2 + R / 6, R / 4} },
        { {-R / 6, 0}, {-R / 6, -R / 6} }, // >>-dd-->
        { {-R / 6, -R / 6}, {0, -R / 6} },
        { {0, -R / 6}, {0, R / 4} },
        { {R / 6, R / 4}, {R / 6, -R / 7} }, // >>-ddt->
        { {R / 6, -R / 7}, {R / 6 + R / 32, -R / 7 - R / 32} },
        { {R / 6 + R / 32, -R / 7 - R / 32}, {R / 6 + R / 10, -R / 7} }
    };
#elif __JHEXEN__
    const vgline_t player_arrow[] = {
        { {-R + R / 4, 0}, {0, 0} }, // center line.
        { {-R + R / 4, R / 8}, {R, 0} }, // blade
        { {-R + R / 4, -R / 8}, {R, 0} },
        { {-R + R / 4, -R / 4}, {-R + R / 4, R / 4} }, // crosspiece
        { {-R + R / 8, -R / 4}, {-R + R / 8, R / 4} },
        { {-R + R / 8, -R / 4}, {-R + R / 4, -R / 4} }, // crosspiece connectors
        { {-R + R / 8, R / 4}, {-R + R / 4, R / 4} },
        { {-R - R / 4, R / 8}, {-R - R / 4, -R / 8} }, // pommel
        { {-R - R / 4, R / 8}, {-R + R / 8, R / 8} },
        { {-R - R / 4, -R / 8}, {-R + R / 8, -R / 8} }
    };
#endif
#undef R
    const vgline_t crossHair1[] = { // + (open center)
        { {-1,  0}, {-.4f, 0} },
        { { 0, -1}, { 0,  -.4f} },
        { { 1,  0}, { .4f, 0} },
        { { 0,  1}, { 0,   .4f} }
    };
    const vgline_t crossHair2[] = { // > <
        { {-1, -.714f}, {-.286f, 0} },
        { {-1,  .714f}, {-.286f, 0} },
        { { 1, -.714f}, { .286f, 0} },
        { { 1,  .714f}, { .286f, 0} }
    };
    const vgline_t crossHair3[] = { // square
        { {-1, -1}, {-1,  1} },
        { {-1,  1}, { 1,  1} },
        { { 1,  1}, { 1, -1} },
        { { 1, -1}, {-1, -1} }
    };
    const vgline_t crossHair4[] = { // square (open center)
        { {-1, -1}, {-1, -.5f} },
        { {-1, .5f}, {-1, 1} },
        { {-1, 1}, {-.5f, 1} },
        { {.5f, 1}, {1, 1} },
        { { 1, 1}, {1, .5f} },
        { { 1, -.5f}, {1, -1} },
        { { 1, -1}, {.5f, -1} },
        { {-.5f, -1}, {-1, -1} }
    };
    const vgline_t crossHair5[] = { // diamond
        { { 0, -1}, { 1,  0} },
        { { 1,  0}, { 0,  1} },
        { { 0,  1}, {-1,  0} },
        { {-1,  0}, { 0, -1} }
    };
    const vgline_t crossHair6[] = { // ^
        { {-1, -1}, { 0,  0} },
        { { 0,  0}, { 1, -1} }
    };

    R_NewVectorGraphic(VG_KEYSQUARE, keysquare, sizeof(keysquare) / sizeof(keysquare[0]));
    R_NewVectorGraphic(VG_TRIANGLE, thintriangle_guy, sizeof(thintriangle_guy) / sizeof(thintriangle_guy[0]));
    R_NewVectorGraphic(VG_ARROW, player_arrow, sizeof(player_arrow) / sizeof(player_arrow[0]));
#if !__JHEXEN__
    R_NewVectorGraphic(VG_CHEATARROW, cheat_player_arrow, sizeof(cheat_player_arrow) / sizeof(cheat_player_arrow[0]));
#endif
    R_NewVectorGraphic(VG_XHAIR1, crossHair1, sizeof(crossHair1) / sizeof(crossHair1[0]));
    R_NewVectorGraphic(VG_XHAIR2, crossHair2, sizeof(crossHair2) / sizeof(crossHair2[0]));
    R_NewVectorGraphic(VG_XHAIR3, crossHair3, sizeof(crossHair3) / sizeof(crossHair3[0]));
    R_NewVectorGraphic(VG_XHAIR4, crossHair4, sizeof(crossHair4) / sizeof(crossHair4[0]));
    R_NewVectorGraphic(VG_XHAIR5, crossHair5, sizeof(crossHair5) / sizeof(crossHair5[0]));
    R_NewVectorGraphic(VG_XHAIR6, crossHair6, sizeof(crossHair6) / sizeof(crossHair6[0]));
}

void R_InitRefresh(void)
{
    VERBOSE(Con_Message("R_InitRefresh: Loading data for referesh.\n"))

    R_LoadColorPalettes();
    R_LoadCompositeFonts();
    R_LoadVectorGraphics();

    {
    float mul = 1.4f;
    DD_SetVariable(DD_PSPRITE_LIGHTLEVEL_MULTIPLIER, &mul);
    }
}

/**
 * Common Post Engine Initialization routine.
 * Game-specific post init actions should be placed in eg D_PostInit()
 * (for jDoom) and NOT here.
 */
void G_CommonPostInit(void)
{
    VERBOSE(G_PrintMapList());

    GUI_Init();
    R_InitRefresh();

    // Init the save system and create the game save directory
    SV_Init();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_ReadTypes();
    XG_Register(); // Register XG classnames.
#endif

    R_SetBorderGfx(borderLumps);

    Con_Message("P_Init: Init Playloop state.\n");
    P_Init();

    Con_Message("Hu_LoadData: Setting up heads up display.\n");
    Hu_LoadData();
#if __JHERETIC__ || __JHEXEN__
    Hu_InventoryInit();
#endif

    Con_Message("ST_Init: Init status bar.\n");
    ST_Init();

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    Cht_Init();
#endif

    Con_Message("Hu_MenuInit: Init miscellaneous info.\n");
    Hu_MenuInit();
    Hu_MsgInit();

    // From this point on, the shortcuts are always active.
    DD_Execute(true, "activatebcontext shortcut");

    Con_Message("AM_Init: Init automap.\n");
    AM_Init();

    // Create the various line lists (spechits, anims, buttons etc).
    spechit = P_CreateIterList();
    linespecials = P_CreateIterList();
}

/**
 * Retrieve the current game state.
 *
 * @return              The current game state.
 */
gamestate_t G_GetGameState(void)
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

    if(!Hu_MenuIsActive())
    {
        // Any key/button down pops up menu if in demos.
        if((G_GetGameAction() == GA_NONE && !singledemo && Get(DD_PLAYBACK)) ||
           (G_GetGameState() == GS_INFINE && !FI_IsMenuTrigger()))
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

    if(G_GetGameAction() == GA_QUIT)
        return;

    if(state < 0 || state >= NUM_GAME_STATES)
        Con_Error("G_ChangeGameState: Invalid state %i.\n", (int) state);

    if(gameState != state)
    {
#if _DEBUG
// Log gamestate changes in debug builds, with verbose.
VERBOSE(Con_Message("G_ChangeGameState: New state %s.\n",
                    getGameStateStr(state)));
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

static void initFinaleConditions(finale_conditions_t* cons)
{
    memset(cons, 0, sizeof(*cons));

    // Only the server is able to figure out the truth values of all the conditions
    // (clients use the server-provided presets).
    if(!IS_SERVER)
        return;

#if __JHEXEN__
    cons->secret = false;

    // Current hub has been completed?
    cons->leavehub = (P_GetMapCluster(gameMap) != P_GetMapCluster(nextMap));
#else
    cons->secret = secretExit;
    // Only Hexen has hubs.
    cons->leavehub = false;
#endif
}

static void startFinale(const char* script, finale_mode_t mode)
{
    finale_conditions_t c, *cons = &c;
    gamestate_t prevGameState = G_GetGameState();

    G_SetGameAction(GA_NONE);

    initFinaleConditions(&c);
    if(mode != FIMODE_OVERLAY)
    {
        G_ChangeGameState(GS_INFINE);
    }

    FI_ScriptBegin(script, mode, prevGameState, IS_SERVER? cons : 0);
}

boolean G_StartFinale2(const char* script, finale_mode_t mode)
{
    startFinale(script, mode);
    return true;
}

boolean G_StartFinale(const char* finaleName, finale_mode_t mode)
{
    void* script;

    if(Def_Get(DD_DEF_FINALE, finaleName, &script))
    {
        startFinale(script, mode);
        return true;
    }

    Con_Message("G_StartFinale: Warning, script \"%s\" not defined.\n", finaleName);
    return false;
}

/**
 * Begin the titlescreen animation sequence.
 */
void G_StartTitle(void)
{
    G_StopDemo();
    userGame = false;

    // The title script must always be defined.
    if(!G_StartFinale("title", FIMODE_LOCAL))
    {
        Con_Error("G_StartTitle: A title script must be defined.");
    }
}

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
/**
 * Begin the helpscreen animation sequence.
 */
void G_StartHelp(void)
{
    Hu_MenuCommand(MCMD_CLOSEFAST);
    G_StartFinale("help", FIMODE_LOCAL);
}
#endif

void G_DoLoadMap(void)
{
    int i;
    char* lname, *ptr;
    ddfinale_t fin;
    boolean hasBrief;

#if __JHEXEN__
    static int firstFragReset = 1;
#endif

    mapStartTic = (int) GAMETIC; // Fr time calculation.

    // If we're the server, let clients know the map will change.
    NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t           *plr = &players[i];

        if(plr->plr->inGame && plr->playerState == PST_DEAD)
            plr->playerState = PST_REBORN;

#if __JHEXEN__
        if(!IS_NETGAME || (IS_NETGAME != 0 && deathmatch != 0) ||
            firstFragReset == 1)
        {
            memset(plr->frags, 0, sizeof(plr->frags));
            firstFragReset = 0;
        }
#else
        memset(plr->frags, 0, sizeof(plr->frags));
#endif
    }

#if __JHEXEN__
    SN_StopAllSequences();
#endif

    // Set all player mobjs to NULL, clear control state toggles etc.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].plr->mo = NULL;
        G_ResetLookOffset(i);
    }

    // Determine whether there is a briefing to run before the map starts
    // (played after the map has been loaded).
    hasBrief = G_BriefingEnabled(gameEpisode, gameMap, &fin);
    if(!hasBrief)
    {
#if __JHEXEN__
        /**
         * \kludge Due to the way music is managed with Hexen, unless we
         * explicitly stop the current playing track the engine will not
         * change tracks. This is due to the use of the runtime-updated
         * "currentmap" definition (the engine thinks music has not changed
         * because the current Music definition is the same).
         *
         * The only reason it worked previously was because the
         * waiting-for-map-load song was started prior to load.
         *
         * \todo Rethink the Music definition stuff with regard to Hexen.
         * Why not create definitions during startup by parsing MAPINFO?
         */
        S_StopMusic();
        //S_StartMusic("chess", true); // Waiting-for-map-load song
#endif
        S_MapMusic(gameEpisode, gameMap);
        S_PauseMusic(true);
    }

    P_SetupMap(gameEpisode, gameMap, 0, gameSkill);
    Set(DD_DISPLAYPLAYER, CONSOLEPLAYER); // View the guy you are playing.
    G_SetGameAction(GA_NONE);
    nextMap = 0;

    Z_CheckHeap();

    // Clear cmd building stuff.
    G_ResetMousePos();

    sendPause = paused = false;

    G_ControlReset(-1); // Clear all controls for all local players.

    // Set the game status cvar for map name.
    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    if(lname)
    {
        ptr = strchr(lname, ':'); // Skip the E#M# or Map #.
        if(ptr)
        {
            lname = ptr + 1;
            while(*lname && isspace(*lname))
                lname++;
        }
    }

#if __JHEXEN__
    // In jHexen we can look in the MAPINFO for the map name
    if(!lname)
        lname = P_GetMapName(gameMap);
#endif

    // Set the map name
    // If still no name, call it unnamed.
    if(!lname)
    {
        Con_SetString("map-name", UNNAMEDMAP, 1);
    }
    else
    {
        Con_SetString("map-name", lname, 1);
    }

    // Start a briefing, if there is one.
    if(hasBrief)
    {
        G_StartFinale2(fin.script, FIMODE_BEFORE);
    }
    else // No briefing, start the map.
    {
        G_ChangeGameState(GS_MAP);
        S_PauseMusic(false);
    }
}

/**
 * Get info needed to make ticcmd_ts for the players.
 * Return false if the event should be checked for bindings.
 */
boolean G_Responder(event_t *ev)
{
    if(G_GetGameAction() == GA_QUIT)
        return false; // Eat all events once shutdown has begun.

    // With the menu active, none of these should respond to input events.
    if(G_GetGameState() == GS_MAP && !Hu_MenuIsActive() && !Hu_IsMessageActive())
    {
        // Try the chatmode responder.
        if(Chat_Responder(ev))
            return true;

#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
        // Check for cheats.
        if(G_EventSequenceResponder(ev))
            return true;
#endif
    }

    // Try a menu object responder.
    if(Hu_MenuObjectResponder(ev))
        return true;

    // We may wish to eat the event depending on type...
    if(G_AdjustControlState(ev))
        return true;

    // The event wasn't used.
    return false;
}

/**
 * Updates the game status cvars based on game and player data.
 * Called each tick by G_Ticker().
 */
void G_UpdateGSVarsForPlayer(player_t* pl)
{
    int                 i, plrnum;
    gamestate_t         gameState;

    if(!pl)
        return;

    plrnum = pl - players;
    gameState = G_GetGameState();

    gsvHealth = pl->health;
#if !__JHEXEN__
    // Map stats
    gsvKills = pl->killCount;
    gsvItems = pl->itemCount;
    gsvSecrets = pl->secretCount;
#endif
        // armor
#if __JHEXEN__
    gsvArmor = FixedDiv(PCLASS_INFO(pl->class)->autoArmorSave +
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

static void runGameAction(void)
{
    if(G_GetGameAction() == GA_QUIT)
    {
#define QUITWAIT_MILLISECONDS 1500

        static uint quitTime = 0;

        if(quitTime == 0)
        {
            quitTime = Sys_GetRealTime();

            Hu_MenuCommand(MCMD_CLOSEFAST);

            if(!IS_NETGAME)
            {
#if __JDOOM__ || __JDOOM64__
                // Play an exit sound if it is enabled.
                if(cfg.menuQuitSound)
                {
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
# if __JDOOM64__
                        SFX_PEPAIN,
# else
                        SFX_BOSCUB,
# endif
                        SFX_SLOP,
                        SFX_SKESWG,
                        SFX_KNTDTH,
                        SFX_BSPACT,
                        SFX_SGTATK
                    };

                    if(gameMode == commercial)
                        S_LocalSound(quitsounds2[P_Random() & 7], NULL);
                    else
                        S_LocalSound(quitsounds[P_Random() & 7], NULL);
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

        // No game state changes occur once we have begun to quit.
        return;

#undef QUITWAIT_MILLISECONDS
    }

    // Do things to change the game state.
    {gameaction_t currentAction;
    while((currentAction = G_GetGameAction()) != GA_NONE)
    {
        switch(currentAction)
        {
#if __JHEXEN__ || __JSTRIFE__
        case GA_INITNEW:
            G_DoInitNew();
            break;

        case GA_SINGLEREBORN:
            G_DoSingleReborn();
            break;
#endif

        case GA_LEAVEMAP:
            G_DoWorldDone();
            break;

        case GA_LOADMAP:
            G_DoLoadMap();
            break;

        case GA_NEWGAME:
            G_DoNewGame();
            break;

        case GA_LOADGAME:
            G_DoLoadGame();
            break;

        case GA_SAVEGAME:
            G_DoSaveGame();
            break;

        case GA_MAPCOMPLETED:
            G_DoMapCompleted();
            break;

        case GA_VICTORY:
            G_SetGameAction(GA_NONE);
            break;

        case GA_SCREENSHOT:
            G_DoScreenShot();
            G_SetGameAction(GA_NONE);
            break;

        case GA_NONE:
        default:
            break;
        }
    }}
}

/**
 * The core of the timing loop. Game state, game actions etc occur here.
 *
 * @param ticLength     How long this tick is, in seconds.
 */
void G_Ticker(timespan_t ticLength)
{
    static gamestate_t oldGameState = -1;
    static trigger_t fixed = {1.0 / TICSPERSEC};

    int i;

    // Always tic:
    Hu_FogEffectTicker(ticLength);
    Hu_MenuTicker(ticLength);
    Hu_MsgTicker(ticLength);

    if(IS_CLIENT && !Get(DD_GAME_READY))
        return;

    // Do player reborns if needed.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = &players[i];

        if(plr->plr->inGame && plr->playerState == PST_REBORN &&
           !P_MobjIsCamera(plr->plr->mo))
            G_DoReborn(i);

        // Player has left?
        if(plr->playerState == PST_GONE)
        {
            plr->playerState = PST_REBORN;
            if(plr->plr->mo)
            {
                if(!IS_CLIENT)
                {
                    P_SpawnTeleFog(plr->plr->mo->pos[VX],
                                   plr->plr->mo->pos[VY],
                                   plr->plr->mo->angle + ANG180);
                }

                // Let's get rid of the mobj.
#ifdef _DEBUG
Con_Message("G_Ticker: Removing player %i's mobj.\n", i);
#endif
                P_MobjRemove(plr->plr->mo, true);
                plr->plr->mo = NULL;
            }
        }
    }

    runGameAction();

    if(G_GetGameAction() != GA_QUIT)
    {
        // Update the viewer's look angle
        //G_LookAround(CONSOLEPLAYER);

        if(!IS_CLIENT)
        {
            // Enable/disable sending of frames (delta sets) to clients.
            Set(DD_ALLOW_FRAMES, G_GetGameState() == GS_MAP);

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

    if(G_GetGameState() == GS_MAP && !IS_DEDICATED)
    {
        ST_Ticker(ticLength);
        AM_Ticker(ticLength);
    }

    // Update view window size.
    R_ViewWindowTicker(ticLength);

    // The following is restricted to fixed 35 Hz ticks.
    if(M_RunTrigger(&fixed, ticLength))
    {
        // Do main actions.
        switch(G_GetGameState())
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
            if(oldGameState != G_GetGameState())
            {
                // Update game status cvars.
                gsvInMap = 0;
                Con_SetString("map-name", NOTAMAPNAME, 1);
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

#if __JHEXEN__ || __JSTRIFE__
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
    p->plr->flags |= DDPF_FILTER; // Server: Send the change to the client.
    p->damageCount = 0; // No palette changes.
    p->bonusCount = 0;

#if __JHEXEN__
    p->poisonCount = 0;
#endif

    Hu_LogEmpty(p - players);
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
    ddplayer->fixCounter.pos++;
    ddplayer->fixCounter.mom++;

/*    ddplayer->fixAcked.angles =
        ddplayer->fixAcked.pos =
        ddplayer->fixAcked.mom = -1;
#ifdef _DEBUG
    Con_Message("ClearPlayer: fixacked set to -1 (counts:%i, %i, %i)\n",
                ddplayer->fixCounter.angles,
                ddplayer->fixCounter.pos,
                ddplayer->fixCounter.mom);
#endif*/
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
#elif __JHEXEN__ || __JSTRIFE__
    uint            worldTimer;
#endif

    if(player < 0 || player >= MAXPLAYERS)
        return; // Wha?

    p = &players[player];

    memcpy(frags, p->frags, sizeof(frags));
    killcount = p->killCount;
    itemcount = p->itemCount;
    secretcount = p->secretCount;
#if __JHEXEN__ || __JSTRIFE__
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
#if __JHEXEN__ || __JSTRIFE__
    p->worldTimer = worldTimer;
    p->colorMap = cfg.playerColor[player];
#endif
#if __JHEXEN__
    p->class = cfg.playerClass[player];
#endif
    p->useDown = p->attackDown = true; // Don't do anything immediately.
    p->playerState = PST_LIVE;
    p->health = maxHealth;

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

    // We'll need to update almost everything.
#if __JHERETIC__
    p->update |=
        PSF_STATE | PSF_HEALTH | PSF_ARMOR_TYPE | PSF_ARMOR_POINTS |
        PSF_INVENTORY | PSF_POWERS | PSF_KEYS | PSF_OWNED_WEAPONS | PSF_AMMO |
        PSF_MAX_AMMO | PSF_PENDING_WEAPON | PSF_READY_WEAPON;
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

void G_DoReborn(int plrNum)
{
    if(plrNum < 0 || plrNum >= MAXPLAYERS)
        return; // Wha?

    // Clear the currently playing script, if any.
    FI_Reset();

    if(!IS_NETGAME)
    {
        // We've just died, don't do a briefing now.
        briefDisabled = true;

#if __JHEXEN__
        if(SV_HxRebornSlotAvailable())
        {   // Use the reborn code if the slot is available
            G_SetGameAction(GA_SINGLEREBORN);
        }
        else
        {   // Start a new game if there's no reborn info
            G_SetGameAction(GA_NEWGAME);
        }
#else
        // Reload the map from scratch.
        G_SetGameAction(GA_LOADMAP);
#endif
    }
    else
    {   // In a net game.
        P_RebornPlayer(plrNum);
    }
}

#if __JHEXEN__
void G_StartNewInit(void)
{
    SV_HxInitBaseSlot();
    SV_HxClearRebornSlot();

    P_ACSInitNewGame();

    // Default the player start spot group to 0
    rebornPosition = 0;
}

void G_StartNewGame(skillmode_t skill)
{
    G_StartNewInit();
    G_InitNew(skill, 0, P_TranslateMap(0));
}
#endif

/**
 * Leave the current map and start intermission routine.
 * (if __JHEXEN__ the intermission will only be displayed when exiting a
 * hub and in DeathMatch games)
 *
 * @param newMap        ID of the map we are entering.
 * @param _entryPoint   Entry point on the new map.
 * @param secretExit
 */
void G_LeaveMap(uint newMap, uint _entryPoint, boolean _secretExit)
{
    if(cyclingMaps && mapCycleNoExit)
        return;

#if __JHEXEN__
    if(shareware && newMap != DDMAXINT && newMap > 3)
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
      if(secretExit && (gameMode == commercial) && !P_MapExists(0, 30))
          secretExit = false;
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
    if((gameMap == 7) && (gameMode != commercial))
    {
        return true;
    }

#elif __JHERETIC__
    if(gameMap == 7)
    {
        return true;
    }

#elif __JHEXEN__ || __JSTRIFE__
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

    Con_BusyWorkerEnd();
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
            AM_Open(AM_MapForPlayer(i), false, true);

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
    char levid[8];

    P_MapId(gameEpisode, gameMap, levid);

    if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && (minfo.flags & MIF_NO_INTERMISSION))
    {
        G_WorldDone();
        return;
    }
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
    if(gameMode != commercial && gameMap == 8)
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
    S_StartMusic(gameMode == commercial? "dm2int" : "inter", true);
#elif __JHERETIC__
    S_StartMusic("intr", true);
#elif __JHEXEN__
    S_StartMusic("hub", true);
#endif
    S_PauseMusic(true);

    Con_Busy(BUSYF_TRANSITION, NULL, prepareIntermission, NULL);

#if __JHERETIC__
    // @fixme is this necessary at this time?
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
    int             i;
    ddmapinfo_t     minfo;
    char            levid[8];
    wbstartstruct_t *info = &wmInfo;

    info->maxFrags = 0;

    P_MapId(gameEpisode, gameMap, levid);

    // See if there is a par time definition.
    if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && minfo.parTime > 0)
        info->parTime = TICRATE * (int) minfo.parTime;
    else
        info->parTime = -1; // Unknown.

    info->pNum = CONSOLEPLAYER;
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t       *p = &players[i];
        wbplayerstruct_t *pStats = &info->plyr[i];

        pStats->inGame = p->plr->inGame;
        pStats->kills = p->killCount;
        pStats->items = p->itemCount;
        pStats->secret = p->secretCount;
        pStats->time = mapTime;
        memcpy(pStats->frags, p->frags, sizeof(pStats->frags));
    }
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
    FI_Reset();

    if(G_DebriefingEnabled(gameEpisode, gameMap, &fin) &&
       G_StartFinale(fin.script, FIMODE_AFTER))
    {
        return;
    }

    // We have either just returned from a debriefing or there wasn't one.
    briefDisabled = false;

    G_SetGameAction(GA_LEAVEMAP);
}

void G_DoWorldDone(void)
{
#if __JHEXEN__
    SV_MapTeleport(nextMap, nextMapEntryPoint);
    rebornPosition = nextMapEntryPoint;
#else
# if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    gameMap = nextMap;
# endif

    G_DoLoadMap();
#endif

    G_SetGameAction(GA_NONE);
}

#if __JHEXEN__
/**
 * Called by G_Ticker based on gameaction.  Loads a game from the reborn
 * save slot.
 */
void G_DoSingleReborn(void)
{
    G_SetGameAction(GA_NONE);
    SV_LoadGame(SV_HxGetRebornSlot());
}
#endif

/**
 * Can be called by the startup code or the menu task.
 */
#if __JHEXEN__ || __JSTRIFE__
void G_LoadGame(int slot)
{
    gameLoadSlot = slot;
    G_SetGameAction(GA_LOADGAME);
}
#else
void G_LoadGame(const char* name)
{
    M_TranslatePath(saveName, name, FILENAME_T_MAXLEN);
    G_SetGameAction(GA_LOADGAME);
}
#endif

/**
 * Called by G_Ticker based on gameaction.
 */
void G_DoLoadGame(void)
{
    G_StopDemo();
    FI_Reset();
    G_SetGameAction(GA_NONE);

#if __JHEXEN__
    SV_LoadGame(gameLoadSlot);
    if(!IS_NETGAME)
    {                           // Copy the base slot to the reborn slot
        SV_HxUpdateRebornSlot();
    }
#else
    SV_LoadGame(saveName);
#endif
}

/**
 * Called by the menu task.
 *
 * @param description       A 24 byte text string.
 */
void G_SaveGame(int slot, const char* description)
{
    saveGameSlot = slot;
    strncpy(saveDescription, description, MNDATA_EDIT_TEXT_MAX_LENGTH);
    G_SetGameAction(GA_SAVEGAME);
}

/**
 * Called by G_Ticker based on gameaction.
 */
void G_DoSaveGame(void)
{
#if __JHEXEN__
    SV_SaveGame(saveGameSlot, saveDescription);
#else
    filename_t name;

    SV_GetSaveGameFileName(name, saveGameSlot, FILENAME_T_MAXLEN);
    SV_SaveGame(name, saveDescription);
#endif

    G_SetGameAction(GA_NONE);
    saveDescription[0] = 0;

    P_SetMessage(&players[CONSOLEPLAYER], TXT_GAMESAVED, false);
}

#if __JHEXEN__ || __JSTRIFE__
void G_DeferredNewGame(skillmode_t skill)
{
    dSkill = skill;
    G_SetGameAction(GA_NEWGAME);
}

void G_DoInitNew(void)
{
    SV_HxInitBaseSlot();
    G_InitNew(dSkill, dEpisode, dMap);
    G_SetGameAction(GA_NONE);
}
#endif

/**
 * Can be called by the startup code or the menu task, CONSOLEPLAYER,
 * DISPLAYPLAYER, playeringame[] should be set.
 */
void G_DeferedInitNew(skillmode_t skill, uint episode, uint map)
{
    dSkill = skill;
    dEpisode = episode;
    dMap = map;

#if __JHEXEN__ || __JSTRIFE__
    G_SetGameAction(GA_INITNEW);
#else
    G_SetGameAction(GA_NEWGAME);
#endif
}

void G_DoNewGame(void)
{
    G_StopDemo();
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(!IS_NETGAME)
    {
        deathmatch = false;
        respawnMonsters = false;
        noMonstersParm = ArgExists("-nomonsters")? true : false;
    }
    G_InitNew(dSkill, dEpisode, dMap);
#else
    G_StartNewGame(dSkill);
#endif
    G_SetGameAction(GA_NONE);
}

/**
 * Start a new game.
 */
void G_InitNew(skillmode_t skill, uint episode, uint map)
{
    int i;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    int speed;
#endif

    // Close any open automaps.
    for(i = 0; i < MAXPLAYERS; ++i)
        if(players[i].plr->inGame)
            AM_Open(AM_MapForPlayer(i), false, true);

    // If there are any InFine scripts running, they must be stopped.
    FI_Reset();

    if(paused)
    {
        paused = false;
    }

    if(skill < SM_BABY)
        skill = SM_BABY;
    if(skill > NUM_SKILL_MODES - 1)
        skill = NUM_SKILL_MODES - 1;

    // Make sure that the episode and map numbers are good.
    G_ValidateMap(&episode, &map);

    M_ResetRandom();

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__ || __JSTRIFE__
    respawnMonsters = respawnParm;
#endif

#if __JDOOM__ || __JHERETIC__
    // Is respawning enabled at all in nightmare skill?
    if(skill == SM_NIGHTMARE)
        respawnMonsters = cfg.respawnMonstersNightmare;
#endif

//// \kludge Doom/Heretic Fast Monters/Missiles
#if __JDOOM__ || __JDOOM64__
    // Fast monsters?
    if(fastParm
# if __JDOOM__
        || (skill == SM_NIGHTMARE && gameSkill != SM_NIGHTMARE)
# endif
        )
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
            STATES[i].tics = 1;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
            STATES[i].tics = 4;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
            STATES[i].tics = 1;
    }
    else
    {
        for(i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
            STATES[i].tics = 2;
        for(i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
            STATES[i].tics = 8;
        for(i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
            STATES[i].tics = 2;
    }
#endif

    // Fast missiles?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
# if __JDOOM64__
    speed = fastParm;
# elif __JDOOM__
    speed = (fastParm || (skill == SM_NIGHTMARE && gameSkill != SM_NIGHTMARE));
# else
    speed = skill == SM_NIGHTMARE;
# endif

    for(i = 0; MonsterMissileInfo[i].type != -1; ++i)
    {
        MOBJINFO[MonsterMissileInfo[i].type].speed =
            MonsterMissileInfo[i].speed[speed];
    }
#endif
// <-- KLUDGE

    if(!IS_CLIENT)
    {
        // Force players to be initialized upon first map load.
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            player_t           *plr = &players[i];

            plr->playerState = PST_REBORN;
#if __JHEXEN__ || __JSTRIFE__
            plr->worldTimer = 0;
#else
            plr->didSecret = false;
#endif
        }
    }

    userGame = true; // Will be set false if a demo.
    paused = false;
    gameEpisode = episode;
    gameMap = map;
    gameSkill = skill;

    NetSv_UpdateGameConfig();

    G_DoLoadMap();

#if __JHEXEN__
    // Initialize the sky.
    P_InitSky(map);
#endif
}

/**
 * Return the index of this map.
 */
uint G_GetMapNumber(uint episode, uint map)
{
#if __JHEXEN__ || __JSTRIFE__
    return P_TranslateMap(map);
#elif __JDOOM64__
    return map;
#else
  #if __JDOOM__
    if(gameMode == commercial)
        return map;
    else
  #endif
    {
        return map + episode * 9; // maps per episode.
    }
#endif
}

/**
 * Compose the name of the map lump identifier.
 */
void P_MapId(uint episode, uint map, char* lumpName)
{
#if __JDOOM64__
    sprintf(lumpName, "MAP%02u", map+1);
#elif __JDOOM__
    if(gameMode == commercial)
        sprintf(lumpName, "MAP%02u", map+1);
    else
        sprintf(lumpName, "E%uM%u", episode+1, map+1);
#elif  __JHERETIC__
    sprintf(lumpName, "E%uM%u", episode+1, map+1);
#else
    sprintf(lumpName, "MAP%02u", map+1);
#endif
}

/**
 * return               @c true if the specified map is present.
 */
boolean P_MapExists(uint episode, uint map)
{
    char buf[9];
    P_MapId(episode, map, buf);
    return W_CheckNumForName(buf) >= 0;
}

/**
 * return               Name of the source file containing the map if present, else 0.
 */
const char* P_MapSourceFile(uint episode, uint map)
{
    lumpnum_t lump;
    char buf[9];
    P_MapId(episode, map, buf);
    if((lump = W_CheckNumForName(buf)) >= 0)
        return W_LumpSourceFile(lump);
    return 0;
}

/**
 * Returns true if the specified (episode, map) pair can be used.
 * Otherwise the values are adjusted so they are valid.
 */
boolean G_ValidateMap(uint* episode, uint* map)
{
    boolean ok = true;

#if __JDOOM64__
    if(*map > 98)
    {
        *map = 98;
        ok = false;
    }
#elif __JDOOM__
    if(gameMode == shareware)
    {
        // only start episode 0 on shareware
        if(*episode != 0)
        {
            *episode = 0;
            ok = false;
        }
    }
    else
    {
        // Allow episodes 0-8.
        if(*episode > 8)
        {
            *episode = 8;
            ok = false;
        }
    }

    if(gameMode == commercial)
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

    if(gameMode == shareware) // Shareware version checks
    {
        if(*episode != 0)
        {
            *episode = 0;
            ok = false;
        }
    }
    else if(gameMode == extended) // Extended version checks
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
#elif __JHEXEN__ || __JSTRIFE__
    if(*map > 98)
    {
        *map = 98;
        ok = false;
    }
#endif

    // Check that the map truly exists.
    if(!P_MapExists(*episode, *map))
    {
        // (0,0) should exist always?
        *episode = 0;
        *map = 0;
        ok = false;
    }

    return ok;
}

/**
 * Return the next map according to the default map progression.
 *
 * @param episode       Current episode.
 * @param map           Current map.
 * @param secretExit
 * @return              The next map.
 */
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
    if(gameMode == commercial)
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
    char                id[10], *ptr;
    ddmapinfo_t         info;

    // Compose the map identifier.
    P_MapId(episode, map, id);

    // Get the map info definition.
    if(!Def_Get(DD_DEF_MAP_INFO, id, &info))
    {
        // There is no map information for this map...
        return "";
    }

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
    char mapId[9];

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
                    P_MapId(episode, k, mapId);
                    Con_Printf("%s%s", mapId, k != i ? "," : "");
                }
            }
            else
            {
                P_MapId(episode, rangeStart, mapId);
                Con_Printf("%s-", mapId);
                P_MapId(episode, i-1, mapId);
                Con_Printf("%s", mapId);
            }
            Con_Printf(": %s\n", M_PrettyPath(current));

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
    if(gameMode == registered)
    {
        numEpisodes = 3;
        maxMapsPerEpisode = 9;
    }
    else if(gameMode == retail)
    {
        numEpisodes = 4;
        maxMapsPerEpisode = 9;
    }
    else
    {
        numEpisodes = 1;
        maxMapsPerEpisode = 99;
    }
#elif __JHERETIC__
    if(gameMode == extended)
        numEpisodes = 6;
    else if(gameMode == registered)
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
        for(map = 0; map < maxMapsPerEpisode-1; ++map)
        {
            sourceList[map] = P_MapSourceFile(episode, map);
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
    char mid[20];

    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled || G_GetGameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
        return false;

    // Is there such a finale definition?
    P_MapId(episode, map, mid);

    return Def_Get(DD_DEF_FINALE_BEFORE, mid, fin);
}

/**
 * Check if there is a finale after the map.
 * Returns true if a finale was found.
 */
int G_DebriefingEnabled(uint episode, uint map, ddfinale_t* fin)
{
    char mid[20];

    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled)
        return false;
#if __JHEXEN__
    if(cfg.overrideHubMsg && G_GetGameState() == GS_MAP &&
       !(nextMap == DDMAXINT && nextMapEntryPoint == DDMAXINT) &&
       P_GetMapCluster(map) != P_GetMapCluster(nextMap))
        return false;
#endif
    if(G_GetGameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
        return false;

    // Is there such a finale definition?
    P_MapId(episode, map, mid);
    return Def_Get(DD_DEF_FINALE_AFTER, mid, fin);
}

/**
 * Stops both playback and a recording. Called at critical points like
 * starting a new game, or ending the game in the menu.
 */
void G_StopDemo(void)
{
    DD_Execute(true, "stopdemo");
}

void G_DemoEnds(void)
{
    G_ChangeGameState(GS_WAITING);

    if(singledemo)
    {
        G_SetGameAction(GA_QUIT);
        return;
    }

    FI_DemoEnds();
}

void G_DemoAborted(void)
{
    G_ChangeGameState(GS_WAITING);
    FI_DemoEnds();
}

void G_ScreenShot(void)
{
    G_SetGameAction(GA_SCREENSHOT);
}

void G_DoScreenShot(void)
{
    int                 i;
    filename_t          name;
    char*               numPos;

    // Use game mode as the file name base.
    sprintf(name, "%s-", (char *) G_GetVariable(DD_GAME_MODE));
    numPos = name + strlen(name);

    // Find an unused file name.
    for(i = 0; i < 1e6; ++i) // Stop eventually...
    {
        sprintf(numPos, "%03i.tga", i);
        if(!M_FileExists(name))
            break;
    }

    M_ScreenShot(name, 24);
    Con_Message("Wrote %s.\n", name);
}

DEFCC(CCmdListMaps)
{
    Con_Message("Loaded maps:\n");
    G_PrintMapList();
    return true;
}
