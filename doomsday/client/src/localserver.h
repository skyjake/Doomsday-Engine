/*
 * The Doomsday Engine Project -- dengcl
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOCALSERVER_H
#define LOCALSERVER_H

#include <de/deng.h>

/**
 * Responsible for starting and stopping the local server process.
 *
 * @ingroup client
 */
class LocalServer
{
public:
    /**
     * Starts the local server.
     *
     * @param listenOnPort  Port on which the server listens for incoming communications.
     */
    LocalServer(de::duint16 listenOnPort);
    
    /**
     * Stops the local server.
     */
    ~LocalServer();
    
private:
    de::duint16 _listenOnPort;
};

#endif /* LOCALSERVER_H */
