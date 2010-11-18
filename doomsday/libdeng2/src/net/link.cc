/*
 * The Doomsday Engine Project -- libdeng2
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

#include "de/Link"
#include "de/Socket"
#include "de/Address"
#include "de/Message"
#include "de/Time"
#include "de/Writer"
#include "de/Packet"
#include "de/App"
#include "de/Protocol"

using namespace de;

Link::Link(const Address& address) : _socket(0)
{
    _socket = new Socket(address);
}

Link::Link(Socket* socket) : _socket(socket)
{}

Link::~Link()
{
    flush();
    
    FOR_AUDIENCE(Deletion, i) i->linkBeingDeleted(*this);
    
    // Close the socket.
    _socket->close();
    
    delete _socket;
}

bool Link::hasIncoming() const
{
    return _socket->hasIncoming();
}

void Link::flush()
{
    _socket->flush();
}

Address Link::peerAddress() const
{
    return _socket->peerAddress();
}

void Link::send(const IByteArray& data)
{
    _socket->mode.set(Socket::CHANNEL_1_BIT, mode[CHANNEL_1_BIT]);
    *_socket << data;
}

Message* Link::receive()
{
    Message* b = _socket->receive();
    if(b)
    {
        // A message was waiting.
        return b;
    }    
    /*
    if(!_socket->isOpen())
    {
        /// @throw DisconnectedError The receiver is no longer running, which indicates
        /// that the remote end has closed the connection.
        throw DisconnectedError("Link::receive", "Link has been closed");
    }*/
    return 0;
}

void Link::socketDisconnected()
{}

void Link::socketError(QAbstractSocket::SocketError /*error*/)
{}
