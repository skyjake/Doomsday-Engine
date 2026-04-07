/** @file remoteuser.h  User that is communicating with the server over a network socket.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef REMOTEUSER_H
#define REMOTEUSER_H

#include <de/id.h>
#include <de/socket.h>
#include <de/transmitter.h>

/**
 * User that is communicating with the server over a network socket.
 * @ingroup server
 */
class RemoteUser : public de::Transmitter
{
public:
    /**
     * Constructs a new remote user from a connected network socket.
     * Each user is automatically assigned a unique identifier.
     *
     * @param socket  Socket used to communicate with the user. Ownership
     *                given to RemoteUser.
     */
    RemoteUser(de::Socket *socket);

    ~RemoteUser();

    de::Id id() const;

    /**
     * Returns the name of the user, if one has been provided.
     */
    de::String name() const;

    /**
     * Returns the network address of the user.
     */
    de::Address const address() const;

    /**
     * Determines if the user has joined the game in progress at the server.
     */
    bool isJoined() const;

    /**
     * Determines if the remote user is actually connecting from the local host
     * rather than from some remote one.
     */
    bool isFromLocalHost() const;

    /**
     * Relinquishes ownership of the user's socket.
     * @return Caller gets ownership of the returned socket.
     */
    de::Socket *takeSocket();

    // Implements Transmitter.
    void send(const de::IByteArray &data);

    DE_AUDIENCE(Destroy, void aboutToDestroyRemoteUser(RemoteUser &))

    void handleIncomingPackets();

protected:
    void socketDisconnected();

private:
    DE_PRIVATE(d)
};

#endif // REMOTEUSER_H
