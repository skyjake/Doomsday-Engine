/*
 * The Doomsday Engine Project
 *
 * Copyright © 2010-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/identifiedpacket.h"
#include "de/writer.h"
#include "de/reader.h"

namespace de {

static IdentifiedPacket::Id idGen = 0;

IdentifiedPacket::IdentifiedPacket(const Type &type, Id i)
    : Packet(type), _id(i)
{}

void IdentifiedPacket::setId(Id id)
{
    _id = id;
}

void IdentifiedPacket::operator >> (Writer &to) const
{
    Packet::operator >> (to);
    to << id();
}

void IdentifiedPacket::operator << (Reader &from)
{
    Packet::operator << (from);
    from >> _id;
}

IdentifiedPacket::Id IdentifiedPacket::id() const
{
    if (!_id)
    {
        // Late assignment of the id. If the id is never asked, one is never set.
        _id = ++idGen;
    }
    return _id;
}

} // namespace de
