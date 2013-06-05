/** @file map/face.h World Map Face Geometry.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_WORLD_MAP_FACE
#define DENG_WORLD_MAP_FACE

#include <de/aabox.h>

#include <de/Vector>

#include "MapElement"

namespace de {

class HEdge;
class Mesh;

/**
 * Face geometry.
 *
 * @ingroup map
 */
class Face
{
    DENG2_NO_COPY  (Face)
    DENG2_NO_ASSIGN(Face)

public:
    /// Total number of half-edge's in the face geometry.
    int _hedgeCount;

public:
    explicit Face(Mesh &mesh);

    /**
     * Returns the mesh the face is a part of.
     */
    Mesh &mesh() const;

    /**
     * Returns a pointer to the first half-edge in the face geometry (note that
     * half-edges are sorted in a clockwise order). May return @c 0 if there is
     * no half-edge linked to the face.
     */
    HEdge *hedge() const;

    /**
     * Total number of half-edges in the face geometry.
     */
    int hedgeCount() const;

    /**
     * Change the first half-edge in the face geometry.
     */
    void setHEdge(HEdge *newHEdge);

    /**
     * Returns the axis-aligned bounding box which encompases all the vertexes
     * which define the face geometry.
     */
    AABoxd const &aaBox() const;

    /**
     * Update the face geometry's axis-aligned bounding box to encompass all vertexes.
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
     * Determines whether the face geometry is currently convex.
     *
     * @note Due to the potential computational complexity of determining convexity
     * this should be called sparingly/only when necessary.
     *
     * @todo Cache this result.
     */
    bool isConvex() const;

    /**
     * Returns a pointer to the map element attributed to the face. May return @c 0
     * if not attributed.
     */
    MapElement *mapElement() const;

    /**
     * Change the MapElement to which the face is attributed.
     *
     * @param newMapElement  New MapElement to attribute to the face. Ownership is
     *                       unaffected. Can be @c 0 (to clear the attribution).
     *
     * @see mapElement()
     */
    void setMapElement(MapElement *newMapElement);

#ifdef DENG_DEBUG
    /**
     * Output a textual, human-readable description/representation of the face to
     * the application's output log.
     */
    void print() const;
#endif

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_MAP_FACE
