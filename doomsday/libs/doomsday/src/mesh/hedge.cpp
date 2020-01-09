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

#include "doomsday/mesh/hedge.h"
#include "doomsday/mesh/face.h"
#include "doomsday/world/subsector.h"
#include "doomsday/world/convexsubspace.h"

#include <de/Log>
#include <de/String>

namespace mesh {

using namespace de;

DE_PIMPL_NOREF(HEdge)
{
    HEdge *twin = nullptr; // Linked @em twin half-edge (that on the other side of "this" half-edge).
    HEdge *neighbors[2]{}; // Previous (CounterClockwise) and next half-edge (clockwise) around the @em face.

    // For quicker lookups:
    bool subsectorMissing = false;
    world::Subsector *subsector = nullptr;
};

HEdge::HEdge(Mesh &mesh, world::Vertex &vertex)
    : MeshElement(mesh)
    , d(new Impl)
    , _vertex(&vertex)
    , _face(nullptr)
{}

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

void HEdge::setFace(Face *newFace)
{
    _face = newFace;
}

bool HEdge::hasNeighbor(ClockDirection direction) const
{
    return d->neighbors[int(direction)] != nullptr;
}

HEdge &HEdge::neighbor(ClockDirection direction) const
{
    HEdge *neighbor = d->neighbors[int(direction)];
    if(neighbor) return *neighbor;
    /// @throw MissingNeighborError Attempted with no relevant neighbor attributed.
    throw MissingNeighborError("HEdge::neighbor",
                               std::string("No ") +
                                   (direction == Clockwise ? "clockwise" : "counterclockwise") +
                                   " neighbor is attributed");
}

void HEdge::setNeighbor(ClockDirection direction, HEdge *newNeighbor)
{
    d->neighbors[int(direction)] = newNeighbor;
}

world::Subsector *HEdge::subsector() const
{
    if (d->subsectorMissing) return nullptr;
    if (!d->subsector)
    {
        if (_face && _face->hasMapElement() && _face->mapElement().type() == DMU_SUBSPACE)
        {
            d->subsector = _face->mapElementAs<world::ConvexSubspace>().subsectorPtr();
        }
        else
        {
            d->subsectorMissing = true;
        }
    }
    return d->subsector;
}

}  // namespace mesh
