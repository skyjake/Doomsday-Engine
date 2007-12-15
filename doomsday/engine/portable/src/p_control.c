/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
 * p_control.c: Player Controls
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_play.h" // for P_LocalToConsole()
#include "de_network.h"
#include "de_misc.h"
#include "de_system.h"
#include "de_graphics.h"

#include "b_main.h"
#include "b_device.h"

// MACROS ------------------------------------------------------------------

/*
// Number of triggered impulses buffered into each player's control state
// table.  The buffer is emptied when a ticcmd is built.
#define MAX_IMPULSES 	8
#define MAX_DESCRIPTOR_LENGTH 20

#define SLOW_TURN_TIME  (6.0f / 35)
*/
// TYPES -------------------------------------------------------------------

/**
 * The control descriptors contain a mapping between symbolic control
 * names and the identifier numbers.
 */
/*
typedef struct controldesc_s {
	char    name[MAX_DESCRIPTOR_LENGTH + 1];
} controldesc_t;

typedef struct controlclass_s {
	uint    count;
	controldesc_t *desc;
} controlclass_t;
*/
/**
 * Each player has his own control state table.
 */
/*typedef struct controlstate_s {
	// The axes are updated whenever their values are needed,
	// i.e. during the call to P_BuildCommand.
	controlaxis_t *axes;

	// The toggles are modified via console commands.
	controltoggle_t *toggles;

	// The triggered impulses are stored into a ring buffer.
	uint    head, tail;
	impulse_t impulses[MAX_IMPULSES];
} controlstate_t;
*/

typedef struct impulsecounter_s {
    int     control;
    short   counts[DDMAXPLAYERS];
} impulsecounter_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(ListPlayerControls);
D_CMD(ClearControlAccumulation);
D_CMD(Impulse);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

//static uint controlFind(controlclass_t *cl, const char *name);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

//extern unsigned int numBindClasses;

// PUBLIC DATA DEFINITIONS -------------------------------------------------
/*
// Control class names - [singular, plural].
const char *ctlClassNames[NUM_CONTROL_CLASSES][NUM_CONTROL_CLASSES] = {
    {{"Axis"}, {"Axes"}},
    {{"Toggle"}, {"Toggles"}},
    {{"Impulse"}, {"Impulses"}}
};*/

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*static int ctlInfo = false;
static controlclass_t ctlClass[NUM_CONTROL_CLASSES];
static controlstate_t ctlState[DDMAXPLAYERS];
*/

static playercontrol_t* playerControls;
static impulsecounter_t** impulseCounts;
static int playerControlCount;

// CODE --------------------------------------------------------------------

static playercontrol_t* P_AllocPlayerControl(void)
{
    playerControls = M_Realloc(playerControls, sizeof(playercontrol_t) *
                               ++playerControlCount);
    impulseCounts = M_Realloc(impulseCounts, sizeof(impulsecounter_t*) *
                              playerControlCount);
    memset(&playerControls[playerControlCount - 1], 0, sizeof(playercontrol_t));
    impulseCounts[playerControlCount - 1] = NULL;
    return &playerControls[playerControlCount - 1];
}

/**
 * Register the console commands and cvars of the player controls subsystem.
 */
void P_ControlRegister(void)
{
    C_CMD("listcontrols",   "",     ListPlayerControls);
    C_CMD("impulse",        NULL,   Impulse);
    C_CMD("resetctlaccum",  "",     ClearControlAccumulation);
}

/**
 * This function is exported, so that plugins can register their controls.
 */
void P_NewPlayerControl(int id, controltype_t type, const char *name, const char* bindClass)
{
    playercontrol_t *pc = P_AllocPlayerControl();
    pc->id = id;
    pc->type = type;
    pc->name = strdup(name);
    pc->bindClassName = strdup(bindClass);

    if(type == CTLT_IMPULSE)
    {
        // Also allocate the impulse counter.
        impulseCounts[pc - playerControls] = M_Calloc(sizeof(impulsecounter_t));
    }
}

