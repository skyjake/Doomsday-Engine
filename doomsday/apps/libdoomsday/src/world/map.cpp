/** @file world.cpp  Base for world maps.
 *
 * @authors Copyright � 2014-2015 Daniel Swanson <danij@dengine.net>
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

DENG2_PIMPL(Map)
, DENG2_OBSERVES(Record, Deletion)
{
    res::MapManifest *manifest = nullptr;  ///< Not owned, may be @c nullptr.

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        if(manifest) manifest->audienceForDeletion() -= this;
        DENG2_FOR_PUBLIC_AUDIENCE2(Deletion, i) i->mapBeingDeleted(self);
    }

    void recordBeingDeleted(Record &record)
    {
        // The manifest is not owned by us, it may be deleted by others.
        if(manifest == &record)
        {
            manifest = nullptr;
        }
    }

    DENG2_PIMPL_AUDIENCE(Deletion)
};

DENG2_AUDIENCE_METHOD(Map, Deletion)

Map::Map(res::MapManifest *manifest) : d(new Instance(this))
{
    setManifest(manifest);
}

Map::~Map()
{}

String Map::id() const
{
    if(!hasManifest()) return "";
    return manifest().gets("id");
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
    if(d->manifest) d->manifest->audienceForDeletion() -= d;

    d->manifest = newManifest;

    if(d->manifest) d->manifest->audienceForDeletion() += d;
}

}  // namespace world
