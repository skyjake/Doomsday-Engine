/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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
 * dd_plugin.c: Plugin Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_defs.h"

// MACROS ------------------------------------------------------------------

#define HOOKMASK(x)     ((x) & 0xffffff)

// TYPES -------------------------------------------------------------------

typedef struct {
    int     exclude;
    hookfunc_t list[MAX_HOOKS];
} hookreg_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

hookreg_t hooks[NUM_HOOK_TYPES];

// CODE --------------------------------------------------------------------

/**
 * This routine is called by plugin DLLs who want to register hooks.
 *
 * @return              @c true, iff the hook was successfully registered.
 */
int Plug_AddHook(int hookType, hookfunc_t hook)
{
    int     i, type = HOOKMASK(hookType);

    // The type must be good.
    if(type < 0 || type >= NUM_HOOK_TYPES)
        return false;

    // Exclusive hooks.
    if(hookType & HOOKF_EXCLUSIVE)
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
    if(i == MAX_HOOKS)
        return false;           // No more hooks allowed!

    // Add the hook.
    hooks[type].list[i] = hook;
    return true;
}

/**
 * Removes the given hook.
 *
 * @return              @c true iff it was found.
 */
int Plug_RemoveHook(int hookType, hookfunc_t hook)
{
    int     i, type = HOOKMASK(hookType);

    // The type must be good.
    if(type < 0 || type >= NUM_HOOK_TYPES)
        return false;
    for(i = 0; i < MAX_HOOKS; i++)
        if(hooks[type].list[i] == hook)
        {
            hooks[type].list[i] = NULL;
            if(hookType & HOOKF_EXCLUSIVE)
            {
                // Exclusive hook removed; allow normal hooks.
                hooks[type].exclude = false;
            }
            return true;
        }
    return false;
}

/**
 * Executes all the hooks of the given type. Bit zero of the return value
 * is set if a hook was executed successfully (returned true). Bit one is
 * set if all the hooks that were executed returned true.
 */
int Plug_DoHook(int hookType, int parm, void *data)
{
    int     i, ret = 0;
    boolean allGood = true;

    // Try all the hooks.
    for(i = 0; i < MAX_HOOKS; i++)
        if(hooks[hookType].list[i])
        {
            if(hooks[hookType].list[i] (hookType, parm, data))
            {
                // One hook executed; return nonzero from this routine.
                ret = 1;
            }
            else
                allGood = false;
        }

    if(ret && allGood)
        ret |= 2;

    return ret;
}

/**
 * Check if a plugin is available of the specified type.
 *
 * @param hookType      Type of hook to check we have a plugin for.
 * @return              @c true, if a plugin is available for this hook type.
 */
int Plug_CheckForHook(int hookType)
{
    int     i;

    // Try all the hooks.
    for(i = 0; i < MAX_HOOKS; i++)
        if(hooks[hookType].list[i])
            return true;

    return false;
}
