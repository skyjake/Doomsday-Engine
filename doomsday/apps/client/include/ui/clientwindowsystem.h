/** @file clientwindowsystem.h
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENTWINDOWSYSTEM_H
#define DE_CLIENTWINDOWSYSTEM_H

#include <de/WindowSystem>
#include <de/ui/Stylist>
#include "ConfigProfiles"

#undef main

class ClientWindow;

/**
 * Client-side window system for managing ClientWindow instances.
 */
class ClientWindowSystem : public de::WindowSystem
{
public:
    ClientWindowSystem();

    ConfigProfiles &settings();

    /**
     * Constructs a new window using the default configuration. Note that the
     * default configuration is saved persistently when the engine shuts down
     * and is restored when the engine is restarted.
     *
     * Command line options (e.g., -xpos) can be used to modify the window
     * configuration.
     *
     * @param id     Identifier of the window ("main" for the main window).
     *
     * @return ClientWindow instance. Ownership is retained by the window system.
     */
    ClientWindow *createWindow(de::String const &id = "main");

    static ClientWindow &main();
    static ClientWindow *mainPtr();

protected:
    void closingAllWindows();
    bool rootProcessEvent(de::Event const &event);
    void rootUpdate();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENTWINDOWSYSTEM_H
