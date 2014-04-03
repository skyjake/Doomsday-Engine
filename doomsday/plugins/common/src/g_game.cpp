/** @file g_game.c  Top-level (common) game routines.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
 * @authors Copyright © 1993-1996 id Software, Inc.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "g_common.h"

#include "am_map.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "fi_lib.h"
#include "g_controls.h"
#include "g_eventsequence.h"
#include "g_update.h"
#include "gamesession.h"
#include "hu_lib.h"
#include "hu_chat.h"
#include "hu_inventory.h"
#include "hu_log.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_pspr.h"
#include "hu_stuff.h"
#include "p_actor.h"
#include "p_inventory.h"
#include "p_map.h"
#if __JHEXEN__
#  include "p_mapinfo.h"
#endif
#include "p_mapsetup.h"
#include "p_mapspec.h"
#include "p_savedef.h"
#include "p_saveio.h"
#include "p_saveg.h"
#include "p_sound.h"
#include "p_start.h"
#include "p_tick.h"
#include "p_user.h"
#include "player.h"
#include "r_common.h"
#include "saveslots.h"
#include "x_hair.h"

#include <cctype>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <de/App>
#include <de/ArrayValue>
#include <de/NativePath>
#include <de/NumberValue>

using namespace common;

static GameSession session;

#define BODYQUEUESIZE       (32)

#define READONLYCVAR        CVF_READ_ONLY|CVF_NO_MAX|CVF_NO_MIN|CVF_NO_ARCHIVE

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
struct missileinfo_s {
    mobjtype_t  type;
    float       speed[2];
}
MonsterMissileInfo[] =
{
#if __JDOOM__ || __JDOOM64__
    { MT_BRUISERSHOT, {15, 20} },
    { MT_HEADSHOT, {10, 20} },
    { MT_TROOPSHOT, {10, 20} },
# if __JDOOM64__
    { MT_BRUISERSHOTRED, {15, 20} },
    { MT_NTROSHOT, {20, 40} },
# endif
#elif __JHERETIC__
    { MT_IMPBALL, {10, 20} },
    { MT_MUMMYFX1, {9, 18} },
    { MT_KNIGHTAXE, {9, 18} },
    { MT_REDAXE, {9, 18} },
    { MT_BEASTBALL, {12, 20} },
    { MT_WIZFX1, {18, 24} },
    { MT_SNAKEPRO_A, {14, 20} },
    { MT_SNAKEPRO_B, {14, 20} },
    { MT_HEADFX1, {13, 20} },
    { MT_HEADFX3, {10, 18} },
    { MT_MNTRFX1, {20, 26} },
    { MT_MNTRFX2, {14, 20} },
    { MT_SRCRFX1, {20, 28} },
    { MT_SOR2FX1, {20, 28} },
#endif
    { mobjtype_t(-1), {-1, -1} }
};
#endif

D_CMD(CycleTextureGamma);
D_CMD(EndSession);
D_CMD(HelpScreen);

D_CMD(ListMaps);
D_CMD(WarpMap);

D_CMD(LoadSession);
D_CMD(SaveSession);
D_CMD(QuickLoadSession);
D_CMD(QuickSaveSession);
D_CMD(DeleteSavedSession);
D_CMD(InspectSavedSession);

D_CMD(OpenLoadMenu);
D_CMD(OpenSaveMenu);

void G_PlayerReborn(int player);

void G_DoPlayDemo();
void G_DoMapCompleted();
void G_DoEndDebriefing();
void G_DoVictory();
void G_DoScreenShot();
void G_DoQuitGame();

void G_StopDemo();

/**
 * Updates game status cvars for the specified player.
 */
void G_UpdateGSVarsForPlayer(player_t *pl);

/**
 * Updates game status cvars for the current map.
 */
void G_UpdateGSVarsForMap();

void R_LoadVectorGraphics();

int Hook_DemoStop(int hookType, int val, void *parm);

game_config_t cfg; // The global cfg.

GameRuleset gameRules;
uint gameEpisode;

Uri *gameMapUri;
uint gameMapEntrance; ///< Entry point, for reborn.
uint gameMap;

uint nextMap;
uint nextMapEntrance;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
dd_bool secretExit;
#endif

dd_bool monsterInfight;

player_t players[MAXPLAYERS];

int totalKills, totalItems, totalSecret; // For intermission.

dd_bool singledemo; // Quit after playing a demo from cmdline.
dd_bool briefDisabled;

dd_bool precache = true; // If @c true, load all graphics at start.
dd_bool customPal; // If @c true, a non-IWAD palette is in use.

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
wbstartstruct_t wmInfo; // Params for world map / intermission.
#endif

// Game Action Variables:
static de::String gaSaveSessionSlot;
static bool gaSaveSessionGenerateDescription = true;
static de::String gaSaveSessionUserDescription;
static de::String gaLoadSessionSlot;

#if __JDOOM__ || __JDOOM64__
mobj_t *bodyQueue[BODYQUEUESIZE];
int bodyQueueSlot;
#endif

static SaveSlots *sslots;

// vars used with game status cvars
int gsvEpisode;
int gsvMap;
#if __JHEXEN__
int gsvHub;
#endif
char *gsvMapAuthor;// = "Unknown";
int gsvMapMusic = -1;
char *gsvMapTitle;// = "Unknown";

int gsvInMap;
int gsvCurrentMusic;

int gsvArmor;
int gsvHealth;

#if !__JHEXEN__
int gsvKills;
int gsvItems;
int gsvSecrets;
#endif

int gsvCurrentWeapon;
int gsvWeapons[NUM_WEAPON_TYPES];
int gsvKeys[NUM_KEY_TYPES];
int gsvAmmo[NUM_AMMO_TYPES];

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
int gsvInvItems[NUM_INVENTORYITEM_TYPES];
#endif

#if __JHEXEN__
int gsvWPieces[4];
#endif

static gamestate_t gameState = GS_STARTUP;

cvartemplate_t gamestatusCVars[] =
{
    {"game-music", READONLYCVAR, CVT_INT, &gsvCurrentMusic, 0, 0, 0},
    {"game-skill", READONLYCVAR, CVT_INT, &gameRules.skill, 0, 0, 0},
    {"game-state", READONLYCVAR, CVT_INT, &gameState, 0, 0, 0},
    {"game-state-map", READONLYCVAR, CVT_INT, &gsvInMap, 0, 0, 0},
#if !__JHEXEN__
    {"game-stats-kills", READONLYCVAR, CVT_INT, &gsvKills, 0, 0, 0},
    {"game-stats-items", READONLYCVAR, CVT_INT, &gsvItems, 0, 0, 0},
    {"game-stats-secrets", READONLYCVAR, CVT_INT, &gsvSecrets, 0, 0, 0},
#endif

    {"map-author", READONLYCVAR, CVT_CHARPTR, &gsvMapAuthor, 0, 0, 0},
    {"map-episode", READONLYCVAR, CVT_INT, &gsvEpisode, 0, 0, 0},
#if __JHEXEN__
    {"map-hub", READONLYCVAR, CVT_INT, &gsvHub, 0, 0, 0},
#endif
    {"map-id", READONLYCVAR, CVT_INT, &gsvMap, 0, 0, 0},
    {"map-music", READONLYCVAR, CVT_INT, &gsvMapMusic, 0, 0, 0},
    {"map-name", READONLYCVAR, CVT_CHARPTR, &gsvMapTitle, 0, 0, 0},

    {"player-health", READONLYCVAR, CVT_INT, &gsvHealth, 0, 0, 0},
    {"player-armor", READONLYCVAR, CVT_INT, &gsvArmor, 0, 0, 0},
    {"player-weapon-current", READONLYCVAR, CVT_INT, &gsvCurrentWeapon, 0, 0, 0},

#if __JDOOM__ || __JDOOM64__
    // Ammo
    {"player-ammo-bullets", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CLIP], 0, 0, 0},
    {"player-ammo-shells", READONLYCVAR, CVT_INT, &gsvAmmo[AT_SHELL], 0, 0, 0},
    {"player-ammo-cells", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CELL], 0, 0, 0},
    {"player-ammo-missiles", READONLYCVAR, CVT_INT, &gsvAmmo[AT_MISSILE], 0, 0, 0},
    // Weapons
    {"player-weapon-fist", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0, 0},
    {"player-weapon-pistol", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0, 0},
    {"player-weapon-shotgun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0, 0},
    {"player-weapon-chaingun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0, 0},
    {"player-weapon-mlauncher", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIFTH], 0, 0, 0},
    {"player-weapon-plasmarifle", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SIXTH], 0, 0, 0},
    {"player-weapon-bfg", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SEVENTH], 0, 0, 0},
    {"player-weapon-chainsaw", READONLYCVAR, CVT_INT, &gsvWeapons[WT_EIGHTH], 0, 0, 0},
    {"player-weapon-sshotgun", READONLYCVAR, CVT_INT, &gsvWeapons[WT_NINETH], 0, 0, 0},
    // Keys
    {"player-key-blue", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUECARD], 0, 0, 0},
    {"player-key-yellow", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOWCARD], 0, 0, 0},
    {"player-key-red", READONLYCVAR, CVT_INT, &gsvKeys[KT_REDCARD], 0, 0, 0},
    {"player-key-blueskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUESKULL], 0, 0, 0},
    {"player-key-yellowskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOWSKULL], 0, 0, 0},
    {"player-key-redskull", READONLYCVAR, CVT_INT, &gsvKeys[KT_REDSKULL], 0, 0, 0},
#elif __JHERETIC__
    // Ammo
    {"player-ammo-goldwand", READONLYCVAR, CVT_INT, &gsvAmmo[AT_CRYSTAL], 0, 0, 0},
    {"player-ammo-crossbow", READONLYCVAR, CVT_INT, &gsvAmmo[AT_ARROW], 0, 0, 0},
    {"player-ammo-dragonclaw", READONLYCVAR, CVT_INT, &gsvAmmo[AT_ORB], 0, 0, 0},
    {"player-ammo-hellstaff", READONLYCVAR, CVT_INT, &gsvAmmo[AT_RUNE], 0, 0, 0},
    {"player-ammo-phoenixrod", READONLYCVAR, CVT_INT, &gsvAmmo[AT_FIREORB], 0, 0, 0},
    {"player-ammo-mace", READONLYCVAR, CVT_INT, &gsvAmmo[AT_MSPHERE], 0, 0, 0},
     // Weapons
    {"player-weapon-staff", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0, 0},
    {"player-weapon-goldwand", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0, 0},
    {"player-weapon-crossbow", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0, 0},
    {"player-weapon-dragonclaw", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0, 0},
    {"player-weapon-hellstaff", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIFTH], 0, 0, 0},
    {"player-weapon-phoenixrod", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SIXTH], 0, 0, 0},
    {"player-weapon-mace", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SEVENTH], 0, 0, 0},
    {"player-weapon-gauntlets", READONLYCVAR, CVT_INT, &gsvWeapons[WT_EIGHTH], 0, 0, 0},
    // Keys
    {"player-key-yellow", READONLYCVAR, CVT_INT, &gsvKeys[KT_YELLOW], 0, 0, 0},
    {"player-key-green", READONLYCVAR, CVT_INT, &gsvKeys[KT_GREEN], 0, 0, 0},
    {"player-key-blue", READONLYCVAR, CVT_INT, &gsvKeys[KT_BLUE], 0, 0, 0},
    // Inventory items
    {"player-artifact-ring", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVULNERABILITY], 0, 0, 0},
    {"player-artifact-shadowsphere", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVISIBILITY], 0, 0, 0},
    {"player-artifact-crystalvial", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALTH], 0, 0, 0},
    {"player-artifact-mysticurn", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUPERHEALTH], 0, 0, 0},
    {"player-artifact-tomeofpower", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TOMBOFPOWER], 0, 0, 0},
    {"player-artifact-torch", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TORCH], 0, 0, 0},
    {"player-artifact-firebomb", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FIREBOMB], 0, 0, 0},
    {"player-artifact-egg", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_EGG], 0, 0, 0},
    {"player-artifact-wings", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FLY], 0, 0, 0},
    {"player-artifact-chaosdevice", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORT], 0, 0, 0},
