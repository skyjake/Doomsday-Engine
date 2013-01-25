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

#include "de/IdentifiedPacket"
#include "de/Writer"
#include "de/Reader"

namespace de {

static IdentifiedPacket::Id idGen = 0;

IdentifiedPacket::IdentifiedPacket(Type const &type, Id i)
    : Packet(type), _id(i)
{}

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
    if(!_id)
    {
        // Late assignment of the id. If the id is never asked, one is never set.
        _id = ++idGen;
    }
    return _id;
}

} // namespace de
