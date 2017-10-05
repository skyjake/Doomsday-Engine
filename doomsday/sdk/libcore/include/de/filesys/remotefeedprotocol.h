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

#ifndef LIBDENG2_REMOTEFEEDPROTOCOL_H
#define LIBDENG2_REMOTEFEEDPROTOCOL_H

#include "../DictionaryValue"
#include "../File"
#include "../IdentifiedPacket"
#include "../Protocol"

namespace de {

/**
 * Packet for requesting information about remote files. @ingroup fs
 */
class DENG2_PUBLIC RemoteFeedQueryPacket : public IdentifiedPacket
{
public:
    enum Query { ListFiles, FileContents };

public:
    RemoteFeedQueryPacket();

    void setQuery(Query query);
    void setPath(String const &path);

    Query query() const;
    String path() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    static Packet *fromBlock(Block const &block);

private:
    Query _query;
    String _path;
};

/**
 * Packet that contains information about a set of files. Used as a response
 * to the ListFiles query. @ingroup fs
 */
class DENG2_PUBLIC RemoteFeedMetadataPacket : public IdentifiedPacket
{
public:
    RemoteFeedMetadataPacket();

    void addFile(File const &file);

    DictionaryValue const &metadata() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    static Packet *fromBlock(Block const &block);

private:
    DictionaryValue _metadata;
};

/**
 * Network message protocol for remote feeds.
 */
class DENG2_PUBLIC RemoteFeedProtocol : public Protocol
{
public:
    DENG2_ERROR(TypeError);

    enum PacketType
    {
        Unknown,
        Query,          ///< Query for file metadata or contents.
        Metadata,       ///< Response containing metadata.
    };

public:
    RemoteFeedProtocol();

    static PacketType recognize(Packet const &packet);
};

} // namespace de

#endif // LIBDENG2_REMOTEFEEDPROTOCOL_H