#elif __JHEXEN__
    // Mana
    {"player-mana-blue", READONLYCVAR, CVT_INT, &gsvAmmo[AT_BLUEMANA], 0, 0, 0},
    {"player-mana-green", READONLYCVAR, CVT_INT, &gsvAmmo[AT_GREENMANA], 0, 0, 0},
    // Keys
    {"player-key-steel", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY1], 0, 0, 0},
    {"player-key-cave", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY2], 0, 0, 0},
    {"player-key-axe", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY3], 0, 0, 0},
    {"player-key-fire", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY4], 0, 0, 0},
    {"player-key-emerald", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY5], 0, 0, 0},
    {"player-key-dungeon", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY6], 0, 0, 0},
    {"player-key-silver", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY7], 0, 0, 0},
    {"player-key-rusted", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY8], 0, 0, 0},
    {"player-key-horn", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEY9], 0, 0, 0},
    {"player-key-swamp", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEYA], 0, 0, 0},
    {"player-key-castle", READONLYCVAR, CVT_INT, &gsvKeys[KT_KEYB], 0, 0, 0},
    // Weapons
    {"player-weapon-first", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FIRST], 0, 0, 0},
    {"player-weapon-second", READONLYCVAR, CVT_INT, &gsvWeapons[WT_SECOND], 0, 0, 0},
    {"player-weapon-third", READONLYCVAR, CVT_INT, &gsvWeapons[WT_THIRD], 0, 0, 0},
    {"player-weapon-fourth", READONLYCVAR, CVT_INT, &gsvWeapons[WT_FOURTH], 0, 0, 0},
    // Weapon Pieces
    {"player-weapon-piece1", READONLYCVAR, CVT_INT, &gsvWPieces[0], 0, 0, 0},
    {"player-weapon-piece2", READONLYCVAR, CVT_INT, &gsvWPieces[1], 0, 0, 0},
    {"player-weapon-piece3", READONLYCVAR, CVT_INT, &gsvWPieces[2], 0, 0, 0},
    {"player-weapon-allpieces", READONLYCVAR, CVT_INT, &gsvWPieces[3], 0, 0, 0},
    // Inventory items
    {"player-artifact-defender", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_INVULNERABILITY], 0, 0, 0},
    {"player-artifact-quartzflask", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALTH], 0, 0, 0},
    {"player-artifact-mysticurn", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUPERHEALTH], 0, 0, 0},
    {"player-artifact-mysticambit", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_HEALINGRADIUS], 0, 0, 0},
    {"player-artifact-darkservant", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SUMMON], 0, 0, 0},
    {"player-artifact-torch", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TORCH], 0, 0, 0},
    {"player-artifact-porkalator", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_EGG], 0, 0, 0},
    {"player-artifact-wings", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_FLY], 0, 0, 0},
    {"player-artifact-repulsion", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BLASTRADIUS], 0, 0, 0},
    {"player-artifact-flechette", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_POISONBAG], 0, 0, 0},
    {"player-artifact-banishment", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORTOTHER], 0, 0, 0},
    {"player-artifact-speed", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_SPEED], 0, 0, 0},
    {"player-artifact-might", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BOOSTMANA], 0, 0, 0},
    {"player-artifact-bracers", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_BOOSTARMOR], 0, 0, 0},
    {"player-artifact-chaosdevice", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_TELEPORT], 0, 0, 0},
    {"player-artifact-skull", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZSKULL], 0, 0, 0},
    {"player-artifact-heart", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBIG], 0, 0, 0},
    {"player-artifact-ruby", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMRED], 0, 0, 0},
    {"player-artifact-emerald1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMGREEN1], 0, 0, 0},
    {"player-artifact-emerald2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMGREEN2], 0, 0, 0},
    {"player-artifact-sapphire1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBLUE1], 0, 0, 0},
    {"player-artifact-sapphire2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEMBLUE2], 0, 0, 0},
    {"player-artifact-daemoncodex", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZBOOK1], 0, 0, 0},
    {"player-artifact-liberoscura", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZBOOK2], 0, 0, 0},
    {"player-artifact-flamemask", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZSKULL2], 0, 0, 0},
    {"player-artifact-glaiveseal", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZFWEAPON], 0, 0, 0},
    {"player-artifact-holyrelic", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZCWEAPON], 0, 0, 0},
    {"player-artifact-sigilmagus", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZMWEAPON], 0, 0, 0},
    {"player-artifact-gear1", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR1], 0, 0, 0},
    {"player-artifact-gear2", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR2], 0, 0, 0},
    {"player-artifact-gear3", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR3], 0, 0, 0},
    {"player-artifact-gear4", READONLYCVAR, CVT_INT, &gsvInvItems[IIT_PUZZGEAR4], 0, 0, 0},
#endif
    {"", 0, CVT_NULL, 0, 0, 0, 0}
};

ccmdtemplate_t gameCmds[] = {
    { "deletegamesave",  "ss",   CCmdDeleteSavedSession, 0 },
    { "deletegamesave",  "s",    CCmdDeleteSavedSession, 0 },
    { "endgame",         "",     CCmdEndSession, 0 },
    { "helpscreen",      "",     CCmdHelpScreen, 0 },
    { "inspectgamesave", "s",    CCmdInspectSavedSession, 0 },
    { "listmaps",        "",     CCmdListMaps, 0 },
    { "loadgame",        "ss",   CCmdLoadSession, 0 },
    { "loadgame",        "s",    CCmdLoadSession, 0 },
    { "loadgame",        "",     CCmdOpenLoadMenu, 0 },
    { "quickload",       "",     CCmdQuickLoadSession, 0 },
    { "quicksave",       "",     CCmdQuickSaveSession, 0 },
    { "savegame",        "sss",  CCmdSaveSession, 0 },
    { "savegame",        "ss",   CCmdSaveSession, 0 },
    { "savegame",        "s",    CCmdSaveSession, 0 },
    { "savegame",        "",     CCmdOpenSaveMenu, 0 },
    { "togglegamma",     "",     CCmdCycleTextureGamma, 0 },
    { "", "", 0, 0 }
};

// Deferred new game arguments:
static uint dMapEntrance;
static Uri *dMapUri; ///< @todo fixme: Never free'd
static GameRuleset dRules;

static gameaction_t gameAction;
static dd_bool quitInProgress;

void G_Register()
{
    for(int i = 0; gamestatusCVars[i].path[0]; ++i)
    {
        Con_AddVariable(gamestatusCVars + i);
    }

    C_VAR_BYTE("game-save-confirm",              &cfg.confirmQuickGameSave,  0, 0, 1);
    C_VAR_BYTE("game-save-confirm-loadonreborn", &cfg.confirmRebornLoad,     0, 0, 1);
    C_VAR_BYTE("game-save-last-loadonreborn",    &cfg.loadLastSaveOnReborn,  0, 0, 1);

    // Aliases for obsolete cvars:
    C_VAR_BYTE("menu-quick-ask",                 &cfg.confirmQuickGameSave, 0, 0, 1);

    for(int i = 0; gameCmds[i].name[0]; ++i)
    {
        Con_AddCommand(gameCmds + i);
    }

    C_CMD("warp", "i", WarpMap);
    C_CMD("setmap", "i", WarpMap); // alias
#if __JDOOM__ || __JHERETIC__
# if __JDOOM__
    if(!(gameModeBits & GM_ANY_DOOM2))
# endif
    {
        C_CMD("warp", "ii", WarpMap);
        C_CMD("setmap", "ii", WarpMap); // alias
    }
#endif
}

dd_bool G_QuitInProgress()
{
    return quitInProgress;
}

void G_SetGameAction(gameaction_t newAction)
{
    if(G_QuitInProgress()) return;

    if(gameAction != newAction)
    {
        gameAction = newAction;
    }
}

void G_SetGameActionNewSession(Uri const &mapUri, uint mapEntrance, GameRuleset const &rules)
{
    if(!dMapUri)
    {
        dMapUri = Uri_New();
    }
    Uri_Copy(dMapUri, &mapUri);
    dMapEntrance = mapEntrance;
    dRules       = rules; // make a copy.

    G_SetGameAction(GA_NEWSESSION);
}

bool G_SetGameActionSaveSession(de::String slotId, de::String *userDescription)
{
    if(!COMMON_GAMESESSION->savingPossible()) return false;
    if(!G_SaveSlots().has(slotId)) return false;

    gaSaveSessionSlot = slotId;

    if(userDescription && !userDescription->isEmpty())
    {
        // A new description.
        gaSaveSessionGenerateDescription = false;
        gaSaveSessionUserDescription = *userDescription;
    }
    else
    {
        // Reusing the current name or generating a new one.
        gaSaveSessionGenerateDescription = (userDescription && userDescription->isEmpty());
        gaSaveSessionUserDescription.clear();
    }

    G_SetGameAction(GA_SAVESESSION);
    return true;
}

bool G_SetGameActionLoadSession(de::String slotId)
{
    if(!COMMON_GAMESESSION->loadingPossible()) return false;

    // Check whether this slot is in use. We do this here also because we need to provide our
    // caller with instant feedback. Naturally this is no guarantee that the game-save will
    // be accessible come load time.

    try
    {
        if(G_SaveSlots()[slotId].isLoadable())
        {
            // Everything appears to be in order - schedule the game-save load!
            gaLoadSessionSlot = slotId;
            G_SetGameAction(GA_LOADSESSION);
            return true;
        }

        App_Log(DE2_RES_ERROR, "Cannot load from save slot '%s': not in use",
                slotId.toLatin1().constData());
    }
    catch(SaveSlots::MissingSlotError const &)
    {}

    return false;
}

void G_SetGameActionMapCompleted(uint newMap, uint _entryPoint, dd_bool _secretExit)
{
#if __JHEXEN__
    DENG2_UNUSED(_secretExit);
#else
    DENG2_UNUSED2(newMap, _entryPoint);
#endif

    if(IS_CLIENT) return;
    if(cyclingMaps && mapCycleNoExit) return;

#if __JHEXEN__
    if((gameMode == hexen_betademo || gameMode == hexen_demo) && newMap != DDMAXINT && newMap > 3)
    {
        // Not possible in the 4-map demo.
        P_SetMessage(&players[CONSOLEPLAYER], 0, "PORTAL INACTIVE -- DEMO");
        return;
    }
#endif

#if __JHEXEN__
    nextMap         = newMap;
    nextMapEntrance = _entryPoint;
#else
    secretExit      = _secretExit;

# if __JDOOM__
    // If no Wolf3D maps, no secret exit!
    if(secretExit && (gameModeBits & GM_ANY_DOOM2))
    {
        Uri *mapUri      = G_ComposeMapUri(0, 30);
        AutoStr *mapPath = Uri_Compose(mapUri);
        if(!P_MapExists(Str_Text(mapPath)))
        {
            secretExit = false;
        }
        Uri_Delete(mapUri);
    }
# endif
#endif

    G_SetGameAction(GA_MAPCOMPLETED);
}

gameaction_t G_GameAction()
{
    return gameAction;
}

/// @return  Absolute path to a saved session in /home/savegames
static inline de::String composeSavedSessionPathForSlot(int slot)
{
    return de::String("/home/savegames") / G_IdentityKey() / SAVEGAMENAME + de::String::number(slot) + ".save";
}

static void initSaveSlots()
{
    delete sslots;
    sslots = new SaveSlots;

    // Setup the logical save slot bindings.
    int const gameMenuSaveSlotWidgetIds[NUMSAVESLOTS] = {
        MNF_ID0, MNF_ID1, MNF_ID2, MNF_ID3, MNF_ID4, MNF_ID5,
#if !__JHEXEN__
        MNF_ID6, MNF_ID7
#endif
    };
    for(int i = 0; i < NUMSAVESLOTS; ++i)
    {
        sslots->add(de::String::number(i), true, composeSavedSessionPathForSlot(i),
                    gameMenuSaveSlotWidgetIds[i]);
    }
    sslots->add("auto", false, composeSavedSessionPathForSlot(AUTO_SLOT));
}

/**
 * Common Pre Game Initialization routine.
 * Game-specfic pre init actions should be placed in eg D_PreInit() (for jDoom)
 */
