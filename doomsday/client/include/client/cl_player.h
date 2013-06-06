/** @file cl_player.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/**
 * Clientside Player Management
 */

#ifndef DENG_CLIENT_PLAYER_H
#define DENG_CLIENT_PLAYER_H

#include "cl_mobj.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Information about a client player.
 */
typedef struct clplayerstate_s {
    thid_t          clMobjId;
    float           forwardMove;
    float           sideMove;
    int             angle;
    angle_t         turnDelta;
    int             friction;
    int             pendingFixes;
    thid_t          pendingFixTargetClMobjId;
    angle_t         pendingAngleFix;
    float           pendingLookDirFix;
    coord_t         pendingOriginFix[3];
    coord_t         pendingMomFix[3];
} clplayerstate_t;

extern float pspMoveSpeed;
extern float cplrThrustMul;

void            Cl_InitPlayers(void);

/**
 * Used in DEMOS. (Not in regular netgames.)
 * Applies the given dx and dy to the local player's coordinates.
 *
 * @param dx            Viewpoint X delta.
 * @param dy            Viewpoint Y delta.
 * @param z             Viewpoint absolute Z coordinate.
 * @param onground      If @c true the mobj's Z will be set to floorz, and
 *                      the player's viewheight is set so that the viewpoint
 *                      height is param 'z'.
 *                      If @c false the mobj's Z will be param 'z' and
 *                      viewheight is zero.
 */
void ClPlayer_MoveLocal(coord_t dx, coord_t dy, coord_t z, boolean onground);

void ClPlayer_UpdateOrigin(int plrnum);

void            ClPlayer_HandleFix(void);
void            ClPlayer_ApplyPendingFixes(int plrNum);
void            ClPlayer_ReadDelta2(boolean skip);
clplayerstate_t *ClPlayer_State(int plrNum);
mobj_t         *ClPlayer_LocalGameMobj(int plrNum);
boolean         ClPlayer_IsFreeToMove(int plrnum);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DENG_CLIENT_PLAYER_H
