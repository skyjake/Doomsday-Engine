/** @file hedge.h  Mesh Geometry Half-Edge.
 *
 * @authors Copyright © 2011-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DATA_MESH_HEDGE_H
#define DATA_MESH_HEDGE_H

#include <de/Error>
#include <de/Vector>

#include "Mesh"
#include "Vertex"

namespace world { class Subsector; }

namespace de {

/**
 * Mesh half-edge geometry.
 *
 * @ingroup world
 */
class HEdge : public MeshElement
{
public:
    /// Required vertex is missing. @ingroup errors
    //DENG2_ERROR(MissingVertexError);

    /// Required twin half-edge is missing. @ingroup errors
    DENG2_ERROR(MissingTwinError);

    /// Required face is missing. @ingroup errors
    //DENG2_ERROR(MissingFaceError);

    /// Required neighbor half-edge is missing. @ingroup errors
    DENG2_ERROR(MissingNeighborError);

public:
    HEdge(Mesh &mesh, Vertex &vertex);

    /**
     * Returns @c true iff a vertex is linked to the half-edge.
     */
    //bool hasVertex() const;

    /**
     * Returns the vertex of the half-edge.
     */
    inline Vertex &vertex() const { return *_vertex; }

    /**
     * Change the linked Vertex of the half-edge.
     *
     * @param newVertex  Vertex to attribute as the @em half-edge. Ownership is unaffected.
     *                   Use @c nullptr (to clear the attribution).
     *
     * @see hasVertex(), vertex()
     */
    //void setVertex(Vertex &newVertex);

    /**
     * Convenient accessor returning the origin coordinates for the vertex of the half-edge.
     *
     * @see vertex()
     */
    inline Vec2d const &origin() const { return vertex().origin(); }

    /**
     * Returns @c true iff a @em twin is linked to the half-edge.
     */
    bool hasTwin() const;

    /**
     * Returns the linked @em twin of the half-edge.
     */
    HEdge &twin() const;

    /**
     * Change the linked @em twin half-edge.
     *
     * @param newTwin  HEdge to attribute as the @em twin half-edge. Ownership is unaffected.
     *                 Use @c nullptr (to clear the attribution).
     *
     * @see hasTwin(), twin()
     */
    void setTwin(HEdge *newTwin);

    /**
     * Returns @c true if the half-edge is part of some Face geometry.
     */
    inline bool hasFace() const { return _face != nullptr; }

    /**
     * Returns the Face geometry the half-edge is a part of.
     *
     * @see hasFace()
     */
    inline Face &face() const { 
        DENG2_ASSERT(_face != nullptr);
        return *_face;
    }

    /**
     * Change the Face to which the half-edge is attributed.
     *
     * @param newFace  New Face to attribute to the half-edge. Ownership is unaffected. Use
     *                 @c nullptr (to clear the attribution).
     *
     * @see hasFace(), face()
     */
    void setFace(Face *newFace);

    /**
     * Returns @c true if the half-edge has a neighbor in the specifed direction.
     */
    bool hasNeighbor(ClockDirection direction) const;

    /**
     * Returns the neighbor half-edge in the specified @a direction.
     *
     * @param direction  Relative direction of the desired neighbor half-edge.
     */
    HEdge &neighbor(ClockDirection direction) const;

    /**
     * Change the neighbor half-edge in the specified @a direction.
     *
     * @param direction    Relative direction for the new neighbor half-edge.
     * @param newNeighbor  Half-edge to attribute as the new neighbor. Ownership is unaffected.
     *
     * @see hasNeighbor(), neighbor()
     */
    void setNeighbor(ClockDirection direction, HEdge *newNeighbor);

    /**
     * Returns @c true if the half-edge has a next (clockwise) neighbor.
     *
     * @see hasNeighbor()
     */
    inline bool hasNext() const { return hasNeighbor(Clockwise); }

    /**
     * Returns the @em clockwise neighbor half-edge.
     *
     * @see neighbor(), hasNext()
     */
    inline HEdge &next() const { return neighbor(Clockwise); }

    /**
     * Change the HEdge attributed as the next (clockwise) neighbor.
     *
     * @param newNext  Half-edge to attribute as the next (clockwise) neighbor. Ownership is
     *                 unaffected.
     *
     * @see setNeighbor(), next()
     */
    inline void setNext(HEdge *newNext) { setNeighbor(Clockwise, newNext); }

    /**
     * Returns @c true iff the half-edge has a previous (anticlockwise) neighbor.
     *
     * @see hasNeighbor()
     */
    inline bool hasPrev() const { return hasNeighbor(Anticlockwise); }

    /**
     * Returns the @em anticlockwise neighbor half-edge.
     *
     * @see neighbor(), hasPrev()
     */
    inline HEdge &prev() const { return neighbor(Anticlockwise); }

    /**
     * Change the HEdge attributed as the next (clockwise) neighbor.
     *
     * @param newPrev  Half-edge to attribute as the previous (anticlockwise) neighbor.
     *                 Ownership is unaffected.
     *
     * @see setNeighbor(), prev()
     */
    inline void setPrev(HEdge *newPrev) { setNeighbor(Anticlockwise, newPrev); }

    world::Subsector *subsector() const;

private:
    DENG2_PRIVATE(d)

    // Heavily used; visible for inline access:
    Vertex *_vertex; 
    Face *_face;     ///< Face geometry to which the half-edge is attributed (if any).
};

}  // namespace de

#endif  // DATA_MESH_HEDGE_H
