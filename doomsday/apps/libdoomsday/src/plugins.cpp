/** @file plugins.cpp
 *
 * @authors Copyright © 2003-2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/plugins.h"
#include "doomsday/doomsdayapp.h"

#include <de/findfile.h>
#include <de/strutil.h>
#include <QThreadStorage>

#define HOOKMASK(x)         ((x) & 0xffffff)

using namespace de;

struct ThreadState
{
    pluginid_t currentPlugin;
    ThreadState() : currentPlugin(0) {}
};

#ifndef DENG2_QT_4_8_OR_NEWER // Qt 4.7 requires a pointer as the local data type
# define DENG_LOCAL_DATA_POINTER
QThreadStorage<ThreadState *> pluginState; ///< Thread-local plugin state.
void initLocalData() {
    if(!pluginState.hasLocalData()) pluginState.setLocalData(new ThreadState);
}
#else
QThreadStorage<ThreadState> pluginState; ///< Thread-local plugin state.
#endif

DENG2_PIMPL_NOREF(Plugins)
{
    GETGAMEAPI getGameAPI = nullptr;
    game_export_t gameExports;

    struct hookreg_t {
        int exclude;
        struct {
            hookfunc_t func;
            pluginid_t pluginId;
        } list[MAX_HOOKS]; /// @todo Remove arbitrary MAX_HOOKS.
    };

    typedef ::Library *PluginHandle;

    ::Library *hInstPlug[MAX_PLUGS]; /// @todo Remove arbitrary MAX_PLUGS.
    hookreg_t hooks[NUM_HOOK_TYPES];

    Instance()
    {
        zap(gameExports);
        zap(hInstPlug);
        zap(hooks);
    }

    PluginHandle *findFirstUnusedPluginHandle()
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

    static int loadPlugin(void * /*libraryFile*/, char const *fileName,
                          char const *pluginPath, void *dptr)
    {
        Instance *d = (Instance *) dptr;
        typedef void (*PluginInitializer)(void);

        DENG_UNUSED(fileName);
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
#ifdef UNIX
            String const fn = Path(pluginPath).fileName();
            if(fn.contains("libfmodex") || fn.contains("libassimp"))
            {
                // No need to warn about these shared libs.
                return 0;
            }
#endif
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
        PluginHandle *handle = d->findFirstUnusedPluginHandle();
        pluginid_t plugId    = handle - d->hInstPlug + 1;
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

        d->setActivePluginId(plugId);
        initializer();
        d->setActivePluginId(0);

        return 0; // Continue iteration.
    }

    bool unloadPlugin(PluginHandle *handle)
    {
        DENG2_ASSERT(handle != 0);
        if(!*handle) return false;

        Library_Delete(*handle);
        *handle = 0;
        return true;
    }

    void setActivePluginId(pluginid_t id)
    {
#ifdef DENG_LOCAL_DATA_POINTER
        initLocalData();
        pluginState.localData()->currentPlugin = id;
#else
        pluginState.localData().currentPlugin = id;
#endif
    }

    pluginid_t activePluginId() const
    {
#ifdef DENG_LOCAL_DATA_POINTER
        initLocalData();
        return pluginState.localData()->currentPlugin;
#else
        return pluginState.localData().currentPlugin;
#endif
    }

    DENG2_PIMPL_AUDIENCE(PublishAPI)
    DENG2_PIMPL_AUDIENCE(Notification)
};

DENG2_AUDIENCE_METHOD(Plugins, PublishAPI)
DENG2_AUDIENCE_METHOD(Plugins, Notification)

Plugins::Plugins() : d(new Instance)
{}

void Plugins::publishAPIs(::Library *lib)
{
    DENG2_FOR_AUDIENCE2(PublishAPI, i)
    {
        i->publishAPIToPlugin(lib);
    }
}

void Plugins::notify(int notification, void *data)
{
    DENG2_FOR_AUDIENCE2(Notification, i)
    {
        i->pluginSentNotification(notification, data);
    }
}

bool Plugins::exchangeGameEntryPoints(pluginid_t pluginId)
{
    zap(d->gameExports);

    if(pluginId != 0)
    {
        // Do the API transfer.
        if(!(d->getGameAPI = (GETGAMEAPI) findEntryPoint(pluginId, "GetGameAPI")))
        {
            return false;
        }

        game_export_t *gameExPtr = d->getGameAPI();
        std::memcpy(&d->gameExports, gameExPtr, MIN_OF(sizeof(d->gameExports), gameExPtr->apiSize));
    }
    else
    {
        d->getGameAPI = nullptr;
    }
    return true;
}

game_export_t &Plugins::gameExports() const
{
    return d->gameExports;
}

