/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_control.c: Player Controls
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

/*
 * Number of triggered impulses buffered into each player's control
 * state table.  The buffer is emptied when a ticcmd is built.
 */
#define MAX_IMPULSES 	8

#define SLOW_TURN_TIME	(6.0f / 35)

// TYPES -------------------------------------------------------------------

// Control classes.
enum
{
	CC_AXIS,
	CC_TOGGLE,
	CC_IMPULSE,
	NUM_CONTROL_CLASSES
};

// Built-in controls.
enum
{
	CTL_WALK = 0,
	CTL_SIDESTEP,
	CTL_ZFLY,
	CTL_TURN,
	CTL_LOOK
};

/*
 * The control descriptors contain a mapping between symbolic control
 * names and the identifier numbers.
 */ 
typedef struct controldesc_s {
	char name[20];
} controldesc_t;

typedef struct controlclass_s {
	int count;
	controldesc_t *desc;
} controlclass_t;

/*
 * Each player has his own control state table.
 */
typedef struct controlstate_s {
	// The axes are updated whenever their values are needed,
	// i.e. during the call to P_BuildCommand.
	controlaxis_t *axes;

	// The toggles are modified via console commands.
	controltoggle_t *toggles;

	// The triggered impulses are stored into a ring buffer.
	int head, tail;
	impulse_t impulses[MAX_IMPULSES];
} controlstate_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static controlclass_t ctlClass[NUM_CONTROL_CLASSES];
static controlstate_t ctlState[DDMAXPLAYERS];

// CODE --------------------------------------------------------------------

/*
 * Create a new control descriptor.
 */
controldesc_t *P_ControlAdd(int class, const char *name)
{
	controlclass_t *c = &ctlClass[class];
	controldesc_t *d;

	c->desc = realloc(c->desc, sizeof(*c->desc) * ++c->count);
	d = c->desc + c->count - 1;
	memset(d, 0, sizeof(*d));
	strncpy(d->name, name, sizeof(d->name) - 1);
	return d;
}

/*
 * Look up the index of the specified control.
 */
int P_ControlFind(controlclass_t *cl, const char *name)
{
	int i;

	for(i = 0; i < cl->count; i++)
	{
		if(!stricmp(cl->desc[i].name, name))
			return i;
	}
	return -1;
}

/*
 * Look up the index of the specified axis control.
 */
int P_ControlFindAxis(const char *name)
{
	return P_ControlFind(&ctlClass[CC_AXIS], name);
}

/*
 * Update the state of the axis and return a pointer to it.
 */
float P_ControlGetAxis(int player, const char *name)
{
	int idx = P_ControlFind(&ctlClass[CC_AXIS], name);
	controlaxis_t *axis;
	float pos;

#ifdef _DEBUG
	if(idx < 0)
	{
		Con_Error("P_ControlGetAxis: '%s' is undefined.\n", name);
	}
#endif
	
	axis = ctlState[player].axes + idx;
	pos = axis->pos;

	// Update the axis position, if the axis toggle control is active.
	if(axis->toggle && axis->toggle->state != TG_MIDDLE)
	{
		pos = (axis->toggle->state == TG_POSITIVE? 1 : -1);

		// During the slow turn time, the speed is halved.
		if(Sys_GetSeconds() - axis->toggle->time < SLOW_TURN_TIME)
		{
			pos /= 2;
		}
	}

	return pos;
}

/*
 * Return the name of an axis control.
 */
const char *P_ControlGetAxisName(int index)
{
	if(index < 0 || index >= ctlClass[CC_AXIS].count) return NULL;
	return ctlClass[CC_AXIS].desc[index].name;
}

/*
 * Returns a bitmap that specifies which toggle controls are currently
 * active.  Only the UPPER CASE toggles are included.  The other
 * toggle controls are intended for local use only.
 */
int P_ControlGetToggles(int player)
{
	int bits = 0, pos = 0, i;
	controlclass_t *cc = &ctlClass[CC_TOGGLE];
	controlstate_t *state = &ctlState[player];

	for(i = 0; i < cc->count; i++)
	{
		// Only the upper case toggles are included.
		if(isupper(cc->desc[i].name[0]))
		{
			if(state->toggles[i].state != TG_OFF)
			{
				bits |= 1 << pos;
			}
			pos++;
		}
	}

#ifdef _DEBUG
	// We assume that we can only use up to seven bits for the
	// controls.
	if(pos >= 7)
	{
		Con_Error("P_ControlGetToggles: Out of bits (%i).\n", pos);
	}
#endif
		
	return bits;
}

/*
 * Initialize the control descriptors.  These could be read from a
 * file.  The game is able to define new controls in addition to the
 * ones listed here.
 */