playercontrol_t* P_PlayerControlById(int id)
{
    int     i;

    for(i = 0; i < playerControlCount; ++i)
    {
        if(playerControls[i].id == id)
            return playerControls + i;
    }
    return NULL;
}

playercontrol_t* P_PlayerControlByName(const char* name)
{
    int     i;

    for(i = 0; i < playerControlCount; ++i)
    {
        if(!strcasecmp(playerControls[i].name, name))
            return playerControls + i;
    }
    return NULL;
}

void P_ControlShutdown(void)
{
    int     i;

    for(i = 0; i < playerControlCount; ++i)
    {
        free(playerControls[i].name);
        free(playerControls[i].bindClassName);
        M_Free(impulseCounts[i]);
    }
    playerControlCount = 0;
    M_Free(playerControls);
    playerControls = 0;
    M_Free(impulseCounts);
    impulseCounts = 0;
}

void P_GetControlState(int playerNum, int control, float* pos, float* relativeOffset)
{
    float tmp;
    struct bclass_s* bc = 0;
    struct dbinding_s* binds = 0;

#if _DEBUG
    // Check that this is really a numeric control.
    {
        playercontrol_t* pc = P_PlayerControlById(control);
        assert(pc);
        assert(pc->type == CTLT_NUMERIC);
    }
#endif

    // Ignore NULLs.
    if(!pos) pos = &tmp;
    if(!relativeOffset) relativeOffset = &tmp;

    binds = B_GetControlDeviceBindings(P_ConsoleToLocal(playerNum), control, &bc);
    B_EvaluateDeviceBindingList(binds, pos, relativeOffset, bc);
}

/**
 * @return  Number of times the impulse has been triggered since the last call.
 */
int P_GetImpulseControlState(int playerNum, int control)
{
    playercontrol_t* pc = P_PlayerControlById(control);
    short *counter;
    int count = 0;

    assert(pc != 0);

#if _DEBUG
    // Check that this is really an impulse control.
    assert(pc->type == CTLT_IMPULSE);
#endif
    if(!impulseCounts[pc - playerControls])
        return 0;

    counter = &impulseCounts[pc - playerControls]->counts[playerNum];
    count = *counter;
    *counter = 0;
    return count;
}

void P_Impulse(int playerNum, int control)
{
    playercontrol_t* pc = P_PlayerControlById(control);

    assert(pc);

    // Check that this is really an impulse control.
    if(pc->type != CTLT_IMPULSE)
    {
        Con_Message("P_Impulse: Control '%s' is not an impulse control.\n", pc->name);
        return;
    }

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return;

    impulseCounts[pc - playerControls]->counts[playerNum]++;
}

void P_ImpulseByName(int playerNum, const char* control)
{
    playercontrol_t* pc = P_PlayerControlByName(control);
    if(pc)
    {
        P_Impulse(playerNum, pc->id);
    }
}

D_CMD(ClearControlAccumulation)
{
    int     i, p;
    playercontrol_t* pc;

    for(i = 0; i < playerControlCount; ++i)
    {
        pc = &playerControls[i];
        for(p = 0; p < DDMAXPLAYERS; ++p)
        {
            if(pc->type == CTLT_NUMERIC)
                P_GetControlState(p, pc->id, NULL, NULL);
            else if(pc->type == CTLT_IMPULSE)
                P_GetImpulseControlState(p, pc->id);
        }
    }
    return true;
}

#if 0
/**
 * Initialize the control state table of the specified player. The control
 * descriptors must be fully initialized before this is called.
 */
void P_ControlTableInit(int player)
{
	uint        i;
    controlstate_t *s = &ctlState[player];

	// Allocate toggle states.
	s->toggles =
        M_Calloc(ctlClass[CC_TOGGLE].count * sizeof(controltoggle_t));

	// Allocate an axis state for each axis control.
	s->axes = M_Calloc(ctlClass[CC_AXIS].count * sizeof(controlaxis_t));
	for(i = 0; i < ctlClass[CC_AXIS].count; ++i)
	{
		// The corresponding toggle control.  (Always exists.)
		s->axes[i].toggle = &s->toggles[
			controlFind(&ctlClass[CC_TOGGLE],
						  ctlClass[CC_AXIS].desc[i].name) - 1];
	}

	// Clear the impulse buffer.
	s->head = s->tail = 0;
}

