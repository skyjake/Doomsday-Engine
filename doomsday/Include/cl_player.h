/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * cl_player.h: Clientside Player Management
 */

#ifndef __DOOMSDAY_CLIENT_PLAYER_H__
#define __DOOMSDAY_CLIENT_PLAYER_H__

#include "cl_mobj.h"

typedef struct playerstate_s {
	clmobj_t	*cmo;
	thid_t		mobjId;
	int			forwardMove;
	int			sideMove;
	int			angle;
	angle_t		turnDelta;
	int			friction;
} playerstate_t;

extern int psp_move_speed;
extern int cplr_thrust_mul;
extern playerstate_t playerstate[MAXPLAYERS];

void Cl_InitPlayers(void);
void Cl_LocalCommand(void);
void Cl_MovePlayer(ddplayer_t *pl);
void Cl_MoveLocalPlayer(int dx, int dy, int dz, boolean onground);
void Cl_UpdatePlayerPos(ddplayer_t *pl);
void Cl_MovePsprites(void);
void Cl_CoordsReceived(void);
int Cl_ReadPlayerDelta(void);
void Cl_ReadPlayerDelta2(boolean skip);
boolean Cl_IsFreeToMove(int player);

#endif 
