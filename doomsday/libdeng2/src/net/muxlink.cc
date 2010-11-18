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

#include "de/MuxLink"
#include "de/Link"
#include "de/Socket"
#include "de/Message"

using namespace de;

MuxLink::MuxLink(const Address& address) : _link(0), _defaultChannel(*this, 0)
{
    _link = new Link(address);
}

MuxLink::MuxLink(Socket* socket) : _link(0), _defaultChannel(*this, 0)
{
    _link = new Link(socket);
}

MuxLink::~MuxLink()
{
    delete _link;
}

Address MuxLink::peerAddress() const
{
    return _link->peerAddress();
}

void MuxLink::demux()
{
    while(_link->hasIncoming())
    {
        QScopedPointer<Message> message(_link->receive());
        // We will quietly ignore channels we can't receive.
        duint chan = message->channel();
        if(chan < NUM_CHANNELS)
        {
            _buffers[chan].append(message.take());
        }
    }
}

void MuxLink::Channel::send(const IByteArray& data)
{
    _mux._link->mode.set(Link::CHANNEL_1_BIT, _channel == 1);
    *_mux._link << data;
}

Message* MuxLink::Channel::receive()
{
    _mux.demux();
    if(_mux._buffers[_channel].isEmpty())
    {
        return 0;
    }
    return _mux._buffers[_channel].takeFirst();
}

bool MuxLink::Channel::hasIncoming()
{
    _mux.demux();
    return !_mux._buffers[_channel].isEmpty();
}
