/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_MESSAGE_H
#define LIBDENG2_MESSAGE_H

#include "../Block"
#include "../Address"

namespace de {

/**
 * Data block with the sender's network address and a multiplex channel.
 *
 * @ingroup net
 */
class DENG2_PUBLIC Message : public Block
{
public:
    typedef duint Channel;

public:
    Message(IByteArray const &other);
    Message(Address const &addr, Channel channel, Size initialSize = 0);
    Message(Address const &addr, Channel channel, IByteArray const &other);
    Message(Address const &addr, Channel channel, IByteArray const &other, Offset at, Size count);

    /**
     * Returns the address associated with the block.
     */
    Address const &address() const {
        return _address;
    }

    /**
     * Sets the channel over which the block was received.
     *
     * @param channel  Multiplex channel.
     */
    void setChannel(Channel channel) { _channel = channel; }

    /**
     * Returns the channel over which the block was received.
     */
    Channel channel() const { return _channel; }

private:
    Address _address;
    Channel _channel;
};

} // namespace de

#endif // LIBDENG2_MESSAGE_H
