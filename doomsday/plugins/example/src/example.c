/** @file example.c Example of Doomsday plugin which is called at startup.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include <doomsday.h>
#include "version.h"

/*
 * We are using the public Con API for accessing the Console and the Plug API
 * for registing a plugin hook. The DENG_USING_API() macro makes a public API
 * available to be called within this source file. Place it in a common header
 * file to allow many source files to access the API.
 *
 * In addition to invoking this macro, you must also perform the API exchange
 * (see the bottom of this source file).
 */
DENG_USING_API(Con);
DENG_USING_API(Plug);

/**
 * This function will be called ASAP after Doomsday has completed startup.
 * In this example, the function has been declared static to highlight that
 * it isn't exported from the plugin.
 *
 * @return  This operation should return non-zero if successful.
 */
static int ExampleHook(int hookType, int parm, void *data)
{
    DENG_UNUSED(hookType);
    DENG_UNUSED(parm);
    DENG_UNUSED(data);

    Con_Message("ExampleHook: Hook successful!");
    return true;
}

// Exported functions for interfacing with the engine ------------------------

/**
 * Declares the type of the plugin so the engine knows how to treat it. Called
 * during plugin loading, before DP_Initialize().
 */
DENG_EXTERN_C char const *deng_LibraryType(void)
{
    return "deng-plugin/generic";
}

/**
 * This function is called automatically when the plugin is loaded. We let the
 * engine know what we'd like to do.
 */
DENG_EXTERN_C void DP_Initialize(void)
{
    Plug_AddHook(HOOK_STARTUP, ExampleHook);
}

/*
 * Public APIs that are being used in this plugin. Each API that is used in the
 * plugin must be declared once; while you can invoke DENG_USING_API() many
 * times, DENG_DECLARE_API() can only be invoked once in the plugin.
 *
 * See engine/api/apis.h and the api_* headers for a list of all the available
 * public APIs.
 */
DENG_DECLARE_API(Con);
DENG_DECLARE_API(Plug);

/*
 * The API exchange will guarantee that we get the correct version of each API.
 * In this case, we are using the latest version of the Console API
 * (DE_API_CONSOLE).
 *
 * The macro will define a function called "deng_API". It is exported so the
 * engine can call it during plugin loading (on Windows, see "example.def").
 */
DENG_API_EXCHANGE(
    DENG_GET_API(DE_API_CONSOLE, Con);
    DENG_GET_API(DE_API_PLUGIN, Plug);
)