void G_CommonPreInit()
{
    if(!gameMapUri)
    {
        gameMapUri = Uri_New();
    }
    Uri_Clear(gameMapUri);
    quitInProgress = false;
    verbose = CommandLine_Exists("-verbose");

    // Register hooks.
    Plug_AddHook(HOOK_DEMO_STOP, Hook_DemoStop);

    // Setup the players.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *pl = players + i;

        pl->plr = DD_GetPlayer(i);
        pl->plr->extraData = (void*) &players[i];

        /// @todo Only necessary because the engine does not yet unload game plugins when they
        /// are not in use; thus a game change may leave these pointers dangling.
        for(int k = 0; k < NUMPSPRITES; ++k)
        {
            pl->pSprites[k].state = NULL;
            pl->plr->pSprites[k].statePtr = NULL;
        }
    }

    G_RegisterBindClasses();
    P_RegisterMapObjs();

    R_LoadVectorGraphics();
    R_LoadColorPalettes();

    P_InitPicAnims();

    // Add our cvars and ccmds to the console databases.
    G_ConsoleRegistration();     // Main command list.
    D_NetConsoleRegistration();  // For network.
    G_Register();                // Top level game cvars and commands.
    Pause_Register();
    G_ControlRegister();         // For controls/input.
    SaveSlots::consoleRegister(); // Game-save system.
    Hu_MenuRegister();           // For the menu.
    GUI_Register();              // For the UI library.
    Hu_MsgRegister();            // For the game messages.
    ST_Register();               // For the hud/statusbar.
    WI_Register();               // For the interlude/intermission.
    X_Register();                // For the crosshair.
    FI_StackRegister();          // For the InFine lib.
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_Register();
#endif

    Con_SetString2("map-author", "Unknown", SVF_WRITE_OVERRIDE);
    Con_SetString2("map-name",   "Unknown", SVF_WRITE_OVERRIDE);
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

void R_LoadColorPalettes()
{
#define PALLUMPNAME         "PLAYPAL"
#define PALENTRIES          (256)
#define PALID               (0)

    lumpnum_t lumpNum = W_GetLumpNumForName(PALLUMPNAME);
    uint8_t colors[PALENTRIES*3];
    colorpaletteid_t palId;
    Str xlatId;
    Str_InitStd(&xlatId);

    // Record whether we are using a custom palette.
    customPal = W_LumpIsCustom(lumpNum);

    W_ReadLumpSection(lumpNum, colors, 0 + PALID * (PALENTRIES * 3), PALENTRIES * 3);
    palId = R_CreateColorPalette("R8G8B8", PALLUMPNAME, colors, PALENTRIES);

#if __JHEXEN__
    // Load the translation tables.
    {
    int const numPerClass = (gameMode == hexen_v10? 3 : 7);

    int i, cl;
    int xlatNum;
    lumpnum_t lumpNum;
    Str lumpName;
    Str_Reserve(Str_InitStd(&lumpName), 8);

    // In v1.0, the color translations are a bit different. There are only
    // three translation maps per class, whereas Doomsday assumes seven maps
    // per class. Thus we'll need to account for the difference.

    xlatNum = 0;
    for(cl = 0; cl < 3; ++cl)
    for(i = 0; i < 7; ++i)
    {
        if(i == numPerClass) break; // Not present.

        Str_Clear(&lumpName);
        if(xlatNum < 10)
        {
            Str_Appendf(&lumpName, "TRANTBL%i", xlatNum);
        }
        else
        {
            Str_Appendf(&lumpName, "TRANTBL%c", 'A' + (xlatNum - 10));
        }
        xlatNum++;

        App_Log(DE2_DEV_RES_MSG, "Reading translation table '%s' as tclass=%i tmap=%i",
                Str_Text(&lumpName), cl, i);

        if(-1 != (lumpNum = W_CheckLumpNumForName(Str_Text(&lumpName))))
        {
            uint8_t const *mappings = W_CacheLump(lumpNum);
            Str_Appendf(Str_Clear(&xlatId), "%i", 7 * cl + i);
            R_CreateColorPaletteTranslation(palId, &xlatId, mappings);
            W_UnlockLump(lumpNum);
        }
    }

    Str_Free(&lumpName);
    }
#else
    // Create the translation tables to map the green color ramp to gray,
    // brown, red. Could be read from a lump instead?
    {
    uint8_t xlat[PALENTRIES];
    int xlatNum, palIdx;

    for(xlatNum = 0; xlatNum < 3; ++xlatNum)
    {
        // Translate just the 16 green colors.
        for(palIdx = 0; palIdx < 256; ++palIdx)
        {
#  if __JHERETIC__
            if(palIdx >= 225 && palIdx <= 240)
            {
                xlat[palIdx] = xlatNum == 0? 114 + (palIdx - 225) /*yellow*/ :
                               xlatNum == 1? 145 + (palIdx - 225) /*red*/ :
                                             190 + (palIdx - 225) /*blue*/;
            }
#  else
            if(palIdx >= 0x70 && palIdx <= 0x7f)
            {
                // Map green ramp to gray, brown, red.
                xlat[palIdx] = xlatNum == 0? 0x60 + (palIdx & 0xf) :
                               xlatNum == 1? 0x40 + (palIdx & 0xf) :
                                             0x20 + (palIdx & 0xf);
            }
#  endif
            else
            {
                // Keep all other colors as is.
                xlat[palIdx] = palIdx;
            }
        }
        Str_Appendf(Str_Clear(&xlatId), "%i", xlatNum);
        R_CreateColorPaletteTranslation(palId, &xlatId, xlat);
    }
    }
#endif

    Str_Free(&xlatId);

#undef PALID
#undef PALENTRIES
#undef PALLUMPNAME
}

/**
 * @todo Read this information from a definition (ideally with more user
 *       friendly mnemonics...).
 */
