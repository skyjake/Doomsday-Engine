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

MuxLink::MuxLink(const Address& address) : link_(0), defaultChannel_(*this, 0)
{
    link_ = new Link(address);
}

MuxLink::MuxLink(Socket* socket) : link_(0), defaultChannel_(*this, 0)
{
    link_ = new Link(socket);
}

MuxLink::~MuxLink()
{
    delete link_;
}

Address MuxLink::peerAddress() const
{
    return link_->peerAddress();
}

void MuxLink::demux()
{
    while(link_->hasIncoming())
    {
        std::auto_ptr<Message> message(link_->receive());
        // We will quietly ignore channels we can't receive.
        duint chan = message->channel();
        if(chan < NUM_CHANNELS)
        {
            buffers_[chan].put(message.release());
        }
    }
}

void MuxLink::Channel::send(const IByteArray& data)
{
    mux_.link_->mode.set(Link::CHANNEL_1_BIT, channel_ == 1);
    *mux_.link_ << data;
}

Message* MuxLink::Channel::receive()
{
    mux_.demux();
    return mux_.buffers_[channel_].get();
}

bool MuxLink::Channel::hasIncoming()
{
    mux_.demux();
    return !mux_.buffers_[channel_].empty();
}
