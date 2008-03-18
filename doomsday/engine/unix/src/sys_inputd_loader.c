/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * sys_inputd_loader.c: Loader for di*.so
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#ifdef UNIX
#  include "sys_dylib.h"
#endif

#include "de_console.h"
#include "sys_inputd.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

inputdriver_t inputd_external;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static lt_dlhandle handle;
static void (*driverShutdown) (void);

// CODE --------------------------------------------------------------------

static void dummyVoid(void)
{
}

static void *Imp(const char *fn)
{
	return lt_dlsym(handle, fn);
}

void DI_UnloadExternal(void)
{
	driverShutdown();
	lt_dlclose(handle);
	handle = NULL;
}

inputdriver_t *DI_ImportExternal(void)
{
	inputdriver_t      *d = &inputd_external;

	// Clear everything.
	memset(d, 0, sizeof(*d));

	d->Init = Imp("DI_Init");
	driverShutdown = Imp("DI_Shutdown");
	d->Event = Imp("DI_Event");
    d->MousePresent = Imp("DI_MousePresent");
    d->JoystickPresent = Imp("DI_JoystickPresent");
    d->GetKeyEvents = Imp("DI_GetKeyEvents");
    d->GetMouseState = Imp("DI_GetMouseState");
    d->GetJoystickState = Imp("DI_GetJoystickState");

	// We should free the DLL at shutdown.
	d->Shutdown = DI_UnloadExternal;
	return d;
}

/**
 * Only "SDLInput" is currently supported.
 */
inputdriver_t *DI_Load(const char *name)
{
	filename_t          fn;

#ifdef MACOSX
	sprintf(fn, "di%s.bundle", name);
#else
	// Compose the name, use the prefix "di".
	sprintf(fn, "libdi%s", name);
	strcat(fn, ".so");
#endif

	if((handle = lt_dlopenext(fn)) == NULL)
	{
		Con_Message("DI_Load: Loading of %s failed.\n", fn);
		return NULL;
	}

	return DI_ImportExternal();
}