void R_LoadVectorGraphics()
{
#define R           (1.0f)
#define NUMITEMS(x) (sizeof(x)/sizeof((x)[0]))
#define Pt           Point2Rawf

    Point2Rawf const keyPoints[] = {
        Pt(-3 * R / 4, 0), Pt(-3 * R / 4, -R / 4), // Mid tooth.
        Pt(    0,      0), Pt(   -R,      0), Pt(   -R, -R / 2), // Shaft and end tooth.

        Pt(    0,      0), Pt(R / 4, -R / 2), // Bow.
        Pt(R / 2, -R / 2), Pt(R / 2,  R / 2),
        Pt(R / 4,  R / 2), Pt(    0,      0),
    };
    def_svgline_t const key[] = {
        { 2, &keyPoints[ 0] },
        { 3, &keyPoints[ 2] },
        { 6, &keyPoints[ 5] }
    };
    Point2Rawf const thintrianglePoints[] = {
        Pt(-R / 2,  R - R / 2),
        Pt(     R,          0), // `
        Pt(-R / 2, -R + R / 2), // /
        Pt(-R / 2,  R - R / 2) // |>
    };
    def_svgline_t const thintriangle[] = {
        { 4, thintrianglePoints },
    };
#if __JDOOM__ || __JDOOM64__
    Point2Rawf const arrowPoints[] = {
        Pt(    -R + R / 8, 0 ), Pt(             R, 0), // -----
        Pt( R - R / 2, -R / 4), Pt(             R, 0), Pt( R - R / 2,  R / 4), // ----->
        Pt(-R - R / 8, -R / 4), Pt(    -R + R / 8, 0), Pt(-R - R / 8,  R / 4), // >---->
        Pt(-R + R / 8, -R / 4), Pt(-R + 3 * R / 8, 0), Pt(-R + R / 8,  R / 4), // >>--->
    };
    def_svgline_t const arrow[] = {
        { 2, &arrowPoints[ 0] },
        { 3, &arrowPoints[ 2] },
        { 3, &arrowPoints[ 5] },
        { 3, &arrowPoints[ 8] }
    };
#elif __JHERETIC__ || __JHEXEN__
    Point2Rawf const arrowPoints[] = {
        Pt(-R + R / 4,      0), Pt(         0,      0), // center line.
        Pt(-R + R / 4,  R / 8), Pt(         R,      0), Pt(-R + R / 4, -R / 8), // blade

        Pt(-R + R / 8, -R / 4), Pt(-R + R / 4, -R / 4), // guard
        Pt(-R + R / 4,  R / 4), Pt(-R + R / 8,  R / 4),
        Pt(-R + R / 8, -R / 4),

        Pt(-R + R / 8, -R / 8), Pt(-R - R / 4, -R / 8), // hilt
        Pt(-R - R / 4,  R / 8), Pt(-R + R / 8,  R / 8),
    };
    def_svgline_t const arrow[] = {
        { 2, &arrowPoints[ 0] },
        { 3, &arrowPoints[ 2] },
        { 5, &arrowPoints[ 5] },
        { 4, &arrowPoints[10] }
    };
#endif
#if __JDOOM__
    Point2Rawf const cheatarrowPoints[] = {
        Pt(    -R + R / 8, 0 ), Pt(             R, 0), // -----
        Pt( R - R / 2, -R / 4), Pt(             R, 0), Pt( R - R / 2,  R / 4), // ----->
        Pt(-R - R / 8, -R / 4), Pt(    -R + R / 8, 0), Pt(-R - R / 8,  R / 4), // >---->
        Pt(-R + R / 8, -R / 4), Pt(-R + 3 * R / 8, 0), Pt(-R + R / 8,  R / 4), // >>--->

        Pt(        -R / 2,      0), Pt(        -R / 2, -R / 6), // >>-d--->
        Pt(-R / 2 + R / 6, -R / 6), Pt(-R / 2 + R / 6,  R / 4),

        Pt(        -R / 6,      0), Pt(        -R / 6, -R / 6), // >>-dd-->
        Pt(             0, -R / 6), Pt(             0,  R / 4),

        Pt(         R / 6,  R / 4), Pt(         R / 6, -R / 7), // >>-ddt->
        Pt(R / 6 + R / 32, -R / 7 - R / 32), Pt(R / 6 + R / 10, -R / 7)
    };
    def_svgline_t const cheatarrow[] = {
        { 2, &cheatarrowPoints[ 0] },
        { 3, &cheatarrowPoints[ 2] },
        { 3, &cheatarrowPoints[ 5] },
        { 3, &cheatarrowPoints[ 8] },
        { 4, &cheatarrowPoints[11] },
        { 4, &cheatarrowPoints[15] },
        { 4, &cheatarrowPoints[19] }
    };
#endif

    Point2Rawf const crossPoints[] = { // + (open center)
        Pt(-R,  0), Pt(-R / 5 * 2,          0),
        Pt( 0, -R), Pt(         0, -R / 5 * 2),
        Pt( R,  0), Pt( R / 5 * 2,          0),
        Pt( 0,  R), Pt(         0,  R / 5 * 2)
    };
    def_svgline_t const cross[] = {
        { 2, &crossPoints[0] },
        { 2, &crossPoints[2] },
        { 2, &crossPoints[4] },
        { 2, &crossPoints[6] }
    };
    Point2Rawf const twinanglesPoints[] = { // > <
        Pt(-R, -R * 10 / 14), Pt(-(R - (R * 10 / 14)), 0), Pt(-R,  R * 10 / 14), // >
        Pt( R, -R * 10 / 14), Pt(  R - (R * 10 / 14) , 0), Pt( R,  R * 10 / 14), // <
    };
    def_svgline_t const twinangles[] = {
        { 3, &twinanglesPoints[0] },
        { 3, &twinanglesPoints[3] }
    };
    Point2Rawf const squarePoints[] = { // square
        Pt(-R, -R), Pt(-R,  R),
        Pt( R,  R), Pt( R, -R),
        Pt(-R, -R)
    };
    def_svgline_t const square[] = {
        { 5, squarePoints },
    };
    Point2Rawf const squarecornersPoints[] = { // square (open center)
        Pt(   -R, -R / 2), Pt(-R, -R), Pt(-R / 2,      -R), // topleft
        Pt(R / 2,     -R), Pt( R, -R), Pt(     R,  -R / 2), // topright
        Pt(   -R,  R / 2), Pt(-R,  R), Pt(-R / 2,       R), // bottomleft
        Pt(R / 2,      R), Pt( R,  R), Pt(     R,   R / 2), // bottomright
    };
    def_svgline_t const squarecorners[] = {
        { 3, &squarecornersPoints[ 0] },
        { 3, &squarecornersPoints[ 3] },
        { 3, &squarecornersPoints[ 6] },
        { 3, &squarecornersPoints[ 9] }
    };
    Point2Rawf const anglePoints[] = { // v
        Pt(-R, -R), Pt(0,  0), Pt(R, -R)
    };
    def_svgline_t const angle[] = {
        { 3, anglePoints }
    };

    if(IS_DEDICATED) return;

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

#undef P
#undef R
#undef NUMITEMS
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

void R_InitRefresh()
{
    if(IS_DEDICATED) return;

    App_Log(DE2_RES_VERBOSE, "Loading data for refresh...");

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

void R_InitHud()
{
    Hu_LoadData();

#if __JHERETIC__ || __JHEXEN__
    App_Log(DE2_LOG_VERBOSE, "Initializing inventory...");
    Hu_InventoryInit();
#endif

    App_Log(DE2_LOG_VERBOSE, "Initializing statusbar...");
    ST_Init();

    App_Log(DE2_LOG_VERBOSE, "Initializing menu...");
    Hu_MenuInit();

    App_Log(DE2_LOG_VERBOSE, "Initializing status-message/question system...");
    Hu_MsgInit();
}

SaveSlots &G_SaveSlots()
{
    DENG2_ASSERT(sslots != 0);
    return *sslots;
}

static de::game::SavedSession *savedSessionByUserDescription(de::String description)
{
    if(!description.isEmpty())
    {
        de::Folder &saveFolder = DENG2_APP->rootFolder().locate<de::Folder>(de::String("/home/savegames") / G_IdentityKey());
        DENG2_FOR_EACH_CONST(de::Folder::Contents, i, saveFolder.contents())
        {
            if(de::game::SavedSession *session = i->second->maybeAs<de::game::SavedSession>())
            {
                if(!session->metadata().gets("userDescription", "").compareWithoutCase(description))
                {
                    return session;
                }
            }
        }
    }
    return 0; // Not found.
}

/// @todo Encapsulate in SaveSlots?
de::String G_SaveSlotIdFromUserInput(de::String str)
{
    // Perhaps a user description of a saved session?
    if(SaveSlot *sslot = G_SaveSlots().slot(savedSessionByUserDescription(str)))
    {
        return sslot->id();
    }

    // Perhaps a saved session file name?
    de::String savePath = de::String("/home/savegames") / G_IdentityKey() / str + ".save";
    if(SaveSlot *sslot = G_SaveSlots().slot(DENG2_APP->rootFolder().tryLocate<de::game::SavedSession const>(savePath)))
    {
        return sslot->id();
    }

    // Perhaps a mnemonic?
    if(!str.compareWithoutCase("last") || !str.compareWithoutCase("<last>"))
    {
        return de::String::number(Con_GetInteger("game-save-last-slot"));
    }
    if(!str.compareWithoutCase("quick") || !str.compareWithoutCase("<quick>"))
    {
        return de::String::number(Con_GetInteger("game-save-quick-slot"));
    }
    if(!str.compareWithoutCase("auto") || !str.compareWithoutCase("<auto>"))
    {
        return "auto";
    }

    // Perhaps a unique slot identifier?
    if(G_SaveSlots().has(str))
    {
        return str;
    }

    // Unknown/not found.
    return "";
}

void G_CommonPostInit()
{
    R_InitRefresh();
    FI_StackInit();
    GUI_Init();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_ReadTypes();
#endif

    App_Log(DE2_LOG_VERBOSE, "Initializing playsim...");
    P_Init();

    App_Log(DE2_LOG_VERBOSE, "Initializing head-up displays...");
    R_InitHud();

    initSaveSlots();

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
 * @note Game-specific actions should be placed in G_Shutdown rather than here.
 */
void G_CommonShutdown()
{
    COMMON_GAMESESSION->end();

    Plug_RemoveHook(HOOK_DEMO_STOP, Hook_DemoStop);

    Hu_MsgShutdown();
    Hu_UnloadData();
    D_NetClearBuffer();

    P_Shutdown();
    G_ShutdownEventSequences();

    FI_StackShutdown();
    Hu_MenuShutdown();
    ST_Shutdown();
    GUI_Shutdown();

    delete sslots; sslots = 0;
    Uri_Delete(gameMapUri); gameMapUri = 0;
}

gamestate_t G_GameState()
{
    return gameState;
}

static char const *getGameStateStr(gamestate_t state)
{
    struct statename_s {
        gamestate_t state;
        char const *name;
    } stateNames[] =
    {
        { GS_MAP,          "GS_MAP" },
        { GS_INTERMISSION, "GS_INTERMISSION" },
        { GS_FINALE,       "GS_FINALE" },
        { GS_STARTUP,      "GS_STARTUP" },
        { GS_WAITING,      "GS_WAITING" },
        { GS_INFINE,       "GS_INFINE" },
        { gamestate_t(-1), 0 }
    };
    for(uint i = 0; stateNames[i].name; ++i)
    {
        if(stateNames[i].state == state)
            return stateNames[i].name;
    }
    return 0;
}

/**
 * Called when the gameui binding context is active. Triggers the menu.
 */
int G_UIResponder(event_t *ev)
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

void G_ChangeGameState(gamestate_t state)
{
    dd_bool gameUIActive = false;
    dd_bool gameActive = true;

    if(G_QuitInProgress()) return;

    if(state < 0 || state >= NUM_GAME_STATES)
    {
        DENG_ASSERT(!"G_ChangeGameState: Invalid state");
        return;
    }

    if(gameState != state)
    {
        App_Log(DE2_DEV_NOTE, "Game state changed to %s", getGameStateStr(state));

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

    if(!IS_DEDICATED)
    {
        if(gameUIActive)
        {
            DD_Execute(true, "activatebcontext gameui");
            B_SetContextFallback("gameui", G_UIResponder);
        }
        DD_Executef(true, "%sactivatebcontext game", gameActive? "" : "de");
    }
}

dd_bool G_StartFinale(char const *script, int flags, finale_mode_t mode, char const *defId)
{
    if(!script || !script[0])
        return false;

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        // Clear the message queue for all local players.
        ST_LogEmpty(i);

        // Close the automap for all local players.
        ST_AutomapOpen(i, false, true);

#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }

    G_SetGameAction(GA_NONE);
    FI_StackExecuteWithId(script, flags, mode, defId);

    return true;
}

void G_StartHelp()
{
    if(G_QuitInProgress()) return;
    if(IS_CLIENT)
    {
        /// @todo Fix this properly: http://sf.net/p/deng/bugs/1082/
        return;
    }

    ddfinale_t fin;
    if(Def_Get(DD_DEF_FINALE, "help", &fin))
    {
        Hu_MenuCommand(MCMD_CLOSEFAST);
        G_StartFinale(fin.script, FF_LOCAL, FIMODE_NORMAL, "help");
        return;
    }
    App_Log(DE2_SCR_WARNING, "InFine script 'help' not defined");
}

/**
 * Prints a banner to the console containing information pertinent to the current map
 * (e.g., map title, author...).
 */
static void printMapBanner()
{
    char const *title = P_MapTitle(0/*current map*/);

    App_Log(DE2_LOG_MESSAGE, DE2_ESC(R));
    if(title)
    {
        char buf[64];
#if __JHEXEN__
        mapinfo_t const *mapInfo = P_MapInfo(0/*current map*/);
        int warpNum = (mapInfo? mapInfo->warpTrans : -1);
        dd_snprintf(buf, 64, "Map: %s (%u) - " DE2_ESC(b) "%s", Str_Text(Uri_ToString(gameMapUri)), warpNum + 1, title);
#else
        dd_snprintf(buf, 64, "Map: %s - " DE2_ESC(b) "%s", Str_Text(Uri_ToString(gameMapUri)), title);
#endif
        App_Log(DE2_LOG_NOTE, "%s", buf);
    }

#if !__JHEXEN__
    char const *author = P_MapAuthor(0/*current map*/, P_MapIsCustom(Str_Text(Uri_Compose(gameMapUri))));
    if(!author) author = "Unknown";

    App_Log(DE2_LOG_NOTE, "Author: %s", author);
#endif
    App_Log(DE2_LOG_MESSAGE, DE2_ESC(R));
}

void G_BeginMap()
{
    G_ChangeGameState(GS_MAP);

    if(!IS_DEDICATED)
    {
        R_SetViewPortPlayer(CONSOLEPLAYER, CONSOLEPLAYER); // View the guy you are playing.
        R_ResizeViewWindow(RWF_FORCE|RWF_NO_LERP);
    }

    G_ControlReset(-1); // Clear all controls for all local players.
    G_UpdateGSVarsForMap();

    // Time can now progress in this map.
    mapTime = actualMapTime = 0;

    printMapBanner();

    // The music may have been paused for the briefing; unpause.
    S_PauseMusic(false);
}

int G_Responder(event_t *ev)
{
    DENG2_ASSERT(ev);

    // Eat all events once shutdown has begun.
    if(G_QuitInProgress()) return true;

    if(G_GameState() == GS_MAP)
    {
        Pause_Responder(ev);

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

int G_PrivilegedResponder(event_t *ev)
{
    // Ignore all events once shutdown has begun.
    if(G_QuitInProgress()) return false;

    if(Hu_MenuPrivilegedResponder(ev))
        return true;

    // Process the screen shot key right away?
    if(ev->type == EV_KEY && ev->data1 == DDKEY_F1)
    {
        if(CommandLine_Check("-devparm"))
        {
            if(ev->state == EVS_DOWN)
                G_ScreenShot();
            return true; // All F1 events are eaten.
        }
    }

    return false; // Not eaten.
}

void G_UpdateGSVarsForPlayer(player_t *pl)
{
    if(!pl) return;

    gsvHealth  = pl->health;
#if !__JHEXEN__
    // Map stats
    gsvKills   = pl->killCount;
    gsvItems   = pl->itemCount;
    gsvSecrets = pl->secretCount;
#endif
        // armor
#if __JHEXEN__
    gsvArmor   = FixedDiv(PCLASS_INFO(pl->class_)->autoArmorSave +
                          pl->armorPoints[ARMOR_ARMOR] +
                          pl->armorPoints[ARMOR_SHIELD] +
                          pl->armorPoints[ARMOR_HELMET] +
                          pl->armorPoints[ARMOR_AMULET], 5 * FRACUNIT) >> FRACBITS;
#else
    gsvArmor   = pl->armorPoints;
#endif
    // Owned keys
    for(int i = 0; i < NUM_KEY_TYPES; ++i)
    {
#if __JHEXEN__
        gsvKeys[i] = (pl->keys & (1 << i))? 1 : 0;
#else
        gsvKeys[i] = pl->keys[i];
#endif
    }
    // current weapon
    gsvCurrentWeapon = pl->readyWeapon;

    // owned weapons
    for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        gsvWeapons[i] = pl->weapons[i].owned;
    }

#if __JHEXEN__
    // weapon pieces
    gsvWPieces[0] = (pl->pieces & WPIECE1)? 1 : 0;
    gsvWPieces[1] = (pl->pieces & WPIECE2)? 1 : 0;
    gsvWPieces[2] = (pl->pieces & WPIECE3)? 1 : 0;
    gsvWPieces[3] = (pl->pieces == 7)? 1 : 0;
#endif
    // Current ammo amounts.
    for(int i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        gsvAmmo[i] = pl->ammo[i].owned;
    }

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    // Inventory items.
    for(int i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        if(pl->plr->inGame && G_GameState() == GS_MAP)
            gsvInvItems[i] = P_InventoryCount(pl - players, inventoryitemtype_t(IIT_FIRST + i));
        else
            gsvInvItems[i] = 0;
    }
#endif
}

void G_UpdateGSVarsForMap()
{
    char const *mapAuthor = P_MapAuthor(0/*current map*/, false/*don't supress*/);
    char const *mapTitle  = P_MapTitle(0/*current map*/);

    if(!mapAuthor) mapAuthor = "Unknown";
    Con_SetString2("map-author", mapAuthor, SVF_WRITE_OVERRIDE);

    if(!mapTitle) mapTitle = "Unknown";
    Con_SetString2("map-name", mapTitle, SVF_WRITE_OVERRIDE);
}

void G_DoQuitGame()
{
#define QUITWAIT_MILLISECONDS 1500

    static uint quitTime = 0;

    if(!quitInProgress)
    {
        quitInProgress = true;
        quitTime = Timer_RealMilliseconds();

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

    if(Timer_RealMilliseconds() > quitTime + QUITWAIT_MILLISECONDS)
    {
        Sys_Quit();
    }
    else
    {
        float t = (Timer_RealMilliseconds() - quitTime) / (float) QUITWAIT_MILLISECONDS;
        quitDarkenOpacity = t*t*t;
    }

#undef QUITWAIT_MILLISECONDS
}

static void runGameAction()
{
    gameaction_t currentAction;

    // Do things to change the game state.
    while((currentAction = G_GameAction()) != GA_NONE)
    {
        BusyMode_FreezeGameForBusyMode();

        switch(currentAction)
        {
        case GA_NEWSESSION:
            G_SetGameAction(GA_NONE);
            COMMON_GAMESESSION->end();
            COMMON_GAMESESSION->begin(*dMapUri, dMapEntrance, dRules);
            break;

        case GA_LOADSESSION:
            G_SetGameAction(GA_NONE);
            COMMON_GAMESESSION->end();

            // Attempt to load the saved game session.
            try
            {
                COMMON_GAMESESSION->load(G_SaveSlots()[gaLoadSessionSlot].saveName());

                // Make note of the last used save slot.
                Con_SetInteger2("game-save-last-slot", gaLoadSessionSlot.toInt(), SVF_WRITE_OVERRIDE);
            }
            catch(de::Error const &er)
            {
                LOG_RES_WARNING("Error loading from save slot #%s:\n")
                        << gaLoadSessionSlot << er.asText();
            }

            // Return to the title loop if loading did not succeed.
            if(!COMMON_GAMESESSION->hasBegun())
            {
                COMMON_GAMESESSION->endAndBeginTitle();
            }
            break;

        case GA_SAVESESSION:
            G_SetGameAction(GA_NONE);
            try
            {
                COMMON_GAMESESSION->save(G_SaveSlots()[gaSaveSessionSlot].saveName(),
                                         gaSaveSessionUserDescription);

                // Make note of the last used save slot.
                Con_SetInteger2("game-save-last-slot", gaSaveSessionSlot.toInt(), SVF_WRITE_OVERRIDE);
            }
            catch(de::Error const &er)
            {
                LOG_RES_WARNING("Error saving to save slot #%s:\n")
                        << gaSaveSessionSlot << er.asText();
            }
            break;

        case GA_QUIT:
            G_DoQuitGame();
            // No further game state changes occur once we have begun to quit.
            return;

        case GA_SCREENSHOT:
            G_SetGameAction(GA_NONE);
            G_DoScreenShot();
            break;

        case GA_LEAVEMAP:
            G_SetGameAction(GA_NONE);
            COMMON_GAMESESSION->leaveMap();
            break;

        case GA_RESTARTMAP:
            G_SetGameAction(GA_NONE);
            COMMON_GAMESESSION->reloadMap();
            break;

        case GA_MAPCOMPLETED:
            G_DoMapCompleted();
            break;

        case GA_ENDDEBRIEFING:
            G_DoEndDebriefing();
            break;

        case GA_VICTORY:
            G_SetGameAction(GA_NONE);
            break;

        default: break;
        }
    }
}

static int rebornLoadConfirmed(msgresponse_t response, int, void *)
{
    if(response == MSG_YES)
    {
        G_SetGameAction(GA_RESTARTMAP);
    }
    else
    {
        // Player seemingly wishes to extend their stay in limbo?
        player_t *plr    = players;
        plr->rebornWait  = PLAYER_REBORN_TICS;
        plr->playerState = PST_DEAD;
    }
    return true;
}

/**
 * Do needed reborns for any fallen players.
 */
static void rebornPlayers()
{
    if(!IS_NETGAME && P_CountPlayersInGame(LocalOnly) == 1)
    {
        if(Player_WaitingForReborn(&players[0]))
        {
            // Are we still awaiting a response to a previous confirmation?
            if(Hu_IsMessageActiveWithCallback(rebornLoadConfirmed))
                return;

            // Do we need user confirmation?
            if(COMMON_GAMESESSION->progressRestoredOnReload() && cfg.confirmRebornLoad)
            {
                S_LocalSound(SFX_REBORNLOAD_CONFIRM, NULL);
                de::String savegameDescription = COMMON_GAMESESSION->userDescription();
                if(savegameDescription.isEmpty()) // Not yet saved.
                {
                    savegameDescription = "(Unsaved)";
                }
                Str msg; Str_Appendf(Str_Init(&msg), REBORNLOAD_CONFIRM, savegameDescription.toUtf8().constData());
                Hu_MsgStart(MSG_YESNO, Str_Text(&msg), rebornLoadConfirmed, 0, 0);
                return;
            }

            rebornLoadConfirmed(MSG_YES, 0, 0);
        }
        return;
    }

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr = &players[i];

        if(Player_WaitingForReborn(plr))
        {
            P_RebornPlayerInMultiplayer(i);
        }

        // Player has left?
        if((int)plr->playerState == PST_GONE)
        {
            plr->playerState  = PST_REBORN;
            ddplayer_t *ddplr = plr->plr;
            if(mobj_t *plmo = ddplr->mo)
            {
                if(!IS_CLIENT)
                {
                    P_SpawnTeleFog(plmo->origin[VX], plmo->origin[VY], plmo->angle + ANG180);
                }

                // Let's get rid of the mobj.
                App_Log(DE2_DEV_MAP_MSG, "rebornPlayers: Removing player %i's mobj", i);

                P_MobjRemove(plmo, true);
                ddplr->mo = 0;
            }
        }
    }
}

/**
 * The core of the timing loop. Game state, game actions etc occur here.
 *
 * @param ticLength  How long this tick is, in seconds.
 */
void G_Ticker(timespan_t ticLength)
{
    static gamestate_t oldGameState = gamestate_t(-1);

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
            Set(DD_CLIENT_PAUSED, Pause_IsPaused());
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
                Con_SetString2("map-author", "Unknown", SVF_WRITE_OVERRIDE);
                Con_SetString2("map-name",   "Unknown", SVF_WRITE_OVERRIDE);
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

void G_PlayerLeaveMap(int player)
{
    player_t *p = &players[player];

#if __JHEXEN__
    dd_bool newHub = true;

    Uri *nextMapUri = G_ComposeMapUri(gameEpisode, nextMap);
    newHub = (P_MapInfo(0/*current map*/)->hub != P_MapInfo(nextMapUri)->hub);
    Uri_Delete(nextMapUri); nextMapUri = 0;
#endif

#if __JHEXEN__
    // Remember if flying.
    int flightPower = p->powers[PT_FLIGHT];
#endif

#if __JHERETIC__
    // Empty the inventory of excess items
    for(int i = 0; i < NUM_INVENTORYITEM_TYPES; ++i)
    {
        inventoryitemtype_t type = inventoryitemtype_t(IIT_FIRST + i);
        uint count = P_InventoryCount(player, type);

        if(count)
        {
            if(type != IIT_FLY)
            {
                count--;
            }

            for(uint j = 0; j < count; ++j)
            {
                P_InventoryTake(player, type, true);
            }
        }
    }
#endif

#if __JHEXEN__
    if(newHub)
    {
        uint count = P_InventoryCount(player, IIT_FLY);
        for(uint i = 0; i < count; ++i)
        {
            P_InventoryTake(player, IIT_FLY, true);
        }
    }
#endif

    // Remove their powers.
    p->update |= PSF_POWERS;
    de::zap(p->powers);

#if __JHEXEN__
    if(!newHub && !gameRules.deathmatch)
    {
        p->powers[PT_FLIGHT] = flightPower; // Restore flight.
    }
#endif

    // Remove their keys.
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    p->update |= PSF_KEYS;
    de::zap(p->keys);
#else
    if(!gameRules.deathmatch && newHub)
    {
        p->keys = 0;
    }
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
        p->readyWeapon = weapontype_t(p->plr->mo->special1); // Restore weapon.
        p->morphTics = 0;
    }
#endif

    p->plr->lookDir = 0;
    p->plr->mo->flags &= ~MF_SHADOW; // Cancel invisibility.
    p->plr->extraLight = 0; // Cancel gun flashes.
    p->plr->fixedColorMap = 0; // Cancel IR goggles.

    // Clear filter.
    p->plr->flags &= ~DDPF_VIEW_FILTER;
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
static void clearPlayer(player_t *p)
{
    DENG2_ASSERT(p != 0);

    player_t playerCopy;
    ddplayer_t ddPlayerCopy;

    // Take a backup of the old data.
    memcpy(&playerCopy, p, sizeof(*p));
    memcpy(&ddPlayerCopy, p->plr, sizeof(*p->plr));

    // Clear everything.
    memset(p->plr, 0, sizeof(*p->plr));
    memset(p, 0, sizeof(*p));

    // Restore important data:

    // The pointer to ddplayer.
    p->plr = playerCopy.plr;

#if __JHERETIC__ || __JHEXEN__ || __JDOOM64__
    P_InventoryEmpty(p - players);
    P_InventorySetReadyItem(p - players, IIT_NONE);
#endif

    // Restore the pointer to this player.
    p->plr->extraData = p;

    // Restore the inGame status.
    p->plr->inGame = ddPlayerCopy.inGame;
    p->plr->flags = ddPlayerCopy.flags & ~(DDPF_INTERYAW | DDPF_INTERPITCH);

    // Don't clear the start spot.
    p->startSpot = playerCopy.startSpot;

    // Restore counters.
    memcpy(&p->plr->fixCounter, &ddPlayerCopy.fixCounter, sizeof(ddPlayerCopy.fixCounter));
    memcpy(&p->plr->fixAcked,   &ddPlayerCopy.fixAcked,   sizeof(ddPlayerCopy.fixAcked));

    p->plr->fixCounter.angles++;
    p->plr->fixCounter.origin++;
    p->plr->fixCounter.mom++;
}

/**
 * Called after a player dies (almost everything is cleared and then
 * re-initialized).
 */
void G_PlayerReborn(int player)
{
    if(player < 0 || player >= MAXPLAYERS)
        return; // Wha?

    App_Log(DE2_DEV_MAP_NOTE, "G_PlayerReborn: reseting player %i", player);

    player_t *p = &players[player];

    int frags[MAXPLAYERS];
    DENG_ASSERT(sizeof(p->frags) == sizeof(frags));

    memcpy(frags, p->frags, sizeof(frags));
    int killcount    = p->killCount;
    int itemcount    = p->itemCount;
    int secretcount  = p->secretCount;
#if __JHEXEN__
    uint worldTimer  = p->worldTimer;
#endif

#if __JHERETIC__
    dd_bool secret = p->didSecret;
    int spot       = p->startSpot;
#endif

    // Clears (almost) everything.
    clearPlayer(p);

#if __JHERETIC__
    p->startSpot = spot;
#endif

    memcpy(p->frags, frags, sizeof(p->frags));
    p->killCount   = killcount;
    p->itemCount   = itemcount;
    p->secretCount = secretcount;
#if __JHEXEN__
    p->worldTimer  = worldTimer;
#endif
    p->colorMap    = cfg.playerColor[player];
    p->class_      = P_ClassForPlayerWhenRespawning(player, false);
#if __JHEXEN__
    if(p->class_ == PCLASS_FIGHTER && !IS_NETGAME)
    {
        // In Hexen single-player, the Fighter's default color is Yellow.
        p->colorMap = 2;
    }
#endif
    p->useDown      = p->attackDown = true; // Don't do anything immediately.
    p->playerState  = PST_LIVE;
    p->health       = maxHealth;
    p->brain.changeWeapon = WT_NOCHANGE;

#if __JDOOM__ || __JDOOM64__
    p->readyWeapon  = p->pendingWeapon = WT_SECOND;
    p->weapons[WT_FIRST ].owned = true;
    p->weapons[WT_SECOND].owned = true;

    // Initalize the player's ammo counts.
    de::zap(p->ammo);
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
    for(int k = 0; k < NUM_WEAPON_TYPES; ++k)
    {
        App_Log(DE2_DEV_MAP_MSG, "Player %i owns wpn %i: %i", player, k, p->weapons[k].owned);
    }
#endif

#else
    p->readyWeapon = p->pendingWeapon = WT_FIRST;
    p->weapons[WT_FIRST].owned = true;
    localQuakeHappening[player] = false;
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    // Reset maxammo.
    for(int i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        p->ammo[i].max = maxAmmo[i];
    }
#endif

    // Reset viewheight.
    p->viewHeight      = cfg.plrViewHeight;
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
void G_QueueBody(mobj_t *mo)
{
    if(!mo) return;

    // Flush an old corpse if needed.
    if(bodyQueueSlot >= BODYQUEUESIZE)
    {
        P_MobjRemove(bodyQueue[bodyQueueSlot % BODYQUEUESIZE], false);
    }

    bodyQueue[bodyQueueSlot % BODYQUEUESIZE] = mo;
    bodyQueueSlot++;
}
#endif

#if __JDOOM__ || __JDOOM64__
static void G_ApplyGameRuleFastMonsters(dd_bool fast)
{
    static dd_bool oldFast = false;

    // Only modify when the rule changes state.
    if(fast == oldFast) return;
    oldFast = fast;

    /// @fixme Kludge: Assumes the original values speed values haven't been modified!
    for(int i = S_SARG_RUN1; i <= S_SARG_RUN8; ++i)
    {
        STATES[i].tics = fast? 1 : 2;
    }
    for(int i = S_SARG_ATK1; i <= S_SARG_ATK3; ++i)
    {
        STATES[i].tics = fast? 4 : 8;
    }
    for(int i = S_SARG_PAIN; i <= S_SARG_PAIN2; ++i)
    {
        STATES[i].tics = fast? 1 : 2;
    }
    // Kludge end.
}
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
static void G_ApplyGameRuleFastMissiles(dd_bool fast)
{
    static dd_bool oldFast = false;

    // Only modify when the rule changes state.
    if(fast == oldFast) return;
    oldFast = fast;

    /// @fixme Kludge: Assumes the original values speed values haven't been modified!
    for(int i = 0; MonsterMissileInfo[i].type != -1; ++i)
    {
        MOBJINFO[MonsterMissileInfo[i].type].speed =
            MonsterMissileInfo[i].speed[fast? 1 : 0];
    }
    // Kludge end.
}
#endif

/**
 * To be called when a new game begins to effect the game rules. Note that some
 * of the rules may be overridden here (e.g., in a networked game).
 */
void G_ApplyNewGameRules(GameRuleset const &rules)
{
#ifdef DENG2_DEBUG
    if(COMMON_GAMESESSION->hasBegun())
    {
        LOG_WARNING("Applied new rules to a game session in progress!");
    }
#endif

    gameRules = rules;

    if(gameRules.skill < SM_NOTHINGS)
        gameRules.skill = SM_NOTHINGS;
    if(gameRules.skill > NUM_SKILL_MODES - 1)
        gameRules.skill = skillmode_t(NUM_SKILL_MODES - 1);

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(!IS_NETGAME)
    {
        gameRules.deathmatch      = false;
        gameRules.respawnMonsters = false;

        gameRules.noMonsters = CommandLine_Exists("-nomonsters")? true : false;
    }
#endif

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    gameRules.respawnMonsters = CommandLine_Check("-respawn")? true : false;
#endif

#if __JDOOM__ || __JHERETIC__
    // Is respawning enabled at all in nightmare skill?
    if(gameRules.skill == SM_NIGHTMARE)
    {
        gameRules.respawnMonsters = cfg.respawnMonstersNightmare;
    }
#endif

    // Fast monsters?
#if __JDOOM__ || __JDOOM64__
    dd_bool fastMonsters = gameRules.fast;
# if __JDOOM__
    if(gameRules.skill == SM_NIGHTMARE)
    {
        fastMonsters = true;
    }
# endif
    G_ApplyGameRuleFastMonsters(fastMonsters);
#endif

    // Fast missiles?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    dd_bool fastMissiles = gameRules.fast;
# if !__JDOOM64__
    if(gameRules.skill == SM_NIGHTMARE)
    {
        fastMissiles = true;
    }
# endif
    G_ApplyGameRuleFastMissiles(fastMissiles);
#endif

    if(IS_DEDICATED)
    {
        NetSv_ApplyGameRulesFromConfig();
    }
}

GameRuleset &G_Rules()
{
    return gameRules;
}

int G_Ruleset_Skill()
{
    return gameRules.skill;
}

#if !__JHEXEN__
byte G_Ruleset_Fast()
{
    return gameRules.fast;
}
#endif

byte G_Ruleset_Deathmatch()
{
    return gameRules.deathmatch;
}

byte G_Ruleset_NoMonsters()
{
    return gameRules.noMonsters;
}

#if __JHEXEN__
byte G_Ruleset_RandomClasses()
{
    return gameRules.randomClasses;
}
#endif

#if !__JHEXEN__
byte G_Ruleset_RespawnMonsters()
{
    return gameRules.respawnMonsters;
}
#endif

/**
 * @return  @c true iff the game has been completed.
 */
dd_bool G_IfVictory()
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
        {
            return true;
        }
    }
    else if((gameModeBits & GM_ANY_DOOM) && gameMap == 7)
    {
        return true;
    }
#elif __JHERETIC__
    if(gameMap == 7)
    {
        return true;
    }
#elif __JHEXEN__
    if(nextMap == DDMAXINT && nextMapEntrance == DDMAXINT)
    {
        return true;
    }
#endif
    return false;
}

static int prepareIntermission(void * /*context*/)
{
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    wmInfo.episode    = gameEpisode;
    wmInfo.currentMap = gameMap;
    wmInfo.nextMap    = nextMap;
    wmInfo.didSecret  = players[CONSOLEPLAYER].didSecret;

# if __JDOOM__ || __JDOOM64__
    wmInfo.maxKills   = totalKills;
    wmInfo.maxItems   = totalItems;
    wmInfo.maxSecret  = totalSecret;

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

void G_DoMapCompleted()
{
    G_SetGameAction(GA_NONE);

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        if(players[i].plr->inGame)
        {
            ST_AutomapOpen(i, false, true);

#if __JHERETIC__ || __JHEXEN__
            Hu_InventoryOpen(i, false);
#endif

            G_PlayerLeaveMap(i); // take away cards and stuff

            // Update this client's stats.
            NetSv_SendPlayerState(i, DDSP_ALL_PLAYERS, PSF_FRAGS | PSF_COUNTERS, true);
        }
    }

    if(!IS_DEDICATED)
    {
        GL_SetFilter(false);
    }

#if __JHEXEN__
    SN_StopAllSequences();
#endif

    // Go to an intermission?
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    ddmapinfo_t minfo;
    if(Def_Get(DD_DEF_MAP_INFO, Str_Text(Uri_Compose(gameMapUri)), &minfo) &&
       (minfo.flags & MIF_NO_INTERMISSION))
    {
        G_IntermissionDone();
        return;
    }

#elif __JHEXEN__
    if(!gameRules.deathmatch)
    {
        G_IntermissionDone();
        return;
    }
#endif

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
# if __JDOOM__
    if((gameModeBits & (GM_DOOM|GM_DOOM_SHAREWARE|GM_DOOM_ULTIMATE)) && gameMap == 8)
    {
        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            players[i].didSecret = true;
        }
    }
# endif

    // Determine the next map.
    nextMap = G_NextLogicalMapNumber(secretExit);
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
    NetSv_Intermission(IMF_BEGIN, (int) nextMap, (int) nextMapEntrance);
#endif

    S_PauseMusic(false);
}

#if __JDOOM__ || __JDOOM64__
void G_PrepareWIData()
{
    wbstartstruct_t *info = &wmInfo;
    info->maxFrags = 0;

    // See if there is a par time definition.
    AutoStr *mapPath = Uri_Compose(gameMapUri);
    ddmapinfo_t minfo;
    if(Def_Get(DD_DEF_MAP_INFO, Str_Text(mapPath), &minfo) && minfo.parTime > 0)
    {
        info->parTime = TICRATE * (int) minfo.parTime;
    }
    else
    {
        info->parTime = -1; // Unknown.
    }

    info->pNum = CONSOLEPLAYER;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *p              = &players[i];
        wbplayerstruct_t *pStats = &info->plyr[i];

        pStats->inGame = p->plr->inGame;
        pStats->kills  = p->killCount;
        pStats->items  = p->itemCount;
        pStats->secret = p->secretCount;
        pStats->time   = mapTime;
        memcpy(pStats->frags, p->frags, sizeof(pStats->frags));
    }
}
#endif

void G_IntermissionDone()
{
    // We have left Intermission, however if there is an InFine for debriefing we should run it now.
    if(G_StartFinale(G_InFineDebriefing(gameMapUri), 0, FIMODE_AFTER, 0))
    {
        // The GA_ENDDEBRIEFING action is taken after the debriefing stops.
        return;
    }

    // We have either just returned from a debriefing or there wasn't one.
    briefDisabled = false;

#if __JDOOM__ || __JDOOM64__
    if(secretExit)
    {
        players[CONSOLEPLAYER].didSecret = true;
    }
#endif

    // Clear the currently playing script, if any.
    FI_StackClear();

    // Has the player completed the game?
    if(G_IfVictory())
    {
        // Victorious!
        G_SetGameAction(GA_VICTORY);
        return;
    }

    G_SetGameAction(GA_LEAVEMAP);
}

void G_DoEndDebriefing()
{
    briefDisabled = true;
    G_IntermissionDone();
}

de::String G_DefaultSavedSessionUserDescription(de::String const &saveName, bool autogenerate)
{
    // If the slot is already in use then choose existing description.
    if(!saveName.isEmpty())
    {
        de::String const existing = COMMON_GAMESESSION->savedUserDescription(saveName);
        if(!existing.isEmpty()) return existing;
    }

    if(!autogenerate) return "";

    // Autogenerate a suitable description.
    de::String description;

    // Include the source file name, for custom maps.
    AutoStr const *mapPath = Uri_Compose(gameMapUri);
    if(P_MapIsCustom(Str_Text(mapPath)))
    {
        de::String const &mapSourcePath = de::String(Str_Text(P_MapSourceFile(Str_Text(mapPath))));
        description += mapSourcePath.fileNameWithoutExtension() + ":";
    }

    // Include the map title.
    de::String mapTitle = de::String(P_MapTitle(0/*current map*/));
    // No map title? Use the identifier. (Some tricksy modders provide us with an empty title).
    /// @todo Move this logic engine-side.
    if(mapTitle.isEmpty() || mapTitle.at(0) == ' ')
    {
        mapTitle = Str_Text(mapPath);
    }
    description += mapTitle;

    // Include the game time also.
    int time = mapTime / TICRATE;
    int const hours   = time / 3600; time -= hours * 3600;
    int const minutes = time / 60;   time -= minutes * 60;
    int const seconds = time;
    description += de::String(" %1:%2:%3")
                         .arg(hours,   2, 10, QChar('0'))
                         .arg(minutes, 2, 10, QChar('0'))
                         .arg(seconds, 2, 10, QChar('0'));

    return description;
}

void G_ApplyCurrentSessionMetadata(de::game::SessionMetadata &metadata)
{
    metadata.clear();

    metadata.set("gameIdentityKey", G_IdentityKey());
    metadata.set("userDescription", ""); // Applied later.
    metadata.set("mapUri",          Str_Text(Uri_Compose(gameMapUri)));
#if !__JHEXEN__
    metadata.set("mapTime",         mapTime);
#endif

    metadata.add("gameRules",       G_Rules().toRecord()); // Takes ownership.

#if !__JHEXEN__
    de::ArrayValue *array = new de::ArrayValue;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        bool playerIsPresent = CPP_BOOL(players[i].plr->inGame);
        *array << de::NumberValue(playerIsPresent, de::NumberValue::Boolean);
    }
    metadata.set("players", array); // Takes ownership.
#endif

    metadata.set("sessionId",       uint(Timer_RealMilliseconds() + (mapTime << 24)));
}

/// @todo Get this from MAPINFO
uint G_EpisodeNumberFor(Uri const *mapUri)
{
#if __JDOOM__ || __JHERETIC__
    de::String path = Str_Text(Uri_Resolved(mapUri));
    if(!path.isEmpty())
    {
# if __JDOOM__
        if(gameModeBits & (GM_ANY_DOOM | ~GM_DOOM_CHEX))
# endif
        {
            if(path.at(0) == 'E' && path.at(2) == 'M')
            {
                return path.substr(1, 1).toInt() - 1;
            }
        }
    }
#else
    DENG2_UNUSED(mapUri);
#endif
    return 0;
}

/// @todo Get this from MAPINFO
uint G_MapNumberFor(Uri const *mapUri)
{
    de::String path = Str_Text(Uri_Resolved(mapUri));
    if(!path.isEmpty())
    {
#if __JDOOM__ || __JHERETIC__
# if __JDOOM__
        if(gameModeBits & (GM_ANY_DOOM | ~GM_DOOM_CHEX))
# endif
        {
            if(path.at(0) == 'E' && path.at(2) == 'M')
            {
                return path.substr(3).toInt() - 1;
            }
        }
#endif
        if(path.beginsWith("MAP"))
        {
            return path.substr(3).toInt() - 1;
        }
    }
    return 0;
}

static int quitGameConfirmed(msgresponse_t response, int /*userValue*/, void * /*userPointer*/)
{
    if(response == MSG_YES)
    {
        G_SetGameAction(GA_QUIT);
    }
    return true;
}

void G_QuitGame()
{
    if(G_QuitInProgress()) return;

    if(Hu_IsMessageActiveWithCallback(quitGameConfirmed))
    {
        // User has re-tried to quit with "quit" when the question is already on
        // the screen. Apparently we should quit...
        DD_Execute(true, "quit!");
        return;
    }

    char const *endString;
#if __JDOOM__ || __JDOOM64__
    endString = endmsg[((int) GAMETIC % (NUM_QUITMESSAGES + 1))];
#else
    endString = GET_TXT(TXT_QUITMSG);
#endif

    Con_Open(false);
    Hu_MsgStart(MSG_YESNO, endString, quitGameConfirmed, 0, NULL);
}

de::String G_IdentityKey()
{
    GameInfo gameInfo;
    if(DD_GameInfo(&gameInfo))
    {
        return Str_Text(gameInfo.identityKey);
    }
    /// @throw Error GameInfo is unavailable.
    throw de::Error("G_IdentityKey", "Failed retrieving GameInfo");
}

uint G_LogicalMapNumber(uint episode, uint map)
{
#if __JHEXEN__
    return P_TranslateMap(map);
    DENG_UNUSED(episode);
#elif __JDOOM64__
    return map;
    DENG_UNUSED(episode);
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

uint G_CurrentLogicalMapNumber()
{
    return G_LogicalMapNumber(gameEpisode, gameMap);
}

Uri *G_ComposeMapUri(uint episode, uint map)
{
    lumpname_t mapId;
#if __JDOOM64__
    dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "MAP%02u", map+1);
    DENG_UNUSED(episode);
#elif __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "MAP%02u", map+1);
    else
        dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "E%uM%u", episode+1, map+1);
#elif  __JHERETIC__
    dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "E%uM%u", episode+1, map+1);
#else
    dd_snprintf(mapId, LUMPNAME_T_MAXLEN, "MAP%02u", map+1);
    DENG_UNUSED(episode);
#endif
    return Uri_NewWithPath2(mapId, RC_NULL);
}

