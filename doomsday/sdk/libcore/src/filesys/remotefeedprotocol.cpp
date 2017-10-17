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
#include "de/BlockValue"
#include "de/TextValue"
#include "de/RecordValue"
#include "de/Folder"

namespace de {

// RemoteFeedQueryPacket ----------------------------------------------------------------

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

// RemoteFeedMetadataPacket -------------------------------------------------------------

static char const *METADATA_PACKET_TYPE = "RFMt";

RemoteFeedMetadataPacket::RemoteFeedMetadataPacket()
    : IdentifiedPacket(METADATA_PACKET_TYPE)
{}

void RemoteFeedMetadataPacket::addFile(File const &file, String const &prefix)
{
    auto const &ns = file.target().objectNamespace();
    auto const status = file.target().status();

    std::unique_ptr<Record> fileMeta(new Record);

    fileMeta->addTime  ("modifiedAt", status.modifiedAt);
    fileMeta->addNumber("type",       status.type() == File::Type::File? 0 : 1);
    if (status.type() == File::Type::Folder)
    {
        fileMeta->addNumber("size", file.target().as<Folder>().contents().size());
    }
    else
    {
        fileMeta->addNumber("size", status.size);
        fileMeta->addBlock ("metaId").value<BlockValue>().block() = file.metaId();
    }
    if (ns.hasSubrecord("package"))
    {
        fileMeta->add("package", new Record(ns.getr("package").dereference(),
                      Record::IgnoreDoubleUnderscoreMembers));
    }
//    if (ns.hasSubrecord("link"))
//    {
//        fileMeta->add("link", new Record(ns.getr("link").dereference()));
//    }

    _metadata.add(new TextValue(prefix / file.name()),
                  new RecordValue(fileMeta.release(), RecordValue::OwnsRecord));
}

void RemoteFeedMetadataPacket::addFolder(Folder const &folder, String prefix)
{
    folder.forContents([this, prefix] (String, File &file)
    {
        // Each file's metadata is included.
        addFile(file, prefix);

//        if (Folder const *subfolder = maybeAs<Folder>(file))
//        {
//            addFileTree(*subfolder, prefix / file.name());
//        }
        return LoopContinue;
    });
}

DictionaryValue const &RemoteFeedMetadataPacket::metadata() const
{
    return _metadata;
}

File::Type RemoteFeedMetadataPacket::toFileType(int value)
{
    return (value == 0? File::Type::File : File::Type::Folder);
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

// RemoteFeedFileContentsPacket ---------------------------------------------------------

static char const *FILE_CONTENTS_PACKET_TYPE = "RFCo";

RemoteFeedFileContentsPacket::RemoteFeedFileContentsPacket()
    : IdentifiedPacket(FILE_CONTENTS_PACKET_TYPE)
    , _startOffset(0)
{}

void RemoteFeedFileContentsPacket::setData(Block const &data)
{
    _data = data;
}

void RemoteFeedFileContentsPacket::setStartOffset(dsize offset)
{
    _startOffset = offset;
}

void RemoteFeedFileContentsPacket::setFileSize(dsize size)
{
    _fileSize = size;
}

Block const &RemoteFeedFileContentsPacket::data() const
{
    return _data;
}

dsize RemoteFeedFileContentsPacket::startOffset() const
{
    return _startOffset;
}

dsize RemoteFeedFileContentsPacket::fileSize() const
{
    return _fileSize;
}

void RemoteFeedFileContentsPacket::operator >> (Writer &to) const
{
    IdentifiedPacket::operator >> (to);
    to << duint64(_fileSize) << duint64(_startOffset) << _data;
}

void RemoteFeedFileContentsPacket::operator << (Reader &from)
{
    IdentifiedPacket::operator << (from);
    from.readAs<duint64>(_fileSize)
        .readAs<duint64>(_startOffset)
        >> _data;
}

Packet *RemoteFeedFileContentsPacket::fromBlock(Block const &block)
{
    return constructFromBlock<RemoteFeedFileContentsPacket>(block, FILE_CONTENTS_PACKET_TYPE);
}

// RemoteFeedProtocol -------------------------------------------------------------------

RemoteFeedProtocol::RemoteFeedProtocol()
{
    define(RemoteFeedQueryPacket::fromBlock);
    define(RemoteFeedMetadataPacket::fromBlock);
    define(RemoteFeedFileContentsPacket::fromBlock);
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
    if (packet.type() == FILE_CONTENTS_PACKET_TYPE)
    {
        DENG2_ASSERT(is<RemoteFeedFileContentsPacket>(&packet));
        return FileContents;
    }
    return Unknown;
}

} // namespace de
