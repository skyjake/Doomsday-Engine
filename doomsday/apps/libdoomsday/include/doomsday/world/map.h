/** @file map.h  Base for world maps.
 *
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_MAP_H
#define LIBDOOMSDAY_WORLD_MAP_H

#include "../libdoomsday.h"
#include "../resource/mapmanifest.h"
#include <de/Deletable>
#include <de/Error>
#include <de/Observers>

namespace world {

/**
 * Base class for world maps.
 */
class LIBDOOMSDAY_PUBLIC Map : public de::Deletable
{
public:
    /// No resource manifest is associated with the map.
    DENG2_ERROR(MissingResourceManifestError);

public:
    /**
     * @param manifest  Resource manifest for the map (Can be set later, @ref setDef).
     */
    explicit Map(res::MapManifest *manifest = nullptr);

    /**
     * Returns @c true if a resource manifest is associated with the map.
     *
     * @see manifest(), setManifest()
     */
    bool hasManifest() const;

    /**
     * Returns the resource manifest for the map.
     *
     * @see hasManifest(), setManifest()
     */
    res::MapManifest &manifest() const;

    /**
     * Change the associated resource manifest to @a newManifest.
     *
     * @see hasManifest(), manifest()
     */
    void setManifest(res::MapManifest *newManifest);

private:
    DENG2_PRIVATE(d)
};

}  // namespace world

#endif  // LIBDOOMSDAY_WORLD_MAP_H
