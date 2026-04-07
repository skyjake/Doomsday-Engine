/** @file clientworld.h  World subsystem for the Client app.
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

#pragma once

#include <de/error.h>
#include <de/observers.h>
#include <de/vector.h>
#include <doomsday/world/world.h>
#include <doomsday/uri.h>

namespace de { class Context; }
namespace world { class Map; }

class Map;

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
class ClientWorld : public world::World
{
public:
//    /// Notified when a new frame begins.
//    DE_DEFINE_AUDIENCE(FrameBegin, void worldSystemFrameBegins(bool resetNextViewer))

//    /// Notified when the "current" frame ends.
//    DE_DEFINE_AUDIENCE(FrameEnd, void worldSystemFrameEnds())

public:
    /**
     * Construct a new world system (no map is loaded by default).
     */
    ClientWorld();

    /**
     * To be called to reset the world back to the initial state. Any currently
     * loaded map will be unloaded and player states are re-initialized.
     *
     * @todo World should observe GameChange.
     */
    void reset();

    Map &map() const;
   
    void aboutToChangeMap() override;
    void mapFinalized() override;
    bool allowAdvanceTime() const override;
    void tick(timespan_t) override;
};
