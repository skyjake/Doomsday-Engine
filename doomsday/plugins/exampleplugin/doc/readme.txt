
                        Doomsday Plugin Example 
                        =======================
                 by Daniel Swanson <danij@dengine.net>
                              22/10/2007


WHAT IS THIS?
------------------------------------------------------------------------
An ultra-light, stripped down example of a Doomsday plugin.

OVERVIEW
------------------------------------------------------------------------
The Doomsday engine uses a hook-based, plugin system. This system allows
any number of plugins to hook themselves to a particular process, thus
the possibility of providing extended functionality not present in the
engine itself and/or access to the engine's public API for other means.

Various plugins are developed alongside the Doomsday Engine itself (such
as dpMapLoad and dpDehRead) and are distributed with it.

The Doomsday API and it's plugin system are public interfaces, making it
possible to either completely replace existing plugins (for example, a
new map loader plugin could allow loading of maps of a format not
initially supported by Doomsday) or to provide extended functionality
through entirely new plugins.

DETAILS
------------------------------------------------------------------------
dpExample is a bare bones example of the minimum requirements of a
Doomsday plugin. As such, it is not particularly useful and does nothing
other than output a message to Doomsday's console to signify that the
hook has been called successfully.

The Doomsday engine will automatically load all plugins from the plugin
directory, provided their names are prefixed 'dp' e.g:

Under WindowsXP:
    c:\Program Files:\Doomsday\bin\dpExample.dll

During the load process, a plugin must register into the engine the
hooks by which it should be called. This is done via the Doomsday public
API function:

    Plug_AddHook(hookType, hookFunc)

@hookType (int)
The type of hook being attached to. The available hooks are as follows:
            
    HOOK_STARTUP         -  Called ASAP after startup.
    HOOK_INIT            -  Called after engine has been initialized.
    HOOK_DEFS            -  Called after DEDs have been loaded.
    HOOK_LOAD_MAP_LUMPS  -  Called when loading map data lumps.

@hookFunc - (int(function)(int type, int param, void *data))
A pointer to the function to be called when the hook is processed.
