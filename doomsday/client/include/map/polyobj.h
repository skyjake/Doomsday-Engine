/**
 * @file polyobj.h
 * Moveable Polygonal Map Objects (Polyobj). @ingroup map
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

#ifndef LIBDENG_MAP_POLYOBJ_H
#define LIBDENG_MAP_POLYOBJ_H

#include "dd_share.h"

#include <de/vector1.h>

// We'll use the base polyobj template directly as our polyobj.
typedef struct polyobj_s {
    DD_BASE_POLYOBJ_ELEMENTS()

    polyobj_s()
    {
        std::memset(this, 0, sizeof(polyobj_s));
    }

    /**
     * Translate the origin of the polyobj in the map coordinate space.
     *
     * @param delta  Movement delta on the X|Y plane.
     */
    bool move(const_pvec2d_t delta);

    /// @copydoc move()
    inline bool move(coord_t x, coord_t y)
    {
        coord_t point[2] = { x, y };
        return move(point);
    }

    /**
     * Rotate the angle of the polyobj in the map coordinate space.
     *
     * @param angle  World angle delta.
     */
    bool rotate(angle_t angle);

    /**
     * Update the axis-aligned bounding box for the polyobj (map coordinate
     * space) to encompass the points defined by it's vertices.
     *
     * @todo Should be private.
     */
    void updateAABox();

    /**
     * Update the tangent space vectors for all surfaces of the polyobj,
     * according to the points defined by the relevant LineDef's vertices.
     */
    void updateSurfaceTangents();

    /**
     * Iterate over the lines of the Polyobj making a callback for each.
     * Iteration ends when all lines have been visited or @a callback
     * returns non-zero.
     *
     * Caller should increment validCount if necessary before calling this
     * function as it is used to prevent repeated processing of lines.
     *
     * @param callback    Callback function ptr.
     * @param paramaters  Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int lineIterator(int (*callback) (LineDef *, void *parameters),
                     void *parameters = 0);

#if 0
    bool property(setargs_t &args) const;
    bool setProperty(setargs_t const &args);
#endif
} Polyobj;

#define POLYOBJ_SIZE        gx.polyobjSize

#endif // LIBDENG_MAP_POLYOB_H
