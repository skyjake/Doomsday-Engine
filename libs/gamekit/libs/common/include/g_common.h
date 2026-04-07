/** @file g_common.h  Top-level (common) game routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
#include <doomsday/uri.h>
#include "common.h"
#include "fi_lib.h"
#include "mobj.h"
#include "player.h"

#if __cplusplus
class SaveSlots;

GameRules &gfw_DefaultGameRules();

#define gfw_DefaultRule(name)           (gfw_DefaultGameRules().values.name)
#define gfw_SetDefaultRule(name, value) GameRules_Set(gfw_DefaultGameRules(), name, value)

extern res::Uri nextMapUri;
extern uint    nextMapEntryPoint;

/**
 * Schedule a new game session (deferred).
 *
 * @param rules        Game rules to apply.
 * @param episodeId    Episode identifier.
 * @param mapUri       Map identifier.
 * @param mapEntrance  Logical map entry point number.
 */
void G_SetGameActionNewSession(const GameRules &rules, de::String episodeId,
                               const res::Uri &mapUri, uint mapEntrance = 0);

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
bool G_SetGameActionSaveSession(de::String slotId, de::String *userDescription = 0);

/**
 * Schedule a game session load (deferred).
 *
 * @param slotId  Unique identifier of the save slot to use.
 *
 * @return  @c true iff @a slotId is in use and loading is presently possible.
 */
bool G_SetGameActionLoadSession(de::String slotId);

/**
 * Schedule a game session map exit, possibly leading into an intermission sequence.
 * (if __JHEXEN__ the intermission will only be displayed when exiting a
 * hub and in DeathMatch games)
 *
 * @param nextMapUri         Unique identifier of the map number we are entering.
 * @param nextMapEntryPoint  Logical map entry point on the new map.
 * @param secretExit         @c true if the exit taken was marked as 'secret'.
 */
void G_SetGameActionMapCompleted(const res::Uri &nextMapUri, uint nextMapEntryPoint = 0,
                                 bool secretExit = false);

/**
 * @param episodeId  Identifier of the episode to lookup the title of.
 */
de::String G_EpisodeTitle(const de::String& episodeId);

/**
 * Returns the effective map-info definition Record associated with the given
 * @a mapUri (which may be the default definition, if invalid/unknown).
 *
 * @param mapUri  Unique identifier for the map to lookup map-info data for.
 *
 * @todo: Should use WorldSystem::mapInfoForMapUri() instead.
 */
de::Record &G_MapInfoForMapUri(const res::Uri &mapUri);

/**
 * @param mapUri  Identifier of the map to lookup the author of.
 */
de::String G_MapAuthor(const res::Uri &mapUri, bool supressGameAuthor = false);

/**
 * @param mapUri  Identifier of the map to lookup the title of.
 */
de::String G_MapTitle(const res::Uri &mapUri);

/**
 * @param mapUri  Identifier of the map to lookup the title of.
 */
res::Uri G_MapTitleImage(const res::Uri &mapUri);

/**
 * Compose a textual, rich-formatted description of the the referenced map, containing
 * pertinent information and/or metadata (such as the title and author).
 *
 * @param episodeId  Unique episode identifier.
 * @param mapUri     Unique map identifier.
 *
 * @return  Rich-formatted description of the map.
 */
de::String G_MapDescription(const de::String& episodeId, const res::Uri &mapUri);

/**
 * Attempt to extract the logical map number encoded in the @a mapUri. Assumes the default
 * form for the current game mode (i.e., MAPXX or EXMY).
 *
 * @param mapUri  Unique identifier of the map.
 *
 * @return  Extracted/decoded map number, otherwise @c 0.
 *
 * @deprecated  Should use map URIs instead.
 */
uint G_MapNumberFor(const res::Uri &mapUri);

/**
 * Compose a Uri for the identified @a episode and @a map combination using the default
 * form for the current game mode (i.e., MAPXX or EXMY).
 *
 * @param episode  Logical episode number.
 * @param map      Logical map number.
 *
 * @return  Resultant Uri.
 *
 * @deprecated  Should use map URIs instead. Map references composed of a logical episode
 * and map number pair are a historical legacy that should only be used when necessary,
 * for compatibility reasons.
 */
res::Uri G_ComposeMapUri(uint episode, uint map);

/**
 * Chooses a default user description for a saved session.
 *
 * @param saveName      Name of the saved session from which the existing description should
 *                      be re-used. Use a zero-length string to disable.
 * @param autogenerate  @c true to generate a useful description (map name, map time, etc...)
 *                      if none exists for the @a saveName referenced.
 */
de::String G_DefaultGameStateFolderUserDescription(const de::String &saveName, bool autogenerate = true);

/**
 * Returns the game's SaveSlots.
 */
SaveSlots &G_SaveSlots();

extern "C" {
#endif // __cplusplus

/**
 * Returns the Map Info flags of the current map in the current game session.
 * @return MIF flags.
 */
unsigned int gfw_MapInfoFlags(void);

/**
 * Returns @c true, if the game is currently quiting.
 */
dd_bool G_QuitInProgress(void);

/**
 * Returns the current logical game state.
 */
gamestate_t G_GameState(void);

/**
 * Change the current logical game state to @a newState.
 */
void G_ChangeGameState(gamestate_t newState);

/**
 * Returns the current game action.
 */
gameaction_t G_GameAction(void);

/**
 * Change the current game action to @a newAction.
 */
void G_SetGameAction(gameaction_t newAction);

/**
 * Reveal the game @em help display.
 */
void G_StartHelp(void);

/// @todo Should not be a global function; mode breaks game session separation.
dd_bool G_StartFinale(const char *script, int flags, finale_mode_t mode, const char *defId);

/**
 * Signal that play on the current map may now begin.
 */
void G_BeginMap(void);

/**
 * To be called when the intermission ends.
 */
void G_IntermissionDone(void);

AutoStr *G_CurrentMapUriPath(void);

void GameRules_UpdateDefaultsFromCVars();

/// @todo remove me
void G_SetGameActionMapCompletedAndSetNextMap(void);

/**
 * Changes the automap rotation mode for all players. Also sets the cvar value so the mode
 * will persist.
 *
 * @param enableRotate  Enable or disable the rotation mode.
 */
void G_SetAutomapRotateMode(byte enableRotate);

D_CMD( CCmdMakeLocal );
D_CMD( CCmdSetCamera );
D_CMD( CCmdSetViewLock );
D_CMD( CCmdLocalMessage );
D_CMD( CCmdExitLevel );

#if __cplusplus
} // extern "C"
#endif

DE_EXTERN_C dd_bool singledemo;

#endif  // LIBCOMMON_GAME_H
