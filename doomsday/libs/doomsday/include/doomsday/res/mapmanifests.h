/** @file mapmanifests.h
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_RESOURCE_MAPMANIFESTS_H
#define LIBDOOMSDAY_RESOURCE_MAPMANIFESTS_H

#include "mapmanifest.h"
#include <de/pathtree.h>

namespace res {

class LIBDOOMSDAY_PUBLIC MapManifests
{
public:
    typedef de::PathTreeT<MapManifest> Tree;

public:
    MapManifests();

    /**
     * Locate the map resource manifest associated with the given, unique @a mapUri.
     *
     * Note that the existence of a resource manifest does not automatically mean the
     * associated resource data is actually loadable.
     */
    res::MapManifest &findMapManifest(const res::Uri &mapUri) const;

    /**
     * Lookup the map resource manifest associated with the given, unique @a mapUri.
     * Note that the existence of a resource manifest does not automatically mean the
     * associated resource data is actually loadable.
     *
     * @return  MapManifest associated with @a mapUri if found; otherwise @c nullptr.
     */
    res::MapManifest *tryFindMapManifest(const res::Uri &mapUri) const;

    /**
     * Returns the total number of map resource manifests in the system.
     */
    int mapManifestCount() const;

    /// @todo make private.
    void initMapManifests();

    const Tree &allMapManifests() const;

private:
    DE_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_MAPMANIFESTS_H
