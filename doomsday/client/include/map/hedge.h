/** @file map/hedge.h World Map Geometry Half-edge.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_MAP_HEDGE
#define DENG_WORLD_MAP_HEDGE

#include <de/Error>
#include <de/Vector>

#include "MapElement"
#include "Vertex"

namespace de {

class Face;

/**
 * Half-edge geometry.
 *
 * @ingroup map
 */
class HEdge
{
    DENG2_NO_COPY(HEdge)
    DENG2_NO_ASSIGN(HEdge)

public:
    /// Required twin half-edge is missing. @ingroup errors
    DENG2_ERROR(MissingTwinError);

    /// Required neighbor half-edge is missing. @ingroup errors
    DENG2_ERROR(MissingNeighborError);

    /// Required face is missing. @ingroup errors
    DENG2_ERROR(MissingFaceError);

public:
    HEdge(Vertex &vertex);

    /**
     * Returns the vertex of the half-edge.
     */
    Vertex &vertex() const;

    /**
     * Convenient accessor method for returning the origin coordinates for the
     * vertex of the half-edge.
     *
     * @see vertex()
     */
    inline Vector2d const &origin() const { return vertex().origin(); }

    /**
     * Returns @c true iff the half-edge has a neighbor in the specifed direction
     * around the face of the polyon.
     */
    bool hasNeighbor(ClockDirection direction) const;

    /**
     * Returns the neighbor half-edge in the specified @a direction around the
     * face of the polygon.
     *
     * @param direction  Relative direction for desired neighbor half-edge.
     */
    HEdge &neighbor(ClockDirection direction) const;

    /**
     * Change the neighbor half-edge in the specified @a direction around the
     * face of the polygon.
     *
     * @param direction  Relative direction for the new neighbor half-edge.
     * @param newprev    Half-edge to attribute as the new neighbor. Ownership is
     *                   unaffected.
     *
     * @see hasNeighbor(), neighbor()
     */
    void setNeighbor(ClockDirection direction, HEdge const *newNeighbor);

    /**
     * Returns @c true iff the half-edge has a next (clockwise) neighbor around
     * the face of the polygon.
     *
     * @see hasNeighbor()
     */
    inline bool hasNext() const { return hasNeighbor(Clockwise); }

    /**
     * Returns the @em clockwise neighbor half-edge around the face of the
     * polygon.
     *
     * @see neighbor(), hasNext()
     */
    inline HEdge &next() const { return neighbor(Clockwise); }

    /**
     * Change the HEdge attributed as the next (clockwise) neighbor of "this"
     * half-edge.
     *
     * @param newNext  Half-edge to attribute as the new next (clockwise) neighbor.
     *                 Ownership is unaffected.
     *
     * @see setNeighbor(), next()
     */
    inline void setNext(HEdge *newNext) { setNeighbor(Clockwise, newNext); }

    /**
     * Returns @c true iff the half-edge has a previous (anticlockwise) neighbor
     * around the face of the polygon.
     *
     * @see hasNeighbor()
     */
    inline bool hasPrev() const { return hasNeighbor(Anticlockwise); }

    /**
     * Returns the @em anticlockwise neighbor half-edge around the face of the
     * polygon.
     *
     * @see neighbor(), hasPrev()
     */
    inline HEdge &prev() const { return neighbor(Anticlockwise); }

    /**
     * Change the HEdge attributed as the next (clockwise) neighbor of "this"
     * half-edge.
     *
     * @param newprev  Half-edge to attribute as the new previous (anticlockwise)
     *                 neighbor. Ownership is unaffected.
     *
     * @see setNeighbor(), prev()
     */
    inline void setPrev(HEdge *newPrev) { setNeighbor(Anticlockwise, newPrev); }

    /**
     * Returns @c true iff a @em twin is linked to the half-edge.
     */
    bool hasTwin() const;

    /**
     * Returns the linked @em twin of the half-edge.
     */
    HEdge &twin() const;

    /**
     * Returns a pointer to the linked @em twin of the half-edge; otherwise @c 0.
     *
     * @see hasTwin()
     */
    inline HEdge *twinPtr() const { return hasTwin()? &twin() : 0; }

    /**
     * Change the linked @em twin half-edge.
     *
     * @param newTwin  New half-edge to attribute as the @em twin half-edge.
     *                 Ownership is unaffected. Can be @c 0 (to clear the
     *                 attribution).
     *
     * @see hasTwin(), twin()
     */
    void setTwin(HEdge const *newTwin);

    /**
     * Returns @c true iff the half-edge is part of some Face geometry.
     */
    bool hasFace() const;

    /**
     * Returns the Face geometry the half-edge is a part of.
     *
     * @see hasFace()
     */
    Face &face() const;

    /**
     * Change the Face to which the half-edge is attributed.
     *
     * @param newFace  New Face to attribute to the half-edge. Ownership is
     *                 unaffected. Can be @c 0 (to clear the attribution).
     *
     * @see hasFace(), face()
     */
    void setFace(Face const *newFace);

    /**
     * Returns a pointer to the map element attributed to the half-edge. May return
     * @c 0 if not attributed.
     */
    MapElement *mapElement() const;

    /**
     * Change the MapElement to which the half-edge is attributed.
     *
     * @param newMapElement  New MapElement to attribute to the half-edge. Ownership
     *                       is unaffected. Can be @c 0 (to clear the attribution).
     *
     * @see mapElement()
     */
    void setMapElement(MapElement *newMapElement);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_WORLD_MAP_HEDGE
