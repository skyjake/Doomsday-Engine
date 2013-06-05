/** @file data/mesh.cpp Mesh Geometry Data Structure.
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

#include "map/mesh.h"

namespace de {

DENG2_PIMPL(Mesh)
{
    /// All vertexes in the mesh.
    //Vertexes vertexes;

    /// All half-edges in the mesh.
    HEdges hedges;

    /// All faces in the mesh.
    Faces faces;

    Instance(Public *i)
        : Base(i)
    {}

    ~Instance()
    {
        //qDeleteAll(vertexes);
        qDeleteAll(faces);
        qDeleteAll(hedges);
    }
};

Mesh::Mesh()
    : d(new Instance(this))
{}

/*Vertex *Mesh::newVertex()
{
    Vertex *vtx = new Vertex();
    d->vertexes.insert(vtx);
    return vtx;
}*/

HEdge *Mesh::newHEdge(Vertex &vertex)
{
    HEdge *hedge = new HEdge(*this, vertex);
    d->hedges.insert(hedge);
    return hedge;
}

Face *Mesh::newFace()
{
    Face *face = new Face(*this);
    d->faces.insert(face);
    return face;
}

/*Mesh::Vertexes const &Mesh::vertexes() const
{
    return d->vertexes;
}*/

Mesh::Faces const &Mesh::faces() const
{
    return d->faces;
}

Mesh::HEdges const &Mesh::hedges() const
{
    return d->hedges;
}

Face *Mesh::firstFace() const
{
    return *d->faces.begin();
}

} // namespace de
