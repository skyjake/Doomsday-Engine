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

#ifndef LIBCORE_TRANSMITTER_H
#define LIBCORE_TRANSMITTER_H

#include "de/libcore.h"
#include "de/iostream.h"

namespace de {

class IByteArray;
class Packet;

/**
 * Abstract base class for objects that can send data.
 *
 * @ingroup net
 */
class DE_PUBLIC Transmitter : public IOStream
{
public:
    virtual ~Transmitter();

    /**
     * Sends an array of data.
     *
     * @param data  Data to send.
     */
    virtual void send(const IByteArray &data) = 0;

    /**
     * Sends a packet. The packet is first serialized and then sent.
     *
     * @param packet  Packet.
     */
    virtual void sendPacket(const Packet &packet);

    /**
     * Sends a packet. The packet is first serialized and then sent.
     *
     * @param packet  Packet.
     */
    virtual Transmitter &operator << (const Packet &packet);

    // Implements IOStream.
    /**
     * Sends an array of data.
     *
     * @param data  Data to send.
     */
    virtual IOStream &operator << (const IByteArray &data);
};

} // namespace de

#endif // LIBCORE_TRANSMITTER_H
