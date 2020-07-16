/** @file packagefeed.h  Links to loaded packages.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_PACKAGEFEED_H
#define LIBCORE_PACKAGEFEED_H

#include "de/feed.h"

namespace de {

class Package;
class PackageLoader;

/**
 * Feed that maintains links to loaded packages.
 *
 * @ingroup fs
 */
class DE_PUBLIC PackageFeed : public Feed
{
public:
    enum LinkMode { LinkIdentifier, LinkVersionedIdentifier };

    typedef std::function<bool (const Package &)> Filter;

public:
    PackageFeed(PackageLoader &loader, LinkMode linkMode = LinkIdentifier);

    void setFilter(const Filter &filter);

    PackageLoader &loader();

    String description() const override;
    PopulatedFiles populate(const Folder &folder) override;
    bool prune(File &file) const override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_PACKAGEFEED_H
