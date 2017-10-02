/** @file plugins.cpp  Plugin loader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "doomsday/world/xg.h"
#include "doomsday/world/actions.h"
#include "doomsday/doomsdayapp.h"

#include <de/NativeFile>
#include <de/findfile.h>
#include <de/strutil.h>
#include <QList>
#include <QThreadStorage>

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
    if (!pluginState.hasLocalData()) pluginState.setLocalData(new ThreadState);
}
#else
QThreadStorage<ThreadState> pluginState; ///< Thread-local plugin state.
#endif

bool Plugins::Hook::operator == (Hook const &other) const
{
    if (!(_pluginId == 0 || other._pluginId == 0))
    {
        if (_pluginId != other._pluginId) return false;
    }
    return _type == other._type && _function == other._function;
}

int Plugins::Hook::execute(int parm, void *data) const
{
    Plugins &plugins           = DoomsdayApp::plugins();
    pluginid_t const oldPlugin = plugins.activePluginId();

    plugins.setActivePluginId(_pluginId);
    int result = _function(_type, parm, data);
    plugins.setActivePluginId(oldPlugin);

    return result;
}

pluginid_t Plugins::Hook::pluginId() const
{
    return _pluginId;
}

DENG2_PIMPL_NOREF(Plugins)
{
    void *(*getGameAPI)(char const *) = nullptr;
    GameExports gameExports;

    typedef ::Library *PluginHandle;

    ::Library *hInstPlug[MAX_PLUGS];  ///< @todo Remove arbitrary MAX_PLUGS.

    typedef QList<Hook> HookRegister;
    HookRegister hooks[NUM_HOOK_TYPES];

    Impl()
    {
        zap(gameExports);
        zap(hInstPlug);
    }

    PluginHandle *findFirstUnusedPluginHandle()
    {
        for (int i = 0; i < MAX_PLUGS; ++i)
        {
            if (!hInstPlug[i])
            {
                return &hInstPlug[i];
            }
        }
        return nullptr;  // none available.
    }

    int loadPlugin(LibraryFile &lib)
    {
        typedef void (*PluginInitializer)(void);

#if !defined (DENG_STATIC_LINK)
        // We are only interested in native files.
        if (!is<NativeFile>(lib.source()))
            return 0;  // Continue iteration.
#endif

        DENG2_ASSERT(!lib.path().isEmpty());
        if (strcasestr("/bin/audio_", lib.path().toUtf8().constData()))
        {
            // Do not touch audio plugins at this point.
            return true;
        }

        ::Library *plugin = Library_New(lib.path().toUtf8().constData());
        if (!plugin)
        {
#ifdef UNIX
            String const fn = Path(lib.path()).fileName();
            if (fn.contains("libfmod") || fn.contains("libassimp"))
            {
                // No need to warn about these shared libs.
                return 0;
            }
#endif
            LOG_RES_WARNING("Failed to load \"%s\": %s") << lib.path() << Library_LastError();
            return 0;  // Continue iteration.
        }

        if (!strcmp(Library_Type(plugin), "deng-plugin/audio"))
        {
            // Audio plugins will be loaded later, on demand.
            Library_Delete(plugin);
            return 0;
        }

        PluginInitializer initializer = de::function_cast<void (*)()>(Library_Symbol(plugin, "DP_Initialize"));
        if (!initializer)
        {
            LOG_RES_WARNING("Cannot load plugin \"%s\": no entrypoint called 'DP_Initialize'")
                    << lib.path();

            // Clearly not a Doomsday plugin.
            Library_Delete(plugin);
            return 0;  // Continue iteration.
        }

        // Assign a handle and ID to the plugin.
        PluginHandle *handle    = findFirstUnusedPluginHandle();
        pluginid_t const plugId = handle - hInstPlug + 1;
        if (!handle)
        {
            LOG_RES_WARNING("Cannot load \"%s\": too many plugins loaded already loaded")
                    << lib.path();

            Library_Delete(plugin);
            return 0;  // Continue iteration.
        }

        // This seems to be a Doomsday plugin.
        LOGDEV_MSG("Plugin id:%i name:%s")
                << plugId << lib.path().fileNameWithoutExtension();

        *handle = plugin;

        setActivePluginId(plugId);
        initializer();
        setActivePluginId(0);

        return 0;  // Continue iteration.
    }

    bool unloadPlugin(PluginHandle *handle)
    {
        DENG2_ASSERT(handle != nullptr);
        if (!*handle) return false;

        Library_Delete(*handle);
        *handle = nullptr;
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

Plugins::Plugins() : d(new Impl)
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

void Plugins::loadAll()
{
    LOG_RES_VERBOSE("Initializing plugins...");

    Library_ForAll([this] (LibraryFile &lib)
    {
        return d->loadPlugin(lib);
    });
}

void Plugins::unloadAll()
{
    for (int i = 0; i < MAX_PLUGS && d->hInstPlug[i]; ++i)
    {
        d->unloadPlugin(&d->hInstPlug[i]);
    }
}

pluginid_t Plugins::activePluginId() const
{
    return d->activePluginId();
}

void Plugins::setActivePluginId(pluginid_t id)
{
    d->setActivePluginId(id);
}

LibraryFile const &Plugins::fileForPlugin(pluginid_t id) const
{
    DENG2_ASSERT(id > 0 && id <= MAX_PLUGS);
    return Library_File(d->hInstPlug[id - 1]);
}

void *Plugins::findEntryPoint(pluginid_t pluginId, char const *fn) const
{
    int const plugIndex = pluginId - 1;
    DENG2_ASSERT(plugIndex >= 0 && plugIndex < MAX_PLUGS);

    void *addr = Library_Symbol(d->hInstPlug[plugIndex], fn);
    if (!addr)
    {
        LOGDEV_RES_WARNING("Error getting address of \"%s\": %s")
                << fn << Library_LastError();
    }
    return addr;
}

bool Plugins::exchangeGameEntryPoints(pluginid_t pluginId)
{
    zap(d->gameExports);

    if (pluginId != 0)
    {
        // Do the API transfer.
        if (!functionAssign(d->getGameAPI, findEntryPoint(pluginId, "GetGameAPI")))
        {
            return false;
        }

        zap(d->gameExports);

        // Query all the known entrypoints.
        #define GET_FUNC_OPTIONAL(Name) { functionAssign(d->gameExports.Name, d->getGameAPI(#Name)); }
        #define GET_FUNC(Name) { GET_FUNC_OPTIONAL(Name); DENG2_ASSERT(d->gameExports.Name); }

        GET_FUNC(PreInit);
        GET_FUNC(PostInit);
        GET_FUNC(TryShutdown);
        GET_FUNC(Shutdown);
        GET_FUNC(UpdateState);
        GET_FUNC(GetInteger);
        GET_FUNC(GetPointer);

        GET_FUNC(NetServerStart);
        GET_FUNC(NetServerStop);
        GET_FUNC(NetConnect);
        GET_FUNC(NetDisconnect);
        GET_FUNC(NetPlayerEvent);
        GET_FUNC(NetWorldEvent);
        GET_FUNC(HandlePacket);

        GET_FUNC(Ticker);

        GET_FUNC(FinaleResponder);
        GET_FUNC(PrivilegedResponder);
        GET_FUNC(Responder);
        GET_FUNC_OPTIONAL(FallbackResponder);

        GET_FUNC_OPTIONAL(BeginFrame);
        GET_FUNC(EndFrame);
        GET_FUNC(DrawViewPort);
        GET_FUNC(DrawWindow);

        GET_FUNC(MobjThinker);
        GET_FUNC(MobjFriction);
        GET_FUNC(MobjCheckPositionXYZ);
        GET_FUNC(MobjTryMoveXYZ);
        GET_FUNC(MobjStateAsInfoText);
        GET_FUNC(MobjRestoreState);

        GET_FUNC(SectorHeightChangeNotification);

        GET_FUNC(FinalizeMapChange);
        GET_FUNC(HandleMapDataPropertyValue);
        GET_FUNC(HandleMapObjectStatusReport);

        #undef GET_FUNC
    }
    else
    {
        d->getGameAPI = nullptr;
    }
    P_GetGameActions();
    XG_GetGameClasses();
    return true;
}

GameExports &Plugins::gameExports() const
{
    return d->gameExports;
}

bool Plugins::hasHook(HookType type) const
{
    DENG2_ASSERT(type >= 0 && type < NUM_HOOK_TYPES);
    return !d->hooks[type].isEmpty();
}

void Plugins::addHook(HookType type, hookfunc_t function)
{
    DENG2_ASSERT(type >= 0 && type < NUM_HOOK_TYPES);

    // The current plugin must be set before calling this. The engine has the
    // responsibility to call setActivePluginId() whenever it passes control
    // to a plugin, and then set it back to zero after it gets control back.
    DENG2_ASSERT(d->activePluginId() != 0);

    if (function)
    {
        // Add the hook. If the plugin is unidentified the ID will be zero.
        Hook temp;
        temp._type     = type;
        temp._function = function;
        temp._pluginId = d->activePluginId();
        if (!d->hooks[type].contains(temp))
        {
            d->hooks[type].append(temp);  // a copy is made.
        }
    }
}

bool Plugins::removeHook(HookType type, hookfunc_t function)
{
    DENG2_ASSERT(type >= 0 && type < NUM_HOOK_TYPES);
    if (function)
    {
        Hook temp;
        temp._type     = type;
        temp._pluginId = 0/*invalid Id - i.e., match any*/;
        temp._function = function;
        return d->hooks[type].removeOne(temp);
    }
    return false;
}

LoopResult Plugins::forAllHooks(HookType type, std::function<de::LoopResult (Hook const &)> func) const
{
    for (Hook const &hook : d->hooks[type])
    {
        if (auto result = func(hook))
            return result;
    }
    return LoopContinue;
}

int Plugins::callAllHooks(HookType type, int parm, void *data)
{
    // Try all the hooks.
    int results = 2;  // Assume all good.
    forAllHooks(type, [&parm, &data, &results] (Hook const &hook)
    {
        if (hook.execute(parm, data))
        {
            results |= 1;   // One success.
        }
        else
        {
            results &= ~2;  // One failure.
        }
        return LoopContinue;
    });
    return (results & 1) ? results : 0;
}

// C wrapper -----------------------------------------------------------------

void Plug_Notify(int notification, void *data)
{
    DoomsdayApp::plugins().notify(notification, data);
}

int Plug_AddHook(HookType type, hookfunc_t function)
{
    DoomsdayApp::plugins().addHook(type, function);
    return true;
}

int Plug_RemoveHook(HookType type, hookfunc_t function)
{
    return DoomsdayApp::plugins().removeHook(type, function);
}

int Plug_CheckForHook(HookType type)
{
    return DoomsdayApp::plugins().hasHook(type);
}
