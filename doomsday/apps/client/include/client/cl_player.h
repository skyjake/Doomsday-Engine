/** @file cl_player.h  Clientside player management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_CLIENT_PLAYER_H
#define DE_CLIENT_PLAYER_H

#include "cl_mobj.h"
#include "clientplayer.h"

DE_EXTERN_C float pspMoveSpeed;
DE_EXTERN_C float cplrThrustMul;

/**
 * Clears the player state table.
 */
void Cl_InitPlayers();

/**
 * Used in DEMOS. (Not in regular netgames.)
 * Applies the given dx and dy to the local player's coordinates.
 *
 * @param dx        Viewpoint X delta.
 * @param dy        Viewpoint Y delta.
 * @param z         Viewpoint absolute Z coordinate.
 * @param onground  If @c true the mobj's Z will be set to floorz, and the player's
 *                  viewheight is set so that the viewpoint height is param 'z'.
 *                  If @c false the mobj's Z will be param 'z' and viewheight is zero.
 */
void ClPlayer_MoveLocal(coord_t dx, coord_t dy, coord_t z, bool onground);

/**
 * Move the (hidden, unlinked) client player mobj to the same coordinates
 * where the real mobj of the player is.
 */
void ClPlayer_UpdateOrigin(int plrnum);

void ClPlayer_HandleFix();

void ClPlayer_ApplyPendingFixes(int plrNum);

/**
 * Reads a single PSV_FRAME2 player delta from the message buffer and
 * applies it to the player in question.
 */
void ClPlayer_ReadDelta();

clplayerstate_t *ClPlayer_State(int plrNum);

/**
 * Returns the gameside local mobj of a player.
 */
mobj_t *ClPlayer_LocalGameMobj(int plrNum);

/**
 * Returns @c true iff the player is free to move according to floorz and ceilingz.
 */
bool ClPlayer_IsFreeToMove(int plrnum);

#endif // DE_CLIENT_PLAYER_H
