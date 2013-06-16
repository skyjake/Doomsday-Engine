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

#include "world/map.h"

namespace de {

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
     * Audience that will be notified when the "current" map changes.
     */
    DENG2_DEFINE_AUDIENCE(MapChange, void currentMapChanged())

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

    /**
     * To be called to reset the world back to the initial state. Any currently
     * loaded map will be unloaded and player states are re-initialized.
     *
     * @todo World should observe Games::GameChange.
     */
    void reset();

    /**
     * To be called following an engine reset to update the world state.
     */
    void update();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

DENG_EXTERN_C boolean ddMapSetup;
DENG_EXTERN_C timespan_t ddMapTime;

#endif // DENG_WORLD_H
