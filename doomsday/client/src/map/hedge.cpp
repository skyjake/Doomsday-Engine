/** @file map/hedge.cpp World Map Geometry Half-Edge.
 *
 * @authors Copyright Â© 2011-2013 Daniel Swanson <danij@dengine.net>
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

#include "Polygon"
#include "Vertex"

#include "map/hedge.h"

namespace de {

DENG2_PIMPL(HEdge)
{
    /// Vertex of the half-edge.
    Vertex *vertex;

    /// Linked @em twin half-edge (that on the other side of "this" half-edge).
    HEdge *twin;

    /// Next half-edge (clockwise) around the @em face.
    HEdge *next;

    /// Previous half-edge (anticlockwise) around the @em face.
    HEdge *prev;

    /// Polygon geometry to which the half-edge is attributed (if any).
    Polygon *poly;

    Instance(Public *i, Vertex &vertex)
        : Base(i),
          vertex(&vertex),
          twin(0),
          next(0),
          prev(0),
          poly(0)
    {}

    inline HEdge **neighborAdr(ClockDirection direction) {
        return direction == Clockwise? &next : &prev;
    }
};

HEdge::HEdge(Vertex &vertex)
    : d(new Instance(this, vertex))
{}

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

bool HEdge::hasPoly() const
{
    return d->poly != 0;
}

Polygon &HEdge::poly() const
{
    if(d->poly)
    {
        return *d->poly;
    }
    /// @throw MissingPolygonError Attempted with no Polygon attributed.
    throw MissingPolygonError("HEdge::poly", "No polygon is attributed");
}

void HEdge::setPoly(Polygon const *newPolygon)
{
    d->poly = const_cast<Polygon *>(newPolygon);
}

} // namespace de
