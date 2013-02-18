/** @file serversystem.h  Subsystem for tending to clients.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef SERVERSYSTEM_H
#define SERVERSYSTEM_H

#include <de/libdeng2.h>
#include <de/System>

/**
 * Subsystem for tending to clients.
 * @ingroup server
 */
class ServerSystem : public de::System
{
public:
    ServerSystem();
    ~ServerSystem();

    /**
     * Start listening for client connections.
     *
     * @param port  TCP port to listen on.
     */
    void start(de::duint16 port);

    void stop();

    bool isRunning() const;

    void timeChanged(de::Clock const &);

private:
    DENG2_PRIVATE(d)
};

#endif // SERVERSYSTEM_H