dd_bool G_ValidateMap(uint *episode, uint *map)
{
    bool ok = true;

#if __JDOOM__
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
    if(gameModeBits & GM_HERETIC_SHAREWARE)
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

    if(*map > 8)
    {
        *map = 8;
        ok = false;
    }
#else
    if(*map > 98)
    {
        *map = 98;
        ok = false;
    }
#endif

    // Check that the map truly exists.
    Uri *uri = G_ComposeMapUri(*episode, *map);
    AutoStr *path = Uri_Compose(uri);
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

uint G_GetNextMap(uint episode, uint map, dd_bool secretExit)
{
#if __JHEXEN__
    Uri *mapUri = G_ComposeMapUri(episode, map);
    int nextMap = G_LogicalMapNumber(episode, P_MapInfo(mapUri)->nextMap);
    Uri_Delete(mapUri);
    return nextMap;

    DENG2_UNUSED(secretExit);

#elif __JDOOM64__
    DENG2_UNUSED(episode);

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
            App_Log(DE2_MAP_WARNING, "No secret exit on map %u!", map + 1);
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
               App_Log(DE2_MAP_WARNING, "No secret exit on map %u!", map+1);
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

uint G_NextLogicalMapNumber(dd_bool secretExit)
{
    return G_GetNextMap(gameEpisode, gameMap, secretExit);
}

/**
 * Print a list of maps and the WAD files where they are from.
 */
void G_PrintFormattedMapList(uint episode, char const **files, uint count)
{
    char const *current = 0;
    uint rangeStart = 0;

    for(uint i = 0; i < count; ++i)
    {
        if(!current && files[i])
        {
            current = files[i];
            rangeStart = i;
        }
        else if(current && (!files[i] || stricmp(current, files[i])))
        {
            // Print a range.
            uint len = i - rangeStart;
            LogBuffer_Printf(DE2_RES_MSG, "  "); // Indentation.
            if(len <= 2)
            {
                for(uint k = rangeStart; k < i; ++k)
                {
                    Uri *mapUri = G_ComposeMapUri(episode, k);
                    AutoStr *path = Uri_ToString(mapUri);
                    LogBuffer_Printf(DE2_RES_MSG, "%s%s", Str_Text(path), (k != i - 1) ? "," : "");
                    Uri_Delete(mapUri);
                }
            }
            else
            {
                Uri *mapUri = G_ComposeMapUri(episode, rangeStart);
                AutoStr *path = Uri_ToString(mapUri);

                LogBuffer_Printf(DE2_RES_MSG, "%s-", Str_Text(path));
                Uri_Delete(mapUri);

                mapUri = G_ComposeMapUri(episode, i-1);
                path = Uri_ToString(mapUri);
                LogBuffer_Printf(DE2_RES_MSG, "%s", Str_Text(path));
                Uri_Delete(mapUri);
            }
            LogBuffer_Printf(DE2_RES_MSG, " " DE2_ESC(2) DE2_ESC(>) "%s\n", F_PrettyPath(current));

            // Moving on to a different file.
            current = files[i];
            rangeStart = i;
        }
    }
}

void G_PrintMapList()
{
#if __JDOOM__
    uint maxEpisodes       = (gameModeBits & GM_ANY_DOOM)? 9 : 1;
    uint maxMapsPerEpisode = (gameModeBits & GM_ANY_DOOM)? 9 : 99;
#elif __JHERETIC__
    uint maxEpisodes       = 9;
    uint maxMapsPerEpisode = 9;
#else
    uint maxEpisodes       = 1;
    uint maxMapsPerEpisode = 99;
#endif

    char const *sourceList[100];

    for(uint episode = 0; episode < maxEpisodes; ++episode)
    {
        de::zap(sourceList);

        // Find the name of each map (not all may exist).
        for(uint map = 0; map < maxMapsPerEpisode; ++map)
        {
            Uri *uri      = G_ComposeMapUri(episode, map);
            AutoStr *path = P_MapSourceFile(Str_Text(Uri_Compose(uri)));
            if(!Str_IsEmpty(path))
            {
                sourceList[map] = Str_Text(path);
            }
            Uri_Delete(uri);
        }
        G_PrintFormattedMapList(episode, sourceList, 99);
    }
}

char const *G_InFine(char const *scriptId)
{
    ddfinale_t fin;
    if(Def_Get(DD_DEF_FINALE, scriptId, &fin))
    {
        return fin.script;
    }
    return 0;
}

char const *G_InFineBriefing(Uri const *mapUri)
{
    DENG2_ASSERT(mapUri != 0);

    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled)
        return 0;

    if(G_GameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
        return 0;

    // Is there such a finale definition?
    ddfinale_t fin;
    if(Def_Get(DD_DEF_FINALE_BEFORE, Str_Text(Uri_Compose(mapUri)), &fin))
    {
        return fin.script;
    }
    return 0;
}

char const *G_InFineDebriefing(Uri const *mapUri)
{
    // If we're already in the INFINE state, don't start a finale.
    if(briefDisabled)
        return 0;

#if __JHEXEN__
    if(cfg.overrideHubMsg && G_GameState() == GS_MAP &&
       !(nextMap == DDMAXINT && nextMapEntrance == DDMAXINT))
    {
        Uri *nextMapUri = G_ComposeMapUri(gameEpisode, nextMap);
        if(P_MapInfo(mapUri)->hub != P_MapInfo(nextMapUri)->hub)
        {
            Uri_Delete(nextMapUri);
            return false;
        }
        Uri_Delete(nextMapUri);
    }
#endif

    if(G_GameState() == GS_INFINE || IS_CLIENT || Get(DD_PLAYBACK))
    {
        return 0;
    }

    // Is there such a finale definition?
    ddfinale_t fin;
    if(Def_Get(DD_DEF_FINALE_AFTER, Str_Text(Uri_Compose(mapUri)), &fin))
    {
        return fin.script;
    }
    return 0;
}

/**
 * Stops both playback and a recording. Called at critical points like
 * starting a new game, or ending the game in the menu.
 */
void G_StopDemo()
{
    DD_Execute(true, "stopdemo");
}

int Hook_DemoStop(int /*hookType*/, int val, void * /*context*/)
{
    bool aborted = val != 0;

    G_ChangeGameState(GS_WAITING);

    if(!aborted && singledemo)
    {
        // Playback ended normally.
        G_SetGameAction(GA_QUIT);
        return true;
    }

    G_SetGameAction(GA_NONE);

    if(IS_NETGAME && IS_CLIENT)
    {
        // Restore normal game state?
        gameRules.deathmatch = false;
        gameRules.noMonsters = false;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        gameRules.respawnMonsters = false;
#endif
#if __JHEXEN__
        gameRules.randomClasses = false;
#endif
    }

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_AutomapOpen(i, false, true);
#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }
    return true;
}

void G_ScreenShot()
{
    G_SetGameAction(GA_SCREENSHOT);
}

/**
 * Find an unused screenshot file name. Uses the game's identity key as the file name base.
 */
static de::String composeScreenshotFileName()
{
    de::String name = G_IdentityKey() + "-";
    int const numPos = name.length();
    for(int i = 0; i < 1e6; ++i) // Stop eventually...
    {
        name += de::String("%1.png").arg(i, 3, 10, QChar('0'));
        if(!F_FileExists(name.toUtf8().constData())) break;
        name.truncate(numPos);
    }
    return name;
}

void G_DoScreenShot()
{
    de::String fileName = composeScreenshotFileName();
    if(M_ScreenShot(fileName.toUtf8().constData(), 24))
    {
        /// @todo Do not use the console player's message log for this notification.
        ///       The engine should implement it's own notification UI system for
        ///       this sort of thing.
        AutoStr *msg = Str_Appendf(AutoStr_NewStd(), "Saved screenshot: %s",
                                   de::NativePath(fileName).pretty().toLatin1().constData());
        P_SetMessage(players + CONSOLEPLAYER, LMF_NO_HIDE, Str_Text(msg));
        return;
    }

    App_Log(DE2_RES_ERROR, "Failed to write screenshot \"%s\"",
            de::NativePath(fileName).pretty().toLatin1().constData());
}

D_CMD(OpenLoadMenu)
{
    DENG2_UNUSED3(src, argc, argv);

    if(!COMMON_GAMESESSION->loadingPossible()) return false;
    DD_Execute(true, "menu loadgame");
    return true;
}

D_CMD(OpenSaveMenu)
{
    DENG2_UNUSED3(src, argc, argv);

    if(!COMMON_GAMESESSION->savingPossible()) return false;
    DD_Execute(true, "menu savegame");
    return true;
}

static int endSessionConfirmed(msgresponse_t response, int /*userValue*/, void * /*context*/)
{
    if(response == MSG_YES)
    {
        if(IS_CLIENT)
        {
            DD_Executef(false, "net disconnect");
        }
        else
        {
            COMMON_GAMESESSION->endAndBeginTitle();
        }
    }
    return true;
}

D_CMD(EndSession)
{
    DENG2_UNUSED3(src, argc, argv);

    if(G_QuitInProgress()) return true;

    if(!COMMON_GAMESESSION->hasBegun())
    {
        if(IS_NETGAME && IS_SERVER)
        {
            App_Log(DE2_NET_ERROR, "%s", ENDNOGAME);
        }
        else
        {
            Hu_MsgStart(MSG_ANYKEY, ENDNOGAME, NULL, 0, NULL);
        }
        return true;
    }

    if(IS_NETGAME && IS_SERVER)
    {
        // Just do it, no questions asked.
        COMMON_GAMESESSION->endAndBeginTitle();
        return true;
    }

    Hu_MsgStart(MSG_YESNO, IS_CLIENT? GET_TXT(TXT_DISCONNECT) : ENDGAME, endSessionConfirmed, 0, NULL);

    return true;
}

static int loadSessionConfirmed(msgresponse_t response, int /*userValue*/, void *context)
{
    de::String *slotId = static_cast<de::String *>(context);
    DENG2_ASSERT(slotId != 0);
    if(response == MSG_YES)
    {
        G_SetGameActionLoadSession(*slotId);
    }
    delete slotId;
    return true;
}

D_CMD(LoadSession)
{
    DENG2_UNUSED(src);

    bool const confirmed = (argc == 3 && !stricmp(argv[2], "confirm"));

    if(G_QuitInProgress()) return false;
    if(!COMMON_GAMESESSION->loadingPossible()) return false;

    if(IS_NETGAME)
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QLOADNET, NULL, 0, NULL);
        return false;
    }

    de::String const slotId = G_SaveSlotIdFromUserInput(argv[1]);
    try
    {
        SaveSlot &sslot = G_SaveSlots()[slotId];
        if(sslot.isLoadable())
        {
            // A known used slot identifier.
            if(confirmed || !cfg.confirmQuickGameSave)
            {
                // Try to schedule a GA_LOADSESSION action.
                S_LocalSound(SFX_MENU_ACCEPT, NULL);
                return G_SetGameActionLoadSession(sslot.id());
            }

            S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
            // Compose the confirmation message.
            de::String const &savegameUserDescription = COMMON_GAMESESSION->savedUserDescription(sslot.saveName());
            AutoStr *msg = Str_Appendf(AutoStr_NewStd(), QLPROMPT, savegameUserDescription.toUtf8().constData());
            Hu_MsgStart(MSG_YESNO, Str_Text(msg), loadSessionConfirmed, 0, new de::String(sslot.id()));
            return true;
        }
    }
    catch(SaveSlots::MissingSlotError const &)
    {}

    if(!stricmp(argv[1], "quick") || !stricmp(argv[1], "<quick>"))
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, NULL);
        Hu_MsgStart(MSG_ANYKEY, QSAVESPOT, NULL, 0, NULL);
        return true;
    }

    if(!G_SaveSlots().has(slotId))
    {
        App_Log(DE2_SCR_WARNING, "Failed to determine save slot from \"%s\"", argv[1]);
    }

    // Clearly the caller needs some assistance...
    // We'll open the load menu if caller is the console.
    // Reasoning: User attempted to load a named game-save however the name
    // specified didn't match anything known. Opening the load menu allows
    // the user to see the names of the known game-saves.
    if(src == CMDS_CONSOLE)
    {
        App_Log(DE2_SCR_MSG, "Opening Load Game menu...");
        DD_Execute(true, "menu loadgame");
        return true;
    }

    // No action means the command failed.
    return false;
}

