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

//#include <de/nativefile.h>
//#include <de/legacy/findfile.h>
//#include <de/legacy/strutil.h>

#include <de/extension.h>
#include <de/list.h>
#include <de/threadlocal.h>

using namespace de;

struct ThreadState
{
    pluginid_t currentPlugin;
    ThreadState() : currentPlugin(0) {}
};

static de::ThreadLocal<ThreadState> pluginState; ///< Thread-local plugin state.

bool Plugins::Hook::operator == (const Hook &other) const
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
    const pluginid_t oldPlugin = plugins.activePluginId();

    plugins.setActivePluginId(_pluginId);
    int result = _function(_type, parm, data);
    plugins.setActivePluginId(oldPlugin);

    return result;
}

pluginid_t Plugins::Hook::pluginId() const
{
    return _pluginId;
}

DE_PIMPL_NOREF(Plugins)
{
    using PluginHandle = String;
    using HookRegister = List<Hook>;

    void *(*getGameAPI)(const char *) = nullptr;
    GameExports               gameExports;
    std::vector<PluginHandle> hInstPlug;
    HookRegister              hooks[NUM_HOOK_TYPES];

    Impl()
    {
        zap(gameExports);
        zap(hInstPlug);
    }

    bool loadPlugin(const char *plugName)
    {
        using PluginInitializer = void (*)(void);
        using PluginType        = const char *(*) (void);

        const char *plugType =
            function_cast<PluginType>(extensionSymbol(plugName, "deng_LibraryType"))();

        if (iCmpStrN(plugType, "deng-plugin/", 12) != 0)
        {
            // Only try to load plugins.
            return false;
        }
        if (iCmpStr(plugType, "deng-plugin/audio") == 0)
        {
            // Audio plugins will be loaded later, on demand.
            return false;
        }

        auto initializer =
            function_cast<PluginInitializer>(extensionSymbol(plugName, "DP_Initialize"));
        if (!initializer)
        {
            LOG_RES_WARNING("Cannot load plugin \"%s\": no entrypoint called 'DP_Initialize'")
                    << plugName;

            // Clearly not a Doomsday plugin.
            //Library_Delete(plugin);
            return false;
        }

        // Assign a handle and ID to the plugin.
        const pluginid_t plugId = hInstPlug.size() + 1; // 1-based
        hInstPlug.push_back(plugName);

        LOGDEV_MSG("Plugin id:%i name:%s") << plugId << plugName;

        setActivePluginId(plugId);
        initializer();
        setActivePluginId(0);
        return true;
    }

    void setActivePluginId(pluginid_t id)
    {
        pluginState.get().currentPlugin = id;
    }

    pluginid_t activePluginId() const
    {
        return pluginState.get().currentPlugin;
    }

    DE_PIMPL_AUDIENCES(PublishAPI, Notification)
};

DE_AUDIENCE_METHODS(Plugins, PublishAPI, Notification)

Plugins::Plugins() : d(new Impl)
{}

void Plugins::publishAPIs(const char *plugName)
{
    DE_NOTIFY(PublishAPI, i)
    {
        i->publishAPIToPlugin(plugName);
    }
}

void Plugins::notify(int notification, void *data)
{
    DE_NOTIFY(Notification, i)
    {
        i->pluginSentNotification(notification, data);
    }
}

void Plugins::loadAll()
{
    LOG_RES_VERBOSE("Initializing plugins...");
    for (const auto &plugName : de::extensions())
    {
        publishAPIs(plugName);
        d->loadPlugin(plugName);
    }
}

pluginid_t Plugins::activePluginId() const
{
    return d->activePluginId();
}

String Plugins::extensionName(pluginid_t pluginId) const
{
    return d->hInstPlug.at(pluginId - 1);
}

void Plugins::setActivePluginId(pluginid_t pluginId)
{
    d->setActivePluginId(pluginId);
}

void *Plugins::findEntryPoint(pluginid_t pluginId, const char *fn) const
{
    const int plugIndex = pluginId - 1;

    void *addr = extensionSymbol(d->hInstPlug.at(plugIndex), fn);
    if (!addr)
    {
        LOGDEV_RES_WARNING("Extension \"%s\" does not have a symbol called \"%s\"")
                << d->hInstPlug.at(plugIndex) << fn;
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
        #define GET_FUNC(Name) { GET_FUNC_OPTIONAL(Name); DE_ASSERT(d->gameExports.Name); }

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
        GET_FUNC(MobjStateAsInfo);
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
    DE_ASSERT(type >= 0 && type < NUM_HOOK_TYPES);
    return !d->hooks[type].isEmpty();
}

void Plugins::addHook(HookType type, hookfunc_t function)
{
    DE_ASSERT(type >= 0 && type < NUM_HOOK_TYPES);

    // The current plugin must be set before calling this. The engine has the
    // responsibility to call setActivePluginId() whenever it passes control
    // to a plugin, and then set it back to zero after it gets control back.
    DE_ASSERT(d->activePluginId() != 0);

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
    DE_ASSERT(type >= 0 && type < NUM_HOOK_TYPES);
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

LoopResult Plugins::forAllHooks(HookType type, const std::function<de::LoopResult (const Hook &)>& func) const
{
    for (const Hook &hook : d->hooks[type])
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
    forAllHooks(type, [&parm, &data, &results] (const Hook &hook)
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
