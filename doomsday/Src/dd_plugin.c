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
 * dd_plugin.c: Plugin Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_defs.h"

// MACROS ------------------------------------------------------------------

#define HOOKMASK(x)		((x) & 0xffffff)

// TYPES -------------------------------------------------------------------

typedef struct
{
	int			exclude;
	hookfunc_t	list[MAX_HOOKS];
} hookreg_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

hookreg_t hooks[NUM_HOOK_TYPES];

// CODE --------------------------------------------------------------------

/*
 * Plug_AddHook
 *	This routine is called by plugin DLLs who want to register hooks.
 *	Returns true iff the hook was successfully registered.
 */
int Plug_AddHook(int hook_type, hookfunc_t hook)
{
	int i, type = HOOKMASK(hook_type);
	
	// The type must be good.
	if(type < 0 || type >= NUM_HOOK_TYPES) return false;

	// Exclusive hooks.
	if(hook_type & HOOKF_EXCLUSIVE)
	{
		hooks[type].exclude = true;
		memset(hooks[type].list, 0, sizeof(hooks[type].list));
	}
	else if(hooks[type].exclude) 
	{
		// An exclusive hook has closed down this list.
		return false; 
	}

	for(i = 0; i < MAX_HOOKS && hooks[type].list[i]; i++);
	if(i == MAX_HOOKS) return false; // No more hooks allowed!

	// Add the hook.
	hooks[type].list[i] = hook;
	return true;
}

/*
 * Plug_RemoveHook
 *	Removes the given hook, and returns true iff it was found.
 */
int Plug_RemoveHook(int hook_type, hookfunc_t hook)
{
	int i, type = HOOKMASK(hook_type);

	// The type must be good.
	if(type < 0 || type >= NUM_HOOK_TYPES) return false;
	for(i = 0; i < MAX_HOOKS; i++)
		if(hooks[type].list[i] == hook)
		{
			hooks[type].list[i] = NULL;
			if(hook_type & HOOKF_EXCLUSIVE)
			{
				// Exclusive hook removed; allow normal hooks.
				hooks[type].exclude = false;
			}
			return true;
		}
	return false;
}

/*
 * Plug_DoHook
 *	Executes all the hooks of the given type. Bit zero of the return value
 *	is set if a hook was executed successfully (returned true). Bit one is
 *	set if all the hooks that were executed returned true.
 */
int Plug_DoHook(int hook_type)
{
	int i, ret = 0;
	boolean allgood = true;
	int parm = 0;
	void *data = NULL;

	// Set the parm and data pointer according to the hook type.
	switch(hook_type)
	{
	case HOOK_DEFS:
		data = &defs;
		break;

	default:
		break;
	}
	
	// Try all the hooks.
	for(i = 0; i < MAX_HOOKS; i++)
		if(hooks[hook_type].list[i])
		{
			if(hooks[hook_type].list[i](hook_type, parm, data)) 
			{
				// One hook executed; return nonzero from this routine.
				ret = 1;
			}
			else allgood = false;
		}
	if(ret && allgood) ret |= 2;
	return ret;
}

