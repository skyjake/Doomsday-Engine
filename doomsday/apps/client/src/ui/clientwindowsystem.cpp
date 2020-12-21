/** @file clientwindowsystem.cpp
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

#include "de_platform.h"
#include "ui/clientwindowsystem.h"
#include "ui/clientstyle.h"
#include "clientapp.h"
#include "dd_main.h"
#include "gl/gl_main.h"

#include <de/PackageLoader>

using namespace de;

DENG2_PIMPL(ClientWindowSystem)
{
    ConfigProfiles settings;

    Impl(Public *i)
        : Base(i)
    {
        self().setStyle(new ClientStyle);
        self().style().load(App::packageLoader().load("net.dengine.client.defaultstyle"));

        using SReg = ConfigProfiles;
        settings.define(SReg::ConfigVariable, "window.main.showFps")
                .define(SReg::ConfigVariable, "window.main.fsaa")
                .define(SReg::ConfigVariable, "window.main.vsync")
                .define(SReg::ConfigVariable, "window.main.maxFps")
                .define(SReg::IntCVar,        "rend-finale-stretch", SCALEMODE_SMART_STRETCH)
                .define(SReg::IntCVar,        "rend-hud-stretch", SCALEMODE_SMART_STRETCH)
                .define(SReg::IntCVar,        "inlude-stretch", SCALEMODE_SMART_STRETCH)
                .define(SReg::IntCVar,        "menu-stretch", SCALEMODE_SMART_STRETCH);
    }
};

ClientWindowSystem::ClientWindowSystem()
    : WindowSystem()
    , d(new Impl(this))
{
    ClientWindow::setDefaultGLFormat();
}

ConfigProfiles &ClientWindowSystem::settings()
{
    return d->settings;
}

ClientWindow *ClientWindowSystem::createWindow(String const &id)
{
    return newWindow<ClientWindow>(id);
}

ClientWindow &ClientWindowSystem::main()
{
    return WindowSystem::main().as<ClientWindow>();
}

ClientWindow *ClientWindowSystem::mainPtr()
{
    return static_cast<ClientWindow *>(WindowSystem::mainPtr());
}

void ClientWindowSystem::closingAllWindows()
{
    // We can't get rid of the windows without tearing down GL stuff first.
    GL_Shutdown();

    WindowSystem::closingAllWindows();
}

bool ClientWindowSystem::rootProcessEvent(Event const &event)
{
    /// @todo Multiwindow? -jk
    return main().root().processEvent(event);
}

void ClientWindowSystem::rootUpdate()
{
    /// @todo Multiwindow? -jk
    main().root().update();
}