void P_ControlInit(void)
{
	/*
	 * All the axis controls automatically get toggleable controls,
	 * too.  All the symbolic names must be unique!
	 *
	 * The controls whose names are in CAPITAL LETTERS will be
	 * included 'as is' in a ticcmd.  The state of other controls
	 * won't be sent over the network.
	 *
	 * The names can be at most 8 chars long.
	 */

	// These must match the CTL_* indices.
	const char *axisCts[] = {
		"WALK",
		"SIDESTEP",
		"ZFLY",
		"turn",
		"look",
		NULL
	};
	const char *toggleCts[] = {
		"ATTACK",
		"USE",
		"JUMP",
		"speed",
		"strafe",
		"mlook",
		"jlook",
		NULL
	};
	const char *impulseCts[] = {
		"weapon1",		
		"weapon2",		
		"weapon3",		
		"weapon4",		
		"weapon5",		
		"weapon6",		
		"weapon7",
		"weapon8",		
		"weapon9",		
		"weapon10",
		"nextwpn",
		"prevwpn",
		"falldown",
		"lookcntr",
		NULL
	};
	int i;
	char buf[80];

	// The toggle controls.
	for(i = 0; toggleCts[i]; i++)
		P_ControlAdd(CC_TOGGLE, toggleCts[i]);

	// The axis controls.
	for(i = 0; axisCts[i]; i++)
	{
		P_ControlAdd(CC_AXIS, axisCts[i]);

		// Also create a toggle for each axis, but use a lower case
		// name so it won't be included in ticcmds.
		strcpy(buf, axisCts[i]);
		strlwr(buf);
		P_ControlAdd(CC_TOGGLE, buf);
	}

	// The impulse controls.
	for(i = 0; impulseCts[i]; i++)
		P_ControlAdd(CC_IMPULSE, impulseCts[i]);
}

/*
 * Free the control descriptors and state tables.
 */
void P_ControlShutdown(void)
{
	int i;

	// Free the control tables of the local players.
	for(i = 0; i < DDMAXPLAYERS; i++)
	{
		if(players[i].flags & DDPF_LOCAL) P_ControlTableFree(i);
	}
	
	for(i = 0; i < NUM_CONTROL_CLASSES; i++)
	{
		free(ctlClass[i].desc);
	}
	memset(ctlClass, 0, sizeof(ctlClass));
}

/*
 * Initialize the control state table of the specified player.  The
 * control descriptors must be fully initialized before this is
 * called.
 */
void P_ControlTableInit(int player)
{
	controlstate_t *s = &ctlState[player];
	int i;

	// Allocate toggle states.
	s->toggles = calloc(ctlClass[CC_TOGGLE].count, sizeof(controltoggle_t));
	
	// Allocate an axis state for each axis control.
	s->axes = calloc(ctlClass[CC_AXIS].count, sizeof(controlaxis_t));
	for(i = 0; i < ctlClass[CC_AXIS].count; i++)
	{
		// The corresponding toggle control.  (Always exists.)
		s->axes[i].toggle = &s->toggles[
			P_ControlFind(&ctlClass[CC_TOGGLE],
						  ctlClass[CC_AXIS].desc[i].name) ];
	}
	
	// Clear the impulse buffer.
	s->head = s->tail = 0;
}

/*
 * Free the memory allocated for the player's control state table.
 */
void P_ControlTableFree(int player)
{
	controlstate_t *s = &ctlState[player];

	free(s->axes);
	free(s->toggles);
	memset(s, 0, sizeof(*s));
}

/*
 * Clear all toggle controls of all players.
 */
void P_ControlReset(void)
{
	int i, k;
	controlstate_t *s;
	
	for(i = 0, s = ctlState; i < DDMAXPLAYERS; i++, s++)
	{
		if(!s->toggles) continue;
		for(k = 0; k < ctlClass[CC_TOGGLE].count; k++)
		{
			s->toggles[k].state = TG_OFF;
		}
	}
}

/*
 * Store the specified impulse to the player's impulse buffer.
 */
void P_ControlImpulse(int player, impulse_t impulse)
{
	controlstate_t *s = &ctlState[player];
	int old = s->tail;

	s->impulses[s->tail] = impulse;
	s->tail = (s->tail + 1) % MAX_IMPULSES;
	if(s->tail == s->head)
	{
		// Oops, must cancel.  The buffer is full.
		s->tail = old;
	}
}

