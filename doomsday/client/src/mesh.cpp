/** @file mesh.cpp Mesh Geometry Data Structure.
 *
 * @authors Copyright © 2008-2013 Daniel Swanson <danij@dengine.net>
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
    /// Mesh owner of the element.
    Mesh &mesh;

    /// MapElement to which the mesh element is attributed (if any).
    MapElement *mapElement;

    Instance(Mesh &mesh) : mesh(mesh), mapElement(0)
    {}
};

Mesh::Element::Element(Mesh &mesh) : d(new Instance(mesh))
{}

Mesh &Mesh::Element::mesh() const
{
    return d->mesh;
}

MapElement *Mesh::Element::mapElement() const
{
    return d->mapElement;
}

void Mesh::Element::setMapElement(MapElement const *newMapElement)
{
    d->mapElement = const_cast<MapElement *>(newMapElement);
}

DENG2_PIMPL(Mesh)
{
    /// All vertexes in the mesh.
    Vertexes vertexes;

    /// All half-edges in the mesh.
    HEdges hedges;

    /// All faces in the mesh.
    Faces faces;

    Instance(Public *i) : Base(i)
    {}

    ~Instance()
    {
        self.clear();
    }
};

Mesh::Mesh() : d(new Instance(this))
{}

void Mesh::clear()
{
    qDeleteAll(d->vertexes); d->vertexes.clear();
    qDeleteAll(d->hedges); d->hedges.clear();
    qDeleteAll(d->faces); d->faces.clear();
}

Vertex *Mesh::newVertex(Vector2d const &origin)
{
    Vertex *vtx = new Vertex(*this, origin);
    d->vertexes.append(vtx);
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
    int sizeBefore = d->vertexes.size();
    d->vertexes.removeOne(&vertex);
    if(sizeBefore != d->vertexes.size())
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

Mesh::Vertexes const &Mesh::vertexes() const
{
    return d->vertexes;
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
