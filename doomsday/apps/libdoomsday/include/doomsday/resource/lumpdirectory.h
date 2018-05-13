/** @file lumpdirectory.h
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_RESOURCE_LUMPDIRECTORY_H
#define LIBDOOMSDAY_RESOURCE_LUMPDIRECTORY_H

#include "../libdoomsday.h"
#include <de/File>

namespace res {

using namespace de;

/**
 * Directory of WAD lumps.
 *
 * Utility for accessing the lump directory of a WAD file. The information is
 * presented as it exists in the file without any modifications.
 */
class LIBDOOMSDAY_PUBLIC LumpDirectory
{
public:
    enum Type { Invalid, Iwad, Pwad };
    enum MapType { None, ExMy, MAPxx };

    struct LIBDOOMSDAY_PUBLIC Entry : public ISerializable
    {
        Block name;     ///< Name of the lump.
        duint32 offset; ///< Position in the file, in bytes.
        duint32 size;   ///< Size of the lump.

        // Implements ISerializable.
        void operator >> (Writer &to) const override;
        void operator << (Reader &from) override;
    };

    typedef dsize Pos;
    static dsize const InvalidPos;

    DENG2_ERROR(OffsetError);

public:
    /**
     * Reads the lump directory from a byte array.
     *
     * @param wadData  Data of a WAD file. No reference to the data is kept
     *                 afterwards.
     */
    LumpDirectory(IByteArray const &wadData);

    bool isValid() const;

    Type type() const;

    MapType mapType() const;

    /**
     * Returns the number of lumps.
     */
    Pos count() const;

    Entry const &entry(Pos pos) const;

    /**
     * Returns the CRC32 of the directory entries. This is not influenced by
     * the contents of the lumps.
     */
    duint32 crc32() const;

    /**
     * Checks if the lump directory has a specific lump. Performance is O(1) (hashed).
     *
     * @param lumpName  Name of a lump.
     *
     * @return @c true, if the lump is in the directory.
     */
    bool has(Block const &lumpName) const;

    duint32 lumpSize(Block const &lumpName) const;

    /**
     * Finds the entry of a lump in the directory. If there are multiple lumps with
     * the same name, this returns the last one in the directory. Performance is O(1)
     * (hashed).
     *
     * @param lumpName  Name of a lump.
     *
     * @return Lump entry information.
     */
    LumpDirectory::Pos find(Block const &lumpName) const;

    QList<LumpDirectory::Pos> findAll(const Block &lumpName) const;

    QList<LumpDirectory::Pos> findMaps() const;

    StringList findMapLumpNames() const;

    /**
     * Lists all the maps, combining individual maps into contiguous ranges.
     * @return Lists of map ranges.
     */
    StringList mapsInContiguousRangesAsText() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_LUMPDIRECTORY_H