D_CMD(QuickLoadSession)
{
    DENG2_UNUSED3(src, argc, argv);
    return DD_Execute(true, "loadgame quick");
}

struct savesessionconfirmed_params_t
{
    de::String slotId;
    de::String userDescription;
};

static int saveSessionConfirmed(msgresponse_t response, int /*userValue*/, void *context)
{
    savesessionconfirmed_params_t *p = static_cast<savesessionconfirmed_params_t *>(context);
    DENG2_ASSERT(p != 0);
    if(response == MSG_YES)
    {
        G_SetGameActionSaveSession(p->slotId, &p->userDescription);
    }
    delete p;
    return true;
}

D_CMD(SaveSession)
{
    DENG2_UNUSED(src);

    bool const confirmed = (argc >= 3 && !stricmp(argv[argc-1], "confirm"));

    if(G_QuitInProgress()) return false;

    if(IS_CLIENT || IS_NETWORK_SERVER)
    {
        App_Log(DE2_LOG_ERROR, "Network savegames are not supported at the moment");
        return false;
    }

    player_t *player = &players[CONSOLEPLAYER];
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

    de::String const slotId = G_SaveSlotIdFromUserInput(argv[1]);
    try
    {
        SaveSlot &sslot = G_SaveSlots()[slotId];
        if(sslot.isUserWritable())
        {
            // A known slot identifier.
            de::String userDescription;
            if(argc >= 3 && stricmp(argv[2], "confirm"))
            {
                userDescription = argv[2];
            }

            if(sslot.isUnused() || confirmed || !cfg.confirmQuickGameSave)
            {
                // Try to schedule a GA_SAVESESSION action.
                S_LocalSound(SFX_MENU_ACCEPT, NULL);
                return G_SetGameActionSaveSession(slotId, &userDescription);
            }

            // Compose the confirmation message.
            userDescription = COMMON_GAMESESSION->savedUserDescription(sslot.saveName());
            AutoStr *msg = Str_Appendf(AutoStr_NewStd(), QSPROMPT, userDescription.toUtf8().constData());

            savesessionconfirmed_params_t *parm = new savesessionconfirmed_params_t;
            parm->slotId          = slotId;
            parm->userDescription = userDescription;

            S_LocalSound(SFX_QUICKSAVE_PROMPT, NULL);
            Hu_MsgStart(MSG_YESNO, Str_Text(msg), saveSessionConfirmed, 0, parm);
            return true;
        }

        App_Log(DE2_LOG_ERROR, "Save slot '%s' is non-user-writable",
                slotId.toLatin1().constData());
    }
    catch(SaveSlots::MissingSlotError const &)
    {}

    if(!stricmp(argv[1], "quick") || !stricmp(argv[1], "<quick>"))
    {
        // No quick-save slot has been nominated - allow doing so now.
        Hu_MenuCommand(MCMD_OPEN);
        Hu_MenuSetActivePage(Hu_MenuFindPageByName("SaveGame"));
        menuNominatingQuickSaveSlot = true;
        return true;
    }

    if(!G_SaveSlots().has(slotId))
    {
        App_Log(DE2_SCR_WARNING, "Failed to determine save slot from \"%s\"", argv[1]);
    }

    // No action means the command failed.
    return false;
}

