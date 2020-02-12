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

namespace de { class Context; class Scheduler; }

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

    enum FrameState
    {
        /**
         * To be called at the beginning of a render frame, so that we can prepare for
         * drawing view(s) of the current map.
         */
        FrameBegins,

        /**
         * To be called at the end of a render frame, so that we can finish up any tasks
         * that must be completed after view(s) have been drawn.
         */
        FrameEnds
    };

    DE_AUDIENCE(MapChange, void worldMapChanged())

    /// Notified when a frame begins or ends.
    DE_AUDIENCE(FrameState, void worldFrameState(FrameState frameState))

    DE_AUDIENCE(PlaneMovement, void planeMovementBegan(const Plane &))

    /// No map is currently loaded. @ingroup errors
    DE_ERROR(MapError);

protected:
    World(); // build using one of the derived classes

public:
    void useDefaultConstructors();

    /**
     * Returns the effective map-info definition Record associated with the given
     * @a mapUri (which may be the default definition, if invalid/unknown).
     *
     * @param mapUri  Unique identifier for the map to lookup map-info data for.
     */
    const de::Record &mapInfoForMapUri(const res::Uri &mapUri) const;

    virtual void reset();
    
    /**
     * @param uri  Universal resource identifier (URI) for the map to change to.
     *             If an empty URI is specified the current map will be unloaded.
     *
     * @return  @c true= the map change completed successfully.
     */
    bool changeMap(const res::Uri &uri);

    /**
     * Unload the currently loaded map (if any).
     *
     * @see changeMap()
     */
    inline void unloadMap() { changeMap({}); }
    /**
     * Returns @c true if a map is currently loaded.
     */
    bool hasMap() const;

    /**
     * Returns the currently loaded map.
     */
    Map &map() const;

    /**
     * Returns a pointer to the currently loaded map, if any.
     */
    inline Map *mapPtr() const { return hasMap() ? &map() : nullptr; }
    
    mobj_t *takeUnusedMobj();
    void    putUnusedMobj(mobj_t *mo);

    Materials &       materials();
    const Materials & materials() const;
    
    de::Scheduler &scheduler();
    
    /**
     * Returns the current world time.
     */
    timespan_t time() const;
    
    /**
     * Advance time in the world.
     *
     * @param delta  Time delta to advance.
     */
    void advanceTime(timespan_t delta);

    virtual bool allowAdvanceTime() const;

    /**
     * Called from P_Ticker() to update world state.
     */
    virtual void tick(timespan_t elapsed);
    
    /**
     * Update the world state after engine reset.
     * Must be called only following an engine reset.
     */
    void update();
    
    void notifyFrameState(FrameState frameState);
    void notifyBeginPlaneMovement(const Plane &);

public:
    /// Scripting helper: get pointer to current instance mobj_t based on the script callstack.
    static mobj_t &contextMobj(const de::Context &);

protected:
    void setMap(Map *map);
    virtual void aboutToChangeMap();
    virtual void mapFinalized();

private:
    DE_PRIVATE(d)
};

} // namespace world

