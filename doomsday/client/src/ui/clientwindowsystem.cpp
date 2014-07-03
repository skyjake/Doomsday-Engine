/** @file clientwindowsystem.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "clientapp.h"
#include "dd_main.h"
#include "gl/gl_main.h"

using namespace de;

DENG2_PIMPL(ClientWindowSystem)
{
    SettingsRegister settings;

    struct ClientStyle : public Style {
        bool isBlurringAllowed() const {
            return !App_GameLoaded();
        }
    };

    Instance(Public *i)
        : Base(i)
    {
        self.setStyle(new ClientStyle);
        self.style().load(App::packageLoader().load("net.dengine.ui.defaultstyle"));

        settings.define(SettingsRegister::ConfigVariable, "window.main.showFps")
                .define(SettingsRegister::IntCVar,        "vid-fsaa", 1)
                .define(SettingsRegister::IntCVar,        "vid-vsync", 1);
    }
};

ClientWindowSystem::ClientWindowSystem()
    : WindowSystem()
    , d(new Instance(this))
{
    ClientWindow::setDefaultGLFormat();
}

SettingsRegister &ClientWindowSystem::settings()
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
