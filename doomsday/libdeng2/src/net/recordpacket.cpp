/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/RecordPacket"
#include "de/Record"
#include "de/Writer"
#include "de/Reader"
#include "de/Block"

namespace de {

static const char* RECORD_PACKET_TYPE = "RECO";

RecordPacket::RecordPacket(String const &name, Id i)
    : IdentifiedPacket(RECORD_PACKET_TYPE, i), _name(name), _record(0)
{
    _record = new Record();
}

RecordPacket::~RecordPacket()
{
    delete _record;
}

void RecordPacket::take(Record *newRecord)
{
    delete _record;
    _record = newRecord;
}

Record *RecordPacket::give()
{
    Record *detached = _record;
    _record = new Record;
    return detached;
}

Variable const &RecordPacket::operator [] (String const &variableName) const
{
    return (*_record)[variableName];
}

String RecordPacket::valueAsText(String const &variableName) const
{
    return (*_record)[variableName].value().asText();
}

void RecordPacket::operator >> (Writer &to) const
{
    IdentifiedPacket::operator >> (to);
    to << _name << *_record;
}

void RecordPacket::operator << (Reader &from)
{
    IdentifiedPacket::operator << (from);
    from >> _name >> *_record;
}

Packet *RecordPacket::fromBlock(Block const &block)
{
    Reader from(block);
    if(checkType(from, RECORD_PACKET_TYPE))
    {    
        std::auto_ptr<RecordPacket> p(new RecordPacket);
        from >> *p.get();
        return p.release();
    }
    return 0;
}

} // namespace de
