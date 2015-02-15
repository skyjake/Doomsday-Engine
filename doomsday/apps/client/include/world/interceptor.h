/** @file interceptor.h  World map element/object ray trace interceptor.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_MAP_INTERCEPTOR_H
#define DENG_WORLD_MAP_INTERCEPTOR_H

#include "world/map.h"
#include "Line"
#include <de/Vector>

/**
 * Provides a mechanism for tracing line / world map object/element interception.
 *
 * Note: For technical reasons it is not presently possible to nest one or more
 * interceptor traces.
 *
 * @ingroup world
 */
class Interceptor
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
    Interceptor(traverser_t callback,
                de::Vector2d const &from = de::Vector2d(),
                de::Vector2d const &to   = de::Vector2d(),
                int flags                = PTF_ALL,
                void *context            = 0);

    /**
     * Provides read-only access to the map space origin of the trace.
     */
    coord_t const *origin() const;

    /**
     * Provides read-only access to the map space direction of the trace.
     */
    coord_t const *direction() const;

    /**
     * Provides read-only access to the current map space opening of the trace.
     */
    LineOpening const &opening() const;

    /**
     * Update the "opening" state for the trace in accordance with the heights
     * defined by the minimal planes which intercept @a line. Only lines within
     * the map specified at @ref trace() call time will be considered.
     *
     * @return  @c true iff after the adjustment the opening range is positive,
     * i.e., the top Z coordinate is greater than the bottom Z.
     */
    bool adjustOpening(Line const *line);

    /**
     * Execute the trace (i.e., cast the ray).
     *
     * @param map  World map in which to execute.
     *
     * @return  Callback return value.
     */
    int trace(de::Map const &map);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_MAP_INTERCEPTOR_H
