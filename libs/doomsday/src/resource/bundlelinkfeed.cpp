/** @file bundlelinkfeed.cpp
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

#include "doomsday/res/bundlelinkfeed.h"

#include <de/linkfile.h>

using namespace de;

namespace res {

BundleLinkFeed::BundleLinkFeed()
{}

String BundleLinkFeed::description() const
{
    return "data bundle links";
}

Feed::PopulatedFiles BundleLinkFeed::populate(const Folder &)
{
    // Links are populated by DataBundle when files are identified.
    return PopulatedFiles();
}

bool BundleLinkFeed::prune(File &file) const
{
    if (const LinkFile *link = maybeAs<LinkFile>(file))
    {
        // Broken links must be removed.
        return link->isBroken();
    }
    return false;
}

} // namespace res
