/** @file lumpdirectory.h
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

    struct Entry
    {
        de::Block name;     ///< Name of the lump.
        de::duint32 offset; ///< Position in the file, in bytes.
        de::duint32 size;   ///< Size of the lump.
    };

    typedef de::dsize Pos;

    DENG2_ERROR(OffsetError);

public:
    /**
     * Reads the lump directory from a byte array.
     *
     * @param wadData  Data of a WAD file. No reference to the data is kept
     *                 afterwards.
     */
    LumpDirectory(de::IByteArray const &wadData);

    bool isValid() const;

    Type type() const;

    /**
     * Returns the number of lumps.
     */
    Pos count() const;

    Entry const &entry(Pos pos) const;

    /**
     * Returns the CRC32 of the directory entries. This is not influenced by
     * the contents of the lumps.
     */
    de::duint32 crc32() const;

    bool has(de::Block const &lumpName) const;

    de::duint32 lumpSize(de::Block const &lumpName) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_LUMPDIRECTORY_H
