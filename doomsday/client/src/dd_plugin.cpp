/** @file dd_plugin.cpp  Plugin subsystem.
 * @ingroup base
 *
 * @todo Convert to C++, rename.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"

#include "api_plugin.h"
#include "de_console.h"
#include "de_defs.h"
#include "dd_main.h"
#include "dd_pinit.h"
#include "library.h"

#ifdef __CLIENT__
#  include "updater/downloaddialog.h"
#endif

#include <de/findfile.h>
#include <de/strutil.h>
#include <QThreadStorage>

#define HOOKMASK(x)         ((x) & 0xffffff)

using namespace de;

struct hookreg_t
{
    int exclude;
    struct {
        hookfunc_t func;
        pluginid_t pluginId;
    } list[MAX_HOOKS]; /// @todo Remove arbitrary MAX_HOOKS.
};

typedef ::Library *PluginHandle;

static ::Library *hInstPlug[MAX_PLUGS]; /// @todo Remove arbitrary MAX_PLUGS.
static hookreg_t hooks[NUM_HOOK_TYPES];

struct ThreadState
{
    pluginid_t currentPlugin;
    ThreadState() : currentPlugin(0) {}
};

#ifndef DENG2_QT_4_8_OR_NEWER // Qt 4.7 requires a pointer as the local data type
#define DENG_LOCAL_DATA_POINTER
static QThreadStorage<ThreadState *> pluginState; ///< Thread-local plugin state.
static void initLocalData() {
    if(!pluginState.hasLocalData()) pluginState.setLocalData(new ThreadState);
}
#else
static QThreadStorage<ThreadState> pluginState; ///< Thread-local plugin state.
#endif

static PluginHandle *findFirstUnusedPluginHandle()
{
    for(int i = 0; i < MAX_PLUGS; ++i)
    {
        if(!hInstPlug[i])
        {
            return &hInstPlug[i];
        }
    }
    return 0; // none available
}

static int loadPlugin(void * /*libraryFile*/, char const *fileName, char const *pluginPath, void *)
{
    typedef void (*PluginInitializer)(void);

    DENG2_ASSERT(fileName != 0 && fileName[0]);
    DENG2_ASSERT(pluginPath != 0 && pluginPath[0]);

    if(strcasestr("/bin/audio_", pluginPath))
    {
        // Do not touch audio plugins at this point.
        return true;
    }

    ::Library *plugin = Library_New(pluginPath);
    if(!plugin)
    {
        LOG_RES_WARNING("Failed to load \"%s\": %s") << pluginPath << Library_LastError();
        return 0; // Continue iteration.
    }

    if(!strcmp(Library_Type(plugin), "deng-plugin/audio"))
    {
        // Audio plugins will be loaded later, on demand.
        Library_Delete(plugin);
        return 0;
    }

    PluginInitializer initializer = de::function_cast<void (*)()>(Library_Symbol(plugin, "DP_Initialize"));
    if(!initializer)
    {
        LOG_RES_WARNING("Cannot load plugin \"%s\": no entrypoint called 'DP_Initialize'")
                << pluginPath;

        // Clearly not a Doomsday plugin.
        Library_Delete(plugin);
        return 0; // Continue iteration.
    }

    // Assign a handle and ID to the plugin.
    PluginHandle *handle = findFirstUnusedPluginHandle();
    pluginid_t plugId    = handle - hInstPlug + 1;
    if(!handle)
    {
        LOG_RES_WARNING("Cannot load \"%s\": too many plugins loaded already loaded")
                << pluginPath;

        Library_Delete(plugin);
        return 0; // Continue iteration.
    }

    // This seems to be a Doomsday plugin.
    LOGDEV_MSG("Plugin id:%i name:%s")
            << plugId << String(pluginPath).fileNameWithoutExtension();

    *handle = plugin;

    DD_SetActivePluginId(plugId);
    initializer();
    DD_SetActivePluginId(0);

    return 0; // Continue iteration.
}

static bool unloadPlugin(PluginHandle *handle)
{
    DENG2_ASSERT(handle != 0);
    if(!*handle) return false;

    Library_Delete(*handle);
    *handle = 0;
    return true;
}

void Plug_LoadAll()
{
    LOG_RES_VERBOSE("Initializing plugins...");

    Library_IterateAvailableLibraries(loadPlugin, 0);
}

void Plug_UnloadAll()
{
    for(int i = 0; i < MAX_PLUGS && hInstPlug[i]; ++i)
    {
        unloadPlugin(&hInstPlug[i]);
    }
}