/*
 * Execute a control command, which will modify the state of a toggle
 * control or send a new impulse.  Returns true if a control was
 * successfully modified.  The command is case-insensitive.  This
 * function is called by the console.
 *
 * The command syntax is as follows: [+|-]name[/N]
 *
 *   name			Toggle state of 'name', local player zero
 * 	 name/3			Toggle state of 'name', local player 3
 *   name/12 		Toggle state of 'name', local player 12 (!)
 *   +name			Set 'name' to on/positive state (local zero)
 *	 ++name/4		Set 'name' to on/positive state (local 4)
 *   -name			Set 'name' to off/middle state (local zero)
 *	 --name/5		Set 'name' to negative state (local 5)
 */
boolean P_ControlExecute(const char *command)
{
	char name[20], *ptr;
	togglestate_t newState = TG_TOGGLE;
	int i, console, localPlayer = 0;
	controlstate_t *state = NULL;
	controltoggle_t *toggle;

	// Check the prefix to see what will be the new state of the
	// toggle.
	if(command[0] == '+' || command[0] == '-')
	{
		if(command[0] == command[1]) // ++ / --
		{
			newState = (command[0] == '+'? TG_POSITIVE : TG_NEGATIVE);
			command += 2;
		}
		else
		{
			newState = (command[0] == '+'? TG_ON : TG_OFF);
			command++;
		}
	}			
	
	// Copy the name of the control.
	for(ptr = name; *command && *command != '/'; command++)
		*ptr++ = *command;
	*ptr++ = 0;

	// Is the local player number specified?
	if(*command == '/')
	{
		// The specified number could be bogus.
		localPlayer = strtol(command + 1, NULL, 10);
	}

	// Which player will be affected?
	console = P_LocalToConsole(localPlayer);
	if(console >= 0 && console < DDMAXPLAYERS && players[console].ingame)
	{
		state = &ctlState[console];
	}
	
	// Is the given name a valid control name?  First check the toggle
	// controls.
	if((i = P_ControlFind(&ctlClass[CC_TOGGLE], name)) >= 0)
	{
		if(state)
		{
			// This is the control that must be changed.
			toggle = state->toggles + i;
			
			if(newState == TG_TOGGLE)
			{
				toggle->state = (toggle->state == TG_ON? TG_OFF : TG_ON);
			}
			else
			{
				toggle->state = newState;
			}

			// Update the toggle time.
			toggle->time = Sys_GetSeconds();
		}
		return true;
	}

	// How about impulses?
	if((i = P_ControlFind(&ctlClass[CC_IMPULSE], name)) >= 0)
	{
		if(state)
		{
			P_ControlImpulse(console, i);
		}
		return true;
	}
	
	return false;
}

/*
 * Update the position of an axis control.  This is called
 * periodically from the axis binding code (for STICK axes).
 */
void P_ControlSetAxis(int player, int axisControlIndex, float pos)
{
	if(player < 0 || player >= DDMAXPLAYERS) return;
	if(ctlState[player].axes == NULL) return;
	
	ctlState[player].axes[axisControlIndex].pos = pos;
}

/*
 * Move a control bound to a POINTER type axis.  This doesn't affect
 * actual position of the axis control (!).
 */
void P_ControlAxisDelta(int player, int axisControlIndex, float delta)
{
	controldesc_t *desc;
	ddplayer_t *plr;
	
	if(player < 0 || player >= DDMAXPLAYERS) return;
	if(ctlState[player].axes == NULL) return;

	plr = &players[player];
	
	// Get a description of the axis control.
	desc = &ctlClass[CC_AXIS].desc[axisControlIndex];

	switch(axisControlIndex)
	{
	case CTL_TURN:
		// Modify the clientside view angle directly.
		plr->clAngle -= (angle_t) (delta/180 * ANGLE_180);
		break;

	case CTL_LOOK:
		// 110 corresponds 85 degrees.
		plr->clLookDir += delta * 110.0f/85.0f;

		// Make sure it doesn't wrap around.
		if(plr->clLookDir > 110) plr->clLookDir = 110;
		if(plr->clLookDir < -110) plr->clLookDir = -110;
		break;

	default:
		// Undefined for other axis controls?
		break;
	}
}

/*
 * Update view angles according to the "turn" or "look" axes.
 * Done for all local players.
 */
void P_ControlTicker(timespan_t time)
{
	// FIXME: Player class turn speed.
	// angleturn[3] = {640, 1280, 320};	// + slow turn
	
	float pos, mul = time * TICSPERSEC * (640 << 16) * 45.0f/ANGLE_45;
	int i;

	for(i = 0; i < DDMAXPLAYERS; i++)
	{
		if(!players[i].ingame || !(players[i].flags & DDPF_LOCAL)) continue;
		
		pos = P_ControlGetAxis(i, "turn");
		P_ControlAxisDelta(i, CTL_TURN, mul * pos);

		pos = P_ControlGetAxis(i, "look");
		P_ControlAxisDelta(i, CTL_LOOK, mul * pos);
	}
}
