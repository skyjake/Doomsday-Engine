/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

Link::Link(const Address& address) : _socket(0), _sender(0), _receiver(0)
{
    _socket = new Socket(address);
    initialize();
}

Link::Link(Socket* socket) : _socket(socket), _sender(0), _receiver(0)
{
    initialize();
}

void Link::initialize()
{
    assert(_socket != 0);
    
    _sender = new SenderThread(*_socket, _outgoing);
    _receiver = new ReceiverThread(*_socket, _incoming);
    
    _sender->start();
    _receiver->start();
    
    _peerAddress = _socket->peerAddress();
}

Link::~Link()
{
    flush();
    
    FOR_AUDIENCE(Deletion, i) i->linkBeingDeleted(*this);
    
    // Inform the threads that they can stop as soon as possible.
    _receiver->stop();
    _sender->stop();

    // Close the socket.
    _socket->close();
    
    // Wake up the sender thread (it's waiting for outgoing packets).
    _outgoing.post();
        
    delete _sender;
    delete _receiver;
    delete _socket;
}

bool Link::hasIncoming() const
{
    return !_incoming.empty();
}

void Link::flush()
{
    while(_sender->isRunning() && !_outgoing.empty())
    {
        Time::sleep(.01);
    }
}

Address Link::peerAddress() const
{
    return _peerAddress;
}

void Link::send(const IByteArray& data)
{
    Message* message = new Message(data);
    message->setChannel(mode[CHANNEL_1_BIT]? 1 : 0);
    _outgoing.put(message);
    _outgoing.post();
}

Message* Link::receive()
{
    Message* b = _incoming.get();
    if(b)
    {
        // A message was waiting.
        return b;
    }    
    if(!_receiver->isRunning())
    {
        /// @throw DisconnectedError The receiver is no longer running, which indicates
        /// that the remote end has closed the connection.
        throw DisconnectedError("Link::receive", "Link has been closed");
    }
    return 0;
}