pluginid_t Plugins::activePluginId() const
{
    return d->activePluginId();
}

void Plugins::setActivePluginId(pluginid_t id)
{
    d->setActivePluginId(id);
}

void Plugins::loadAll()
{
    LOG_RES_VERBOSE("Initializing plugins...");

    Library_IterateAvailableLibraries(Instance::loadPlugin, d);
}

void Plugins::unloadAll()
{
    for(int i = 0; i < MAX_PLUGS && d->hInstPlug[i]; ++i)
    {
        d->unloadPlugin(&d->hInstPlug[i]);
    }
}

LibraryFile const &Plugins::fileForPlugin(pluginid_t id) const
{
    DENG2_ASSERT(id > 0 && id <= MAX_PLUGS);
    return Library_File(d->hInstPlug[id - 1]);
}

bool Plugins::addHook(int hookType, hookfunc_t hook)
{
    int const type = HOOKMASK(hookType);

    // The current plugin must be set before calling this. The engine has the
    // responsibility to call DD_SetActivePluginId() whenever it passes control
    // to a plugin, and then set it back to zero after it gets control back.
    DENG2_ASSERT(d->activePluginId() != 0);

    // The type must be good.
    if(type < 0 || type >= NUM_HOOK_TYPES)
        return false;

    // Exclusive hooks.
    if(hookType & HOOKF_EXCLUSIVE)
    {
        d->hooks[type].exclude = true;
        std::memset(d->hooks[type].list, 0, sizeof(d->hooks[type].list));
    }
    else if(d->hooks[type].exclude)
    {
        // An exclusive hook has closed down this list.
        return false;
    }

    int i;
    for(i = 0; i < MAX_HOOKS && d->hooks[type].list[i].func; ++i) {}

    if(i == MAX_HOOKS)
        return false; // No more hooks allowed!

    // Add the hook. If the plugin is unidentified the ID will be zero.
    d->hooks[type].list[i].func     = hook;
    d->hooks[type].list[i].pluginId = d->activePluginId();
    return true;
}

bool Plugins::removeHook(int hookType, hookfunc_t hook)
{
    int const type = HOOKMASK(hookType);

    // The type must be good.
    if(type < 0 || type >= NUM_HOOK_TYPES)
        return false;

    for(int i = 0; i < MAX_HOOKS; ++i)
    {
        if(d->hooks[type].list[i].func != hook)
            continue;

        d->hooks[type].list[i].func     = 0;
        d->hooks[type].list[i].pluginId = 0;
        if(hookType & HOOKF_EXCLUSIVE)
        {
            // Exclusive hook removed; allow normal hooks.
            d->hooks[type].exclude = false;
        }
        return true;
    }
    return false;
}

bool Plugins::checkForHook(int hookType) const
{
    for(int i = 0; i < MAX_HOOKS; ++i)
    {
        if(d->hooks[hookType].list[i].func)
            return true;
    }
    return false;
}

int Plugins::callHooks(int hookType, int parm, void *data)
{
    int ret = 0;
    bool allGood = true;
    pluginid_t oldPlugin = activePluginId();

    // Try all the hooks.
    for(int i = 0; i < MAX_HOOKS; ++i)
    {
        if(!d->hooks[hookType].list[i].func)
            continue;

        setActivePluginId(d->hooks[hookType].list[i].pluginId);

        if(d->hooks[hookType].list[i].func(hookType, parm, data))
        {
            // One hook executed; return nonzero from this routine.
            ret = 1;
        }
        else
        {
            allGood = false;
        }
    }

    setActivePluginId(oldPlugin);

    if(ret && allGood)
        ret |= 2;

    return ret;
}

void *Plugins::findEntryPoint(pluginid_t pluginId, char const *fn) const
{
    int const plugIndex = pluginId - 1;
    DENG2_ASSERT(plugIndex >= 0 && plugIndex < MAX_PLUGS);

    void *addr = Library_Symbol(d->hInstPlug[plugIndex], fn);
    if(!addr)
    {
        LOGDEV_RES_WARNING("Error getting address of \"%s\": %s")
                << fn << Library_LastError();
    }
    return addr;
}

// C wrapper -----------------------------------------------------------------

void Plug_Notify(int notification, void *data)
{
    DoomsdayApp::plugins().notify(notification, data);
}

int Plug_AddHook(int hookType, hookfunc_t hook)
{
    return DoomsdayApp::plugins().addHook(hookType, hook);
}

int Plug_RemoveHook(int hookType, hookfunc_t hook)
{
    return DoomsdayApp::plugins().removeHook(hookType, hook);
}

int Plug_CheckForHook(int hookType)
{
    return DoomsdayApp::plugins().checkForHook(hookType);
}
