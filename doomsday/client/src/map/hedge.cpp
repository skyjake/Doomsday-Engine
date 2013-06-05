/** @file map/hedge.cpp World Map Geometry Half-Edge.
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

#include "Mesh"
#include "Vertex"

#include "map/hedge.h"

namespace de {

DENG2_PIMPL(HEdge)
{
    /// Mesh owner of the half-edge.
    Mesh *mesh;

    /// Vertex of the half-edge.
    Vertex *vertex;

    /// Linked @em twin half-edge (that on the other side of "this" half-edge).
    HEdge *twin;

    /// Next half-edge (clockwise) around the @em face.
    HEdge *next;

    /// Previous half-edge (anticlockwise) around the @em face.
    HEdge *prev;

    /// Face geometry to which the half-edge is attributed (if any).
    Face *face;

    /// MapElement to which the half-edge is attributed (if any).
    MapElement *mapElement;

    Instance(Public *i, Mesh &mesh, Vertex &vertex)
        : Base(i),
          mesh(&mesh),
          vertex(&vertex),
          twin(0),
          next(0),
          prev(0),
          face(0),
          mapElement(0)
    {}

    inline HEdge **neighborAdr(ClockDirection direction) {
        return direction == Clockwise? &next : &prev;
    }
};

HEdge::HEdge(Mesh &mesh, Vertex &vertex)
    : d(new Instance(this, mesh, vertex))
{}

Mesh &HEdge::mesh() const
{
    DENG_ASSERT(d->mesh != 0);
    return *d->mesh;
}

Vertex &HEdge::vertex() const
{
    return *d->vertex;
}

bool HEdge::hasNeighbor(ClockDirection direction) const
{
    return (*d->neighborAdr(direction)) != 0;
}

HEdge &HEdge::neighbor(ClockDirection direction) const
{
    HEdge **neighborAdr = d->neighborAdr(direction);
    if(*neighborAdr)
    {
        return **neighborAdr;
    }
    /// @throw MissingNeighborError Attempted with no relevant neighbor attributed.
    throw MissingNeighborError("HEdge::neighbor", QString("No %1 neighbor is attributed").arg(direction == Clockwise? "Clockwise" : "Anticlockwise"));
}

void HEdge::setNeighbor(ClockDirection direction, HEdge const *newNeighbor)
{
    *d->neighborAdr(direction) = const_cast<HEdge *>(newNeighbor);
}

bool HEdge::hasTwin() const
{
    return d->twin != 0;
}

HEdge &HEdge::twin() const
{
    if(d->twin)
    {
        return *d->twin;
    }
    /// @throw MissingTwinError Attempted with no twin associated.
    throw MissingTwinError("HEdge::twin", "No twin half-edge is associated");
}

void HEdge::setTwin(HEdge const *newTwin)
{
    d->twin = const_cast<HEdge *>(newTwin);
}

bool HEdge::hasFace() const
{
    return d->face != 0;
}

Face &HEdge::face() const
{
    if(d->face)
    {
        return *d->face;
    }
    /// @throw MissingFaceError Attempted with no Face attributed.
    throw MissingFaceError("HEdge::face", "No face is attributed");
}

void HEdge::setFace(Face const *newFace)
{
    d->face = const_cast<Face *>(newFace);
}

MapElement *HEdge::mapElement() const
{
    return d->mapElement;
}

void HEdge::setMapElement(MapElement *newMapElement)
{
    d->mapElement = newMapElement;
}

} // namespace de
