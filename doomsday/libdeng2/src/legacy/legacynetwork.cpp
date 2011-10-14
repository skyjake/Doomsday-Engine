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
#include "de/ListenSocket"

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

    typedef QHash<int, ListenSocket*> ServerSockets;
    ServerSockets serverSockets;

    /// Socket sets for monitoring multiple sockets conveniently.
    struct SocketSet {
        QList<Socket*> members;
    };
    typedef QHash<int, SocketSet> SocketSets;
    SocketSets sets;

    Instance() : idGen(0) {}
    ~Instance() {
        // Cleanup time! Delete all existing sockets.
        foreach(Socket* s, sockets.values()) {
            delete s;
        }
        foreach(ListenSocket* s, serverSockets.values()) {
            delete s;
        }
    }
    int nextId() {
        /** @todo This will fail after 2.1 billion sockets have been opened.
         * That might take a while, though...
         */
        return ++idGen;
    }
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
    ListenSocket* sock = new ListenSocket(port);
    int id = d->nextId();
    d->serverSockets.insert(id, sock);
    return id;
}

int LegacyNetwork::accept(int serverSocket)
{
    DENG2_ASSERT(d->serverSockets.contains(serverSocket));

    ListenSocket* serv = d->serverSockets[serverSocket];
    Socket* sock = serv->accept();
    if(!sock) return 0;

    int id = d->nextId();
    d->sockets.insert(id, sock);
    return id;
}

int LegacyNetwork::open(const Address& address)
{
    LOG_AS("LegacyNetwork::open");
    try
    {
        Socket* sock = new Socket(address);
        int id = d->nextId();
        d->sockets.insert(id, sock);
        return id;
    }
    catch(const Socket::ConnectionError& er)
    {
        LOG_WARNING(er.asText());
        return 0;
    }
}

void LegacyNetwork::close(int socket)
{
    DENG2_ASSERT(d->sockets.contains(socket));
    delete d->sockets[socket];
    d->sockets.remove(socket);
}

int LegacyNetwork::sendBytes(int socket, const IByteArray& data)
{
    DENG2_ASSERT(d->sockets.contains(socket));
    try
    {
        d->sockets[socket]->send(data);
    }
    catch(const Socket::BrokenError& er)
    {
        LOG_AS("LegacyNetwork::sendBytes");
        LOG_WARNING("Could not send data to socket (%s): ")
                << d->sockets[socket]->peerAddress()
                << er.asText();
        return 0;
    }
    return data.size();
}

int LegacyNetwork::waitToReceiveBytes(int socket, IByteArray& data)
{
    DENG2_ASSERT(d->sockets.contains(socket));
    try
    {

    }
    catch(const Socket::BrokenError& er)
    {
        LOG_AS("LegacyNetwork::waitToReceiveBytes");
        LOG_WARNING(er.asText());
        return 0;
    }
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
