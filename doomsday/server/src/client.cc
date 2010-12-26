/*
 * The Doomsday Engine Project -- dengsv
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "client.h"

using namespace de;

Client::Client(const de::Address& address) : _socket(0), _base(0), _updates(0)
{
    _socket = new Socket(address);
    initialize();
}

Client::Client(de::Socket* socket) : _socket(socket), _base(0), _updates(0)
{
    initialize();
}

void Client::initialize()
{
    Q_ASSERT(_socket != 0);

    // Notify when the socket is disconnected.
    connect(_socket, SIGNAL(disconnected()), this, SIGNAL(disconnected()));

    _base = new Channel(0, *_socket, this);
    _updates = new Channel(1, *_socket, this);

    grantRights();
}

void Client::grantRights()
{
    /// Local clients get the admin rights automatically.
    if(_socket->peerAddress().host() == QHostAddress::LocalHost)
    {
        rights |= AdminRight;
    }
}

de::Socket& Client::socket()
{
    Q_ASSERT(_socket != 0);
    return *_socket;
}

de::Channel& Client::base()
{
    Q_ASSERT(_base != 0);
    return *_base;
}

de::Channel& Client::updates()
{
    Q_ASSERT(_updates != 0);
    return *_updates;
}

void Client::send(const de::IByteArray &data)
{
    base().send(data);
}
