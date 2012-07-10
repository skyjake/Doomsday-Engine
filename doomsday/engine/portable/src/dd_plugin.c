/**\file dd_plugin.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#ifdef UNIX
#  include "library.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_defs.h"

#define HOOKMASK(x)         ((x) & 0xffffff)

typedef struct {
    int exclude;
    hookfunc_t list[MAX_HOOKS];
} hookreg_t;

static hookreg_t hooks[NUM_HOOK_TYPES];
static pluginid_t currentPlugin = 0;

int Plug_AddHook(int hookType, hookfunc_t hook)
{
    int i, type = HOOKMASK(hookType);

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

    for(i = 0; i < MAX_HOOKS && hooks[type].list[i]; ++i) {};
    if(i == MAX_HOOKS)
        return false; // No more hooks allowed!

    // Add the hook.
    hooks[type].list[i] = hook;
    return true;
}

int Plug_RemoveHook(int hookType, hookfunc_t hook)
{
    int i, type = HOOKMASK(hookType);

    if(currentPlugin)
        Con_Error("Plug_RemoveHook: Failed, already processing a hook.");

    // The type must be good.
    if(type < 0 || type >= NUM_HOOK_TYPES)
        return false;
    for(i = 0; i < MAX_HOOKS; ++i)
    {
        if(hooks[type].list[i] != hook)
            continue;
        hooks[type].list[i] = 0;
        if(hookType & HOOKF_EXCLUSIVE)
        {   // Exclusive hook removed; allow normal hooks.
            hooks[type].exclude = false;
        }
        return true;
    }
    return false;
}

int Plug_CheckForHook(int hookType)
{
    size_t i;
    for(i = 0; i < MAX_HOOKS; ++i)
        if(hooks[hookType].list[i])
            return true;
    return false;
}

int DD_CallHooks(int hookType, int parm, void *data)
{
    int ret = 0;
    boolean allGood = true;

    // Try all the hooks.
    { int i;
    for(i = 0; i < MAX_HOOKS; ++i)
    {
        if(!hooks[hookType].list[i])
            continue;

        currentPlugin = i+1;
        if(hooks[hookType].list[i] (hookType, parm, data))
        {   // One hook executed; return nonzero from this routine.
            ret = 1;
        }
        else
            allGood = false;
    }}

    currentPlugin = 0;

    if(ret && allGood)
        ret |= 2;

    return ret;
}

pluginid_t DD_PluginIdForActiveHook(void)
{
    return (pluginid_t)currentPlugin;
}

void* DD_FindEntryPoint(pluginid_t pluginId, const char* fn)
{
#if WIN32
    HINSTANCE* handle = &app.hInstPlug[pluginId-1];
    void* adr = (void*)GetProcAddress(*handle, fn);
    if(!adr)
    {
        LPVOID lpMsgBuf;
        DWORD dw = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                      0, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, 0);
        if(lpMsgBuf)
        {
            Con_Printf("DD_FindEntryPoint: Error locating \"%s\" #%d: %s", fn, dw, (char*)lpMsgBuf);
            LocalFree(lpMsgBuf); lpMsgBuf = 0;
        }
    }
    return adr;
#elif UNIX
    void* addr = 0;
    int plugIndex = pluginId - 1;
    assert(plugIndex >= 0 && plugIndex < MAX_PLUGS);
    addr = Library_Symbol(app.hInstPlug[plugIndex], fn);
    if(!addr)
    {
        Con_Message("DD_FindEntryPoint: Error locating address of \"%s\" (%s).\n", fn,
                    Library_LastError());
    }
    return addr;
#endif
}
