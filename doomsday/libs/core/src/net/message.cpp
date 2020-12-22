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

#include "de/message.h"

namespace de {

Message::Message(const IByteArray &other) : Block(other), _channel(0)
{}

Message::Message(const Address &addr, Channel channel, Size initialSize)
    : Block(initialSize), _address(addr), _channel(channel)
{}

Message::Message(const Address &addr, Channel channel, const IByteArray &other)
    : Block(other), _address(addr), _channel(channel)
{}

Message::Message(const Address &addr, Channel channel, const IByteArray &other, Offset at, Size count)
    : Block(other, at, count), _address(addr), _channel(channel)
{}

} // namespace de
