/**\file dd_plugin.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Plugin Subsystem.
 */

#ifndef LIBDENG_PLUGIN_H
#define LIBDENG_PLUGIN_H

#include "../portable/include/dd_version.h"

#define LIBDENG_PLUGINDESC      "(" DOOMSDAY_NICENAME " Plugin)"

#define MAX_HOOKS           16
#define HOOKF_EXCLUSIVE     0x01000000

typedef int     (*pluginfunc_t) (void);
typedef int     (*hookfunc_t) (int type, int parm, void *data);

// Hook types.
enum {
    HOOK_STARTUP = 0,               // Called ASAP after startup.
    HOOK_INIT = 1,                  // Called after engine has been initialized.
    HOOK_DEFS = 2,                  // Called after DEDs have been loaded.
    HOOK_MAP_CONVERT = 3,           // Called when a map needs converting.
    HOOK_TICKER = 4,                // Called as part of the run loop.
    HOOK_DEMO_STOP = 5,             // Called when demo playback completes.
    HOOK_FINALE_SCRIPT_BEGIN = 6,   // Called as a script begins.
    HOOK_FINALE_SCRIPT_STOP = 7,    // Called as a script stops.
    HOOK_FINALE_SCRIPT_TICKER = 8,  // Called each time a script 'thinks'.
    HOOK_FINALE_EVAL_IF = 9,        // Called to evaluate an IF conditional statement.
    HOOK_VIEWPORT_RESHAPE = 10,     // Called when viewport dimensions change.
    NUM_HOOK_TYPES
};

/// Unique identifier attributed to each plugin during initial startup.
typedef int pluginid_t;

// Paramaters for HOOK_FINALE_EVAL_IF
typedef struct {
    const char* token;
    boolean     returnVal;
} ddhook_finale_script_evalif_paramaters_t;

// Paramaters for HOOK_FINALE_SCRIPT_TICKER
typedef struct {
    boolean runTick;
    boolean canSkip;
} ddhook_finale_script_ticker_paramaters_t;

// Paramaters for HOOK_VIEWPORT_RESHAPE
typedef struct {
    RectRaw geometry; // New/Current.
    RectRaw oldGeometry; // Previous.
} ddhook_viewport_reshape_t;

/**
 * This routine is called by plugins who want to register hooks.
 *
 * @return  @c true, iff the hook was successfully registered.
 */
int Plug_AddHook(int hook_type, hookfunc_t hook);

/**
 * Removes the given hook.
 *
 * @return  @c true iff it was found.
 */
int Plug_RemoveHook(int hook_type, hookfunc_t hook);

/**
 * Check if a plugin is available of the specified type.
 *
 * @param hookType      Type of hook to check we have a plugin for.
 * @return  @c true, if a plugin is available for this hook type.
 */
int Plug_CheckForHook(int hookType);

#endif /* LIBDENG_PLUGIN_H */
