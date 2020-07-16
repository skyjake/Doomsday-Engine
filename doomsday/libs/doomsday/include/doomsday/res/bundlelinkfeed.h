/** @file bundlelinkfeed.h  FS feed for managing data bundle links.
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

#ifndef LIBDOOMSDAY_BUNDLELINKFEED_H
#define LIBDOOMSDAY_BUNDLELINKFEED_H

#include <de/feed.h>

namespace res {

/**
 * FS feed for managing data bundle links.
 *
 * The main responsibility of the feed is to prune broken links.
 */
class BundleLinkFeed : public de::Feed
{
public:
    BundleLinkFeed();

    de::String description() const override;
    PopulatedFiles populate(const de::Folder &folder) override;
    bool prune(de::File &file) const override;
};

} // namespace res

#endif // LIBDOOMSDAY_BUNDLELINKFEED_H
