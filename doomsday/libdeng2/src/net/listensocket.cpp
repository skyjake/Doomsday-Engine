/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ListenSocket"
#include "de/Address"
#include "de/Socket"

#include <QCoreApplication>
#include <QTcpServer>
#include <QThread>

using namespace de;

/*
void internal::Doorman::run()
{
    _socket = new internal::TcpServer;
    _socket->listen(QHostAddress::Any, _port);
    DENG2_ASSERT(_socket->isListening());

    connect(_socket, SIGNAL(takeIncomingSocketDesc(int)), this, SIGNAL(incomingSocketDesc(int)));

    while(!_shouldStop)
    {
        _socket->waitForNewConnection(100);
    }

    delete _socket;
}
*/

struct ListenSocket::Instance
{
#if 0
    /// Pointer to the internal socket data.
    internal::Doorman* doorman;
#endif

    QTcpServer* socket;
    duint16 port;

    /// Incoming connections.
    QList<QTcpSocket*> incoming;

    Instance() : socket(0), port(0) {}
    ~Instance() {
        delete socket;
    }
};

ListenSocket::ListenSocket(duint16 port)
{
    LOG_AS("ListenSocket");

    d = new Instance;
    d->socket = new QTcpServer(this);
    d->port = port;

    if(!d->socket->listen(QHostAddress::Any, d->port))
    {
        /// @throw OpenError Opening the socket failed.
        throw OpenError("ListenSocket", "Port " + QString::number(d->port) + ": " +
                        d->socket->errorString());
    }

    connect(d->socket, SIGNAL(newConnection()), this, SLOT(acceptNewConnection()));
}

ListenSocket::~ListenSocket()
{
    delete d;
}

void ListenSocket::acceptNewConnection()
{
    LOG_AS("ListenSocket::acceptNewConnection");

    d->incoming << d->socket->nextPendingConnection();

    emit incomingConnection();
}

Socket* ListenSocket::accept()
{
    if(d->incoming.empty())
    {
        return 0;
    }
    /*
    QTcpSocket* s = new QTcpSocket;
    s->setSocketDescriptor(d->incoming.takeFirst());
    */

    QTcpSocket* s = d->incoming.takeFirst();
    LOG_MSG("Accepted new connection from %s.") << s->peerAddress().toString();

    // We can use this constructor because we are Socket's friend.
    return new Socket(s);
}

duint16 ListenSocket::port() const
{
    return d->port;
}
