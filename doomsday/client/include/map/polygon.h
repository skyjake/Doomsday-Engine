/** @file map/polygon.h World Map Polygon Geometry.
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

#ifndef DENG_WORLD_MAP_POLYGON
#define DENG_WORLD_MAP_POLYGON

#include <de/aabox.h>

#include <de/Vector>

class HEdge;

namespace de {

/**
 * Polygon geometry.
 *
 * @ingroup map
 */
class Polygon
{
public: /// @todo Make private:
    /// First half-edge in the face geometry. Ordered by angle, clockwise starting
    /// from the smallest angle.
    HEdge *_hedge;

    /// Number of HEdge's in the face.
    int _hedgeCount;

public:
    Polygon();

    /**
     * Returns a pointer to the first half-edge of the Face of the polygon (note
     * that half-edges are sorted in a clockwise order). May return @c 0 if there
     * is no half-edge linked to the face.
     */
    HEdge *firstHEdge() const;

    /**
     * Total number of half-edges in the polygon.
     */
    int hedgeCount() const;

    /**
     * Returns the axis-aligned bounding box which encompases all the vertexes
     * which define the geometry.
     */
    AABoxd const &aaBox() const;

    /**
     * Update the geometry's axis-aligned bounding box to encompass all vertexes.
     */
    void updateAABox();

    /**
     * Returns the point described by the average origin coordinates of all the
     * vertexes which define the geometry.
     */
    Vector2d const &center() const;

    /**
     * Update the center point of the geometry.
     *
     * @pre Axis-aligned bounding box must have been initialized.
     */
    void updateCenter();

    /**
     * Determines whether the geometry of the polygon is currently convex.
     *
     * @note Due to the potential computational complexity of determining convexity
     * this should be called sparingly/only when necessary.
     *
     * @todo Cache this result.
     */
    bool isConvex() const;

#ifdef DENG_DEBUG
    /**
     * Output a textual, human-readable description/representation of the polygon
     * to the application's output log.
     */
    void print() const;
#endif

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_MAP_POLYGON
