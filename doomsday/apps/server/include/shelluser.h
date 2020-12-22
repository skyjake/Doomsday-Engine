/** @file shelluser.h  Remote user of a shell connection.
 * @ingroup server
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef SERVER_SHELLUSER_H
#define SERVER_SHELLUSER_H

#include <de/socket.h>
#include <doomsday/network/link.h>
#include "users.h"

/**
 * Remote user of a shell connection.
 *
 * Contains all the user-specific data pertinent to a remote connected shell
 * user.
 *
 * This is a server-side class representing a remote user connected to the
 * server through a shell link.
 *
 * @ingroup server
 */
class ShellUser : public network::Link, public User
{
public:
    /**
     * Constructs a new shell user from a previously opened socket.
     *
     * @param socket  Open socket. User takes ownership.
     */
    ShellUser(de::Socket *socket);

    /**
     * Send an initial data set to the shell user. This is only called once,
     * right after a shell user has connected.
     */
    void sendInitialUpdate();

    void sendGameState();
    void sendMapOutline();
    void sendPlayerInfo();

    de::Address address() const override;

protected:
    void handleIncomingPackets();

private:
    DE_PRIVATE(d)
};

#endif // SERVER_SHELLUSER_H
