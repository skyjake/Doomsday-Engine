/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Transmitter"
#include "de/Block"
#include "de/Packet"
#include "de/Writer"
#include "de/Reader"

using namespace de;

Transmitter::~Transmitter()
{}

Transmitter& Transmitter::operator << (const IByteArray& data)
{
    send(data);
    return *this;
}

Transmitter& Transmitter::operator << (const Packet& packet)
{
    sendPacket(packet);
    return *this;
}

void Transmitter::sendPacket(const Packet& packet)
{
    Block data;
    Writer(data) << packet;
    send(data);
}
