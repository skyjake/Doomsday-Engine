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
 * con_action.c: Action Commands (Player Controls)
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

action_t *ddactions = NULL;		// Pointer to the actions list.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// Con_DefineActions
//===========================================================================
void Con_DefineActions(action_t *acts)
{
	// Store a pointer to the list of actions.
	ddactions = acts;
}

//===========================================================================
// Con_ClearActions
//===========================================================================
void Con_ClearActions(void)
{
	action_t *act;
	for(act = ddactions; act->name[0]; act++) act->on = false;
}

//===========================================================================
// Con_ActionCommand
//	The command begins with a '+' or a '-'.
//	Returns true if the action was changed successfully.
//	If has_prefix is false, the state of the action is negated.
//===========================================================================
int Con_ActionCommand(char *cmd, boolean has_prefix)
{
	char		prefix = cmd[0];
	int			v1, v2;
	char		name8[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	action_t	*act;

	if(ddactions == NULL) return false;
		
	strncpy(name8, cmd + (has_prefix? 1 : 0), 8);
	v1 = *(int*) name8;
	v2 = *(int*) &name8[4];

	// Try to find a matching action by searching through the list.
	for(act = ddactions; act->name[0]; act++)
	{
		if(v1 == *(int*) act->name && v2 == *(int*) &act->name[4])
		{
			// This is a match!
			if(has_prefix)
				act->on = prefix=='+'? true : false;
			else
				act->on = !act->on;
			return true;
		}
	}
	return false;
}

//===========================================================================
// CCmdListActs
//===========================================================================
int CCmdListActs(int argc, char **argv)
{
	action_t	*act;

	Con_Message("Action commands registered by the game DLL:\n");
	for(act = ddactions; act->name[0]; act++)
		Con_Message("  %s\n", act->name);
	return true;
}


