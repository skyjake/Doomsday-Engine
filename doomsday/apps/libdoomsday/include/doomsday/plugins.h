/** @file plugins.h  Plugin loader.
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

#ifndef LIBDOOMSDAY_PLUGINS_H
#define LIBDOOMSDAY_PLUGINS_H

#include <de/str.h>
#include <de/rect.h>

#define MAX_HOOKS               16
#define HOOKF_EXCLUSIVE         0x01000000

/**
 * Unique identifier assigned to each plugin during initial startup.
 * Zero is not a valid ID.
 */
typedef int pluginid_t;

typedef int (*pluginfunc_t) (void);
typedef int (*hookfunc_t) (int type, int parm, void *data);

/// Maximum allowed number of plugins.
/// @todo Remove fixed size limit.
#define MAX_PLUGS   32

/// Hook types.
enum {
    HOOK_STARTUP = 0,               ///< Called ASAP after startup.
    HOOK_INIT = 1,                  ///< Called after engine has been initialized.
    HOOK_DEFS = 2,                  ///< Called after DEDs have been loaded.
    HOOK_MAP_CONVERT = 3,           ///< Called when a map needs converting.
    HOOK_TICKER = 4,                ///< Called as part of the run loop.
    HOOK_DEMO_STOP = 5,             ///< Called when demo playback completes.
    HOOK_FINALE_SCRIPT_BEGIN = 6,   ///< Called as a script begins.
    HOOK_FINALE_SCRIPT_STOP = 7,    ///< Called as a script stops.
    HOOK_FINALE_SCRIPT_TICKER = 8,  ///< Called each time a script 'thinks'.
    HOOK_FINALE_EVAL_IF = 9,        ///< Called to evaluate an IF conditional statement.
    HOOK_VIEWPORT_RESHAPE = 10,     ///< Called when viewport dimensions change.
    HOOK_SAVEGAME_CONVERT = 11,     ///< Called when a legacy savegame needs converting.
    HOOK_GAME_INIT = 12,            /**< Called when initializing a loaded game. This occurs
                                         once all startup resources are loaded but @em before
                                         parsing of definitions and processing game data.
                                         This is a suitable time for game data conversion. */
    HOOK_MAPINFO_CONVERT = 13,      ///< Called when map definition data needs converting.

    NUM_HOOK_TYPES
};

/// Parameters for HOOK_FINALE_EVAL_IF
typedef struct {
    const char* token;
    dd_bool     returnVal;
} ddhook_finale_script_evalif_paramaters_t;

/// Parameters for HOOK_FINALE_SCRIPT_TICKER
typedef struct {
    dd_bool runTick;
    dd_bool canSkip;
} ddhook_finale_script_ticker_paramaters_t;

/// Parameters for HOOK_VIEWPORT_RESHAPE
typedef struct {
    RectRaw geometry; // New/Current.
    RectRaw oldGeometry; // Previous.
} ddhook_viewport_reshape_t;

/// Parameters for HOOK_SAVEGAME_CONVERT
typedef struct {
    Str sourcePath;
    Str outputPath;
    Str fallbackGameId;
} ddhook_savegame_convert_t;

/// Parameters for HOOK_MAPINFO_CONVERT
typedef struct {
    Str paths; // ';' delimited
    Str translated;
    Str translatedCustom;
} ddhook_mapinfo_convert_t;

/// Parameters for DD_NOTIFY_PLAYER_WEAPON_CHANGED
typedef struct {
    int player;
    int weapon;             ///< Number of the weapon.
    char const *weaponId;   ///< Defined in Values (includes power-ups) (UTF-8).
} ddnotify_player_weapon_changed_t;

/// Parameters for DD_NOTIFY_PSPRITE_STATE_CHANGED
typedef struct {
    int player;
    struct state_s const *state;
} ddnotify_psprite_state_changed_t;

#ifdef __cplusplus

#include <de/Observers>
#include "libdoomsday.h"
#include "library.h"
#include "gameapi.h"

/**
 * Plugin loader.
 */
class LIBDOOMSDAY_PUBLIC Plugins
{
public:
    DENG2_DEFINE_AUDIENCE2(PublishAPI,   void publishAPIToPlugin(Library *))
    DENG2_DEFINE_AUDIENCE2(Notification, void pluginSentNotification(int id, void *data))

public:
    Plugins();

    void publishAPIs(Library *lib);

    void notify(int notification, void *data);

    /**
     * Loads all the plugins from the library directory. Note that audio plugins
     * are not loaded here, they are managed by AudioDriver.
     */
    void loadAll();

    /**
     * Unloads all plugins.
     */
    void unloadAll();

    bool exchangeGameEntryPoints(pluginid_t pluginId);

    /**
     * Returns the current game plugin's entrypoints.
     */
    game_export_t &gameExports() const;

    /**
     * Sets the ID of the currently active plugin in the current thread.
     *
     * @param id  Plugin id.
     */
    void setActivePluginId(pluginid_t id);

    /**
     * @return Unique identifier of the currently active plugin. The currently
     * active plugin is tracked separately for each thread.
     */
    pluginid_t activePluginId() const;

    de::LibraryFile const &fileForPlugin(pluginid_t id) const;

    bool addHook(int hookType, hookfunc_t hook);

    bool removeHook(int hookType, hookfunc_t hook);

    bool checkForHook(int hookType) const;

    /**
     * Executes all the hooks of the given type. Bit zero of the return value
     * is set if a hook was executed successfully (returned true). Bit one is
     * set if all the hooks that were executed returned true.
     */
    int callHooks(int hookType, int parm, void *data);

    /**
     * Locate the address of the named, exported procedure in the plugin.
     */
    void *findEntryPoint(pluginid_t pluginId, char const *fn) const;

private:
    DENG2_PRIVATE(d)
};

#endif // __cplusplus

/**
 * Registers a new hook function. A plugin can call this to add a hook
 * function to be executed at the time specified by @a hook_type.
 *
 * @param hook_type  Hook type.
 * @param hook       Pointer to hook function.
 *
 * @return  @c true, iff the hook was successfully registered.
 */
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC
int Plug_AddHook(int hook_type, hookfunc_t hook);

/**
 * Removes @a hook from the registered hook functions.
 *
 * @param hook_type  Hook type.
 * @param hook       Pointer to hook function.
 *
 * @return  @c true iff it was found.
 */
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC
int Plug_RemoveHook(int hook_type, hookfunc_t hook);

/**
 * Check if there are any hooks of type @a hookType registered.
 *
 * @param hookType      Type of hook to check for.
 *
 * @return  @c true, if one or more hooks are available for type @a hookType.
 */
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC
int Plug_CheckForHook(int hookType);

/**
 * Provides a way for plugins (e.g., games) to notify the engine of
 * important events.
 *
 * @param notification  One of the DD_NOTIFY_* enums.
 * @param param         Additional arguments about the notification,
 *                      depending on the notification type.
 */
LIBDOOMSDAY_EXTERN_C LIBDOOMSDAY_PUBLIC
void Plug_Notify(int notification, void *data);

#endif // LIBDOOMSDAY_PLUGINS_H

