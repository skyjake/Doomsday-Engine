/** @file remotefeedprotocol.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/RemoteFeedProtocol"
#include "de/TextValue"
#include "de/RecordValue"
#include "de/Folder"

namespace de {

// RemoteFeedQueryPacket ------------------------------------------------------

static char const *QUERY_PACKET_TYPE = "RFQu";

RemoteFeedQueryPacket::RemoteFeedQueryPacket()
    : IdentifiedPacket(QUERY_PACKET_TYPE)
{}

void RemoteFeedQueryPacket::setQuery(Query query)
{
    _query = query;
}

void RemoteFeedQueryPacket::setPath(String const &path)
{
    _path = path;
}

RemoteFeedQueryPacket::Query RemoteFeedQueryPacket::query() const
{
    return _query;
}

String RemoteFeedQueryPacket::path() const
{
    return _path;
}

void RemoteFeedQueryPacket::operator >> (Writer &to) const
{
    IdentifiedPacket::operator >> (to);
    to << duint8(_query) << _path;
}

void RemoteFeedQueryPacket::operator << (Reader &from)
{
    IdentifiedPacket::operator << (from);
    from.readAs<duint8>(_query);
    from >> _path;
}

Packet *RemoteFeedQueryPacket::fromBlock(Block const &block)
{
    return constructFromBlock<RemoteFeedQueryPacket>(block, QUERY_PACKET_TYPE);
}

// RemoteFeedMetadataPacket ---------------------------------------------------

static char const *METADATA_PACKET_TYPE = "RFMt";

RemoteFeedMetadataPacket::RemoteFeedMetadataPacket()
    : IdentifiedPacket(METADATA_PACKET_TYPE)
{}

void RemoteFeedMetadataPacket::addFile(File const &file)
{
    auto const &ns = file.objectNamespace();
    auto const status = file.status();

    std::unique_ptr<Record> fileMeta(new Record);

    fileMeta->addTime  ("modifiedAt", status.modifiedAt);
    fileMeta->addNumber("type",       status.type());
    if (status.type() == File::Status::FOLDER)
    {
        fileMeta->addNumber("size", file.as<Folder>().contents().size());
    }
    else
    {
        fileMeta->addNumber("size", status.size);
    }
    if (ns.hasSubrecord("package"))
    {
        fileMeta->add("package", new Record(ns["package"].valueAsRecord(),
                      Record::IgnoreDoubleUnderscoreMembers));
    }

    _metadata.add(new TextValue(file.name()),
                  new RecordValue(fileMeta.release(), RecordValue::OwnsRecord));
}

DictionaryValue const &RemoteFeedMetadataPacket::metadata() const
{
    return _metadata;
}

void RemoteFeedMetadataPacket::operator >> (Writer &to) const
{
    IdentifiedPacket::operator >> (to);
    to << _metadata;
}

void RemoteFeedMetadataPacket::operator << (Reader &from)
{
    IdentifiedPacket::operator << (from);
    from >> _metadata;
}

Packet *RemoteFeedMetadataPacket::fromBlock(Block const &block)
{
    return constructFromBlock<RemoteFeedMetadataPacket>(block, METADATA_PACKET_TYPE);
}

// RemoteFeedProtocol ---------------------------------------------------------

RemoteFeedProtocol::RemoteFeedProtocol()
{
    define(RemoteFeedQueryPacket::fromBlock);
    define(RemoteFeedMetadataPacket::fromBlock);
}

RemoteFeedProtocol::PacketType RemoteFeedProtocol::recognize(Packet const &packet)
{
    if (packet.type() == QUERY_PACKET_TYPE)
    {
        DENG2_ASSERT(is<RemoteFeedQueryPacket>(&packet));
        return Query;
    }
    if (packet.type() == METADATA_PACKET_TYPE)
    {
        DENG2_ASSERT(is<RemoteFeedMetadataPacket>(&packet));
        return Metadata;
    }
    return Unknown;
}

} // namespace de
