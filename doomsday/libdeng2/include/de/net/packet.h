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

#ifndef LIBDENG2_PACKET_H
#define LIBDENG2_PACKET_H

#include "../ISerializable"
#include "../Address"
#include "../String"

namespace de {

class String;
    
/**
 * Base class for all network packets in the libdeng2 network communications protocol.
 * All packets are based on this.
 *
 * @ingroup protocol
 */
class DENG2_PUBLIC Packet : public ISerializable
{
public:
    /// While deserializing, an invalid type identifier was encountered. @ingroup errors
    DENG2_SUB_ERROR(DeserializationError, InvalidTypeError);

    /// Length of a type identifier.
    static const dint TYPE_SIZE = 4;

    typedef String Type;

public:
    /**
     * Constructs an empty packet.
     *
     * @param type  Type identifier of the packet.
     */
    Packet(const Type& type);

    virtual ~Packet() {}

    /**
     * Returns the type identifier of the packet.
     */
    const Type& type() const { return _type; }

    /**
     * Determines where the packet was received from.
     */
    const Address& from() const { return _from; }

    /**
     * Sets the address where the packet was received from.
     *
     * @param from  Address of the sender.
     */
    void setFrom(const Address& from) { _from = from; }

    /**
     * Execute whatever action the packet defines. This is called for all packets
     * once received and interpreted by the protocol. A packet defined outside
     * libdeng2 may use this to add functionality to the packet.
     */
    virtual void execute() const;

    // Implements ISerializable.
    void operator >> (Writer& to) const;
    void operator << (Reader& from);

protected:
    /**
     * Sets the type identifier.
     *
     * @param t  Type identifier. Must be exactly TYPE_SIZE characters long.
     */
    void setType(const Type& t);

public:
    /**
     * Checks if the packet starting at the current offset in the reader
     * has the given type identifier.
     *
     * @param from  Reader.
     * @param type  Packet identifier.
     */
    static bool checkType(Reader& from, const String& type);

private:
    /// The type is identified with a four-character string.
    Type _type;

    /// Address where the packet was received from.
    Address _from;
};

} // namespace de

#endif /* LIBDENG2_PACKET_H */
