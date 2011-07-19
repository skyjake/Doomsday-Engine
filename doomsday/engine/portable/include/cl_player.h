/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
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
 * cl_player.h: Clientside Player Management
 */

#ifndef __DOOMSDAY_CLIENT_PLAYER_H__
#define __DOOMSDAY_CLIENT_PLAYER_H__

#include "cl_mobj.h"

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
    int             pendingFixTargetClMobjId;
    int             pendingAngleFix;
    float           pendingLookDirFix;
    float           pendingPosFix[3];
    float           pendingMomFix[3];
} clplayerstate_t;

extern float pspMoveSpeed;
extern float cplrThrustMul;

void            Cl_InitPlayers(void);
void            ClPlayer_MoveLocal(float dx, float dy, float dz, boolean onground);
void            ClPlayer_UpdatePos(int plrnum);
void            ClPlayer_CoordsReceived(void);
void            ClPlayer_HandleFix(void);
void            ClPlayer_ApplyPendingFixes(int plrNum);
void            ClPlayer_ReadDelta2(boolean skip);
clplayerstate_t *ClPlayer_State(int plrNum);
mobj_t         *ClPlayer_LocalGameMobj(int plrNum);
struct mobj_s  *ClPlayer_ClMobj(int plrNum);
boolean         ClPlayer_IsFreeToMove(int plrnum);

#endif
