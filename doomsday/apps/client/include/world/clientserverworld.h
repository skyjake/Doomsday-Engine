/** @file worldsystem.h  World subsystem.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef WORLDSYSTEM_H
#define WORLDSYSTEM_H

#include <de/liblegacy.h>
#include <de/Error>
#include <de/Observers>
#include <de/Scheduler>
#include <de/Vector>
#include <doomsday/world/world.h>
#include <doomsday/uri.h>

namespace world { class Map; }

/**
 * Ideas for improvement:
 *
 * "background loading" - it would be very cool if map loading happened in
 * another thread. This way we could be keeping busy while players watch the
 * intermission animations.
 *
 * "seamless world" - multiple concurrent maps with no perceivable delay when
 * players move between them.
 *
 * @ingroup world
 */
class ClientServerWorld : public World
{
public:
    /// No map is currently loaded. @ingroup errors
    DE_ERROR(MapError);

#ifdef __CLIENT__
    /// Notified when a new frame begins.
    DE_AUDIENCE(FrameBegin, void worldSystemFrameBegins(bool resetNextViewer))

    /// Notified when the "current" frame ends.
    DE_AUDIENCE(FrameEnd, void worldSystemFrameEnds())
#endif

public:
    /**
     * Construct a new world system (no map is loaded by default).
     */
    ClientServerWorld();

    /**
     * To be called to reset the world back to the initial state. Any currently
     * loaded map will be unloaded and player states are re-initialized.
     *
     * @todo World should observe GameChange.
     */
    void reset();

    /**
     * To be called following an engine reset to update the world state.
     */
    void update();

    de::Scheduler &scheduler();

    /**
     * Provides access to the currently loaded map.
     */
    world::Map &map() const;

    /**
     * Returns a pointer to the currently loaded map, if any.
     */
    inline world::Map *mapPtr() const { return hasMap() ? &map() : nullptr; }

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
    inline void unloadMap() { changeMap(res::Uri()); }

    /**
     * Returns the effective map-info definition Record associated with the given
     * @a mapUri (which may be the default definition, if invalid/unknown).
     *
     * @param mapUri  Unique identifier for the map to lookup map-info data for.
     */
    const de::Record &mapInfoForMapUri(const res::Uri &mapUri) const;

    /**
     * Advance time in the world.
     *
     * @param delta  Time delta to apply.
     */
    void advanceTime(timespan_t delta);

    /**
     * Returns the current world time.
     */
    timespan_t time() const;

    void tick(timespan_t elapsed);

#ifdef __CLIENT__
    /**
     * To be called at the beginning of a render frame, so that we can prepare for
     * drawing view(s) of the current map.
     */
    void beginFrame(bool resetNextViewer = false);

    /**
     * To be called at the end of a render frame, so that we can finish up any tasks
     * that must be completed after view(s) have been drawn.
     */
    void endFrame();
#endif  // __CLIENT__

public:
    /// Scripting helper: get pointer to current instance mobj_t based on the script callstack.
    static mobj_t &contextMobj(const de::Context &);

private:
    DE_PRIVATE(d)
};

#endif  // WORLDSYSTEM_H
