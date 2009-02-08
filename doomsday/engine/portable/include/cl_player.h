/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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

typedef struct clplayerstate_s {
    clmobj_t       *cmo;
    thid_t          mobjId;
    int             forwardMove;
    int             sideMove;
    int             angle;
    angle_t         turnDelta;
    int             friction;
} clplayerstate_t;

extern float pspMoveSpeed;
extern float cplrThrustMul;
extern clplayerstate_t clPlayerStates[DDMAXPLAYERS];

void            Cl_InitPlayers(void);
void            Cl_LocalCommand(void);
void            Cl_MovePlayer(int plrnum);
void            Cl_MoveLocalPlayer(float dx, float dy, float dz, boolean onground);
void            Cl_UpdatePlayerPos(int plrnum);
//void            Cl_MovePsprites(void);
void            Cl_CoordsReceived(void);
void            Cl_HandlePlayerFix(void);
int             Cl_ReadPlayerDelta(void);
void            Cl_ReadPlayerDelta2(boolean skip);
boolean         Cl_IsFreeToMove(int plrnum);

#endif
