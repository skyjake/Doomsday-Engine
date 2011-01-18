/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2010, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Channel"
#include "de/Message"

using namespace de;

Channel::Channel(duint channelNumber, Socket& socket, QObject *parent)
    : QObject(parent), _channelNumber(channelNumber), _socket(&socket)
{
    connect(_socket, SIGNAL(messageReady()), this, SLOT(checkMessageReady()));

    // The channel becomes inoperable once the link is gone.
    connect(_socket, SIGNAL(destroyed()), this, SLOT(socketDestroyed()));
}

Channel::~Channel()
{
    while(!_receivedMessages.isEmpty())
    {
        delete _receivedMessages.takeLast();
    }
}

void Channel::socketDestroyed()
{
    _socket = 0;
}

Message* Channel::receive()
{
    if(_receivedMessages.isEmpty())
    {
        return 0;
    }
    return _receivedMessages.takeFirst();
}

Message* Channel::peek()
{
    if(_receivedMessages.isEmpty())
    {
        return 0;
    }
    return _receivedMessages.first();
}

void Channel::send(const IByteArray &data)
{
    if(!_socket)
    {
        /// @throws SocketError The socket is no longer available.
        throw SocketError("Channel::send", "Socket has been destroyed");
    }

    _socket->setChannel(_channelNumber);
    _socket->send(data);
}

void Channel::checkMessageReady()
{
    Message* msg = _socket->peek();
    if(msg->channel() == _channelNumber)
    {
        // It's on this channel, take the message.
        _receivedMessages.append(_socket->receive());
        emit messageReady();
    }
}

Socket& Channel::socket()
{
    Q_ASSERT(_socket != 0);
    return *_socket;
}
