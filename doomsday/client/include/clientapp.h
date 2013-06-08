/** @file clientapp.h  The client application.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENTAPP_H
#define CLIENTAPP_H

#include <de/GuiApp>
#include <de/GLShaderBank>
#include "network/serverlink.h"
#include "ui/inputsystem.h"
#include "ui/windowsystem.h"
#include "ui/widgets/widgetactions.h"
#include "Games"
#include "world/world.h"

/**
 * The client application.
 */
class ClientApp : public de::GuiApp
{
public:
    ClientApp(int &argc, char **argv);

    /**
     * Sets up all the subsystems of the application. Must be called before the
     * event loop is started.
     */
    void initialize();

    void preFrame();
    void postFrame();

public:
    static bool haveApp();
    static ClientApp &app();
    static ServerLink &serverLink();
    static InputSystem &inputSystem();
    static WindowSystem &windowSystem();
    static WidgetActions &widgetActions();
    static de::GLShaderBank &glShaderBank();
    static de::Games &games();
    static de::World &world();

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENTAPP_H
