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

#include <cctype>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <de/App>
#include <de/NativePath>
#include <de/RecordValue>
#include <doomsday/defs/episode.h>
#include <doomsday/uri.h>

#include "am_map.h"
#include "animdefs.h"
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

using namespace de;
using namespace common;

static GameSession session;

GameRuleset defaultGameRules;

#define BODYQUEUESIZE       (32)

#define READONLYCVAR        CVF_READ_ONLY|CVF_NO_MAX|CVF_NO_MIN|CVF_NO_ARCHIVE

D_CMD(CycleTextureGamma);
D_CMD(EndSession);
D_CMD(HelpScreen);

D_CMD(ListMaps);
D_CMD(LeaveMap);
D_CMD(WarpMap);
D_CMD(LoadSession);
D_CMD(SaveSession);
D_CMD(QuickLoadSession);
D_CMD(QuickSaveSession);
D_CMD(DeleteSavedSession);

D_CMD(OpenLoadMenu);
D_CMD(OpenSaveMenu);

void G_PlayerReborn(int player);

void G_StopDemo();

/**
 * Updates game status cvars for the specified player.
 */
void G_UpdateGSVarsForPlayer(player_t *pl);

void R_LoadVectorGraphics();

int Hook_DemoStop(int hookType, int val, void *parm);

game_config_t cfg; // The global cfg.

static de::Uri nextMapUri;
static uint nextMapEntryPoint;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
bool secretExit;
#endif

dd_bool monsterInfight;

player_t players[MAXPLAYERS];

int totalKills, totalItems, totalSecret; // For intermission.

dd_bool singledemo; // Quit after playing a demo from cmdline.
dd_bool briefDisabled;

dd_bool precache = true; // If @c true, load all graphics at start.
dd_bool customPal; // If @c true, a non-IWAD palette is in use.

wbstartstruct_t wmInfo; // Intermission parameters.

// Game Action Variables:
static GameRuleset gaNewSessionRules;
static String gaNewSessionEpisodeId;
static de::Uri gaNewSessionMapUri;
static uint gaNewSessionMapEntrance;

static String gaSaveSessionSlot;
static bool gaSaveSessionGenerateDescription = true;
static String gaSaveSessionUserDescription;
static String gaLoadSessionSlot;

#if __JDOOM__ || __JDOOM64__
mobj_t *bodyQueue[BODYQUEUESIZE];
int bodyQueueSlot;
#endif

static SaveSlots *sslots;

// vars used with game status cvars

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

static gameaction_t gameAction;
static gamestate_t gameState = GS_STARTUP;
static dd_bool quitInProgress;

static void registerGameStatusCVars()
{
    cvartemplate_t cvars[] = {
        {"game-music", READONLYCVAR, CVT_INT, &gsvCurrentMusic, 0, 0, 0},
        {"game-state", READONLYCVAR, CVT_INT, &gameState, 0, 0, 0},
        {"game-state-map", READONLYCVAR, CVT_INT, &gsvInMap, 0, 0, 0},
#if !__JHEXEN__
        {"game-stats-kills", READONLYCVAR, CVT_INT, &gsvKills, 0, 0, 0},
        {"game-stats-items", READONLYCVAR, CVT_INT, &gsvItems, 0, 0, 0},
        {"game-stats-secrets", READONLYCVAR, CVT_INT, &gsvSecrets, 0, 0, 0},
#endif

        {"map-author", READONLYCVAR, CVT_CHARPTR, &gsvMapAuthor, 0, 0, 0},
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
        {NULL, 0, CVT_NULL, 0, 0, 0, 0}
    };
    Con_AddVariableList(cvars);
}

void G_ConsoleRegister()
{
    GameSession::consoleRegister();

    C_VAR_BYTE("game-save-confirm",              &cfg.confirmQuickGameSave,  0, 0, 1);
    /* Alias */ C_VAR_BYTE("menu-quick-ask",     &cfg.confirmQuickGameSave,  0, 0, 1);
    C_VAR_BYTE("game-save-confirm-loadonreborn", &cfg.confirmRebornLoad,     0, 0, 1);
    C_VAR_BYTE("game-save-last-loadonreborn",    &cfg.loadLastSaveOnReborn,  0, 0, 1);

    C_CMD("deletegamesave",     "ss",       DeleteSavedSession);
    C_CMD("deletegamesave",     "s",        DeleteSavedSession);
    C_CMD("endgame",            "s",        EndSession);
    C_CMD("endgame",            "",         EndSession);
    C_CMD("helpscreen",         "",         HelpScreen);
    C_CMD("leavemap",           "",         LeaveMap);
    C_CMD("leavemap",           "s",        LeaveMap);
    C_CMD("loadgame",           "ss",       LoadSession);
    C_CMD("loadgame",           "s",        LoadSession);
    C_CMD("loadgame",           "",         OpenLoadMenu);
    C_CMD("quickload",          "",         QuickLoadSession);
    C_CMD("quicksave",          "",         QuickSaveSession);
    C_CMD("savegame",           "sss",      SaveSession);
    C_CMD("savegame",           "ss",       SaveSession);
    C_CMD("savegame",           "s",        SaveSession);
    C_CMD("savegame",           "",         OpenSaveMenu);
    C_CMD("togglegamma",        "",         CycleTextureGamma);
    C_CMD("warp",               nullptr,    WarpMap);
    /* Alias */ C_CMD("setmap", nullptr,    WarpMap);

    registerGameStatusCVars();
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

void G_SetGameActionNewSession(GameRuleset const &rules, String episodeId,
    de::Uri const &mapUri, uint mapEntrance)
{
    gaNewSessionRules       = rules; // make a copy.
    gaNewSessionEpisodeId   = episodeId;
    gaNewSessionMapUri      = mapUri;
    gaNewSessionMapEntrance = mapEntrance;

    G_SetGameAction(GA_NEWSESSION);
}

bool G_SetGameActionSaveSession(String slotId, String *userDescription)
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

bool G_SetGameActionLoadSession(String slotId)
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

        LOG_RES_ERROR("Cannot load from save slot '%s': not in use") << slotId;
    }
    catch(SaveSlots::MissingSlotError const &)
    {}

    return false;
}

void G_SetGameActionMapCompleted(de::Uri const &nextMapUri, uint nextMapEntryPoint, bool secretExit)
{
#if __JHEXEN__
    DENG2_UNUSED(secretExit);
#else
    DENG2_UNUSED(nextMapEntryPoint);
#endif

    if(IS_CLIENT) return;
    if(::cyclingMaps && ::mapCycleNoExit) return;

#if __JHEXEN__
    if((::gameMode == hexen_betademo || ::gameMode == hexen_demo) &&
       !(nextMapUri.path() == "MAP01" ||
         nextMapUri.path() == "MAP02" ||
         nextMapUri.path() == "MAP03" ||
         nextMapUri.path() == "MAP04"))
    {
        // Not possible in the 4-map demo.
        P_SetMessage(&players[CONSOLEPLAYER], 0, "PORTAL INACTIVE -- DEMO");
        return;
    }
#endif

    ::nextMapUri        = nextMapUri;
#if __JHEXEN__
    ::nextMapEntryPoint = nextMapEntryPoint;
#else
    ::secretExit        = secretExit;

# if __JDOOM__
    // If no Wolf3D maps, no secret exit!
    if(::secretExit && (::gameModeBits & GM_ANY_DOOM2))
    {
        if(!P_MapExists(de::Uri("Maps:MAP31", RC_NULL).compose().toUtf8().constData()))
        {
            ::secretExit = false;
        }
    }
# endif
#endif

    G_SetGameAction(GA_MAPCOMPLETED);
}

void G_SetGameActionMapCompletedAndSetNextMap()
{
    G_SetGameActionMapCompleted(COMMON_GAMESESSION->mapUriForNamedExit("next"));
}

