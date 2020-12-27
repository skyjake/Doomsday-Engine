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

#pragma once

#include "mesh.h"
#include "../world/vertex.h"

#include <de/error.h>
#include <de/vector.h>

namespace world { class Subsector; }

namespace mesh {

/**
 * Mesh half-edge geometry.
 *
 * @ingroup world
 */
class LIBDOOMSDAY_PUBLIC HEdge : public MeshElement
{
public:
    /// Required twin half-edge is missing. @ingroup errors
    DE_ERROR(MissingTwinError);

    /// Required neighbor half-edge is missing. @ingroup errors
    DE_ERROR(MissingNeighborError);

public:
    HEdge(Mesh &mesh, world::Vertex &vertex);

    /**
     * Returns the vertex of the half-edge.
     */
    inline world::Vertex &vertex() const { return *_vertex; }

    /**
     * Convenient accessor returning the origin coordinates for the vertex of the half-edge.
     *
     * @see vertex()
     */
    inline const de::Vec2d &origin() const { return vertex().origin(); }

    /**
     * Returns @c true iff a @em twin is linked to the half-edge.
     */
    inline bool hasTwin() const
    {
        return _twin != nullptr;
    }

    /**
     * Returns the linked @em twin of the half-edge.
     */
    inline HEdge &twin() const
    {
        if (_twin) return *_twin;
        /// @throw MissingTwinError Attempted with no twin associated.
        throw MissingTwinError("HEdge::twin", "No twin half-edge is associated");
    }

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
    inline Face &face() const
    {
        DE_ASSERT(_face != nullptr);
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
    bool hasNeighbor(de::ClockDirection direction) const
    {
        return _neighbors[int(direction)] != nullptr;
    }

    /**
     * Returns the neighbor half-edge in the specified @a direction.
     *
     * @param direction  Relative direction of the desired neighbor half-edge.
     */
    inline HEdge &neighbor(de::ClockDirection direction) const
    {
        if (HEdge *neighbor = _neighbors[int(direction)])
        {
            return *neighbor;
        }
        /// @throw MissingNeighborError Attempted with no relevant neighbor attributeds.
        throw MissingNeighborError("HEdge::neighbor",
                                   std::string("No ") +
                                       (direction == de::Clockwise ? "clockwise" : "counterclockwise") +
                                       " neighbor is attributed");
    }

    /**
     * Change the neighbor half-edge in the specified @a direction.
     *
     * @param direction    Relative direction for the new neighbor half-edge.
     * @param newNeighbor  Half-edge to attribute as the new neighbor. Ownership is unaffected.
     *
     * @see hasNeighbor(), neighbor()
     */
    void setNeighbor(de::ClockDirection direction, HEdge *newNeighbor);

    /**
     * Returns @c true if the half-edge has a next (clockwise) neighbor.
     *
     * @see hasNeighbor()
     */
    inline bool hasNext() const { return hasNeighbor(de::Clockwise); }

    /**
     * Returns the @em clockwise neighbor half-edge.
     *
     * @see neighbor(), hasNext()
     */
    inline HEdge &next() const { return neighbor(de::Clockwise); }

    /**
     * Change the HEdge attributed as the next (clockwise) neighbor.
     *
     * @param newNext  Half-edge to attribute as the next (clockwise) neighbor. Ownership is
     *                 unaffected.
     *
     * @see setNeighbor(), next()
     */
    inline void setNext(HEdge *newNext) { setNeighbor(de::Clockwise, newNext); }

    /**
     * Returns @c true iff the half-edge has a previous (CounterClockwise) neighbor.
     *
     * @see hasNeighbor()
     */
    inline bool hasPrev() const { return hasNeighbor(de::CounterClockwise); }

    /**
     * Returns the @em CounterClockwise neighbor half-edge.
     *
     * @see neighbor(), hasPrev()
     */
    inline HEdge &prev() const { return neighbor(de::CounterClockwise); }

    /**
     * Change the HEdge attributed as the next (clockwise) neighbor.
     *
     * @param newPrev  Half-edge to attribute as the previous (CounterClockwise) neighbor.
     *                 Ownership is unaffected.
     *
     * @see setNeighbor(), prev()
     */
    inline void setPrev(HEdge *newPrev) { setNeighbor(de::CounterClockwise, newPrev); }

    world::Subsector *subsector() const;

private:
    world::Vertex *_vertex;
    Face *         _face = nullptr; // Face geometry to which the half-edge is attributed (if any).
    HEdge *_twin = nullptr; // Linked @em twin half-edge (that on the other side of "this" half-edge).
    HEdge *_neighbors[2]{}; // Previous (CounterClockwise) and next half-edge (clockwise) around the @em
                            // face.
    mutable bool              _subsectorMissing = false;
    mutable world::Subsector *_subsector        = nullptr;
};

}  // namespace mesh
