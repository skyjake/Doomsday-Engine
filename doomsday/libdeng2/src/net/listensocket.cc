/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

using namespace de;

ListenSocket::ListenSocket(duint16 port) : _socket(0), _port(port)
{
    _socket = new QTcpServer();
    if(!_socket->listen(QHostAddress::Any, _port))
    {
        /// @throw OpenError Opening the socket failed.
        throw OpenError("ListenSocket::ListenSocket", _socket->errorString());
    }

    Q_ASSERT(_socket->isListening());

    connect(_socket, SIGNAL(newConnection()), this, SLOT(acceptNewConnection()));
}

ListenSocket::~ListenSocket()
{
    delete _socket;
}

void ListenSocket::acceptNewConnection()
{
    QTcpSocket* s = _socket->nextPendingConnection();
    _incoming.append(s);

    emit incomingConnection();
}

Socket* ListenSocket::accept()
{
    if(_incoming.empty())
    {
        return 0;
    }
    // We can use this constructor because we are Socket's friend.
    return new Socket(_incoming.takeFirst());
}

duint16 ListenSocket::port() const
{
    return _port;
}