/**
 * Free the memory allocated for the player's control state table.
 */
static void controlTableFree(int player)
{
	controlstate_t *s = &ctlState[player];

	M_Free(s->axes);
	M_Free(s->toggles);
	memset(s, 0, sizeof(*s));
}

/**
 * Free the control descriptors and state tables.
 */
void P_ControlShutdown(void)
{
	uint        i;

	// Free the control tables of the local players.
	for(i = 0; i < DDMAXPLAYERS; ++i)
	{
		if(players[i].flags & DDPF_LOCAL)
            controlTableFree(i);
	}

	for(i = 0; i < NUM_CONTROL_CLASSES; ++i)
	{
        if(ctlClass[i].desc)
		    M_Free(ctlClass[i].desc);
	}
	memset(ctlClass, 0, sizeof(ctlClass));
}

/**
 * Look up the index of the specified control.
 */
static uint controlFind(controlclass_t *cl, const char *name)
{
	uint        i;

    // \fixme Use a faster than O(n) linear search.
	for(i = 0; i < cl->count; ++i)
	{
		if(!stricmp(cl->desc[i].name, name))
			return i + 1; // index + 1
	}

    return 0;
}

/**
 * Look up the index of the specified axis control.
 */
uint P_ControlFindAxis(const char *name)
{
	return controlFind(&ctlClass[CC_AXIS], name);
}

/**
 * Update the state of the axis and return a pointer to it.
 */
float P_ControlGetAxis(int player, const char *name)
{
	uint        idx = controlFind(&ctlClass[CC_AXIS], name);
	controlaxis_t *axis;
	float       pos;

#ifdef _DEBUG
	if(idx == 0)
	{
		Con_Error("P_ControlGetAxis: '%s' is undefined.\n", name);
	}
#endif

	axis = &ctlState[player].axes[idx - 1];
	pos = axis->pos;

	// Update the axis position, if the axis toggle control is active.
	if(axis->toggle && axis->toggle->state != TG_MIDDLE)
	{
		pos = (axis->toggle->state == TG_POSITIVE? 1 : -1);

		// During the slow turn time, the speed is halved.
/*      // DJS - The time and strength of the slow speed modifier should be
        //       specified when the axis is registered by the game.
        //       A time of zero, would mean no slow turn timer.
        if(Sys_GetSeconds() - axis->toggle->time < SLOW_TURN_TIME)
		{
			pos /= 2;
		}
*/
    }
    //Con_Message("P_ControlGetAxis(%s): returning pos = %f\n", name, pos);

	return pos;
}

/**
 * Retrieve the name of an axis control.
 *
 * @param index     The index of the axis control to retrieve the name of.
 *
 * @return          The name of an axis control at index.
 */
const char *P_ControlGetAxisName(uint index)
{
    return ctlClass[CC_AXIS].desc[index].name;
}

/**
 * Create a new control descriptor.
 */
static controldesc_t *controlAdd(uint class, const char *name)
{
	controlclass_t *c = &ctlClass[class];
	controldesc_t *d;

	c->desc = M_Realloc(c->desc, sizeof(*c->desc) * ++c->count);
	d = c->desc + c->count - 1;
	memset(d, 0, sizeof(*d));
	strncpy(d->name, name, sizeof(d->name) - 1);
	return d;
}

/**
 * Register a new player control descriptor.
 * Part of the Doomsday public API.
 *
 * Axis controls will automatically get toggleable controls too.
 *
 * The controls whose names are in CAPITAL LETTERS will be included 'as is'
 * in a ticcmd.  The state of other controls won't be sent over the network.
 *
 * All the symbolic names must be unique and at most MAX_DESCRIPTOR_LENGTH
 * chars long!
 *
 * @param cClass        Control class of the descriptor being registered.
 * @param name          A unique name for the new control descriptor.
 */
