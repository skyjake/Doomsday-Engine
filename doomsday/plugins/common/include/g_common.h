/** @file g_common.h  Top-level (common) game routines.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GAME_H
#define LIBCOMMON_GAME_H

#include "dd_share.h"
#include "mobj.h"
#include "player.h"

DENG_EXTERN_C dd_bool singledemo;

DENG_EXTERN_C dd_bool gameInProgress;
DENG_EXTERN_C uint gameEpisode;

DENG_EXTERN_C Uri *gameMapUri;
DENG_EXTERN_C uint gameMapEntrance;
DENG_EXTERN_C uint gameMap; ///< @todo refactor away.

#if __cplusplus
extern "C" {
#endif

void G_Register(void);

dd_bool G_QuitInProgress(void);

/**
 * Returns the current logical game state.
 */
gamestate_t G_GameState(void);

/**
 * Change the game's state.
 *
 * @param state  The state to change to.
 */
void G_ChangeGameState(gamestate_t state);

gameaction_t G_GameAction(void);

void G_SetGameAction(gameaction_t action);

uint G_GenerateSessionId(void);

void G_ReadLegacySessionMetadata(void *metadata, Reader *reader);

/**
 * Begin the titlescreen animation sequence.
 */
void G_StartTitle(void);

/**
 * Begin the helpscreen animation sequence.
 */
void G_StartHelp(void);

/**
 * Signal that play on the current map may now begin.
 */
void G_BeginMap(void);

/**
 * Leave the current map and start intermission routine.
 * (if __JHEXEN__ the intermission will only be displayed when exiting a
 * hub and in DeathMatch games)
 *
 * @param newMap         Logical map number we are entering (i.e., not a warp/translated number).
 * @param mapEntryPoint  Logical map entry point on the new map.
 * @param secretExit     @c true if the exit taken was marked as 'secret'.
 */
void G_LeaveMap(uint newMap, uint mapEntryPoint, dd_bool secretExit);

/**
 * Called when a player leaves a map.
 *
 * Jobs include; striping keys, inventory and powers from the player and configuring other
 * player-specific properties ready for the next map.
 *
 * @param player  Id of the player to configure.
 */
void G_PlayerLeaveMap(int player);

/**
 * Returns the logical episode number for the identified map.
 *
 * @param mapUri  Unique identifier of the map to lookup.
 */
uint G_EpisodeNumberFor(Uri const *mapUri);

/**
 * Returns the logical map number for the identified map.
 *
 * @param mapUri  Unique identifier of the map to lookup.
 */
uint G_MapNumberFor(Uri const *mapUri);

/**
 * Compose a Uri for the identified @a episode and @a map combination.
 *
 * @param episode  Logical episode number.
 * @param map      Logical map number.
 *
 * @return  Resultant Uri. Must be destroyed with Uri_Delete() when no longer needed.
 */
Uri *G_ComposeMapUri(uint episode, uint map);

/**
 * Determine if the specified @a episode and @a map value pair are valid and if not,
 * adjust their are values within the ranges defined by the current game type and mode.
 *
 * @param episode  Logical episode number to be validated.
 * @param map      Logical map number to be validated.
 *
 * @return  @c true= The original @a episode and @a map value pair were already valid.
 */
dd_bool G_ValidateMap(uint *episode, uint *map);

/**
 * Return the next map according to the default map progression.
 *
 * @param episode     Current episode.
 * @param map         Current map.
 * @param secretExit
 *
 * @return  The next map.
 */
uint G_GetNextMap(uint episode, uint map, dd_bool secretExit);

/// @return  Logical map number.
uint G_NextLogicalMapNumber(dd_bool secretExit);

/// @return  Logical map number.
uint G_LogicalMapNumber(uint episode, uint map);

/// @return  Logical map number.
uint G_CurrentLogicalMapNumber(void);

AutoStr *G_GenerateUserSaveDescription(void);

int G_Ruleset_Skill();
#if !__JHEXEN__
byte G_Ruleset_Fast();
#endif
byte G_Ruleset_Deathmatch();
byte G_Ruleset_NoMonsters();
#if __JHEXEN__
byte G_Ruleset_RandomClasses();
#else
byte G_Ruleset_RespawnMonsters();
#endif

D_CMD( CCmdMakeLocal );
D_CMD( CCmdSetCamera );
D_CMD( CCmdSetViewLock );
D_CMD( CCmdLocalMessage );
D_CMD( CCmdExitLevel );

#if __cplusplus
} // extern "C"
#endif

#if __cplusplus
#include <de/game/MapStateReader>
#include <de/String>
#include "gamerules.h"

class SaveSlots;

/**
 * Returns the game identity key (from the engine).
 */
de::String G_IdentityKey();

/**
 * Translates a legacy game mode identifier to a game identity key.
 */
de::String G_IdentityKeyForLegacyGamemode(int gamemode, int saveVersion);

/**
 * @param mapUri       Map identifier.
 * @param mapEntrance  Logical map entry point number.
 * @param rules        Game rules to apply.
 */
void G_NewSession(Uri const *mapUri, uint mapEntrance, GameRuleset const *rules);

void G_DeferredNewSession(Uri const *mapUri, uint mapEntrance, GameRuleset const *rules);

void G_EndSession(void);

/**
 * Determines whether game session loading is presently possible.
 */
bool G_SessionLoadingPossible();

/**
 * Determines whether game session saving is presently possible.
 */
bool G_SessionSavingPossible();

/**
 * Schedule a game session save (deferred).
 *
 * @param slotId           Unique identifier of the save slot to use.
 * @param userDescription  New user description for the game-save. Can be @c NULL in which
 *                         case it will not change if the slot has already been used.
 *                         If an empty string a new description will be generated automatically.
 *
 * @return  @c true iff @a slotId is valid and saving is presently possible.
 */
bool G_SaveSession(de::String slotId, de::String *userDescription = 0);

/**
 * Schedule a game session load (deferred).
 *
 * @param slotId  Unique identifier of the save slot to use.
 *
 * @return  @c true iff @a slotId is in use and loading is presently possible.
 */
bool G_LoadSession(de::String slotId);

/**
 * @return  New SessionMetadata (record). Ownership is given to the caller.
 */
de::game::SessionMetadata *G_CurrentSessionMetadata();

/**
 * Returns the game's SaveSlots.
 */
SaveSlots &G_SaveSlots();

/**
 * Returns the game's (i.e., the app's) SavedSessionRepository.
 */
de::game::SavedSessionRepository &G_SavedSessionRepository();

/**
 * Parse @a str and determine whether it references a logical game-save slot.
 *
 * @param str  String to be parsed. Parse is divided into three passes.
 *             Pass 1: Check for a known game-save description which matches this.
 *                 Search is in logical save slot creation order.
 *             Pass 2: Check for keyword identifiers.
 *                 <auto>  = The "auto save" slot.
 *                 <last>  = The last used slot.
 *                 <quick> = The currently nominated "quick save" slot.
 *             Pass 3: Check for a unique save slot identifier.
 *
 * @return  The parsed slot id if found; otherwise a zero-length string.
 */
de::String G_SaveSlotIdFromUserInput(de::String str);

/**
 * Returns the game's GameRuleset.
 */
GameRuleset &G_Rules();

#endif // __cplusplus

#endif // LIBCOMMON_GAME_H
