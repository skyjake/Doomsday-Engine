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
#include "doorman.h"

#include <QCoreApplication>
#include <QThread>

using namespace de;

void internal::Doorman::run()
{
    _socket = new internal::TcpServer;
    _socket->listen(QHostAddress::Any, _port);
    Q_ASSERT(_socket->isListening());

    connect(_socket, SIGNAL(takeIncomingSocketDesc(int)), this, SIGNAL(incomingSocketDesc(int)));

    while(!_shouldStop)
    {
        _socket->waitForNewConnection(100);
    }

    delete _socket;
}

struct ListenSocket::Instance
{
    /// Pointer to the internal socket data.
    internal::Doorman* doorman;

    duint16 port;

    /// Incoming connections.
    QList<QTcpSocket*> incoming;

    Instance() : doorman(0), port(0) {}
    ~Instance() {
        doorman->stopAndWait();
        delete doorman;
    }
};

ListenSocket::ListenSocket(duint16 port)
{
    LOG_AS("ListenSocket");

    d = new Instance;

    d->doorman = new internal::Doorman(port);
    connect(d->doorman, SIGNAL(incomingSocketDesc(int)), this, SLOT(acceptNewConnection(int)));

    d->doorman->start();

/*    if(!d->socket->listen(QHostAddress::Any, d->port))
    {
        /// @throw OpenError Opening the socket failed.
        throw OpenError("ListenSocket", "Port " + QString::number(d->port) + ": " +
                        d->socket->errorString());
    }
*/

    //connect(d->socket, SIGNAL(newConnection()), this, SLOT(acceptNewConnection()));
}

ListenSocket::~ListenSocket()
{
    delete d;
}

void ListenSocket::acceptNewConnection(int handle)
{
    LOG_AS("ListenSocket::acceptNewConnection");

    QTcpSocket* s = new QTcpSocket;
    s->setSocketDescriptor(handle);
    d->incoming.append(s);

    LOG_MSG("Accepted new connection from %s.") << s->peerAddress().toString();

    emit incomingConnection();
}

Socket* ListenSocket::accept()
{
    if(d->incoming.empty())
    {
        return 0;
    }
    // We can use this constructor because we are Socket's friend.
    return new Socket(d->incoming.takeFirst());
}

duint16 ListenSocket::port() const
{
    return d->port;
}
