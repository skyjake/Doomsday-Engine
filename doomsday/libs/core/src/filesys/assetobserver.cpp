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

#include "de/filesys/AssetObserver"
#include "de/App"
#include "de/FileSystem"
#include "de/LinkFile"
#include "de/Loop"
#include <regex>

namespace de {
namespace filesys {
        
static const std::string PREFIX = "asset";

DE_PIMPL(AssetObserver)
, DE_OBSERVES(FileIndex, Addition)
, DE_OBSERVES(FileIndex, Removal)
{
    const std::regex pattern;

    static FileIndex const &linkIndex() {
        return App::fileSystem().indexFor(DE_TYPE_NAME(LinkFile));
    }

    static String assetIdentifier(File const &link) {
        DE_ASSERT(link.name().beginsWith(String::fromStdString(PREFIX + ".")));
        return link.name().mid(6);
    }

    Impl(Public *i, String const &regex)
        : Base(i)
        , pattern(PREFIX + "\\." + regex.toStdString(), std::regex::icase)
    {
        // We will observe available model assets.
        linkIndex().audienceForAddition() += this;
        linkIndex().audienceForRemoval()  += this;
    }

    void fileAdded(File const &link, FileIndex const &)
    {
        // Only matching assets cause notifications.
        if (!std::regex_match(link.name().toStdString(), pattern)) return;

        const String ident = assetIdentifier(link);
        Loop::mainCall([this, ident]()
        {
            DE_FOR_PUBLIC_AUDIENCE2(Availability, i)
            {
                i->assetAvailabilityChanged(ident, Added);
            }
        });
    }

    void fileRemoved(File const &link, FileIndex const &)
    {
        // Only matching assets cause notifications.
        if (!std::regex_match(link.name().toStdString(), pattern)) return;

        const String ident = assetIdentifier(link);
        Loop::mainCall([this, ident]()
        {
            DE_FOR_PUBLIC_AUDIENCE2(Availability, i)
        {
                i->assetAvailabilityChanged(ident, Removed);
        }
        });
    }

    DE_PIMPL_AUDIENCE(Availability)
};

DE_AUDIENCE_METHOD(AssetObserver, Availability)

AssetObserver::AssetObserver(String const &regexPattern)
    : d(new Impl(this, regexPattern))
{}

} // namespace filesys
} // namespace de
