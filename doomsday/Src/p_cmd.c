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
 * p_cmd.c: Tic Commands
 *
 * Tic commands are generated out of controller state. There is one
 * command per input tic (35 Hz).  The commands are used to control
 * all players.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Build one command for the specified player.  This routine is used
 * to generate commands for local players.  The commands are added to
 * the command buffer.
 *
 * This function is called from the input thread.
 */
void P_BuildCommand(int playerNumber)
{
	ddplayer_t *player = players + playerNumber;
	//client_t *client = clients + playerNumber;
	float pos;
	ticcmd_t cmd;

	// The command will stay 'empty' if no controls are active.
	memset(&cmd, 0, sizeof(cmd));

	// Examine the state of controllers to see which controls are
	// active.
	pos = P_ControlGetAxis(playerNumber, "walk");

	// The player's class affects the movement speed.
	cmd.forwardMove = (char) (0x10 * -pos);

	// The command is now complete.  Insert it into the client's
	// command buffer, where it will be read from by the refresh
	// thread.
	Net_NewLocalCmd(&cmd, playerNumber);
}
