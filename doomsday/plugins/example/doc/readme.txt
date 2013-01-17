
                        Doomsday Plugin Example 
                        =======================
                 by Daniel Swanson <danij@dengine.net>
                              28/02/2009


WHAT IS THIS?
------------------------------------------------------------------------
An ultra-light, stripped down example of a Doomsday plugin.

OVERVIEW
------------------------------------------------------------------------
The Doomsday engine uses a hook-based plugin system. This system allows
any number of plugins to hook themselves to a particular process, thus
the possibility of providing extended functionality not present in the
engine itself and/or access to the engine's public API for other means.

Various plugins are developed alongside the Doomsday Engine itself (such
as WadMapConverter and DehRead) and are distributed with it.

The Doomsday API and its plugin system are public interfaces, making it
possible to either completely replace existing plugins (for example, a
new map loader plugin could allow loading of maps of a format not
initially supported by Doomsday) or to provide extended functionality
through entirely new plugins.

DETAILS
------------------------------------------------------------------------
The Example Plugin is a bare bones example of the minimum requirements
of a Doomsday plugin. As such, it is not particularly useful and does
nothing other than output a message to Doomsday's console to signify
that the hook has been called successfully.

Doomsday will automatically load all plugins from the plugin directory, for instance:

Under Windows:
    C:\Program Files\Doomsday\bin\plugins\example.dll

During the load process, a plugin must register into the engine the
hooks by which it should be called. This is done via the Doomsday public
API function:

    Plug_AddHook(hookType, hookFunc)

@hookType (int)
The type of hook being attached to. Doomsday presents various hooks,
defined in
    
    engine/api/api_plugin.h

@hookFunc - (int(function)(int type, int param, void *data))
A pointer to the function to be called when the hook is processed.
