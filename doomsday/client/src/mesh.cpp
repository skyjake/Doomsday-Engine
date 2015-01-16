/** @file mesh.cpp  Mesh Geometry Data Structure.
 *
 * @authors Copyright © 2008-2014 Daniel Swanson <danij@dengine.net>
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

#include <QtAlgorithms>

#include "HEdge"
#include "Face"
#include "Vertex"

#include "mesh.h"

namespace de {

DENG2_PIMPL_NOREF(Mesh::Element)
{
    Mesh *mesh = nullptr;              ///< Owner of the element.
    MapElement *mapElement = nullptr;  ///< Attributed MapElement if any (not owned).
};

Mesh::Element::Element(Mesh &mesh) : d(new Instance)
{
    d->mesh = &mesh;
}

Mesh &Mesh::Element::mesh() const
{
    DENG2_ASSERT(d->mesh);
    return *d->mesh;
}

bool Mesh::Element::hasMapElement() const
{
    return d->mapElement != nullptr;
}

MapElement &Mesh::Element::mapElement()
{
    if(d->mapElement) return *d->mapElement;
    /// @throw MissingMapElement  Attempted with no map element attributed.
    throw MissingMapElementError("Mesh::Element::mapElement", "No map element is attributed");
}

MapElement const &Mesh::Element::mapElement() const
{
    return const_cast<Mesh::Element *>(this)->mapElement();
}

void Mesh::Element::setMapElement(MapElement const *newMapElement)
{
    d->mapElement = const_cast<MapElement *>(newMapElement);
}

DENG2_PIMPL_NOREF(Mesh)
{
    Vertexs vertexs;  ///< All vertexs in the mesh.
    HEdges hedges;    ///< All half-edges in the mesh.
    Faces faces;      ///< All faces in the mesh.
};

Mesh::Mesh() : d(new Instance)
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
    Vertex *vtx = new Vertex(*this, origin);
    d->vertexs.append(vtx);
    return vtx;
}

HEdge *Mesh::newHEdge(Vertex &vertex)
{
    HEdge *hedge = new HEdge(*this, vertex);
    d->hedges.append(hedge);
    return hedge;
}

Face *Mesh::newFace()
{
    Face *face = new Face(*this);
    d->faces.append(face);
    return face;
}

void Mesh::removeVertex(Vertex &vertex)
{
    int sizeBefore = d->vertexs.size();
    d->vertexs.removeOne(&vertex);
    if(sizeBefore != d->vertexs.size())
    {
        delete &vertex;
    }
}

void Mesh::removeHEdge(HEdge &hedge)
{
    int sizeBefore = d->hedges.size();
    d->hedges.removeOne(&hedge);
    if(sizeBefore != d->hedges.size())
    {
        delete &hedge;
    }
}

void Mesh::removeFace(Face &face)
{
    int sizeBefore = d->faces.size();
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

} // namespace de
