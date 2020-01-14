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

#pragma once

#include "../libdoomsday.h"
#include "factory.h"
#include "mobj.h"

#include <de/Observers>
#include <de/System>

namespace de { class Context; }

namespace world {

class Map;
class Materials;

/**
 * Base class for the game world.
 *
 * There can only be one instance of the world at a time.
 */
class LIBDOOMSDAY_PUBLIC World : public de::System
{
public:
    static World &get();

    static int ddMapSetup; // map setup is in progress
    static int validCount;

    DE_AUDIENCE(MapChange, void worldMapChanged())

public:
    World();

    /**
     * Returns the effective map-info definition Record associated with the given
     * @a mapUri (which may be the default definition, if invalid/unknown).
     *
     * @param mapUri  Unique identifier for the map to lookup map-info data for.
     */
    const de::Record &mapInfoForMapUri(const res::Uri &mapUri) const;

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
    world::Map &map() const;

    world::Materials &       materials();
    const world::Materials & materials() const;
    
    virtual bool allowAdvanceTime() const;

    /**
     * Called from P_Ticker() to update world state.
     */
    virtual void tick(timespan_t elapsed);
    
public:
    /// Scripting helper: get pointer to current instance mobj_t based on the script callstack.
    static mobj_t &contextMobj(const de::Context &);

protected:
    void setMap(world::Map *map);

private:
    DE_PRIVATE(d)
};

} // namespace world

