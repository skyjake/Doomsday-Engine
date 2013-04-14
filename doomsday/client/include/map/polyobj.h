/** @file polyobj.h Moveable Polygonal Map-Object (Polyobj).
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

#include <QList>
#include <QSet>
#include <de/vector1.h>

/**
 * @ingroup map
 */
typedef struct polyobj_s
{
public:
    typedef QList<Line *> Lines;
    typedef QSet<Vertex *> Vertexes;

public:
    DD_BASE_POLYOBJ_ELEMENTS()

    polyobj_s()
    {
        bspLeaf = 0;
        idx = 0;
        tag = 0;
        validCount = 0;
        dest[0] = dest[1] = 0;
        angle = destAngle = 0;
        angleSpeed = 0;
        _lines = new Lines;
        _uniqueVertexes = new Vertexes;
        originalPts = 0;
        prevPts = 0;
        speed = 0;
        crush = false;
        seqType = 0;
        buildData.index = 0;
    }

    // Does nothing about the user data section.
    ~polyobj_s()
    {
        foreach(Line *line, lines())
        {
            delete line->front()._leftHEdge;
        }

        delete static_cast<Lines *>(_lines);
        delete static_cast<Vertexes *>(_uniqueVertexes);
    }

    /**
     * Provides access to the list of Lines for the polyobj.
     */
    Lines const &lines() const;

    /**
     * Returns the total number of Lines for the polyobj.
     */
    inline uint lineCount() const { return lines().count(); }

    /**
     * To be called once all Lines have been added in order to compile the set
     * of unique vertexes for the polyobj. A vertex referenced by multiple lines
     * is only included once in this set.
     */
    void buildUniqueVertexes();

    /**
     * Provides access to the set of unique vertexes for the polyobj.
     *
     * @see buildUniqueVertexSet()
     */
    Vertexes const &uniqueVertexes() const;

    /**
     * Returns the total number of unique Vertexes for the polyobj.
     *
     * @see buildUniqueVertexSet()
     */
    inline uint uniqueVertexCount() const { return uniqueVertexes().count(); }

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
     * according to the points defined by the relevant Line's vertices.
     */
    void updateSurfaceTangents();

#if 0
    bool property(setargs_t &args) const;
    bool setProperty(setargs_t const &args);
#endif
} Polyobj;

#define POLYOBJ_SIZE        gx.polyobjSize

#endif // LIBDENG_MAP_POLYOB_H
