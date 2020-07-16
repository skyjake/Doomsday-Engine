/** @file lumpcatalog.h  Catalog of lumps from multiple bundles.
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

#ifndef LIBDOOMSDAY_LUMPCATALOG_H
#define LIBDOOMSDAY_LUMPCATALOG_H

#include "../libdoomsday.h"
#include "doomsday/res/databundle.h"

#include <de/string.h>
#include <de/block.h>

namespace res {

using namespace de;

/**
 * Catalog of lumps from multiple bundles.
 *
 * This is a utility for locating and reading lumps from a set of data bundles.
 * It does not cache data: caching should either occur in the File objects or
 * a LumpBank that is backed by a LumpCatalog.
 */
class LIBDOOMSDAY_PUBLIC LumpCatalog
{
public:
    using LumpPos   = std::pair<const DataBundle *, LumpDirectory::Pos>;
    using LumpRange = std::pair<const DataBundle *, LumpDirectory::Range>;

public:
    LumpCatalog();
    
    LumpCatalog(const LumpCatalog &);

    void clear();

    /**
     * Sets the list of packages where data lumps are to be read from. Only data bundle
     * packages of Wad and Lump types are used.
     *
     * @param packageIds  List of packages. These are specifiesd in "load order", meaning
     *                    later ones override the contents of earlier ones.
     *
     * @return @c true, if the list of packages is different than the one set previously.
     */
    bool setPackages(const StringList &packageIds);

    void setBundles(const List<const DataBundle *> &bundles);

    inline void setBundles(std::initializer_list<const DataBundle *> bundles)
    {
        setBundles(List<const DataBundle *>(bundles));
    }

    LumpPos find(const String &lumpName) const;

    List<LumpPos> findAll(const String &lumpName) const;

    List<LumpRange> flatRanges() const;

    /**
     * Reads the contents of a lump.
     *
     * @param lumpName          Name of a lump.
     * @param lumpIndexOffset   Offset to apply to the lump index (e.g., for reading data
     *                          in relation to a start marker).
     * @return  Lump data.
     */
    Block read(const String &lumpName) const;
    
    Block read(const LumpPos &lump) const;

    String lumpName(const LumpPos &lump) const;

    de::StringList packages() const;

private:
    DE_PRIVATE(d)
};

} // namespace res

inline res::LumpCatalog::LumpPos operator+(const res::LumpCatalog::LumpPos &pos, int offset)
{
    return res::LumpCatalog::LumpPos{pos.first, pos.second + offset};
}

#endif // LIBDOOMSDAY_LUMPCATALOG_H
