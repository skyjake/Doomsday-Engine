/**\file g_common.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
#include "common.h"

#if __cplusplus
extern "C" {
#endif

enum {
    JOYAXIS_NONE,
    JOYAXIS_MOVE,
    JOYAXIS_TURN,
    JOYAXIS_STRAFE,
    JOYAXIS_LOOK
};

extern boolean singledemo;

void            G_Register(void);

void            G_EndGame(void);
boolean         G_QuitInProgress(void);

/**
 * Begin the titlescreen animation sequence.
 */
void            G_StartTitle(void);

/**
 * Begin the helpscreen animation sequence.
 */
void            G_StartHelp(void);

const char*     P_GetGameModeName(void);

gamestate_t     G_GameState(void);
void            G_ChangeGameState(gamestate_t state);

gameaction_t    G_GameAction(void);
void            G_SetGameAction(gameaction_t action);

/**
 * @param map           Logical map number (i.e., not a warp/translated number).
 * @param mapEntryPoint Logical map entry point number.
 */
void            G_NewGame(skillmode_t skill, uint episode, uint map, uint mapEntryPoint);
void            G_DeferredNewGame(skillmode_t skill, uint episode, uint map, uint mapEntryPoint);

#if __JHEXEN__
/**
 * Same as @ref G_DeferredNewGame() except a GA_SETMAP action is queued
 * instead of GA_NEWGAME.
 */
void            G_DeferredSetMap(skillmode_t skill, uint episode, uint map, uint mapEntryPoint);
#endif

/**
 * Signal that play on the current map may now begin.
 */
void            G_BeginMap(void);

/**
 * Leave the current map and start intermission routine.
 * (if __JHEXEN__ the intermission will only be displayed when exiting a
 * hub and in DeathMatch games)
 *
 * @param newMap        Logical map number we are entering (i.e., not a warp/translated number).
 * @param mapEntryPoint Logical map entry point on the new map.
 * @param secretExit    @c true if the exit taken was marked as 'secret'.
 */
void            G_LeaveMap(uint newMap, uint mapEntryPoint, boolean secretExit);

/// @return  Generated name. Must be released with Str_Delete()
AutoStr*        G_GenerateSaveGameName(void);

int             P_CameraXYMovement(mobj_t* mo);
int             P_CameraZMovement(mobj_t* mo);
void            P_Thrust3D(struct player_s* player, angle_t angle, float lookdir, coord_t forwardMove, coord_t sideMove);

D_CMD( CCmdMakeLocal );
D_CMD( CCmdSetCamera );
D_CMD( CCmdSetViewLock );
D_CMD( CCmdLocalMessage );
D_CMD( CCmdExitLevel );

#if __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_GAME_H */
