/** @file world.h  The game world.
 *
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_WORLD_H
#define LIBDOOMSDAY_WORLD_H

#include "../libdoomsday.h"
#include "mobj.h"

#include <de/Observers>
#include <de/System>

namespace de
{
    class Context;
}

namespace world
{
    class BaseMap;
    class Materials;
}

/**
 * Base class for the game world.
 *
 * Singleton: there can only be one instance of the world at a time.
 */
class LIBDOOMSDAY_PUBLIC World : public de::System
{
public:
    static World &get();

public:
    World();

    virtual void reset();

    /**
     * Returns @c true if a map is currently loaded.
     */
    bool hasMap() const;

    /**
     * Provides access to the currently loaded map.
     *
     * @see hasMap()
     */
    world::BaseMap &map() const;

    world::Materials &       materials();
    world::Materials const & materials() const;

    // Systems observe the passage of time.
    void timeChanged(de::Clock const &) override;

protected:
    void setMap(world::BaseMap *map);

public:
    /// Notified whenever the "current" map changes.
    DE_DEFINE_AUDIENCE2(MapChange, void worldMapChanged())

public:  /// @todo make private:
    void notifyMapChange();

private:
    DE_PRIVATE(d)
};

#endif  // LIBDOOMSDAY_WORLD_H
