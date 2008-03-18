/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 * sys_inputd_dummy.c: Dummy Input Driver
 *
 * Used when interactive user input is not needed/required.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "sys_inputd.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

int     DI_DummyInit(void);
void    DI_DummyShutdown(void);
void    DI_DummyEvent(int type);
boolean DI_MousePresent(void);
boolean DI_JoystickPresent(void);
size_t  DI_GetKeyEvents(keyevent_t *evbuf, size_t bufsize);
void    DI_GetMouseState(mousestate_t *state);
void    DI_GetJoystickState(joystate_t * state);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

inputdriver_t inputd_dummy = {
	DI_DummyInit,
	DI_DummyShutdown,
    DI_DummyEvent,
    DI_MousePresent,
    DI_JoystickPresent,
    DI_GetKeyEvents,
    DI_GetMouseState,
    DI_GetJoystickState
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited;

// CODE --------------------------------------------------------------------

/**
 * Init the dummy input driver.
 */
int DI_DummyInit(void)
{
	if(inited)
		return true; // Already initialized.

	inited = true;
	return true;
}

/**
 * Shut everything down.
 */
void DI_DummyShutdown(void)
{
	inited = false;
}

/**
 * The Event function is called to tell the driver about certain critical
 * events like the beginning and end of an update cycle.
 */
void DI_DummyEvent(int type)
{
	// Do nothing.
}

boolean DI_MousePresent(void)
{
    return false;
}

boolean DI_JoystickPresent(void)
{
    return false;
}

size_t DI_GetKeyEvents(keyevent_t *evbuf, size_t bufsize)
{
    // Do nothing.
    return 0;
}

void DI_GetMouseState(mousestate_t *state)
{
    // Do nothing.
}

void DI_GetJoystickState(joystate_t *state)
{
    // Do nothing.
}

size_t DI_GetConsoleKeyEvents(keyevent_t *evbuf, size_t bufsize)
{
    // Do nothing.
    return 0;
}