void P_RegisterPlayerControl(uint cClass, const char *name)
{
    uint        i;

    if(!name || !name[0])
        Con_Error("P_RegisterPlayerControl: Control missing name.");

    if(cClass >= NUM_CONTROL_CLASSES)
        Con_Error("P_RegisterPlayerControl: Unknown class %i. "
                  "Could not register control \"%s\".", cClass, name);

    // Names must be at most 16 chars long.
    if(strlen(name) > MAX_DESCRIPTOR_LENGTH)
        Con_Error("P_RegisterPlayerControl: Cannot register control \"%s\""
                  " - too many characters (%i max).",
                  name, MAX_DESCRIPTOR_LENGTH);

    // Names must be unique.
    // (Actually, there can be both upper and lower-case versions when
    // dealing with axis controls but as far as the games are concerned;
    // they must be completely unique).
    for(i = 0; i < NUM_CONTROL_CLASSES; ++i)
    {
        if(controlFind(&ctlClass[i], name) > 0)
            Con_Error("P_RegisterPlayerControl: There already exists a "
                      "control with the name \"%s\". Names must be unique.",
                      name);
    }

    controlAdd(cClass, name);

    if(cClass == CC_AXIS)
    {
	    char        buf[MAX_DESCRIPTOR_LENGTH + 1];
        // Also create a toggle for each axis, but use a lower case name so
        // it won't be included in ticcmds.
		strncpy(buf, name, sizeof(buf) - 1);
		strlwr(buf);
		controlAdd(CC_TOGGLE, buf);
    }
}

/**
 * Retrieve a bitmap that specifies which toggle controls are currently
 * active. Only the UPPER CASE toggles are included. The other toggle
 * controls are intended for local use only.
 *
 * @param player        Player num whose toggle control states to collect.
 *
 * @return              The created bitmap.
 */
int P_ControlGetToggles(int player)
{
	int         bits = 0;
    uint        pos = 0, i;
	controlclass_t *cc = &ctlClass[CC_TOGGLE];
	controlstate_t *state = &ctlState[player];

	for(i = 0; i < cc->count; ++i)
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
	// We assume that we can only use up to seven bits for the controls.
	if(pos >= 7)
		Con_Error("P_ControlGetToggles: Out of bits (%i).\n", pos);
#endif

	return bits;
}

/**
 * Clear all toggle controls.
 *
 * @param player        Player number whose toggle controls to clear.
 *                      If negative, clear ALL player's toggle controls.
 */
void P_ControlReset(int player)
{
    int         console;
	uint        i, k;
	controlstate_t *s;

    if(player < 0)
    {   // Clear toggles for ALL players.
	    for(i = 0, s = ctlState; i < DDMAXPLAYERS; ++i, s++)
	    {
		    if(!s->toggles)
                continue;

            for(k = 0; k < ctlClass[CC_TOGGLE].count; ++k)
		    {
			    s->toggles[k].state = TG_OFF;
		    }
	    }
    }
    else
    {   // Clear toggles for local player #n.
        console = P_LocalToConsole(player);
	    if(console >= 0 && console < DDMAXPLAYERS)
        {
            s = &ctlState[console];
            for(k = 0; k < ctlClass[CC_TOGGLE].count; ++k)
		    {
			    s->toggles[k].state = TG_OFF;
		    }
        }
    }
}

/**
 * Store the specified impulse to the player's impulse buffer.
 *
 * @param player        Player number the impulse is for.
 * @param impulse       The impluse to be executed.
 */
static void controlImpulse(int player, impulse_t impulse)
{
    controlstate_t *s = &ctlState[player];
	uint        old = s->tail;

	s->impulses[s->tail] = impulse;
	s->tail = (s->tail + 1) % MAX_IMPULSES;

    if(s->tail == s->head)
	{
		// Oops, must cancel.  The buffer is full.
		s->tail = old;
	}
}

