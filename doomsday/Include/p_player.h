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
 * p_player.h: Players and Player Classes
 */

#ifndef __DOOMSDAY_PLAYERS_H__
#define __DOOMSDAY_PLAYERS_H__

// Class flags.
#define PCF_CAMERA			0x1	// Invisible camera.
#define PCF_VERTICAL_FLY	0x2	// Fly in traditional vertical mode.

/*
 * Player classes define the characteristics 
 */
typedef struct playerclass_s {
	int flags;
	int allowActions;			// TCAF-bits for all permitted actions.
	fixed_t forwardMove[2];		// Normal speed and high speed.
	fixed_t sideMove[2];
	fixed_t upMove[2];			// Vertical flying speed.
	fixed_t startTurnSpeed[2];	// Initial (normal + speed).
	fixed_t turnSpeed[2];		// Continued (normal + speed).
} playerclass_t;

int P_LocalToConsole(int localPlayer);

#endif
