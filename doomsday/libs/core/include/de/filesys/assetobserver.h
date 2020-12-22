/** @file assetobserver.h  Utility for observing available assets.
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

#ifndef LIBCORE_FILESYS_ASSETOBSERVER_H
#define LIBCORE_FILESYS_ASSETOBSERVER_H

#include "de/fileindex.h"

namespace de {
namespace filesys {

/**
 * Utility for observing available assets.
 *
 * Assumes that App has a PackageFeed linking the assets under "/packs".
 *
 * @ingroup fs
 */
class DE_PUBLIC AssetObserver
{
public:
    enum Event { Added, Removed };

public:
    /**
     * Constructs an observer that notifies when assets matching @a regexPattern become
     * available or are unloaded.
     *
     * @param regexPattern  Pattern for asset identifier without the "asset." prefix.
     */
    AssetObserver(const String &regexPattern);

    /// Notified when an asset matching the provided regular expression is added or removed.
    DE_AUDIENCE(Availability, void assetAvailabilityChanged(const String &identifier, Event event))

private:
    DE_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // LIBCORE_FILESYS_ASSETOBSERVER_H