/**
 * Is the given string a valid control command?
 *
 * @param cmd           Ptr to the string to be tested.
 *
 * @return              @c true, if valid.
 */
boolean P_IsValidControl(const char *cmd)
{
	char        name[20], *ptr;
    uint        i, idx;
    int         console, localPlayer = 0;

    // Check the prefix to see what will be the new state of the
	// toggle.
	if(cmd[0] == '+' || cmd[0] == '-')
	{
		if(cmd[0] == cmd[1]) // ++ / --
			cmd += 2;
		else
			cmd++;
	}

    // Copy the name of the control and find the end.
    for(ptr = name, i = 0; *cmd && *cmd != '/'; cmd++, ++i)
    {
        if(i >= 20)
            return false;

        *ptr++ = *cmd;
    }
    *ptr++ = 0;

	// Is the local player number specified?
	if(*cmd == '/')
	{
		// The specified number could be bogus.
		localPlayer = strtol(cmd + 1, NULL, 10);
	}

	// Which player will be affected?
	console = P_LocalToConsole(localPlayer);
	if(console >= 0 && console < DDMAXPLAYERS)
	{
	    // Is the given name a valid control name?  First check the toggle
	    // controls.
        if((idx = controlFind(&ctlClass[CC_TOGGLE], name)) > 0)
		    return true;

	    // How about impulses?
	    if((idx = controlFind(&ctlClass[CC_IMPULSE], name)) > 0)
		    return true;
    }

	return false;
}

/**
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
 *
 * @param cmd           The action command to search for.
 *
 * @return              @c true, if the action was changed
 *                      successfully.
 */
boolean P_ControlExecute(const char *cmd)
{
	char        name[MAX_DESCRIPTOR_LENGTH + 1], *ptr;
    uint        idx;
    int         console, localPlayer = 0;
    togglestate_t newState = TG_TOGGLE;
	controlstate_t *state = NULL;

    if(!cmd || !cmd[0] || strlen(cmd) > MAX_DESCRIPTOR_LENGTH)
        return false;

    // Check the prefix to see what will be the new state of the
	// toggle.
	if(cmd[0] == '+' || cmd[0] == '-')
	{
		if(cmd[0] == cmd[1]) // ++ / --
		{
			newState = (cmd[0] == '+'? TG_POSITIVE : TG_NEGATIVE);
			cmd += 2;
		}
		else
		{
			newState = (cmd[0] == '+'? TG_ON : TG_OFF);
			cmd++;
		}
	}

    // Copy the name of the control and find the end.
    for(ptr = name; *cmd && *cmd != '/'; cmd++)
        *ptr++ = *cmd;
    *ptr++ = 0;

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
        state = &ctlState[console];
    }

    if(state)
    {
	    // Is the given name a valid control name?  First check the toggle
	    // controls.
        if((idx = controlFind(&ctlClass[CC_TOGGLE], name)) > 0)
	    {
		    // This is the control that must be changed.
		    controltoggle_t *toggle = state->toggles + idx - 1;

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
		    return true;
	    }

	    // How about impulses?
	    if((idx = controlFind(&ctlClass[CC_IMPULSE], name)) > 0)
	    {
	        controlImpulse(console, idx - 1);
		    return true;
	    }
    }

	return false;
}

/**
 * Update the position of an axis control. This is called periodically
 * from the axis binding code (for STICK axes).
 */

void P_ControlSetAxis(int player, uint axisControlIndex, float pos)
{
	if(player < 0 || player >= DDMAXPLAYERS)
        return;

    if(ctlState[player].axes == NULL)
        return;
#if _DEBUG
    if(axisControlIndex > ctlClass[CC_AXIS].count)
        Con_Error("P_ControlSetAxis: Invalid axis index %i.",
                  axisControlIndex);
#endif

    ctlState[player].axes[axisControlIndex].pos = pos;
}

/**
 * Move a control bound to a POINTER type axis.
 */
