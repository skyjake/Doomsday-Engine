/** @file assetobserver.cpp  Utility for observing available assets.
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

#include "de/filesys/assetobserver.h"
#include "de/app.h"
#include "de/filesystem.h"
#include "de/linkfile.h"
#include "de/loop.h"
#include "de/regexp.h"

namespace de {
namespace filesys {
        
static const char *PREFIX_DOT = "asset.";
static const char *PREFIX_SLASH_DOT = "asset\\.";

DE_PIMPL(AssetObserver)
, DE_OBSERVES(FileIndex, Addition)
, DE_OBSERVES(FileIndex, Removal)
{
    RegExp pattern;

    static const FileIndex &linkIndex() {
        return FS::get().indexFor(DE_TYPE_NAME(LinkFile));
    }

    static String assetIdentifier(const File &link) {
        DE_ASSERT(link.name().beginsWith(PREFIX_DOT));
        return link.name().substr(BytePos(6));
    }

    Impl(Public *i, const String &regex)
        : Base(i)
        , pattern(PREFIX_SLASH_DOT + regex, CaseInsensitive)
    {
        // We will observe available model assets.
        linkIndex().audienceForAddition() += this;
        linkIndex().audienceForRemoval()  += this;
    }

    void fileAdded(const File &link, const FileIndex &)
    {
        // Only matching assets cause notifications.
        if (!pattern.exactMatch(link.name())) return;

        const String ident = assetIdentifier(link);
        Loop::mainCall([this, ident]()
        {
            DE_NOTIFY_PUBLIC(Availability, i)
            {
                i->assetAvailabilityChanged(ident, Added);
            }
        });
    }

    void fileRemoved(const File &link, const FileIndex &)
    {
        // Only matching assets cause notifications.
        if (!pattern.exactMatch(link.name())) return;

        const String ident = assetIdentifier(link);
        Loop::mainCall([this, ident]()
        {
            DE_NOTIFY_PUBLIC(Availability, i)
        {
                i->assetAvailabilityChanged(ident, Removed);
        }
        });
    }

    DE_PIMPL_AUDIENCE(Availability)
};

DE_AUDIENCE_METHOD(AssetObserver, Availability)

AssetObserver::AssetObserver(const String &regexPattern)
    : d(new Impl(this, regexPattern))
{}

} // namespace filesys
} // namespace de
