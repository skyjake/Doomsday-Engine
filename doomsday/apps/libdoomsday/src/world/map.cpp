/** @file world.cpp  Base for world maps.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "doomsday/world/map.h"

using namespace de;

namespace world {

DENG2_PIMPL_NOREF(Map)
{
    res::MapManifest *manifest = nullptr;  ///< Not owned, may be @c nullptr.
};

Map::Map(res::MapManifest *manifest) : d(new Instance)
{
    setManifest(manifest);
}

bool Map::hasManifest() const
{
    return d->manifest != nullptr;
}

res::MapManifest &Map::manifest() const
{
    if(hasManifest())
    {
        DENG2_ASSERT(d->manifest != nullptr);
        return *d->manifest;
    }
    /// @throw MissingResourceManifestError  No associated resource manifest.
    throw MissingResourceManifestError("world::Map", "No associated resource manifest");
}

void Map::setManifest(res::MapManifest *newManifest)
{
    d->manifest = newManifest;
}

}  // namespace world
