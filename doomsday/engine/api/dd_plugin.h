/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#define MAX_HOOKS           16
#define HOOKF_EXCLUSIVE     0x01000000

typedef int     (*hookfunc_t) (int type, int parm, void *data);

// Hook types.
enum {
    HOOK_STARTUP = 0,              // Called ASAP after startup.
    HOOK_INIT = 1,                 // Called after engine has been initialized.
    HOOK_DEFS = 2,                 // Called after DEDs have been loaded.
    HOOK_MAP_CONVERT = 3,          // Called when a map needs converting.
    HOOK_TICKER = 4,               // Called as part of the run loop.
    HOOK_INFINE_STATE_SERIALIZE_EXTRADATA = 5,
    HOOK_INFINE_STATE_DESERIALIZE_EXTRADATA = 6,
    HOOK_INFINE_SCRIPT_BEGIN = 7,  // Called as a script begins.
    HOOK_INFINE_SCRIPT_TERMINATE = 8, // Called as a script stops.
    HOOK_INFINE_SCRIPT_TICKER = 9, // Called each time a script 'thinks'.
    HOOK_INFINE_EVAL_IF = 10,      // Called to evaluate an IF conditional statement.
    NUM_HOOK_TYPES
};

// Paramaters for HOOK_INFINE_STATE_SERIALIZE_EXTRADATA
typedef struct {
    // Args:
    const void* conditions;

    // Return values:
    byte* outBuf;
    size_t outBufSize;
} ddhook_serializeconditions_paramaters_t;

// Paramaters for HOOK_INFINE_STATE_DESERIALIZE_EXTRADATA
typedef struct {
    const byte* inBuf;
    int* gameState;
    const void* conditions;
    size_t bytesRead;
} ddhook_deserializeconditions_paramaters_t;

// Paramaters for HOOK_INFINE_EVAL_IF
typedef struct {
    const char* token;
    void*       conditions;
    boolean     returnVal;
} ddhook_evalif_paramaters_t;

// Paramaters for HOOK_INFINE_SCRIPT_TERMINATE
typedef struct {
    int         initialGameState;
    void*       conditions;
} ddhook_scriptstop_paramaters_t;

// Paramaters for HOOK_INFINE_SCRIPT_TICKER
typedef struct {
    boolean     runTick;
    boolean     canSkip;
    int         gameState;
    void*       conditions;
} ddhook_scriptticker_paramaters_t;

int             Plug_AddHook(int hook_type, hookfunc_t hook);
int             Plug_RemoveHook(int hook_type, hookfunc_t hook);

// Plug_DoHook is used by the engine to call all functions registered to a hook.
int             Plug_DoHook(int hook_type, int parm, void *data);
int             Plug_CheckForHook(int hookType);

#endif /* LIBDENG_PLUGIN_H */
