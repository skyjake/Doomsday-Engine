/**\file g_common.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GAME_H
#define LIBCOMMON_GAME_H

#include "dd_share.h"
#include "g_controls.h"
#include "mobj.h"
#include "player.h"

enum {
    JOYAXIS_NONE,
    JOYAXIS_MOVE,
    JOYAXIS_TURN,
    JOYAXIS_STRAFE,
    JOYAXIS_LOOK
};

DENG_EXTERN_C dd_bool singledemo;

#if __cplusplus
extern "C" {
#endif

void G_Register(void);

gamestate_t G_GameState(void);
void G_ChangeGameState(gamestate_t state);

gameaction_t G_GameAction(void);
void G_SetGameAction(gameaction_t action);

char const *P_GetGameModeName(void);

uint G_GenerateSessionId(void);

/**
 * Begin the titlescreen animation sequence.
 */
void G_StartTitle(void);

/**
 * Begin the helpscreen animation sequence.
 */
void G_StartHelp(void);

void G_EndGame(void);

dd_bool G_QuitInProgress(void);

/**
 * @param mapUri       Map identifier.
 * @param mapEntrance  Logical map entry point number.
 * @param rules        Game rules to apply.
 */
void G_NewGame(Uri const *mapUri, uint mapEntrance, GameRuleset const *rules);
void G_DeferredNewGame(Uri const *mapUri, uint mapEntrance, GameRuleset const *rules);

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

AutoStr *G_GenerateSaveGameName(void);

D_CMD( CCmdMakeLocal );
D_CMD( CCmdSetCamera );
D_CMD( CCmdSetViewLock );
D_CMD( CCmdLocalMessage );
D_CMD( CCmdExitLevel );

#if __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_GAME_H */