static void initSaveSlots()
{
    delete sslots;
    sslots = new SaveSlots;

    // Setup the logical save slot bindings.
    uint const gameMenuSaveSlotWidgetIds[NUMSAVESLOTS] = {
        MNF_ID0, MNF_ID1, MNF_ID2, MNF_ID3, MNF_ID4, MNF_ID5,
#if !__JHEXEN__
        MNF_ID6, MNF_ID7
#endif
    };
    for(int i = 0; i < NUMSAVESLOTS; ++i)
    {
        sslots->add(String::number(i), true, String(SAVEGAMENAME"%1").arg(i),
                    gameMenuSaveSlotWidgetIds[i]);
    }
}

/**
 * Common Pre Game Initialization routine.
 * Game-specfic pre init actions should be placed in eg D_PreInit() (for jDoom)
 */
void G_CommonPreInit()
{
    quitInProgress = false;
    verbose = CommandLine_Exists("-verbose");

    // Apply the default game rules.
    COMMON_GAMESESSION->applyNewRules(defaultGameRules = GameRuleset());

    // Register hooks.
    Plug_AddHook(HOOK_DEMO_STOP, Hook_DemoStop);

    // Setup the players.
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t *pl = &players[i];

        pl->plr = DD_GetPlayer(i);
        pl->plr->extraData = (void *) pl;

        /// @todo Only necessary because the engine does not yet unload game plugins when they
        /// are not in use; thus a game change may leave these pointers dangling.
        for(int k = 0; k < NUMPSPRITES; ++k)
        {
            pl->pSprites[k].state = nullptr;
            pl->plr->pSprites[k].statePtr = nullptr;
        }
    }

    G_RegisterBindClasses();
    P_RegisterMapObjs();

    R_LoadVectorGraphics();
    R_LoadColorPalettes();

    P_InitPicAnims();

    // Add our cvars and ccmds to the console databases.
    G_ConsoleRegistration();      // Main command list.
    D_NetConsoleRegister();       // For network.
    G_ConsoleRegister();          // Top level game cvars and commands.
    Pause_Register();
    G_ControlRegister();          // For controls/input.
    SaveSlots::consoleRegister(); // Game-save system.
    Hu_MenuRegister();            // For the menu.
    GUI_Register();               // For the UI library.
    Hu_MsgRegister();             // For the game messages.
    ST_Register();                // For the hud/statusbar.
    IN_ConsoleRegister();         // For the interlude/intermission.
    X_Register();                 // For the crosshair.
    FI_StackRegister();           // For the InFine lib.
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_Register();
#endif

    Con_SetString2("map-author", "Unknown", SVF_WRITE_OVERRIDE);
    Con_SetString2("map-name",   "Unknown", SVF_WRITE_OVERRIDE);
}

#if __JHEXEN__
/**
 * @todo all this swapping colors around is rather silly, why not simply
 * reorder the translation tables at load time?
 */
void R_GetTranslation(int plrClass, int plrColor, int *tclass, int *tmap)
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
        int const mapping[3][4] = {
            /* Fighter */ { 1, 2, 0, 3 },
            /* Cleric */  { 1, 0, 2, 3 },
            /* Mage */    { 1, 0, 2, 3 }
        };
        DENG2_ASSERT(plrClass >= 0 && plrClass < 3);
        DENG2_ASSERT(plrColor >= 0 && plrColor < 4);
        mapped = mapping[plrClass][plrColor];
    }
    else
    {
        int const mapping[3][8] = {
            /* Fighter */ { 1, 2, 0, 3, 4, 5, 6, 7 },
            /* Cleric */  { 1, 0, 2, 3, 4, 5, 6, 7 },
            /* Mage */    { 1, 0, 2, 3, 4, 5, 6, 7 }
        };
        DENG2_ASSERT(plrClass >= 0 && plrClass < 3);
        DENG2_ASSERT(plrColor >= 0 && plrColor < 8);
        mapped = mapping[plrClass][plrColor];
    }

    *tclass = (mapped? plrClass : 0);
    *tmap   = mapped;
}

