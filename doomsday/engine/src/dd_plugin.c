/**
 * @file dd_plugin.c
 * Plugin subsystem. @ingroup base
 *
 * @todo Convert to C++, rename.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#define DENG_NO_API_MACROS_PLUGIN

#ifdef UNIX
#  include "library.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_defs.h"
#include "dd_pinit.h"

#include "updater/downloaddialog.h"

#include <de/findfile.h>

#define HOOKMASK(x)         ((x) & 0xffffff)

typedef struct {
    int exclude;
    struct {
        hookfunc_t func;
        pluginid_t pluginId;
    } list[MAX_HOOKS]; /// @todo Remove arbitrary MAX_HOOKS.
} hookreg_t;

typedef Library* PluginHandle;

static Library* hInstPlug[MAX_PLUGS]; /// @todo Remove arbitrary MAX_PLUGS.
static hookreg_t hooks[NUM_HOOK_TYPES];
static pluginid_t currentPlugin = 0; // none

static PluginHandle* findFirstUnusedPluginHandle(void)
{
    int i;
    for(i = 0; i < MAX_PLUGS; ++i)
    {
        if(!hInstPlug[i]) return &hInstPlug[i];
    }
    return 0; // none available
}

static int loadPlugin(void* libraryFile, const char* fileName, const char* pluginPath, void* param)
{
    Library* plugin;
    PluginHandle* handle;
    void (*initializer)(void);
    filename_t name;
    pluginid_t plugId;

    DENG_UNUSED(libraryFile); // this is not C++...
    DENG_UNUSED(param);

    DENG_ASSERT(fileName && fileName[0]);
    DENG_ASSERT(pluginPath && pluginPath[0]);

    if(strcasestr("/bin/audio_", pluginPath))
    {
        // Do not touch audio plugins at this point.
        return true;
    }

    plugin = Library_New(pluginPath);
    if(!plugin)
    {
        Con_Message("  loadPlugin: Did not load \"%s\" (%s).\n", pluginPath, Library_LastError());
        return 0; // Continue iteration.
    }

    if(!strcmp(Library_Type(plugin), "deng-plugin/audio"))
    {
        // Audio plugins will be loaded later, on demand.
        Library_Delete(plugin);
        return 0;
    }

    initializer = Library_Symbol(plugin, "DP_Initialize");
    if(!initializer)
    {
        DEBUG_Message(("  loadPlugin: \"%s\" does not export entrypoint DP_Initialize, ignoring.\n", pluginPath));

        // Clearly not a Doomsday plugin.
        Library_Delete(plugin);
        return 0; // Continue iteration.
    }

    // Assign a handle and ID to the plugin.
    handle = findFirstUnusedPluginHandle();
    plugId = handle - hInstPlug + 1;
    if(!handle)
    {
        DEBUG_Message(("  loadPlugin: Failed acquiring new handle for \"%s\", ignoring.\n", pluginPath));

        Library_Delete(plugin);
        return 0; // Continue iteration.
    }

    // This seems to be a Doomsday plugin.
    _splitpath(pluginPath, NULL, NULL, name, NULL);
    Con_Message("  (id:%i) %s\n", plugId, name);

    *handle = plugin;

    DD_SetActivePluginId(plugId);
    initializer();
    DD_SetActivePluginId(0);

    return 0; // Continue iteration.
}

static boolean unloadPlugin(PluginHandle* handle)
{
    assert(handle);
    if(!*handle) return false;

    Library_Delete(*handle);
    *handle = 0;
    return true;
}

void Plug_LoadAll(void)
{
    Con_Message("Initializing plugins...\n");

    Library_IterateAvailableLibraries(loadPlugin, 0);
}

void Plug_UnloadAll(void)
{
    int i;

    for(i = 0; i < MAX_PLUGS && hInstPlug[i]; ++i)
    {
        unloadPlugin(&hInstPlug[i]);
    }
}

int Plug_AddHook(int hookType, hookfunc_t hook)
{
    int i, type = HOOKMASK(hookType);

    /**
     * The current plugin must be set before calling this. The engine has the
     * responsibility to call DD_SetActivePluginId() whenever it passes control
     * to a plugin, and then set it back to zero after it gets control back.
     */
    DENG_ASSERT(DD_ActivePluginId() != 0);

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

    for(i = 0; i < MAX_HOOKS && hooks[type].list[i].func; ++i) {};
    if(i == MAX_HOOKS)
        return false; // No more hooks allowed!

    // Add the hook. If the plugin is unidentified the ID will be zero.
    hooks[type].list[i].func     = hook;
    hooks[type].list[i].pluginId = DD_ActivePluginId();
    return true;
}

int Plug_RemoveHook(int hookType, hookfunc_t hook)
{
    int i, type = HOOKMASK(hookType);

    /*
    if(currentPlugin)
    {
        LegacyCore_PrintfLogFragmentAtLevel(DE2_LOG_WARNING,
            "Plug_RemoveHook: Failed to remove hook %p of type %i; currently processing a hook.\n",
            hook, hookType);
        return false;
    }
    */

    // The type must be good.
    if(type < 0 || type >= NUM_HOOK_TYPES)
        return false;
    for(i = 0; i < MAX_HOOKS; ++i)
    {
        if(hooks[type].list[i].func != hook)
            continue;
        hooks[type].list[i].func = 0;
        hooks[type].list[i].pluginId = 0;
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
        if(hooks[hookType].list[i].func)
            return true;
    return false;
}

void DD_SetActivePluginId(pluginid_t id)
{
    currentPlugin = id;
}

int DD_CallHooks(int hookType, int parm, void *data)
{
    int ret = 0;
    boolean allGood = true;
    pluginid_t oldPlugin = DD_ActivePluginId();

    // Try all the hooks.
    { int i;
    for(i = 0; i < MAX_HOOKS; ++i)
    {
        if(!hooks[hookType].list[i].func)
            continue;

        DD_SetActivePluginId(hooks[hookType].list[i].pluginId);

        if(hooks[hookType].list[i].func(hookType, parm, data))
        {   // One hook executed; return nonzero from this routine.
            ret = 1;
        }
        else
            allGood = false;
    }}

    DD_SetActivePluginId(oldPlugin);

    if(ret && allGood)
        ret |= 2;

    return ret;
}

pluginid_t DD_ActivePluginId(void)
{
    return currentPlugin;
}

void* DD_FindEntryPoint(pluginid_t pluginId, const char* fn)
{
    void* addr = 0;
    int plugIndex = pluginId - 1;
    assert(plugIndex >= 0 && plugIndex < MAX_PLUGS);
    addr = Library_Symbol(hInstPlug[plugIndex], fn);
    if(!addr)
    {
        Con_Message("DD_FindEntryPoint: Error locating address of \"%s\" (%s).\n", fn,
                    Library_LastError());
    }
    return addr;
}

void Plug_Notify(int notification, void* param)
{
    DENG_UNUSED(param);

#ifdef __CLIENT__
    switch(notification)
    {
    case DD_NOTIFY_GAME_SAVED:
        // If an update has been downloaded and is ready to go, we should
        // re-show the dialog now that the user has saved the game as
        // prompted.
        DEBUG_Message(("Plug_Notify: Game saved.\n"));
        Updater_RaiseCompletedDownloadDialog();
        break;
    }
#else
    DENG_UNUSED(notification);
#endif
}

DENG_DECLARE_API(Plug) =
{
    { DE_API_PLUGIN },
    Plug_AddHook,
    Plug_RemoveHook,
    Plug_CheckForHook,
    Plug_Notify
};
