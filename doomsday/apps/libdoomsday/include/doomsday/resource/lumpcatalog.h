/** @file lumpcatalog.h  Catalog of lumps from multiple bundles.
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

#ifndef LIBDOOMSDAY_LUMPCATALOG_H
#define LIBDOOMSDAY_LUMPCATALOG_H

#include "../libdoomsday.h"
#include <de/String>
#include <de/Block>

namespace res {

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
    LumpCatalog();

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
    bool setPackages(de::StringList const &packageIds);

    de::Block read(de::String const &lumpName) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_LUMPCATALOG_H
