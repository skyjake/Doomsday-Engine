/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LEGACYNETWORK_H
#define LIBDENG2_LEGACYNETWORK_H

#include "../libdeng2.h"
#include "../Time"

namespace de {

class Address;
class Block;
class IByteArray;
class Socket;

/**
 * Network communications for the legacy engine implementation. Implements
 * simple socket networking for streaming blocks of data.
 *
 * There is a C wrapper for LegacyNetwork (see c_wrapper.h).
 */
class DENG2_PUBLIC LegacyNetwork
{
public:
    LegacyNetwork();
    ~LegacyNetwork();

    int openServerSocket(duint16 port);
    int accept(int serverSocket);
    int open(Address const &address);
    void close(int socket);
    de::Address peerAddress(int socket) const;
    bool isOpen(int socket);
    bool isLocal(int socket);

    int sendBytes(int socket, IByteArray const &data);
    bool receiveBlock(int socket, Block &data);

    int newSocketSet();
    void deleteSocketSet(int set);
    void addToSet(int set, int socket);
    void removeFromSet(int set, int socket);
    bool checkSetForActivity(int set);

    /**
     * Checks if there is incoming data for a socket.
     *
     * @param socket  Socket id.
     *
     * @return  @c true if there are one or more incoming messages, otherwise @c false.
     */
    bool incomingForSocket(int socket);

    /**
     * Relinquishes ownership of a socket. The socket is removed from any
     * socket sets, and loses its LegacyNetwork-specific id.
     *
     * @param socket  Socket id.
     *
     * @return de::Socket instance. Caller gets ownership.
     */
    Socket *takeSocket(int socket);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_LEGACYNETWORK_H
