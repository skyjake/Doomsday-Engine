/** @file assetobserver.h  Utility for observing available assets.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_FILESYS_ASSETOBSERVER_H
#define LIBDENG2_FILESYS_ASSETOBSERVER_H

#include "de/FileIndex"

namespace de {
namespace filesys {

/**
 * Utility for observing available assets.
 *
 * Assumes that App has a PackageFeed linking the assets under "/packs".
 *
 * @ingroup fs
 */
class DENG2_PUBLIC AssetObserver
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
    AssetObserver(String const &regexPattern);

    /// Notified when an asset matching the provided regular expression is added or removed.
    DENG2_DEFINE_AUDIENCE2(Availability, void assetAvailabilityChanged(String const &identifier, Event event))

private:
    DENG2_PRIVATE(d)
};

} // namespace filesys
} // namespace de

#endif // LIBDENG2_FILESYS_ASSETOBSERVER_H
