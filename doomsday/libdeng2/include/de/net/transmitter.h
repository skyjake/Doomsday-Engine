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

#ifndef LIBDENG2_TRANSMITTER_H
#define LIBDENG2_TRANSMITTER_H

#include "../libdeng2.h"
#include "../IOStream"

namespace de {

class IByteArray;
class Packet;

/**
 * Abstract base class for objects that can send data.
 *
 * @ingroup net
 */
class DENG2_PUBLIC Transmitter : public IOStream
{
public:
    virtual ~Transmitter();

    /**
     * Sends an array of data.
     *
     * @param data  Data to send.
     */
    virtual void send(const IByteArray& data) = 0;

    /**
     * Sends a packet. The packet is first serialized and then sent.
     *
     * @param packet  Packet.
     */
    virtual void sendPacket(const Packet& packet);

    /**
     * Sends a packet. The packet is first serialized and then sent.
     *
     * @param packet  Packet.
     */
    virtual Transmitter& operator << (const Packet& packet);

    // Implements IOStream.
    /**
     * Sends an array of data.
     *
     * @param data  Data to send.
     */
    virtual IOStream& operator << (const IByteArray& data);
};

} // namespace de

#endif // LIBDENG2_TRANSMITTER_H