void P_ControlAxisDelta(int player, uint axisControlIndex, float delta)
{
//    controldesc_t *desc;
//    ddplayer_t *plr;

	if(player < 0 || player >= DDMAXPLAYERS)
        return;

    if(ctlState[player].axes == NULL)
        return;

#if _DEBUG
    if(axisControlIndex > ctlClass[CC_AXIS].count)
        Con_Error("P_ControlAxisDelta: Invalid axis index %i.",
                  axisControlIndex);
#endif

    ctlState[player].axes[axisControlIndex].pos += delta;

    // FIXME: These should be in PlayerThink.
    /*
    plr = &players[player];

	// Get a descriptor of the axis control.
	desc = &ctlClass[CC_AXIS].desc[axisControlIndex];

	switch(axisControlIndex)
	{
	case CTL_TURN:
        // $unifiedangles
        if(plr->mo)
		    plr->mo->angle -= (angle_t) (delta/180 * ANGLE_180);
		break;

	case CTL_LOOK:
		// 110 corresponds 85 degrees.
        // $unifiedangles
        plr->lookdir += delta * 110.0f/85.0f;
        // Clamp it.
        if(plr->lookdir > 110)
            plr->lookdir = 110;
        if(plr->lookdir < -110)
            plr->lookdir = -110;
		break;

	default:
		// Undefined for other axis controls?
		break;
	}*/
}

/**
 * Update view angles according to the "turn" or "look" axes.
 * Done for all local players.
 *
 * FIXME: The code here is game logic, it does not belong here.
 * Functionality to be done in PlayerThink.
 */
void P_ControlTicker(timespan_t time)
{
#if 0
	/** \fixme  Player class turn speed.
	* angleturn[3] = {640, 1280, 320};	// + slow turn
	*/
	uint        i;
	float       pos, mul;

    mul = time * TICSPERSEC * (640 << 16) * 45.0f/ANGLE_45;
	for(i = 0; i < DDMAXPLAYERS; ++i)
	{
		if(!players[i].ingame || !(players[i].flags & DDPF_LOCAL))
            continue;

		pos = P_ControlGetAxis(i, "turn");
		P_ControlAxisDelta(i, CTL_TURN, mul * pos);

		pos = P_ControlGetAxis(i, "look");
		P_ControlAxisDelta(i, CTL_LOOK, mul * pos);
	}
#endif
}

/**
 * Draws a HUD overlay with information about the state of the game controls as seen
 * by the engine. The cvar @c ctl-info is used for toggling the drawing of this info.
 */
void P_ControlDrawer(void)
{
    int         winWidth, winHeight;

    if(!ctlInfo)
        return;

    // Go into screen projection mode.
    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, theWindow->width, theWindow->height, -1, 1);

    // TODO: Draw control state here.

    // Back to the original.
    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}
#endif

/**
 * Prints a list of the registered control descriptors.
 */
D_CMD(ListPlayerControls)
{
    /*
    uint        i, j;
	char        buf[MAX_DESCRIPTOR_LENGTH+1];

    Con_Message("Player Controls:\n");
    for(i = 0; i < NUM_CONTROL_CLASSES; ++i)
    {
        controlclass_t *cClass = &ctlClass[i];

        if(cClass->count > 0)
        {
            Con_Message("%i %s:\n", cClass->count,
                        ctlClassNames[i][cClass->count > 1]);
            for(j = 0; j < cClass->count; ++j)
            {
		        strncpy(buf, cClass->desc[j].name, sizeof(buf) - 1);
		        strlwr(buf);
                buf[strlen(cClass->desc[j].name)] = 0;
                Con_Message("  %s\n", buf);
            }
        }
    }*/
    return true;
}

D_CMD(Impulse)
{
    int playerNum = consoleplayer;

    if(argc < 2 || argc > 3)
    {
        Con_Printf("Usage:\n  %s (impulse-name)\n  %s (impulse-name) (player-number)\n",
                   argv[0], argv[0]);
        return true;
    }
    if(argc == 3)
    {
        playerNum = strtoul(argv[2], NULL, 10);
    }
    P_ImpulseByName(playerNum, argv[1]);
    return true;
}
