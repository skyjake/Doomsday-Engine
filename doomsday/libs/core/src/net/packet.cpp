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

#include "de/packet.h"

#include "de/byterefarray.h"
#include "de/reader.h"
#include "de/string.h"
#include "de/writer.h"

namespace de {

Packet::Type Packet::typeFromString(const char *fourcc)
{
    Type type;
    std::memcpy(type.data(), fourcc, 4);
    return type;
}

Packet::Packet(const Type &t)
{
    setType(t);
}

void Packet::setType(const Type &t)
{
    _type = t;
}

void Packet::operator >> (Writer &to) const
{
    to.writeBytes(ByteRefArray(_type.data(), 4));
}

void Packet::operator << (Reader &from)
{
    Type ident;
    ByteRefArray ref(ident.data(), 4);
    from.readBytesFixedSize(ref);

    // Having been constructed as a specific type, the identifier is already
    // set and cannot change. Let's check if it's the correct one.
    if (_type != ident)
    {
        throw InvalidTypeError("Packet::operator <<", "Invalid ID");
    }
}

void Packet::execute() const
{}

bool Packet::checkType(Reader &from, const Type &type)
{
    from.mark();
    Type ident;
    ByteRefArray ref(ident.data(), 4);
    from.readBytesFixedSize(ref);
    from.rewind();
    return type == ident;
}

} // namespace de