D_CMD(QuickSaveSession)
{
    DENG2_UNUSED3(src, argc, argv);
    return DD_Execute(true, "savegame quick");
}

static int deleteSavedSessionConfirmed(msgresponse_t response, int /*userValue*/, void *context)
{
    de::String const *saveName = static_cast<de::String const *>(context);
    DENG2_ASSERT(saveName != 0);
    if(response == MSG_YES)
    {
        COMMON_GAMESESSION->deleteSaved(*saveName);
    }
    delete saveName;
    return true;
}

D_CMD(DeleteSavedSession)
{
    DENG2_UNUSED(src);

    if(G_QuitInProgress()) return false;

    bool const confirmed = (argc >= 3 && !stricmp(argv[argc-1], "confirm"));
    try
    {
        SaveSlot &sslot = G_SaveSlots()[G_SaveSlotIdFromUserInput(argv[1])];
        if(sslot.isUserWritable())
        {
            // A known slot identifier.
            if(sslot.isUnused()) return false;

            if(confirmed)
            {
                COMMON_GAMESESSION->deleteSaved(sslot.saveName());
            }
            else
            {
                S_LocalSound(SFX_DELETESAVEGAME_CONFIRM, NULL);

                // Compose the confirmation message.
                de::String const savegameUserDescription = COMMON_GAMESESSION->savedUserDescription(sslot.saveName());
                AutoStr *msg = Str_Appendf(AutoStr_NewStd(), DELETESAVEGAME_CONFIRM, savegameUserDescription.toUtf8().constData());
                Hu_MsgStart(MSG_YESNO, Str_Text(msg), deleteSavedSessionConfirmed, 0, new de::String(sslot.saveName()));
            }
            return true;
        }

        App_Log(DE2_LOG_ERROR, "Save slot '%s' is non-user-writable", sslot.id().toLatin1().constData());
    }
    catch(SaveSlots::MissingSlotError const &)
    {
        App_Log(DE2_SCR_WARNING, "Failed to determine save slot from '%s'", argv[1]);
    }

    // No action means the command failed.
    return false;
}

