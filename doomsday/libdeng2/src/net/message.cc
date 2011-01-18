/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Message"

using namespace de;

Message::Message(const IByteArray& other) : Block(other), _channel(0)
{}

Message::Message(const Address& addr, Channel channel, Size initialSize)
    : Block(initialSize), _address(addr), _channel(channel)
{}

Message::Message(const Address& addr, Channel channel, const IByteArray& other)
    : Block(other), _address(addr), _channel(channel)
{}

Message::Message(const Address& addr, Channel channel, const IByteArray& other, Offset at, Size count)
    : Block(other, at, count), _address(addr), _channel(channel)
{}
