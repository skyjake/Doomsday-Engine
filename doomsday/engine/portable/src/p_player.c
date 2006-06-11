/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_player.c: Players and Player Classes
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"

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
 * Determine which console is used by the given local player.  Local
 * players are numbered starting from zero.
 */
int P_LocalToConsole(int localPlayer)
{
	int     i, count, n;

	for(i = 0, count = 0; i < DDMAXPLAYERS; i++)
	{
		// The numbering begins from the consoleplayer.
		n = (i + consoleplayer) % DDMAXPLAYERS;

		if(players[n].flags & DDPF_LOCAL)
		{
			if(count++ == localPlayer)
				return n;
		}
	}

	// No match!
	return -1;
}