LibraryFile const &Plug_FileForPlugin(pluginid_t id)
{
    DENG2_ASSERT(id > 0 && id <= MAX_PLUGS);
    return Library_File(hInstPlug[id - 1]);
}

#undef Plug_AddHook
DENG_EXTERN_C int Plug_AddHook(int hookType, hookfunc_t hook)
{
    int const type = HOOKMASK(hookType);

    // The current plugin must be set before calling this. The engine has the
    // responsibility to call DD_SetActivePluginId() whenever it passes control
    // to a plugin, and then set it back to zero after it gets control back.
    DENG2_ASSERT(DD_ActivePluginId() != 0);

    // The type must be good.
    if(type < 0 || type >= NUM_HOOK_TYPES)
        return false;

    // Exclusive hooks.
    if(hookType & HOOKF_EXCLUSIVE)
    {
        hooks[type].exclude = true;
        std::memset(hooks[type].list, 0, sizeof(hooks[type].list));
    }
    else if(hooks[type].exclude)
    {
        // An exclusive hook has closed down this list.
        return false;
    }

    int i;
    for(i = 0; i < MAX_HOOKS && hooks[type].list[i].func; ++i) {};

    if(i == MAX_HOOKS)
        return false; // No more hooks allowed!

    // Add the hook. If the plugin is unidentified the ID will be zero.
    hooks[type].list[i].func     = hook;
    hooks[type].list[i].pluginId = DD_ActivePluginId();
    return true;
}

#undef Plug_RemoveHook
DENG_EXTERN_C int Plug_RemoveHook(int hookType, hookfunc_t hook)
{
    int const type = HOOKMASK(hookType);

    // The type must be good.
    if(type < 0 || type >= NUM_HOOK_TYPES)
        return false;

    for(int i = 0; i < MAX_HOOKS; ++i)
    {
        if(hooks[type].list[i].func != hook)
            continue;

        hooks[type].list[i].func     = 0;
        hooks[type].list[i].pluginId = 0;
        if(hookType & HOOKF_EXCLUSIVE)
        {
            // Exclusive hook removed; allow normal hooks.
            hooks[type].exclude = false;
        }

        return true;
    }

    return false;
}

#undef Plug_CheckForHook
DENG_EXTERN_C int Plug_CheckForHook(int hookType)
{
    for(int i = 0; i < MAX_HOOKS; ++i)
    {
        if(hooks[hookType].list[i].func)
            return true;
    }
    return false;
}

void DD_SetActivePluginId(pluginid_t id)
{
#ifdef DENG_LOCAL_DATA_POINTER
    initLocalData();
    pluginState.localData()->currentPlugin = id;
#else
    pluginState.localData().currentPlugin = id;
#endif
}

pluginid_t DD_ActivePluginId()
{
#ifdef DENG_LOCAL_DATA_POINTER
    initLocalData();
    return pluginState.localData()->currentPlugin;
#else
    return pluginState.localData().currentPlugin;
#endif
}

int DD_CallHooks(int hookType, int parm, void *data)
{
    int ret = 0;
    bool allGood = true;
    pluginid_t oldPlugin = DD_ActivePluginId();

    // Try all the hooks.
    for(int i = 0; i < MAX_HOOKS; ++i)
    {
        if(!hooks[hookType].list[i].func)
            continue;

        DD_SetActivePluginId(hooks[hookType].list[i].pluginId);

        if(hooks[hookType].list[i].func(hookType, parm, data))
        {
            // One hook executed; return nonzero from this routine.
            ret = 1;
        }
        else
        {
            allGood = false;
        }
    }

    DD_SetActivePluginId(oldPlugin);

    if(ret && allGood)
        ret |= 2;

    return ret;
}

void *DD_FindEntryPoint(pluginid_t pluginId, char const *fn)
{
    int const plugIndex = pluginId - 1;
    DENG2_ASSERT(plugIndex >= 0 && plugIndex < MAX_PLUGS);

    void *addr = Library_Symbol(hInstPlug[plugIndex], fn);
    if(!addr)
    {
        LOGDEV_RES_WARNING("Error getting address of \"%s\": %s")
                << fn << Library_LastError();
    }
    return addr;
}

#undef Plug_Notify
DENG_EXTERN_C void Plug_Notify(int notification, void *)
{
#ifdef __CLIENT__
    switch(notification)
    {
    case DD_NOTIFY_GAME_SAVED:
        // If an update has been downloaded and is ready to go, we should
        // re-show the dialog now that the user has saved the game as prompted.
        LOG_DEBUG("Plug_Notify: Game saved");
        DownloadDialog::showCompletedDownload();
        break;
    }
#else
    DENG2_UNUSED(notification);
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
