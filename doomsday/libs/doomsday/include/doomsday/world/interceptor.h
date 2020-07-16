/** @file interceptor.h  Map element/object ray trace interceptor.
 * @ingroup world
 *
 * @authors Copyright © 2013-2016 Daniel Swanson <danij@dengine.net>
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
#include "../api_map.h" // traverser_t
#include "map.h"

#include <de/vector.h>

namespace world {

/**
 * Provides a mechanism for tracing line / world map object/element interception.
 *
 * Note: For technical reasons it is not presently possible to nest one or more
 * interceptor traces.
 */
class LIBDOOMSDAY_PUBLIC Interceptor
{
public:
    /**
     * Construct a new interceptor.
     *
     * @param callback  Will be called for each intercepted map element/object.
     * @param from      Map space origin of the trace.
     * @param to        Map space destination of the trace.
     * @param flags     @ref pathTraverseFlags
     * @param context   Passed to the @a callback.
     */
    Interceptor(traverser_t      callback,
                const de::Vec2d &from    = {},
                const de::Vec2d &to      = {},
                int              flags   = PTF_ALL,
                void *           context = nullptr);

    /**
     * Provides read-only access to the map space origin of the trace.
     */
    const double *origin() const;

    /**
     * Provides read-only access to the map space direction of the trace.
     */
    const double *direction() const;

    /**
     * Provides read-only access to the current map space opening of the trace.
     */
    const LineOpening &opening() const;

    /**
     * Update the "opening" state for the trace in accordance with the heights
     * defined by the minimal planes which intercept @a line. Only lines within
     * the map specified at @ref trace() call time will be considered.
     *
     * @return  @c true iff after the adjustment the opening range is positive,
     * i.e., the top Z coordinate is greater than the bottom Z.
     */
    bool adjustOpening(const Line *line);

    /**
     * Execute the trace (i.e., cast the ray).
     *
     * @param map  World map in which to execute.
     *
     * @return  Callback return value.
     */
    int trace(const world::Map &map);

private:
    DE_PRIVATE(d)
};

} // namespace world
