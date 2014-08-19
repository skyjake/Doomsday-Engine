/** @file g_common.h  Top-level (common) game routines.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "fi_lib.h"
#include "mobj.h"
#include "player.h"
#include <doomsday/uri.h>

DENG_EXTERN_C dd_bool singledemo;

DENG_EXTERN_C uint gameMapEntrance;

#if __cplusplus
extern de::Uri gameMapUri;
extern GameRuleset defaultGameRules;

extern "C" {
#endif

void G_ConsoleRegister(void);

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

#if __cplusplus
} // extern "C"

/**
 * Schedule a new game session (deferred).
 *
 * @param rules        Game rules to apply.
 * @param episodeId    Episode identifier.
 * @param mapUri       Map identifier.
 * @param mapEntrance  Logical map entry point number.
 */
void G_SetGameActionNewSession(GameRuleset const &rules, de::String episodeId,
                               de::Uri const &mapUri, uint mapEntrance = 0);

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
void G_SetGameActionMapCompleted(de::Uri const &nextMapUri, uint nextMapEntryPoint, dd_bool secretExit);

/**
 * @param episodeId  Identifier of the episode to lookup the title of.
 */
de::String G_EpisodeTitle(de::String episodeId);

/**
 * @param mapUri  Identifier of the map to lookup the author of.
 */
de::String G_MapAuthor(de::Uri const &mapUri, bool supressGameAuthor = false);

/**
 * @param mapUri  Identifier of the map to lookup the title of.
 */
de::String G_MapTitle(de::Uri const &mapUri);

/**
 * @param mapUri  Identifier of the map to lookup the title of.
 */
patchid_t G_MapTitlePatch(de::Uri const &mapUri);

extern "C" {
#endif

/**
 * Reveal the game @em help display.
 */
void G_StartHelp(void);

/// @todo Should not be a global function; mode breaks game session separation.
dd_bool G_StartFinale(char const *script, int flags, finale_mode_t mode, char const *defId);

/**
 * Signal that play on the current map may now begin.
 */
void G_BeginMap(void);

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
 * Determines whether an intermission should be scheduled (if any) when the players leave the
 * @em current map.
 */
dd_bool G_IntermissionActive(void);

/**
 * To be called to initiate the intermission.
 */
void G_IntermissionBegin(void);

/**
 * To be called when the intermission ends.
 */
void G_IntermissionDone(void);

#if __cplusplus
} // extern "C"

/**
 * Returns the logical map number for the identified map.
 *
 * @param mapUri  Unique identifier of the map to lookup.
 *
 * @deprecated  Should use map URIs instead.
 */
uint G_MapNumberFor(de::Uri const &mapUri);

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
de::Uri G_ComposeMapUri(uint episode, uint map);

extern "C" {
#endif

AutoStr *G_CurrentMapUriPath(void);

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

/// @todo remove me
void G_SetGameActionMapCompletedAndSetNextMap(void);

D_CMD( CCmdMakeLocal );
D_CMD( CCmdSetCamera );
D_CMD( CCmdSetViewLock );
D_CMD( CCmdLocalMessage );
D_CMD( CCmdExitLevel );

#if __cplusplus
} // extern "C"

#include <de/String>

class SaveSlots;

/**
 * Chooses a default user description for a saved session.
 *
 * @param saveName      Name of the saved session from which the existing description should be
 *                      re-used. Use a zero-length string to disable.
 * @param autogenerate  @c true= generate a useful description (map name, map time, etc...) if none
 *                      exists for the referenced save @a slotId.
 */
de::String G_DefaultSavedSessionUserDescription(de::String const &saveName, bool autogenerate = true);

/**
 * Returns the game's SaveSlots.
 */
SaveSlots &G_SaveSlots();

#endif // __cplusplus
#endif // LIBCOMMON_GAME_H
