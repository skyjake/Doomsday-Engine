/** @file serverapp.h  The server application.
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

#ifndef SERVERAPP_H
#define SERVERAPP_H

#include <de/TextApp>
#include "Games"
#include "serversystem.h"
#include "world/world.h"

/**
 * The server application.
 */
class ServerApp : public de::TextApp
{
public:
    ServerApp(int &argc, char **argv);
    ~ServerApp();

    /**
     * Sets up all the subsystems of the application. Must be called before the
     * event loop is started.
     */
    void initialize();

public:
    static bool haveApp();
    static ServerApp &app();
    static ServerSystem &serverSystem();
    static de::Games &games();
    static de::World &world();

private:
    DENG2_PRIVATE(d)
};

#endif // SERVERAPP_H
