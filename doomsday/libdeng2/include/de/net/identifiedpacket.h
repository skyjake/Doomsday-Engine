/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2010-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_IDENTIFIEDPACKET_H
#define LIBDENG2_IDENTIFIEDPACKET_H

#include "../libdeng2.h"
#include "../Packet"

namespace de {

/**
 * Network packet that is identified with a unique identifier.
 *
 * @ingroup net
 */
class DENG2_PUBLIC IdentifiedPacket : public Packet
{
public:
    typedef duint64 Id;

public:
    /**
     * Constructs a new identified packet.
     *
     * @param type  Type of the packet.
     * @param i     Identifier. If zero, a new identifier is generated.
     */
    IdentifiedPacket(Type const &type, Id i = 0);

    /// Returns the id of the packet.
    Id id() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    mutable Id _id;
};

} // namespace de

#endif // LIBDENG2_IDENTIFIEDPACKET_H
