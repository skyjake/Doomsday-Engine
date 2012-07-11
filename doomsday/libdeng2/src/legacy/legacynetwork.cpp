/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/Message"

#include <QList>
#include <QTimer>

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
    if(d->sockets.contains(socket))
    {
        delete d->sockets[socket];
        d->sockets.remove(socket);
    }
    else if(d->serverSockets.contains(socket))
    {
        delete d->serverSockets[socket];
        d->serverSockets.remove(socket);
    }
    else
    {
        DENG2_ASSERT(false /* neither a socket or server socket */);
    }
}

bool LegacyNetwork::isOpen(int socket)
{
    if(!d->sockets.contains(socket)) return false;
    return d->sockets[socket]->isOpen();
}

de::Address LegacyNetwork::peerAddress(int socket) const
{
    DENG2_ASSERT(d->sockets.contains(socket));
    try
    {
        return d->sockets[socket]->peerAddress();
    }
    catch(const Socket::BrokenError& er)
    {
        LOG_AS("LegacyNetwork::peerAddress");
        LOG_WARNING(er.asText());
        return de::Address("0.0.0.0");
    }
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

bool LegacyNetwork::receiveBlock(int socket, Block& data)
{
    DENG2_ASSERT(d->sockets.contains(socket));
    data.clear();
    Message* msg = d->sockets[socket]->receive();
    if(!msg)
    {
        // Nothing was received yet; should've checked first!
        return false;
    }
    data += *msg;
    return true;
}

int LegacyNetwork::newSocketSet()
{
    int id = d->nextId();
    d->sets.insert(id, Instance::SocketSet());
    return id;
}

void LegacyNetwork::deleteSocketSet(int set)
{
    d->sets.remove(set);
}

void LegacyNetwork::addToSet(int set, int socket)
{
    DENG2_ASSERT(d->sets.contains(set));
    DENG2_ASSERT(d->sockets.contains(socket));
    DENG2_ASSERT(!d->sets[set].members.contains(d->sockets[socket]));

    d->sets[set].members << d->sockets[socket];
}

void LegacyNetwork::removeFromSet(int set, int socket)
{
    DENG2_ASSERT(d->sets.contains(set));
    DENG2_ASSERT(d->sockets.contains(socket));

    d->sets[set].members.removeOne(d->sockets[socket]);
}

bool LegacyNetwork::checkSetForActivity(int set)
{
    DENG2_ASSERT(d->sets.contains(set));

    foreach(Socket* sock, d->sets[set].members)
    {
        if(sock->hasIncoming())
        {
            // There are incoming messages ready for reading.
            return true;
        }
        if(!sock->isOpen())
        {
            // Closed sockets as reported as activity so that they can be removed
            // from the set by the caller.
            return true;
        }
    }

    // Nothing of note.
    return false;
}

bool LegacyNetwork::incomingForSocket(int socket)
{
    DENG2_ASSERT(d->sockets.contains(socket));
    return d->sockets[socket]->hasIncoming();
}
