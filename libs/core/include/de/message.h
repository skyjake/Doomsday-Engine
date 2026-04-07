/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBCORE_MESSAGE_H
#define LIBCORE_MESSAGE_H

#include "de/block.h"
#include "de/address.h"

namespace de {

/**
 * Data block with the sender's network address and a multiplex channel.
 *
 * @ingroup net
 */
class DE_PUBLIC Message : public Block
{
public:
    typedef duint Channel;

public:
    Message(const IByteArray &other);
    Message(const Address &addr, Channel channel, Size initialSize = 0);
    Message(const Address &addr, Channel channel, const IByteArray &other);
    Message(const Address &addr, Channel channel, const IByteArray &other, Offset at, Size count);

    /**
     * Returns the address associated with the block.
     */
    const Address &address() const {
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

#endif // LIBCORE_MESSAGE_H
