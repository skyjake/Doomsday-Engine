/** @file mesh.cpp  Mesh Geometry Data Structure.
 *
 * @authors Copyright © 2008-2015 Daniel Swanson <danij@dengine.net>
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
#include "HEdge"
#include "Face"
#include "Vertex"

#include <QtAlgorithms>

namespace de {

Mesh::Element::Element(Mesh &owner)
{
    _owner = &owner;
}

Mesh &Mesh::Element::mesh() const
{
    DENG2_ASSERT(_owner != nullptr);
    return *_owner;
}

bool Mesh::Element::hasMapElement() const
{
    return _mapElement != nullptr;
}

world::MapElement &Mesh::Element::mapElement()
{
    if(_mapElement) return *_mapElement;
    /// @throw MissingMapElement  Attempted with no map element attributed.
    throw MissingMapElementError("Mesh::Element::mapElement", "No map element is attributed");
}

world::MapElement const &Mesh::Element::mapElement() const
{
    return const_cast<Mesh::Element *>(this)->mapElement();
}

void Mesh::Element::setMapElement(world::MapElement *newMapElement)
{
    _mapElement = newMapElement;
}

DENG2_PIMPL_NOREF(Mesh)
{
    Vertexs vertexs;  ///< All vertexs in the mesh.
    HEdges hedges;    ///< All half-edges in the mesh.
    Faces faces;      ///< All faces in the mesh.
};

Mesh::Mesh() : d(new Impl)
{}

Mesh::~Mesh()
{
    clear();
}

void Mesh::clear()
{
    qDeleteAll(d->vertexs); d->vertexs.clear();
    qDeleteAll(d->hedges); d->hedges.clear();
    qDeleteAll(d->faces); d->faces.clear();
}

Vertex *Mesh::newVertex(Vector2d const &origin)
{
    auto *vtx = new Vertex(*this, origin);
    d->vertexs.append(vtx);
    return vtx;
}

HEdge *Mesh::newHEdge(Vertex &vertex)
{
    auto *hedge = new HEdge(*this, vertex);
    d->hedges.append(hedge);
    return hedge;
}

Face *Mesh::newFace()
{
    auto *face = new Face(*this);
    d->faces.append(face);
    return face;
}

void Mesh::removeVertex(Vertex &vertex)
{
    dint sizeBefore = d->vertexs.size();
    d->vertexs.removeOne(&vertex);
    if(sizeBefore != d->vertexs.size())
    {
        delete &vertex;
    }
}

void Mesh::removeHEdge(HEdge &hedge)
{
    dint sizeBefore = d->hedges.size();
    d->hedges.removeOne(&hedge);
    if(sizeBefore != d->hedges.size())
    {
        delete &hedge;
    }
}

void Mesh::removeFace(Face &face)
{
    dint sizeBefore = d->faces.size();
    d->faces.removeOne(&face);
    if(sizeBefore != d->faces.size())
    {
        delete &face;
    }
}

Mesh::Vertexs const &Mesh::vertexs() const
{
    return d->vertexs;
}

Mesh::Faces const &Mesh::faces() const
{
    return d->faces;
}

Mesh::HEdges const &Mesh::hedges() const
{
    return d->hedges;
}

}  // namespace de
