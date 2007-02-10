/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * con_action.c: Action Commands (Player Controls)
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h" // for P_LocalToConsole()
#include "de_network.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

action_t *ddactions = NULL;     // Pointer to the actions list.

// CODE --------------------------------------------------------------------

void Con_DefineActions(action_t *acts)
{
    // Store a pointer to the list of actions.
    ddactions = acts;
}

/**
 * Clear all action controls.
 *
 * @param pnum          Player number whose action controls to clear.
 *                      If negative, clear ALL player's actions controls.
 */
void Con_ClearActions(int pnum)
{
    action_t *act;

    for(act = ddactions; act->name[0]; act++)
    {
        if(pnum < 0)
            memset(act->on, 0, sizeof(act->on));
        else
            act->on[pnum] = false;
    }
}

/**
 * Execute an action command.
 * The command syntax is as follows: [+|-]name[/N]
 *
 *   name			Toggle state of 'name', local player zero
 * 	 name/3			Toggle state of 'name', local player 3
 *   name/12 		Toggle state of 'name', local player 12 (!)
 *   +name			Set 'name' to on/positive state (local zero)
 *   -name			Set 'name' to off/middle state (local zero)
 *
 * @param cmd           The action command to search for.
 * @param hasPrefix     If <code>false</code> the state of the action is
 *                      negated.
 * @return              <code>true</code> if the action was changed
 *                      successfully.
 */
boolean Con_ActionCommand(const char *cmd, boolean hasPrefix)
{
    char    prefix = cmd[0];
    int     v1, v2;
    int     console, localPlayer = 0;
    char    name8[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    action_t *act;

    if(ddactions == NULL)
        return false;

    strncpy(name8, cmd + (hasPrefix ? 1 : 0), 8);
    v1 = *(int *) name8;
    v2 = *(int *) &name8[4];

	// Find the end of the cmd.
	for(; *cmd && *cmd != '/'; cmd++);

	// Is the local player number specified?
	if(*cmd == '/')
	{
		// The specified number could be bogus.
		localPlayer = strtol(cmd + 1, NULL, 10);
	}

	// Which player will be affected?
	console = P_LocalToConsole(localPlayer);
	if(console >= 0 && console < DDMAXPLAYERS && ddplayers[console].ingame)
	{
        // Try to find a matching action by searching through the list.
        for(act = ddactions; act->name[0]; act++)
        {
            if(v1 == *(int *) act->name && v2 == *(int *) &act->name[4])
            {
                // This is a match!
                if(hasPrefix)
                    act->on[console] = (prefix == '+' ? true : false);
                else
                    act->on[console] = !act->on;
                return true;
            }
        }
    }

    return false;
}

/**
 * Prints a list of the action names registered by the game DLL.
 */
D_CMD(ListActs)
{
    action_t *act;

    Con_Message("Action commands registered by the game DLL:\n");
    for(act = ddactions; act->name[0]; act++)
        Con_Message("  %s\n", act->name);

    return true;
}
