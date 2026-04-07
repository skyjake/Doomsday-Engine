/** @file remotefeedprotocol.h  Message protocol for remote feeds.
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

#ifndef LIBCORE_REMOTEFEEDPROTOCOL_H
#define LIBCORE_REMOTEFEEDPROTOCOL_H

#include "dictionaryvalue.h"
#include "file.h"
#include "identifiedpacket.h"
#include "protocol.h"

namespace de {

/**
 * Packet for requesting information about remote files. @ingroup fs
 */
class DE_PUBLIC RemoteFeedQueryPacket : public IdentifiedPacket
{
public:
    enum Query { ListFiles, FileContents };

public:
    RemoteFeedQueryPacket();

    void setQuery(Query query);
    void setPath(const String &path);

    Query query() const;
    String path() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    static Packet *fromBlock(const Block &block);

private:
    Query _query;
    String _path;
};

/**
 * Packet that contains information about a set of files. Used as a response
 * to the ListFiles query. @ingroup fs
 */
class DE_PUBLIC RemoteFeedMetadataPacket : public IdentifiedPacket
{
public:
    RemoteFeedMetadataPacket();

    void addFile(const File &file, const String &prefix = String());
    void addFolder(const Folder &folder, String prefix = String());

    const DictionaryValue &metadata() const;

    static File::Type toFileType(int value);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    static Packet *fromBlock(const Block &block);

private:
    DictionaryValue _metadata;
};

/**
 * Packet that contains a portion of a file. Used as a response to the FileContents
 * query. @ingroup fs
 */
class DE_PUBLIC RemoteFeedFileContentsPacket : public IdentifiedPacket
{
public:
    RemoteFeedFileContentsPacket();

    void setData(const Block &data);
    void setStartOffset(dsize offset);
    void setFileSize(dsize size);

    const Block &data() const;
    dsize startOffset() const;
    dsize fileSize() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    static Packet *fromBlock(const Block &block);

private:
    dsize _startOffset;
    dsize _fileSize;
    Block _data;
};

/**
 * Network message protocol for remote feeds.
 */
class DE_PUBLIC RemoteFeedProtocol : public Protocol
{
public:
    DE_ERROR(TypeError);

    enum PacketType
    {
        Unknown,
        Query,          ///< Query for file metadata or contents.
        Metadata,       ///< Response containing metadata.
        FileContents,
    };

public:
    RemoteFeedProtocol();

    static PacketType recognize(const Packet &packet);
};

} // namespace de

#endif // LIBCORE_REMOTEFEEDPROTOCOL_H
