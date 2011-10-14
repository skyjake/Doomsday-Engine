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

#include "de/LegacyNetwork"
#include "de/Socket"

#include <QList>

using namespace de;

/**
 * @internal Private instance data for LegacyNetwork.
 */
struct LegacyNetwork::Instance
{
    int idGen;

    /// All the currently open sockets, mapped by id.
    typedef QHash<int, Socket*> Sockets;
    Sockets sockets;

    /// Socket sets for monitoring multiple sockets conveniently.
    struct SocketSet {
        QList<Socket*> members;
    };
    typedef QHash<int, SocketSet> SocketSets;
    SocketSets sets;

    Instance() : idGen(0) {}
    ~Instance() {}
};

LegacyNetwork::LegacyNetwork()
{
    d = new Instance;
}

LegacyNetwork::~LegacyNetwork()
{
    delete d;
}

int LegacyNetwork::openServerSocket(duint16 port)
{

}

int LegacyNetwork::accept(int serverSocket)
{

}

int LegacyNetwork::open(const Address& address)
{

}

void LegacyNetwork::close(int socket)
{
}

int LegacyNetwork::sendBytes(int socket, const IByteArray& data)
{

}

int LegacyNetwork::waitToReceiveBytes(int socket, IByteArray& data)
{

}

int LegacyNetwork::newSocketSet()
{

}

void LegacyNetwork::deleteSocketSet(int set)
{

}

void LegacyNetwork::addToSet(int set, int socket)
{

}

void LegacyNetwork::removeFromSet(int set, int socket)
{

}

bool LegacyNetwork::checkSetForActivity(int set, const Time::Delta& wait)
{

}

int LegacyNetwork::bytesReadyForSocket(int socket)
{

}
