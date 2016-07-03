/** @file hedge.cpp  Mesh Geometry Half-Edge.
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

#include "hedge.h"

#include <de/Log>
#include <de/String>
#include "Face"

namespace de {

DENG2_PIMPL_NOREF(HEdge)
{
    Vertex *vertex = nullptr;
    HEdge *twin = nullptr;     ///< Linked @em twin half-edge (that on the other side of "this" half-edge).
    Face *face = nullptr;      ///< Face geometry to which the half-edge is attributed (if any).
    HEdge *next = nullptr;     ///< Next half-edge (clockwise) around the @em face.
    HEdge *prev = nullptr;     ///< Previous half-edge (anticlockwise) around the @em face.

    inline HEdge **neighborAdr(ClockDirection direction)
    {
        return direction == Clockwise? &next : &prev;
    }
};

HEdge::HEdge(Mesh &mesh, Vertex *vertex) : MeshElement(mesh), d(new Impl)
{
    setVertex(vertex);
}

bool HEdge::hasVertex() const
{
    return d->vertex != nullptr;
}

Vertex &HEdge::vertex() const
{
    if(d->vertex) return *d->vertex;
    /// @throw MissingVertexError Attempted with no Vertex attributed.
    throw MissingVertexError("HEdge::vertex", "No vertex is attributed");
}

void HEdge::setVertex(Vertex *newVertex)
{
    d->vertex = newVertex;
}

bool HEdge::hasTwin() const
{
    return d->twin != nullptr;
}

HEdge &HEdge::twin() const
{
    if(d->twin) return *d->twin;
    /// @throw MissingTwinError Attempted with no twin associated.
    throw MissingTwinError("HEdge::twin", "No twin half-edge is associated");
}

void HEdge::setTwin(HEdge *newTwin)
{
    d->twin = newTwin;
}

bool HEdge::hasFace() const
{
    return d->face != nullptr;
}

Face &HEdge::face() const
{
    if(d->face) return *d->face;
    /// @throw MissingFaceError Attempted with no Face attributed.
    throw MissingFaceError("HEdge::face", "No face is attributed");
}

void HEdge::setFace(Face *newFace)
{
    d->face = newFace;
}

bool HEdge::hasNeighbor(ClockDirection direction) const
{
    return (*d->neighborAdr(direction)) != nullptr;
}

HEdge &HEdge::neighbor(ClockDirection direction) const
{
    HEdge **neighborAdr = d->neighborAdr(direction);
    if(*neighborAdr) return **neighborAdr;
    /// @throw MissingNeighborError Attempted with no relevant neighbor attributed.
    throw MissingNeighborError("HEdge::neighbor", String("No ") + (direction == Clockwise? "Clockwise" : "Anticlockwise") + " neighbor is attributed");
}

void HEdge::setNeighbor(ClockDirection direction, HEdge *newNeighbor)
{
    *d->neighborAdr(direction) = newNeighbor;
}

}  // namespace de
