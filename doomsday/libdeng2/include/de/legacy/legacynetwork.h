/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LEGACYNETWORK_H
#define LEGACYNETWORK_H

#include "../libdeng2.h"
#include "../Time"

namespace de {

class Address;
class Block;
class IByteArray;

/**
 * Network communications for the legacy engine implementation.
 * Implements simple socket networking for streaming blocks of data.
 *
 * There is a C wrapper for LegacyNetwork, @see c_wrapper.h
 */
class LegacyNetwork
{
public:
    LegacyNetwork();
    ~LegacyNetwork();

    int openServerSocket(duint16 port);
    int accept(int serverSocket);
    int open(const Address& address);
    void close(int socket);

    int sendBytes(int socket, const IByteArray& data);
    int waitToReceiveBytes(int socket, IByteArray& data);

    int newSocketSet();
    void deleteSocketSet(int set);
    void addToSet(int set, int socket);
    void removeFromSet(int set, int socket);
    bool checkSetForActivity(int set, const Time::Delta& wait);

    /**
     * Checks how many bytes have been received for a socket.
     *
     * @param socket  Socket id.
     *
     * @return  Number of received bytes ready for reading.
     */
    int bytesReadyForSocket(int socket);

private:
    struct Instance;
    Instance* d;
};

}

#endif // LEGACYNETWORK_H