D_CMD(InspectSavedSession)
{
    DENG2_UNUSED2(src, argc);

    de::String slotId = G_SaveSlotIdFromUserInput(argv[1]);
    try
    {
        SaveSlot const &sslot = G_SaveSlots()[slotId];
        de::game::SavedSession const &session = DENG2_APP->rootFolder().locate<de::game::SavedSession>(sslot.savePath());
        LOG_SCR_MSG("%s") << session.metadata().asStyledText();
        LOG_SCR_MSG(_E(D) "Resource: " _E(.)_E(i) "\"%s\"") << session.path();
        return true;
    }
    catch(de::Folder::NotFoundError const &)
    {
        LOG_WARNING("Save slot '%s' is not in use") << slotId;
    }
    catch(SaveSlots::MissingSlotError const &)
    {
        LOG_WARNING("Failed to determine save slot from \"%s\"") << argv[1];
    }

    // No action means the command failed.
    return false;
}

D_CMD(HelpScreen)
{
    DENG2_UNUSED3(src, argc, argv);

    G_StartHelp();
    return true;
}

D_CMD(CycleTextureGamma)
{
    DENG2_UNUSED3(src, argc, argv);

    R_CycleGammaLevel();
    return true;
}

D_CMD(ListMaps)
{
    DENG2_UNUSED3(src, argc, argv);

    App_Log(DE2_RES_MSG, "Available maps:");
    G_PrintMapList();
    return true;
}

/**
 * Warp behavior is as follows:
 *
 * warp (map):      if a game session is in progress
 *                      continue the session and change map
 *                      if Hexen and the targt map is in another hub
 *                          force a new session.
 *                  else
 *                      begin a new game session and warp to the specified map.
 *
 * warp (ep) (map): same as warp (map) but force new session if episode differs.
 *
 * @note In a networked game we must presently force a new game session when a
 * map change outside the normal progression occurs to allow session-level state
 * changes to take effect. In single player this behavior is not necessary.
 *
 * @note "setmap" is an alias of "warp"
 */
D_CMD(WarpMap)
{
    bool const forceNewSession = IS_NETGAME != 0;

    // Only server operators can warp maps in network games.
    /// @todo Implement vote or similar mechanics.
    if(IS_NETGAME && !IS_NETWORK_SERVER)
    {
        return false;
    }

    uint epsd, map;
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
        char const *fmtString = argc == 3? "Unknown map \"%s, %s\"." : "Unknown map \"%s%s\".";
        AutoStr *msg = Str_Appendf(AutoStr_NewStd(), fmtString, argv[1], argc == 3? argv[2] : "");
        P_SetMessage(players + CONSOLEPLAYER, LMF_NO_HIDE, Str_Text(msg));
        return false;
    }
    Uri *newMapUri = G_ComposeMapUri(epsd, map);

#if __JHEXEN__
    // Hexen does not allow warping to the current map.
    if(!forceNewSession && COMMON_GAMESESSION->hasBegun() &&
       Uri_Equality(gameMapUri, newMapUri))
    {
        P_SetMessage(players + CONSOLEPLAYER, LMF_NO_HIDE, "Cannot warp to the current map.");
        Uri_Delete(newMapUri);
        return false;
    }
#endif

    // Close any left open UIs.
    /// @todo Still necessary here?
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *plr     = players + i;
        ddplayer_t *ddplr = plr->plr;
        if(!ddplr->inGame) continue;

        ST_AutomapOpen(i, false, true);
#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(i, false);
#endif
    }
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // So be it.
    briefDisabled = true;

    if(!forceNewSession && COMMON_GAMESESSION->hasBegun())
    {
#if __JHEXEN__
        nextMap         = map;
        nextMapEntrance = 0;
        G_SetGameAction(GA_LEAVEMAP);
#else
        G_SetGameActionNewSession(*newMapUri, 0/*default*/, gameRules);
#endif
    }
    else
    {
        G_SetGameActionNewSession(*newMapUri, 0/*default*/, gameRules);
    }

    // If the command source was "us" the game library then it was probably in
    // response to the local player entering a cheat event sequence, so set the
    // "CHANGING MAP" message.
    // Somewhat of a kludge...
    if(src == CMDS_GAME && !(IS_NETGAME && IS_SERVER))
    {
#if __JHEXEN__
        char const *msg = TXT_CHEATWARP;
        int soundId     = SFX_PLATFORM_STOP;
#elif __JHERETIC__
        char const *msg = TXT_CHEATWARP;
        int soundId     = SFX_DORCLS;
#else //__JDOOM__ || __JDOOM64__
        char const *msg = STSTR_CLEV;
        int soundId     = SFX_NONE;
#endif
        P_SetMessage(players + CONSOLEPLAYER, LMF_NO_HIDE, msg);
        S_LocalSound(soundId, NULL);
    }

    Uri_Delete(newMapUri);
    return true;
}