void Mobj_UpdateTranslationClassAndMap(mobj_t *mo)
{
    DENG2_ASSERT(mo);
    if(mo->player)
    {
        int plrColor = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
        R_GetTranslation(mo->player->class_, plrColor, &mo->tclass, &mo->tmap);
    }
    else if(mo->flags & MF_TRANSLATION)
    {
        mo->tclass = mo->special1;
        mo->tmap   = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
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

    File1 &playpal = CentralLumpIndex()[CentralLumpIndex().findLast(String(PALLUMPNAME) + ".lmp")];

    customPal = playpal.hasCustom(); // Remember whether we are using a custom palette.

    uint8_t colors[PALENTRIES * 3];
    playpal.read(colors, 0 + PALID * (PALENTRIES * 3), PALENTRIES * 3);
    colorpaletteid_t palId = R_CreateColorPalette("R8G8B8", PALLUMPNAME, colors, PALENTRIES);

    ddstring_s xlatId; Str_InitStd(&xlatId);

#if __JHEXEN__
    // Load the translation tables.
    {
        int const numPerClass = (gameMode == hexen_v10? 3 : 7);

        // In v1.0, the color translations are a bit different. There are only
        // three translation maps per class, whereas Doomsday assumes seven maps
        // per class. Thus we'll need to account for the difference.

        int xlatNum = 0;
        for(int cl = 0; cl < 3; ++cl)
        for(int i = 0; i < 7; ++i)
        {
            if(i == numPerClass) break; // Not present.

            String lumpName;
            if(xlatNum < 10)
            {
                lumpName = String("TRANTBL%1").arg(xlatNum);
            }
            else
            {
                lumpName = String("TRANTBL%1").arg('A' + (xlatNum - 10));
            }
            xlatNum++;

            LOGDEV_RES_MSG("Reading translation table '%s' as tclass=%i tmap=%i")
                    << lumpName << cl << i;

            lumpName += ".lmp";
            if(CentralLumpIndex().contains(lumpName))
            {
                File1 &lump = CentralLumpIndex()[CentralLumpIndex().findLast(lumpName)];
                uint8_t const *mappings = lump.cache();
                Str_Appendf(Str_Clear(&xlatId), "%i", 7 * cl + i);
                R_CreateColorPaletteTranslation(palId, &xlatId, mappings);
                lump.unlock();
            }
        }
    }
#else
    // Create the translation tables to map the green color ramp to gray,
    // brown, red. Could be read from a lump instead?
    {
        uint8_t xlat[PALENTRIES];
        for(int xlatNum = 0; xlatNum < 3; ++xlatNum)
        {
            // Translate just the 16 green colors.
            for(int palIdx = 0; palIdx < 256; ++palIdx)
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
fontid_t R_MustFindFontForName(char const *name)
{
    uri_s *uri = Uri_NewWithPath2(name, RC_NULL);
    fontid_t fontId = Fonts_ResolveUri(uri);
    Uri_Delete(uri);
    if(fontId) return fontId;
    Con_Error("Failed loading font \"%s\".", name);
    exit(1); // Unreachable.
}

void R_InitRefresh()
{
    if(IS_DEDICATED) return;

    LOG_RES_VERBOSE("Loading data for refresh...");

    // Setup the view border.
    cfg.screenBlocks = cfg.setBlocks;
    {
        uri_s *paths[9];
        for(int i = 0; i < 9; ++i)
        {
            paths[i] = ((borderGraphics[i] && borderGraphics[i][0])? Uri_NewWithPath2(borderGraphics[i], RC_NULL) : 0);
        }
        R_SetBorderGfx((uri_s const **)paths);
        for(int i = 0; i < 9; ++i)
        {
            if(paths[i])
            {
                Uri_Delete(paths[i]);
            }
        }
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

    float mul = 1.4f;
    DD_SetVariable(DD_PSPRITE_LIGHTLEVEL_MULTIPLIER, &mul);
}

void R_InitHud()
{
    Hu_LoadData();

#if __JHERETIC__ || __JHEXEN__
    LOG_VERBOSE("Initializing inventory...");
    Hu_InventoryInit();
#endif

    LOG_VERBOSE("Initializing statusbar...");
    ST_Init();

    LOG_VERBOSE("Initializing menu...");
    Hu_MenuInit();

    LOG_VERBOSE("Initializing status-message/question system...");
    Hu_MsgInit();
}

SaveSlots &G_SaveSlots()
{
    DENG2_ASSERT(sslots != 0);
    return *sslots;
}

/**
 * @attention Game-specific post init actions should be placed in the game-appropriate
 * post init routine (e.g., D_PostInit() for libdoom) and NOT here.
 */
void G_CommonPostInit()
{
    R_InitRefresh();
    FI_StackInit();
    GUI_Init();

#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    XG_ReadTypes();
#endif

    LOG_VERBOSE("Initializing playsim...");
    P_Init();

    LOG_VERBOSE("Initializing head-up displays...");
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

void G_AutoStartOrBeginTitleLoop()
{
    CommandLine &cmdLine = DENG2_APP->commandLine();

    String startEpisodeId;
    de::Uri startMapUri;

    // A specific episode?
    if(int arg = cmdLine.check("-episode", 1))
    {
        String episodeId = cmdLine.at(arg + 1);
        if(Record const *episodeDef = Defs().episodes.tryFind("id", episodeId))
        {
            // Ensure this is a playable episode.
            de::Uri startMap(episodeDef->gets("startMap"), RC_NULL);
            if(P_MapExists(startMap.compose().toUtf8().constData()))
            {
                startEpisodeId = episodeId;
            }
        }
    }

    // A specific map?
    if(int arg = cmdLine.check("-warp", 1))
    {
        bool haveEpisode = (arg + 2 < cmdLine.count() && !cmdLine.isOption(arg + 2));
        if(haveEpisode)
        {
            if(Record const *episodeDef = Defs().episodes.tryFind("id", cmdLine.at(arg + 1)))
            {
                // Ensure this is a playable episode.
                de::Uri startMap(episodeDef->gets("startMap"), RC_NULL);
                if(P_MapExists(startMap.compose().toUtf8().constData()))
                {
                    startEpisodeId = episodeDef->gets("id");
                }
            }
        }

        // The map.
        bool isNumber;
        int oldMapNumber = cmdLine.at(arg + (haveEpisode? 2 : 1)).toInt(&isNumber);
#if !__JHEXEN__
        if(oldMapNumber > 0) oldMapNumber -= 1; // zero-based.
#endif

        if(!isNumber)
        {
            // It must be a URI, then.
            Block rawMapUri = cmdLine.at(arg + (haveEpisode? 2 : 1)).toUtf8();
            char *args[1] = { const_cast<char *>(rawMapUri.constData()) };
            startMapUri = de::Uri::fromUserInput(args, 1);
            if(startMapUri.scheme().isEmpty()) startMapUri.setScheme("Maps");
        }
        else if(!startEpisodeId.isEmpty())
        {
#if __JHEXEN__
            // Map numbers must be translated in the context of an episode.
            startMapUri = TranslateMapWarpNumber(startEpisodeId, oldMapNumber);
#else
            int oldEpisodeNumber = startEpisodeId.toInt(&isNumber);
            if(oldEpisodeNumber > 0) oldEpisodeNumber -= 1; // zero-based.
            if(isNumber)
            {
                startMapUri = G_ComposeMapUri(oldEpisodeNumber, oldMapNumber);
            }
#endif
        }
        else
        {
            startMapUri = G_ComposeMapUri(0, oldMapNumber);
        }
    }

    // Are we attempting an auto-start?
    bool autoStart = (IS_NETGAME || !startEpisodeId.isEmpty() || !startMapUri.isEmpty());
    if(autoStart)
    {
        if(startEpisodeId.isEmpty())
        {
            // Pick the first playable episode.
            for(auto const &pair : Defs().episodes.lookup("id").elements())
            {
                Record const &episodeDef = *pair.second->as<RecordValue>().record();
                de::Uri startMap(episodeDef.gets("startMap"), RC_NULL);
                if(P_MapExists(startMap.compose().toUtf8().constData()))
                {
                    startEpisodeId = episodeDef.gets("id");
                    break;
                }
            }
        }

        // Ensure that the map exists.
        if(!P_MapExists(startMapUri.compose().toUtf8().constData()))
        {
            startMapUri.clear();

            // Pick the start map from the episode, if specified and playable.
            if(Record const *episodeDef = Defs().episodes.tryFind("id", startEpisodeId))
            {
                de::Uri startMap(episodeDef->gets("startMap"), RC_NULL);
                if(P_MapExists(startMap.compose().toUtf8().constData()))
                {
                    startMapUri = startMap;
                }
            }
        }
    }

    // Are we auto-starting?
    if(!startEpisodeId.isEmpty() && !startMapUri.isEmpty())
    {
        LOG_NOTE("Auto-starting episode '%s', map \"%s\", skill %i")
                << startEpisodeId
                << startMapUri
                << ::defaultGameRules.skill;

        // Don't brief when autostarting.
        ::briefDisabled = true;

        G_SetGameActionNewSession(::defaultGameRules, startEpisodeId, startMapUri);
    }
    else
    {
        COMMON_GAMESESSION->endAndBeginTitle(); // Start up intro loop.
    }
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
        if((gameAction == GA_NONE && !singledemo && Get(DD_PLAYBACK)) ||
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
    if(G_QuitInProgress()) return;

    if(state < 0 || state >= NUM_GAME_STATES)
    {
        DENG2_ASSERT(!"G_ChangeGameState: Invalid state");
        return;
    }

    if(gameState != state)
    {
        gameState = state;
        LOGDEV_NOTE("Game state changed to %s") << getGameStateStr(state);
    }

    // Update the state of the gameui binding context.
    bool gameUIActive = false;
    bool gameActive   = true;
    switch(gameState)
    {
    case GS_FINALE:
    case GS_STARTUP:
    case GS_WAITING:
    case GS_INFINE:
        gameActive = false;
        // Fall through.

    case GS_INTERMISSION:
        gameUIActive = true;
        break;

    default: break;
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
        ST_LogEmpty(i);               // Clear the message queue for all local players.
        ST_CloseAll(i, true/*fast*/); // Close the HUDs left open for all local players.
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

    char const *scriptId = "help";
    if(Record const *finale = Defs().finales.tryFind("id", scriptId))
    {
        Hu_MenuCommand(MCMD_CLOSEFAST);
        G_StartFinale(finale->gets("script").toUtf8().constData(), FF_LOCAL, FIMODE_NORMAL, scriptId);
        return;
    }
    LOG_SCR_WARNING("InFine script '%s' not defined") << scriptId;
}

/**
 * Prints a banner to the console containing information pertinent to the referenced map
 * (e.g., title, author...).
 */
static void printMapBanner(String episodeId, de::Uri const &mapUri)
{
    LOG_MSG(DE2_ESC(R));

    String const title = G_MapTitle(mapUri);
    if(!title.isEmpty())
    {
        Record const *mgNodeDef = 0;
        if(Record const *episode = Defs().episodes.tryFind("id", episodeId))
        {
            mgNodeDef = defn::Episode(*episode).tryFindMapGraphNode(mapUri.compose());
        }

        LOG_NOTE("Map: " DE2_ESC(i) DE2_ESC(b) "%s" DE2_ESC(.) " (%s%s)")
                << title << mapUri
                << (mgNodeDef? String(", warp: %1").arg(mgNodeDef->geti("warpNumber")) : "");
    }

    String const author = G_MapAuthor(mapUri, P_MapIsCustom(mapUri.compose().toUtf8().constData()));
    if(!author.isEmpty())
    {
        LOG_NOTE("Author: " DE2_ESC(i)) << author;
    }

    String const episodeTitle = G_EpisodeTitle(episodeId);
    if(!episodeTitle.isEmpty())
    {
        LOG_NOTE("Episode: " DE2_ESC(i)) << episodeTitle;
    }

    LOG_MSG(DE2_ESC(R));
}

void G_BeginMap()
{
    G_ChangeGameState(GS_MAP);

    if(!IS_DEDICATED)
    {
        R_SetViewPortPlayer(CONSOLEPLAYER, CONSOLEPLAYER); // View the guy you are playing.
        R_ResizeViewWindow(RWF_FORCE|RWF_NO_LERP);
    }

    // Clear all controls for all local players.
    G_ControlReset(-1);

    // Time can now progress in this map.
    mapTime = actualMapTime = 0;

    // The music may have been paused for the briefing; unpause.
    S_PauseMusic(false);

    printMapBanner(COMMON_GAMESESSION->episodeId(), COMMON_GAMESESSION->mapUri());
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
    DENG2_ASSERT(ev);

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
            {
                G_SetGameAction(GA_SCREENSHOT);
            }
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

static sfxenum_t randomQuitSound()
{
#if __JDOOM__ || __JDOOM64__
    if(cfg.menuQuitSound)
    {
# if __JDOOM64__
        static sfxenum_t quitSounds[] = {
            SFX_VILACT,
            SFX_GETPOW,
            SFX_PEPAIN,
            SFX_SLOP,
            SFX_SKESWG,
            SFX_KNTDTH,
            SFX_BSPACT,
            SFX_SGTATK
        };
        static int numQuitSounds = sizeof(quitSounds) / sizeof(quitSounds[0]);
# else
        static sfxenum_t quitSounds[] = {
            SFX_PLDETH,
            SFX_DMPAIN,
            SFX_POPAIN,
            SFX_SLOP,
            SFX_TELEPT,
            SFX_POSIT1,
            SFX_POSIT3,
            SFX_SGTATK
        };
        static int numQuitSounds = sizeof(quitSounds) / sizeof(quitSounds[0]);

        static sfxenum_t quitSounds2[] = {
            SFX_VILACT,
            SFX_GETPOW,
            SFX_BOSCUB,
            SFX_SLOP,
            SFX_SKESWG,
            SFX_KNTDTH,
            SFX_BSPACT,
            SFX_SGTATK
        };
        static int numQuitSounds2 = sizeof(quitSounds2) / sizeof(quitSounds2[0]);
# endif
        sfxenum_t *sndTable = quitSounds;
        int sndTableSize    = numQuitSounds;

# if !__JDOOM64__
        if(gameModeBits & GM_ANY_DOOM2)
        {
            sndTable     = quitSounds2;
            sndTableSize = numQuitSounds2;
        }
# endif

        if(sndTable && sndTableSize > 0)
        {
            return sndTable[P_Random() & (sndTableSize - 1)];
        }
    }
#endif

    return SFX_NONE;
}

/**
 * Determines whether an intermission is enabled and will be scheduled when the players
 * leave the  @em current map.
 */
static bool intermissionEnabled()
{
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
    if(Record const *mapInfo = COMMON_GAMESESSION->mapInfo())
    {
        if(mapInfo->geti("flags") & MIF_NO_INTERMISSION)
        {
            return false;
        }
    }
#elif __JHEXEN__
    if(!COMMON_GAMESESSION->rules().deathmatch)
    {
        return false;
    }
#endif
    return true;
}

/**
 * Returns the unique identifier of the music to play during the intermission.
 */
static String intermissionMusic()
{
#if __JDOOM64__
    return "dm2int";
#elif __JDOOM__
    return (::gameModeBits & GM_ANY_DOOM2)? "dm2int" : "inter";
#elif __JHERETIC__
    return "intr";
#elif __JHEXEN__
    return "hub";
#endif
}

#if __JDOOM__ || __JDOOM64__
void G_PrepareWIData()
{
    wbstartstruct_t *info = &::wmInfo;

    info->maxFrags = 0;

    // See if there is a par time definition.
    info->parTime = -1; // Unknown.
    if(Record const *mapInfo = COMMON_GAMESESSION->mapInfo())
    {
        float parTime = mapInfo->getf("parTime");
        if(parTime > 0)
        {
            info->parTime = TICRATE * int(parTime);
        }
    }

    info->pNum = CONSOLEPLAYER;
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        player_t const *p        = &players[i];
        wbplayerstruct_t *pStats = &info->plyr[i];

        pStats->inGame = p->plr->inGame;
        pStats->kills  = p->killCount;
        pStats->items  = p->itemCount;
        pStats->secret = p->secretCount;
        pStats->time   = mapTime;
        std::memcpy(pStats->frags, p->frags, sizeof(pStats->frags));
    }
}
#endif

static int prepareIntermission(void * /*context*/)
{
    ::wmInfo.nextMap           = ::nextMapUri;
#if __JHEXEN__
    ::wmInfo.nextMapEntryPoint = ::nextMapEntryPoint;
#endif
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    ::wmInfo.currentMap        = COMMON_GAMESESSION->mapUri();
    ::wmInfo.didSecret         = ::players[CONSOLEPLAYER].didSecret;
# if __JDOOM__ || __JDOOM64__
    ::wmInfo.maxKills          = de::max(1, ::totalKills);
    ::wmInfo.maxItems          = de::max(1, ::totalItems);
    ::wmInfo.maxSecret         = de::max(1, ::totalSecret);

    G_PrepareWIData();
# endif
#endif

    IN_Begin(::wmInfo);
    G_ChangeGameState(GS_INTERMISSION);

    BusyMode_WorkerEnd();
    return 0;
}

static void runGameAction()
{
#define QUITWAIT_MILLISECONDS 1500

    static uint quitTime = 0;

    // Run the quit countdown?
    if(::quitInProgress)
    {
        if(Timer_RealMilliseconds() > quitTime + QUITWAIT_MILLISECONDS)
        {
            Sys_Quit();
        }
        else
        {
            ::quitDarkenOpacity = de::cubed((Timer_RealMilliseconds() - quitTime) / (float) QUITWAIT_MILLISECONDS);
        }

        // No further game state changes occur once we have begun to quit.
        return;
    }

    // Do things to change the game state.
    gameaction_t currentAction;
    while((currentAction = ::gameAction) != GA_NONE)
    {
        BusyMode_FreezeGameForBusyMode();

        // The topmost action will now be processed.
        G_SetGameAction(GA_NONE);

        switch(currentAction)
        {
        case GA_NEWSESSION:
            COMMON_GAMESESSION->end();
            COMMON_GAMESESSION->begin(::gaNewSessionRules, ::gaNewSessionEpisodeId,
                                      ::gaNewSessionMapUri, ::gaNewSessionMapEntrance);
            break;

        case GA_LOADSESSION:
            COMMON_GAMESESSION->end();

            try
            {
                SaveSlot const &sslot = G_SaveSlots()[::gaLoadSessionSlot];
                COMMON_GAMESESSION->load(sslot.saveName());

                // Make note of the last used save slot.
                Con_SetInteger2("game-save-last-slot", sslot.id().toInt(), SVF_WRITE_OVERRIDE);
            }
            catch(Error const &er)
            {
                LOG_RES_WARNING("Error loading from save slot #%s:\n")
                        << ::gaLoadSessionSlot << er.asText();
            }

            // Return to the title loop if loading did not succeed.
            if(!COMMON_GAMESESSION->hasBegun())
            {
                COMMON_GAMESESSION->endAndBeginTitle();
            }
            break;

        case GA_SAVESESSION:
            try
            {
                SaveSlot const &sslot = G_SaveSlots()[::gaSaveSessionSlot];
                COMMON_GAMESESSION->save(sslot.saveName(), ::gaSaveSessionUserDescription);

                // Make note of the last used save slot.
                Con_SetInteger2("game-save-last-slot", sslot.id().toInt(), SVF_WRITE_OVERRIDE);
            }
            catch(Error const &er)
            {
                LOG_RES_WARNING("Error saving to save slot #%s:\n")
                        << ::gaSaveSessionSlot << er.asText();
            }
            break;

        case GA_QUIT:
            quitTime = Timer_RealMilliseconds();
            ::quitInProgress = true;

            Hu_MenuCommand(MCMD_CLOSEFAST);

            if(!IS_NETGAME)
            {
                // Play an exit sound if it is enabled.
                S_LocalSound(randomQuitSound(), 0);
                DD_Executef(true, "activatebcontext deui");
            }
            break;

        case GA_LEAVEMAP:
            // Check that the map truly exists.
            if(!P_MapExists(::nextMapUri.compose().toUtf8().constData()))
            {
                ::nextMapUri = de::Uri(COMMON_GAMESESSION->episodeDef()->gets("startMap"), RC_NULL);
            }
            COMMON_GAMESESSION->leaveMap(::nextMapUri, ::nextMapEntryPoint);
            break;

        case GA_RESTARTMAP:
            COMMON_GAMESESSION->reloadMap();
            break;

        case GA_MAPCOMPLETED: {
            // Leaving the current hub?
            dd_bool newHub = true;
#if __JHEXEN__
            if(Record const *episodeDef = COMMON_GAMESESSION->episodeDef())
            {
                defn::Episode epsd(*episodeDef);
                Record const *currentHub = epsd.tryFindHubByMapId(COMMON_GAMESESSION->mapUri().compose());
                newHub = (!currentHub || currentHub != epsd.tryFindHubByMapId(::nextMapUri.compose()));
            }
#endif

            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                ST_CloseAll(i, true/*fast*/);         // hide any HUDs left open
                Player_LeaveMap(&players[i], newHub); // take away cards and stuff
            }

#if __JHEXEN__
            SN_StopAllSequences();
#endif

            if(!IS_DEDICATED)
            {
                GL_SetFilter(false);
            }

            // Go to an intermission?
            if(intermissionEnabled())
            {
#if __JDOOM__
                // Has the secret map been completed?
                if(::gameModeBits & (GM_DOOM | GM_DOOM_SHAREWARE | GM_DOOM_ULTIMATE))
                {
                    de::Uri const mapUri = COMMON_GAMESESSION->mapUri();
                    if(mapUri.path() == "E1M9" ||
                       mapUri.path() == "E2M9" ||
                       mapUri.path() == "E3M9" ||
                       mapUri.path() == "E4M9")
                    {
                        for(int i = 0; i < MAXPLAYERS; ++i)
                        {
                            ::players[i].didSecret = true;
                        }
                    }
                }
#endif

                S_StartMusic(intermissionMusic().toUtf8().constData(), true);
                S_PauseMusic(true);

                BusyMode_RunNewTask(BUSYF_TRANSITION, prepareIntermission, nullptr);
#if __JHERETIC__
                NetSv_SendGameState(0, DDSP_ALL_PLAYERS); // @todo necessary at this time?
#endif
                NetSv_Intermission(IMF_BEGIN, 0, 0);

                S_PauseMusic(false);
            }
            else
            {
                G_IntermissionDone();
            }
            break; }

        case GA_ENDDEBRIEFING:
            ::briefDisabled = true;
            G_IntermissionDone();
            break;

        case GA_SCREENSHOT: {
            // Find an unused screenshot file name.
            String fileName = COMMON_GAMESESSION->gameId() + "-";
            int const numPos = fileName.length();
            for(int i = 0; i < 1e6; ++i) // Stop eventually...
            {
                fileName += String("%1.png").arg(i, 3, 10, QChar('0'));
                if(!F_FileExists(fileName.toUtf8().constData())) break;
                fileName.truncate(numPos);
            }

            if(M_ScreenShot(fileName.toUtf8().constData(), 24))
            {
                /// @todo Do not use the console player's message log for this notification.
                ///       The engine should implement it's own notification UI system for
                ///       this sort of thing.
                String msg = "Saved screenshot: " + NativePath(fileName).pretty();
                P_SetMessage(&::players[CONSOLEPLAYER], LMF_NO_HIDE, msg.toLatin1().constData());
            }
            else
            {
                LOG_RES_WARNING("Failed taking screenshot \"%s\"")
                        << NativePath(fileName).pretty();
            }
            break; }

        default: break;
        }
    }

#undef QUITWAIT_MILLISECONDS
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
    // Reborns are impossible if no game session is in progress.
    if(!COMMON_GAMESESSION->hasBegun()) return;
    // ...or if no map is currently loaded.
    if(G_GameState() != GS_MAP) return;

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
                AutoStr *msg = Str_Appendf(AutoStr_NewStd(), REBORNLOAD_CONFIRM, COMMON_GAMESESSION->userDescription().toUtf8().constData());
                Hu_MsgStart(MSG_YESNO, Str_Text(msg), rebornLoadConfirmed, 0, 0);
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
                LOGDEV_MAP_MSG("rebornPlayers: Removing player %i's mobj") << i;

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

            // Activate briefings once again (disabled for autostart or loading a saved game).
            briefDisabled = false;

            if(IS_DEDICATED)
                break;

            Hu_Ticker();
            break;

        case GS_INTERMISSION:
            IN_Ticker();
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
        {
            NetSv_Ticker();
        }
    }

    oldGameState = gameState;
}

/**
 * Safely clears the player data structures.
 */
static void clearPlayer(player_t *p)
{
    DENG2_ASSERT(p);

    player_t playerCopy;
    ddplayer_t ddPlayerCopy;

    // Take a backup of the old data.
    std::memcpy(&playerCopy, p, sizeof(*p));
    std::memcpy(&ddPlayerCopy, p->plr, sizeof(*p->plr));

    // Clear everything.
    de::zapPtr(p->plr);
    de::zapPtr(p);

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
    p->plr->flags  = ddPlayerCopy.flags & ~(DDPF_INTERYAW | DDPF_INTERPITCH);

    // Don't clear the start spot.
    p->startSpot = playerCopy.startSpot;

    // Restore counters.
    std::memcpy(&p->plr->fixCounter, &ddPlayerCopy.fixCounter, sizeof(ddPlayerCopy.fixCounter));
    std::memcpy(&p->plr->fixAcked,   &ddPlayerCopy.fixAcked,   sizeof(ddPlayerCopy.fixAcked));

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

    LOGDEV_MAP_NOTE("G_PlayerReborn: reseting player %i") << player;

    player_t *p = &players[player];

    int frags[MAXPLAYERS];
    DENG2_ASSERT(sizeof(p->frags) == sizeof(frags));
    std::memcpy(frags, p->frags, sizeof(frags));

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

    std::memcpy(p->frags, frags, sizeof(p->frags));
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

    de::Uri const mapUri = COMMON_GAMESESSION->mapUri();
    if(secret ||
       (mapUri.path() == "E1M9" ||
        mapUri.path() == "E2M9" ||
        mapUri.path() == "E3M9" ||
        mapUri.path() == "E4M9" ||
        mapUri.path() == "E5M9"))
    {
        p->didSecret = true;
    }

#ifdef DENG2_DEBUG
    for(int i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        LOGDEV_MAP_MSG("Player %i owns wpn %i: %i") << player << i << p->weapons[i].owned;
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

int G_Ruleset_Skill()
{
    return COMMON_GAMESESSION->rules().skill;
}

#if !__JHEXEN__
byte G_Ruleset_Fast()
{
    return COMMON_GAMESESSION->rules().fast;
}
#endif

byte G_Ruleset_Deathmatch()
{
    return COMMON_GAMESESSION->rules().deathmatch;
}

byte G_Ruleset_NoMonsters()
{
    return COMMON_GAMESESSION->rules().noMonsters;
}

#if __JHEXEN__
byte G_Ruleset_RandomClasses()
{
    return COMMON_GAMESESSION->rules().randomClasses;
}
#endif

#if !__JHEXEN__
byte G_Ruleset_RespawnMonsters()
{
    return COMMON_GAMESESSION->rules().respawnMonsters;
}
#endif

/**
 * Lookup the debriefing Finale for the current episode and map (if any).
 */
static Record const *finaleDebriefing()
{
    if(::briefDisabled) return 0;

#if __JHEXEN__
    if(::cfg.overrideHubMsg && G_GameState() == GS_MAP)
    {
        defn::Episode epsd(*COMMON_GAMESESSION->episodeDef());
        Record const *currentHub = epsd.tryFindHubByMapId(COMMON_GAMESESSION->mapUri().compose());
        if(!currentHub || currentHub != epsd.tryFindHubByMapId(::nextMapUri.compose()))
        {
            return 0;
        }
    }
#endif

    // In a networked game the server will schedule the debrief.
    if(IS_CLIENT || Get(DD_PLAYBACK)) return 0;

    // If we're already in the INFINE state, don't start a finale.
    if(G_GameState() == GS_INFINE) return 0;

    // Is there such a finale definition?
    return Defs().finales.tryFind("after", COMMON_GAMESESSION->mapUri().compose());
}

/// @todo common::GameSession should handle this -ds
void G_IntermissionDone()
{
    // We have left Intermission, however if there is an InFine for debriefing we should run it now.
    if(Record const *finale = finaleDebriefing())
    {
        if(G_StartFinale(finale->gets("script").toUtf8().constData(), 0, FIMODE_AFTER, 0))
        {
            // The GA_ENDDEBRIEFING action is taken after the debriefing stops.
            return;
        }
    }

    // We have either just returned from a debriefing or there wasn't one.
    ::briefDisabled = false;

#if __JDOOM__ || __JDOOM64__
    if(::secretExit)
    {
        ::players[CONSOLEPLAYER].didSecret = true;
    }
#endif

    // Clear the currently playing script, if any.
    FI_StackClear();

    // Has the player completed the game?
    if(::nextMapUri.isEmpty())
    {
        // Victorious!
        G_SetGameAction(GA_VICTORY);
        return;
    }

    G_SetGameAction(GA_LEAVEMAP);
}

String G_DefaultSavedSessionUserDescription(String const &saveName, bool autogenerate)
{
    // If the slot is already in use then choose existing description.
    if(!saveName.isEmpty())
    {
        String const existing = COMMON_GAMESESSION->savedUserDescription(saveName);
        if(!existing.isEmpty()) return existing;
    }

    if(!autogenerate) return "";

    // Autogenerate a suitable description.
    String description;

    // Include the source file name, for custom maps.
    de::Uri const mapUri = COMMON_GAMESESSION->mapUri();
    String mapUriAsText  = mapUri.compose();
    if(P_MapIsCustom(mapUriAsText.toUtf8().constData()))
    {
        String const mapSourcePath(Str_Text(P_MapSourceFile(mapUriAsText.toUtf8().constData())));
        description += mapSourcePath.fileNameWithoutExtension() + ":";
    }

    // Include the map title.
    String mapTitle = G_MapTitle(mapUri);
    // No map title? Use the identifier. (Some tricksy modders provide us with an empty title).
    /// @todo Move this logic engine-side.
    if(mapTitle.isEmpty() || mapTitle.at(0) == ' ')
    {
        mapTitle = mapUri.path();
    }
    description += mapTitle;

    // Include the game time also.
    int time = mapTime / TICRATE;
    int const hours   = time / 3600; time -= hours * 3600;
    int const minutes = time / 60;   time -= minutes * 60;
    int const seconds = time;
    description += String(" %1:%2:%3")
                       .arg(hours,   2, 10, QChar('0'))
                       .arg(minutes, 2, 10, QChar('0'))
                       .arg(seconds, 2, 10, QChar('0'));

    return description;
}

uint G_MapNumberFor(de::Uri const &mapUri)
{
    String path = mapUri.path();
    if(!path.isEmpty())
    {
#if __JDOOM__ || __JHERETIC__
# if __JDOOM__
        if(gameModeBits & (GM_ANY_DOOM | ~GM_DOOM_CHEX))
# endif
        {
            if(path.at(0).toLower() == 'e' && path.at(2).toLower() == 'm')
            {
                return path.substr(3).toInt() - 1;
            }
        }
#endif
        if(path.beginsWith("map", Qt::CaseInsensitive))
        {
            return path.substr(3).toInt() - 1;
        }
    }
    return 0;
}

AutoStr *G_CurrentMapUriPath()
{
    return AutoStr_FromTextStd(COMMON_GAMESESSION->mapUri().path().toStringRef().toUtf8().constData());
}

de::Uri G_ComposeMapUri(uint episode, uint map)
{
    String mapId;
#if __JDOOM64__
    mapId = String("map%1").arg(map+1, 2, 10, QChar('0'));
    DENG2_UNUSED(episode);
#elif __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        mapId = String("map%1").arg(map+1, 2, 10, QChar('0'));
    else
        mapId = String("e%1m%2").arg(episode+1).arg(map+1);
#elif  __JHERETIC__
    mapId = String("e%1m%2").arg(episode+1).arg(map+1);
#else
    mapId = String("map%1").arg(map+1, 2, 10, QChar('0'));
    DENG2_UNUSED(episode);
#endif
    return de::Uri("Maps", mapId);
}

String G_EpisodeTitle(String episodeId)
{
    String title;
    if(Record const *episodeDef = Defs().episodes.tryFind("id", episodeId))
    {
        title = episodeDef->gets("title");
        // Perhaps the title string is a reference to a Text definition?
        void *ptr;
        if(Def_Get(DD_DEF_TEXT, title.toUtf8().constData(), &ptr) != -1)
        {
            title = (char const *) ptr; // Yes, use the resolved text string.
        }
    }
    return title;
}

String G_MapTitle(de::Uri const &mapUri)
{
    String title;

    // Perhaps a MapInfo definition exists for the map?
    if(Record const *mapInfo = Defs().mapInfos.tryFind("id", mapUri.compose()))
    {
        title = mapInfo->gets("title");
        // Perhaps the title string is a reference to a Text definition?
        void *ptr;
        if(Def_Get(DD_DEF_TEXT, title.toUtf8().constData(), &ptr) != -1)
        {
            title = (char const *) ptr; // Yes, use the resolved text string.
        }
    }

    // Skip the "ExMx" part, if present.
    int idSuffixAt = title.indexOf(':');
    if(idSuffixAt >= 0)
    {
        int subStart = idSuffixAt + 1;
        while(subStart < title.length() && title.at(subStart).isSpace()) { subStart++; }

        return title.substr(subStart);
    }

    return title;
}

String G_MapAuthor(de::Uri const &mapUri, bool supressGameAuthor)
{
    String author;

    // Perhaps a MapInfo definition exists for the map?
    String mapUriAsText = mapUri.compose();
    if(Record const *mapInfo = Defs().mapInfos.tryFind("id", mapUriAsText))
    {
        author = mapInfo->gets("author");
    }

    if(!author.isEmpty())
    {
        // Should we suppress the author?
        /// @todo Do not do this here.
        GameInfo gameInfo;
        DD_GameInfo(&gameInfo);
        if(supressGameAuthor || P_MapIsCustom(mapUriAsText.toUtf8().constData()))
        {
            if(!author.compareWithoutCase(Str_Text(gameInfo.author)))
                return "";
        }
    }

    return author;
}

#if __JDOOM__
static uint episodeNumberFor(de::Uri const &mapUri)
{
    String path = mapUri.path();
    if(!path.isEmpty())
    {
        if(gameModeBits & (GM_ANY_DOOM | ~GM_DOOM_CHEX))
        {
            if(path.at(0).toLower() == 'e' && path.at(2).toLower() == 'm')
            {
                return path.substr(1, 1).toInt() - 1;
            }
        }
    }
    return 0;
}
#endif // __JDOOM__

patchid_t G_MapTitlePatch(de::Uri const &mapUri)
{
#if __JDOOM__ || __JDOOM64__
    uint map = G_MapNumberFor(mapUri);
#  if __JDOOM__
    if(!(gameModeBits & (GM_ANY_DOOM2|GM_DOOM_CHEX)))
    {
        uint episode = episodeNumberFor(mapUri);
        map = (episode * 9) + map;
    }
#  endif
    if(map < pMapNamesSize)
    {
        return pMapNames[map];
    }
#else
    DENG2_UNUSED(mapUri);
#endif
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
        // Restore normal game state.
        GameRuleset newRules(COMMON_GAMESESSION->rules());
        newRules.deathmatch      = false;
        newRules.noMonsters      = false;
#if __JDOOM__ || __JHERETIC__ || __JDOOM64__
        newRules.respawnMonsters = false;
#endif
#if __JHEXEN__
        newRules.randomClasses   = false;
#endif
        COMMON_GAMESESSION->applyNewRules(newRules);
    }

    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_CloseAll(i, true/*fast*/);
    }

    return true;
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
    Hu_MsgStart(MSG_YESNO, endString, quitGameConfirmed, 0, nullptr);
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
        DD_Execute(true, "endgame confirm");
    }
    return true;
}

D_CMD(EndSession)
{
    DENG2_UNUSED3(src, argc, argv);

    if(G_QuitInProgress()) return true;

    if(IS_NETGAME && IS_SERVER)
    {
        LOG_NET_ERROR("Cannot end a networked game session. Stop the server instead");
        return false;
    }

    if(!COMMON_GAMESESSION->hasBegun())
    {
        if(IS_NETGAME && IS_CLIENT)
        {
            LOG_NET_ERROR("%s") << ENDNOGAME;
        }
        else
        {
            Hu_MsgStart(MSG_ANYKEY, ENDNOGAME, nullptr, 0, nullptr);
        }
        return true;
    }

    // Is user confirmation required? (Never if this is a network server).
    bool const confirmed = (argc >= 2 && !qstricmp(argv[argc-1], "confirm"));
    if(confirmed || (IS_NETGAME && IS_SERVER))
    {
        if(IS_NETGAME && IS_CLIENT)
        {
            DD_Executef(false, "net disconnect");
        }
        else
        {
            COMMON_GAMESESSION->endAndBeginTitle();
        }
    }
    else
    {
        Hu_MsgStart(MSG_YESNO, IS_CLIENT? GET_TXT(TXT_DISCONNECT) : ENDGAME, endSessionConfirmed, 0, nullptr);
    }

    return true;
}

static int loadSessionConfirmed(msgresponse_t response, int /*userValue*/, void *context)
{
    String *slotId = static_cast<String *>(context);
    DENG2_ASSERT(slotId != 0);
    if(response == MSG_YES)
    {
        DD_Executef(true, "loadgame %s confirm", slotId->toUtf8().constData());
    }
    delete slotId;
    return true;
}

D_CMD(LoadSession)
{
    DENG2_UNUSED(src);

    bool const confirmed = (argc == 3 && !qstricmp(argv[2], "confirm"));

    if(G_QuitInProgress()) return false;
    if(!COMMON_GAMESESSION->loadingPossible()) return false;

    if(IS_NETGAME)
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, nullptr);
        Hu_MsgStart(MSG_ANYKEY, QLOADNET, nullptr, 0, nullptr);
        return false;
    }

    if(SaveSlot *sslot = G_SaveSlots().slotByUserInput(argv[1]))
    {
        if(sslot->isLoadable())
        {
            // A known used slot identifier.
            if(confirmed || !cfg.confirmQuickGameSave)
            {
                // Try to schedule a GA_LOADSESSION action.
                S_LocalSound(SFX_MENU_ACCEPT, nullptr);
                return G_SetGameActionLoadSession(sslot->id());
            }

            S_LocalSound(SFX_QUICKLOAD_PROMPT, nullptr);
            // Compose the confirmation message.
            String const &existingDescription = COMMON_GAMESESSION->savedUserDescription(sslot->saveName());
            AutoStr *msg = Str_Appendf(AutoStr_NewStd(), QLPROMPT, existingDescription.toUtf8().constData());
            Hu_MsgStart(MSG_YESNO, Str_Text(msg), loadSessionConfirmed, 0, new String(sslot->id()));
            return true;
        }
    }

    if(!qstricmp(argv[1], "quick") || !qstricmp(argv[1], "<quick>"))
    {
        S_LocalSound(SFX_QUICKLOAD_PROMPT, nullptr);
        Hu_MsgStart(MSG_ANYKEY, QSAVESPOT, nullptr, 0, nullptr);
        return true;
    }

    if(!G_SaveSlots().has(argv[1]))
    {
        LOG_SCR_WARNING("Failed to determine save slot from \"%s\"") << argv[1];
    }

    // Clearly the caller needs some assistance...
    // We'll open the load menu if caller is the console.
    // Reasoning: User attempted to load a named game-save however the name
    // specified didn't match anything known. Opening the load menu allows
    // the user to see the names of the known game-saves.
    if(src == CMDS_CONSOLE)
    {
        LOG_SCR_MSG("Opening Load Game menu...");
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
    String slotId;
    String userDescription;
};

static int saveSessionConfirmed(msgresponse_t response, int /*userValue*/, void *context)
{
    savesessionconfirmed_params_t *p = static_cast<savesessionconfirmed_params_t *>(context);
    DENG2_ASSERT(p != 0);
    if(response == MSG_YES)
    {
        DD_Executef(true, "savegame %s \"%s\" confirm", p->slotId.toUtf8().constData(), p->userDescription.toUtf8().constData());
    }
    delete p;
    return true;
}

D_CMD(SaveSession)
{
    DENG2_UNUSED(src);

    bool const confirmed = (argc >= 3 && !qstricmp(argv[argc-1], "confirm"));

    if(G_QuitInProgress()) return false;

    if(IS_CLIENT || IS_NETWORK_SERVER)
    {
        LOG_ERROR("Network savegames are not supported at the moment");
        return false;
    }

    player_t *player = &players[CONSOLEPLAYER];
    if(player->playerState == PST_DEAD || Get(DD_PLAYBACK))
    {
        S_LocalSound(SFX_QUICKSAVE_PROMPT, nullptr);
        Hu_MsgStart(MSG_ANYKEY, SAVEDEAD, nullptr, 0, nullptr);
        return true;
    }

    if(G_GameState() != GS_MAP)
    {
        S_LocalSound(SFX_QUICKSAVE_PROMPT, nullptr);
        Hu_MsgStart(MSG_ANYKEY, SAVEOUTMAP, nullptr, 0, nullptr);
        return true;
    }

    if(SaveSlot *sslot = G_SaveSlots().slotByUserInput(argv[1]))
    {
        if(sslot->isUserWritable())
        {
            String userDescription;
            if(argc >= 3 && qstricmp(argv[2], "confirm"))
            {
                userDescription = argv[2];
            }

            if(sslot->isUnused() || confirmed || !cfg.confirmQuickGameSave)
            {
                // Try to schedule a GA_SAVESESSION action.
                S_LocalSound(SFX_MENU_ACCEPT, nullptr);
                return G_SetGameActionSaveSession(sslot->id(), &userDescription);
            }

            S_LocalSound(SFX_QUICKSAVE_PROMPT, nullptr);

            // Compose the confirmation message.
            String const existingDescription = COMMON_GAMESESSION->savedUserDescription(sslot->saveName());
            AutoStr *msg = Str_Appendf(AutoStr_NewStd(), QSPROMPT, existingDescription.toUtf8().constData());

            savesessionconfirmed_params_t *parm = new savesessionconfirmed_params_t;
            parm->slotId          = sslot->id();
            parm->userDescription = userDescription;

            Hu_MsgStart(MSG_YESNO, Str_Text(msg), saveSessionConfirmed, 0, parm);
            return true;
        }

        LOG_SCR_ERROR("Save slot '%s' is non-user-writable") << sslot->id();
    }

    if(!qstricmp(argv[1], "quick") || !qstricmp(argv[1], "<quick>"))
    {
        // No quick-save slot has been nominated - allow doing so now.
        Hu_MenuCommand(MCMD_OPEN);
        Hu_MenuSetActivePage(Hu_MenuFindPageByName("SaveGame"));
        menuNominatingQuickSaveSlot = true;
        return true;
    }

    if(!G_SaveSlots().has(argv[1]))
    {
        LOG_SCR_WARNING("Failed to determine save slot from \"%s\"") << argv[1];
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
    String const *saveName = static_cast<de::String const *>(context);
    DENG2_ASSERT(saveName != 0);
    if(response == MSG_YES)
    {
        DD_Executef(true, "deletegamesave %s confirm", saveName->toUtf8().constData());
    }
    delete saveName;
    return true;
}

D_CMD(DeleteSavedSession)
{
    DENG2_UNUSED(src);

    if(G_QuitInProgress()) return false;

    bool const confirmed = (argc >= 3 && !qstricmp(argv[argc-1], "confirm"));
    if(SaveSlot *sslot = G_SaveSlots().slotByUserInput(argv[1]))
    {
        if(sslot->isUserWritable())
        {
            // A known slot identifier.
            if(sslot->isUnused()) return false;

            if(confirmed)
            {
                COMMON_GAMESESSION->removeSaved(sslot->saveName());
            }
            else
            {
                S_LocalSound(SFX_DELETESAVEGAME_CONFIRM, nullptr);

                // Compose the confirmation message.
                String const existingDescription = COMMON_GAMESESSION->savedUserDescription(sslot->saveName());
                AutoStr *msg = Str_Appendf(AutoStr_NewStd(), DELETESAVEGAME_CONFIRM, existingDescription.toUtf8().constData());
                Hu_MsgStart(MSG_YESNO, Str_Text(msg), deleteSavedSessionConfirmed, 0, new String(sslot->saveName()));
            }

            return true;
        }

        LOG_SCR_ERROR("Save slot '%s' is non-user-writable") << sslot->id();
    }
    else
    {
        LOG_SCR_WARNING("Failed to determine save slot from '%s'") << argv[1];
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

D_CMD(LeaveMap)
{
    DENG2_UNUSED(src);

    String exitName(argc > 1? argv[1] : "next");

    // Only the server operator can end the map this way.
    if(IS_NETGAME && !IS_NETWORK_SERVER)
        return false;

    if(G_GameState() != GS_MAP)
    {
#if __JHERETIC__ || __JHEXEN__
        S_LocalSound(SFX_CHAT, nullptr);
#else
        S_LocalSound(SFX_OOF, nullptr);
#endif
        LOG_MAP_ERROR("Can only exit a map when in a game!");
        return false;
    }

    G_SetGameActionMapCompleted(COMMON_GAMESESSION->mapUriForNamedExit(exitName));
    return true;
}

/**
 * Warp behavior is as follows:
 *
 * if a game session is in progress and episode id matches current
 *     continue the session and change map
 *     if Hexen and the targt map is in another hub
 *         force a new session.
 * else
 *     begin a new game session and warp to the specified map.
 *
 * @note In a networked game we must presently force a new game session when a
 * map change outside the normal progression occurs to allow session-level state
 * changes to take effect. In single player this behavior is not necessary.
 *
 * @note "setmap" is an alias of "warp"
 */
D_CMD(WarpMap)
{
    // Only server operators can warp maps in network games.
    /// @todo Implement vote or similar mechanics.
    if(IS_NETGAME && !IS_NETWORK_SERVER)
    {
        return false;
    }

    if(argc == 1)
    {
        LOG_SCR_NOTE("Usage: %s (episode) (map)") << argv[0];
        return true;
    }

    // If a session is already in progress, the default episode is the current.
    String episodeId = COMMON_GAMESESSION->episodeId();

    // Otherwise if only one playable episode is defined - select it.
    if(episodeId.isEmpty())
    {
        for(auto const &pair : Defs().episodes.lookup("id").elements())
        {
            Record const &episodeDef = *pair.second->as<RecordValue>().record();
            de::Uri startMap(episodeDef.gets("startMap"), RC_NULL);
            if(!P_MapExists(startMap.compose().toUtf8().constData()))
            {
                continue;
            }

            if(episodeId.isEmpty())
            {
                episodeId = episodeDef.gets("id");
            }
            else
            {
                episodeId.clear();
                break;
            }
        }
    }

    // Has an episode been specified?
    bool const haveEpisode = (argc >= 3);
    if(haveEpisode) episodeId = argv[1];

    // Catch invalid episodes.
    if(Record const *episodeDef = Defs().episodes.tryFind("id", episodeId))
    {
        // Ensure that the episode is playable.
        de::Uri startMap(episodeDef->gets("startMap"), RC_NULL);
        if(!P_MapExists(startMap.compose().toUtf8().constData()))
        {
            LOG_SCR_NOTE("Failed to locate the start map for episode '%s'."
                         " This episode is not playable") << episodeId;
            return false;
        }
    }
    else
    {
        LOG_SCR_NOTE("Unknown episode '%s'") << episodeId;
        return false;
    }

    // The map.
    de::Uri mapUri;
    bool isNumber;
    int oldMapNumber = String(argv[haveEpisode? 2 : 1]).toInt(&isNumber);
#if !__JHEXEN__
    if(oldMapNumber > 0) oldMapNumber -= 1; // zero-based.
#endif

    if(!isNumber)
    {
        // It must be a URI, then.
        Block rawMapUri = String(argv[haveEpisode? 2 : 1]).toUtf8();
        char *args[1] = { const_cast<char *>(rawMapUri.constData()) };
        mapUri = de::Uri::fromUserInput(args, 1);
        if(mapUri.scheme().isEmpty()) mapUri.setScheme("Maps");
    }
    else if(!episodeId.isEmpty())
    {
#if __JHEXEN__
        // Map numbers must be translated in the context of an episode.
        mapUri = TranslateMapWarpNumber(episodeId, oldMapNumber);
#else
        int oldEpisodeNumber = episodeId.toInt(&isNumber);
        if(oldEpisodeNumber > 0) oldEpisodeNumber -= 1; // zero-based.
        if(isNumber)
        {
            mapUri = G_ComposeMapUri(oldEpisodeNumber, oldMapNumber);
        }
#endif
    }
    else
    {
        mapUri = G_ComposeMapUri(0, oldMapNumber);
    }

    // Catch invalid maps.
    if(!P_MapExists(mapUri.compose().toUtf8().constData()))
    {
        String msg("Unknown map");
        if(argc >= 3) msg += String(" \"%1 %2\"").arg(argv[1]).arg(argv[2]);
        else          msg += String(" \"%1\"").arg(argv[1]);

        P_SetMessage(players + CONSOLEPLAYER, LMF_NO_HIDE, msg.toUtf8().constData());
        return false;
    }

    bool forceNewSession = (IS_NETGAME != 0);
    if(COMMON_GAMESESSION->hasBegun())
    {
        if(COMMON_GAMESESSION->episodeId().compareWithoutCase(episodeId))
        {
            forceNewSession = true;
        }
    }

#if __JHEXEN__
    // Hexen does not allow warping to the current map.
    if(!forceNewSession && COMMON_GAMESESSION->mapUri() == mapUri)
    {
        P_SetMessage(players + CONSOLEPLAYER, LMF_NO_HIDE, "Cannot warp to the current map.");
        return false;
    }
#endif

    // Close any left open UIs.
    /// @todo Still necessary here?
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        ST_CloseAll(i, true/*fast*/);
    }
    Hu_MenuCommand(MCMD_CLOSEFAST);

    // Don't brief the player.
    ::briefDisabled = true;

    // So be it.
    if(!forceNewSession && COMMON_GAMESESSION->hasBegun())
    {
#if __JHEXEN__
        ::nextMapUri        = mapUri;
        ::nextMapEntryPoint = 0;
        G_SetGameAction(GA_LEAVEMAP);
#else
        G_SetGameActionNewSession(COMMON_GAMESESSION->rules(), COMMON_GAMESESSION->episodeId(), mapUri);
#endif
    }
    else
    {
        G_SetGameActionNewSession(defaultGameRules, episodeId, mapUri);
    }

    // If the command source was "us" the game library then it was probably in
    // response to the local player entering a cheat event sequence, so set the
    // "CHANGING MAP" message. Somewhat of a kludge...
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
        S_LocalSound(soundId, nullptr);
    }

    return true;
}
