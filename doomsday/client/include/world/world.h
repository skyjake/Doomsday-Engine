/** @file world.h World.
 *
 * Ideas for improvement:
 *
 * "background loading" - it would be very cool if map loading happened in
 * another thread. This way we could be keeping busy while players watch the
 * intermission animations.
 *
 * "seamless world" - multiple concurrent maps with no perceivable delay when
 * players move between them.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_H
#define DENG_WORLD_H

#include <de/libdeng1.h>

#include <de/Error>
#include <de/Observers>

#include "uri.hh"

#ifdef __CLIENT__
class Hand;
#endif

namespace de {

class Map;

/**
 * @ingroup world
 */
class World
{
    DENG2_NO_COPY  (World)
    DENG2_NO_ASSIGN(World)

public:
    /// No map is currently loaded. @ingroup errors
    DENG2_ERROR(MapError);

    /**
     * Notified when the "current" map changes.
     */
    DENG2_DEFINE_AUDIENCE(MapChange, void worldMapChanged(World &world))

#ifdef __CLIENT__
    /**
     * Notified when the "current" frame begins.
     */
    DENG2_DEFINE_AUDIENCE(FrameBegin, void worldFrameBegins(World &world, bool resetNextViewer))

    /**
     * Notified when the "current" frame ends.
     */
    DENG2_DEFINE_AUDIENCE(FrameEnd, void worldFrameEnds(World &world))
#endif

public:
    /**
     * Construct a new world with no "current" map.
     */
    World();

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

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

    /**
     * Returns @c true iff a map is currently loaded.
     */
    bool hasMap() const;

    /**
     * Provides access to the currently loaded map.
     *
     * @see hasMap()
     */
    Map &map() const;

    /**
     * @param uri  Universal resource identifier (URI) for the map to change to.
     *             If an empty URI is specified the current map will be unloaded.
     *
     * @return  @c true= the map change completed successfully.
     */
    bool changeMap(Uri const &uri);

    /**
     * Unload the currently loaded map (if any).
     *
     * @see changeMap()
     */
    inline void unloadMap() { changeMap(Uri()); }

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

    /**
     * Returns the hand of the "user" in the world. Used for manipulating elements
     * for the purposes of runtime map editing.
     *
     * @param distance  The current distance of the hand from the viewer will be
     *                  written here if not @c 0.
     */
    Hand &hand(coord_t *distance = 0) const;

#endif // __CLIENT__

private:
    DENG2_PRIVATE(d)
};

} // namespace de

DENG_EXTERN_C boolean ddMapSetup;
DENG_EXTERN_C timespan_t ddMapTime;

#endif // DENG_WORLD_H
