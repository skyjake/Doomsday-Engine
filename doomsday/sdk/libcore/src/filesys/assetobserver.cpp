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
#include <QRegExp>

namespace de {
namespace filesys {

static QString const PREFIX = "asset";

DENG2_PIMPL(AssetObserver)
, DENG2_OBSERVES(FileIndex, Addition)
, DENG2_OBSERVES(FileIndex, Removal)
{
    QRegExp pattern;
    LoopCallback mainCall;

    static FileIndex const &linkIndex() {
        return App::fileSystem().indexFor(DENG2_TYPE_NAME(LinkFile));
    }

    static String assetIdentifier(File const &link) {
        DENG2_ASSERT(link.name().beginsWith(PREFIX + "."));
        return link.name().mid(6);
    }

    Impl(Public *i, String const &regex)
        : Base(i)
        , pattern(PREFIX + "\\." + regex, Qt::CaseInsensitive)
    {
        // We will observe available model assets.
        linkIndex().audienceForAddition() += this;
        linkIndex().audienceForRemoval()  += this;
    }

    void fileAdded(File const &link, FileIndex const &)
    {
        // Only matching assets cause notifications.
        if (!pattern.exactMatch(link.name())) return;

        String const ident = assetIdentifier(link);
        mainCall.enqueue([this, ident] ()
        {
            DENG2_FOR_PUBLIC_AUDIENCE2(Availability, i)
            {
                i->assetAvailabilityChanged(ident, Added);
            }
        });
    }

    void fileRemoved(File const &link, FileIndex const &)
    {
        // Only matching assets cause notifications.
        if (!pattern.exactMatch(link.name())) return;

        DENG2_FOR_PUBLIC_AUDIENCE2(Availability, i)
        {
            i->assetAvailabilityChanged(assetIdentifier(link), Removed);
        }
    }

    DENG2_PIMPL_AUDIENCE(Availability)
};

DENG2_AUDIENCE_METHOD(AssetObserver, Availability)

AssetObserver::AssetObserver(String const &regexPattern)
    : d(new Impl(this, regexPattern))
{}

} // namespace filesys
} // namespace de
