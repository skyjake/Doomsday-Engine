/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * p_cmd.c: Tic Commands
 *
 * Tic commands are generated out of controller state. There is one command
 * per input tic (35 Hz).  The commands are used to control all players.
 */

// HEADER FILES ------------------------------------------------------------

#if 0

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

/**
 * Combine the source ticcmd with the destination ticcmd.  This is
 * done when there are multiple ticcmds to execute on a single game
 * tick.
 */
void P_MergeCommand(ticcmd_t *dest, ticcmd_t *src)
{
    dest->forwardMove = src->forwardMove;
    dest->sideMove = src->sideMove;
    dest->upMove = src->upMove;
    dest->angle = src->angle;
    dest->pitch = src->pitch;

    dest->actions |= src->actions;
}

/**
 * Build one command for the specified player.  This routine is used
 * to generate commands for local players.  The commands are added to
 * the command buffer.
 *
 * This function is called from the input thread.
 */
void P_BuildCommand(ticcmd_t *cmd, int playerNumber)
{
    memset(cmd, 0, sizeof(*cmd));

#if 0
    ddplayer_t *player = &ddPlayers[playerNumber].shared;
    //client_t *client = clients + playerNumber;

    // Examine the state of controllers to see which controls are
    // active.

    // The player's class affects the movement speed.
    cmd->forwardMove = (char) (0x10 * P_ControlGetAxis(playerNumber, "walk"));

    cmd->sideMove = (char) (0x10 * P_ControlGetAxis(playerNumber, "sidestep"));

    // The view angles are updated elsewhere as the axis positions
    // change.
    if(player->mo)
    {   // These will be sent to the server (or P_MovePlayer).
        cmd->angle = player->mo->angle >> 16; /* $unifiedangles */
    }
    else
        cmd->angle = 0;

    /* $unifiedangles */
    cmd->pitch = player->lookDir / 110 * DDMAXSHORT;

    cmd->actions = P_ControlGetToggles(playerNumber);

//Con_Printf("%i: a=%04x p=%04x\n", playerNumber, cmd->angle, cmd->pitch);

    if(isClient)
    {
        // Clients mirror their local commands.
        memcpy(&players->cmd, cmd, sizeof(*cmd));
    }
#endif
}

#endif
