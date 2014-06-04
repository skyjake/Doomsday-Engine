/** @file convexsubspace.cpp  World map convex subspace.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "world/convexsubspace.h"
#include "Bspleaf"
#include "Face"
#include "Polyobj"
#include "SectorCluster"
#include "Surface"
#include <de/Log>
#include <QtAlgorithms>

using namespace de;

DENG2_PIMPL(ConvexSubspace)
{
    Face &poly;                ///< Convex polygon geometry (not owned).
    Meshes extraMeshes;        ///< Additional meshes (owned).
    Polyobjs polyobjs;         ///< Linked polyobjs (if any, not owned).
    SectorCluster *cluster;    ///< Attributed cluster (if any, not owned).
    int validCount;            ///< Used to prevent repeated processing.
    Vector2d worldGridOffset;  ///< For aligning the materials to the map space grid.
    BspLeaf *bspLeaf;

    Instance(Public *i, Face &poly)
        : Base(i)
        , poly      (poly)
        , cluster   (0)
        , validCount(0)
        , bspLeaf   (0)
    {}

    ~Instance() { qDeleteAll(extraMeshes); }
};

ConvexSubspace::ConvexSubspace(Face &convexPolygon, BspLeaf *bspLeaf)
    : MapElement(DMU_SUBSPACE)
    , d(new Instance(this, convexPolygon))
{
    // Determine the world grid offset.
    d->worldGridOffset = Vector2d(fmod(d->poly.aaBox().minX, 64),
                                  fmod(d->poly.aaBox().maxY, 64));

    d->poly.setMapElement(this);
    setBspLeaf(bspLeaf);
}

ConvexSubspace *ConvexSubspace::newFromConvexPoly(de::Face &poly, BspLeaf *bspLeaf) // static
{
    if(!poly.isConvex())
    {
        /// @throw InvalidPolyError Attempted to attribute a non-convex polygon.
        throw InvalidPolyError("ConvexSubspace::newFromConvexPoly", "Source is non-convex");
    }
    return new ConvexSubspace(poly, bspLeaf);
}

BspLeaf &ConvexSubspace::bspLeaf() const
{
    DENG2_ASSERT(d->bspLeaf != 0);
    return *d->bspLeaf;
}

void ConvexSubspace::setBspLeaf(BspLeaf *newBspLeaf)
{
    d->bspLeaf = newBspLeaf;
}

Face &ConvexSubspace::poly() const
{
    return d->poly;
}

bool ConvexSubspace::contains(Vector2d const &point) const
{
    HEdge const *hedge = poly().hedge();
    do
    {
        Vertex const &va = hedge->vertex();
        Vertex const &vb = hedge->next().vertex();

        if(((va.origin().y - point.y) * (vb.origin().x - va.origin().x) -
            (va.origin().x - point.x) * (vb.origin().y - va.origin().y)) < 0)
        {
            // Outside the BSP leaf's edges.
            return false;
        }

    } while((hedge = &hedge->next()) != poly().hedge());

    return true;
}

void ConvexSubspace::assignExtraMesh(Mesh &newMesh)
{
    LOG_AS("ConvexSubspace");

    int const sizeBefore = d->extraMeshes.size();

    d->extraMeshes.insert(&newMesh);

    if(d->extraMeshes.size() != sizeBefore)
    {
        LOG_DEBUG("Assigned extra mesh to subspace %p") << this;

        // Attribute all faces to "this" subspace.
        foreach(Face *face, newMesh.faces())
        {
            face->setMapElement(this);
        }
    }
}

ConvexSubspace::Meshes const &ConvexSubspace::extraMeshes() const
{
    return d->extraMeshes;
}

void ConvexSubspace::link(Polyobj const &polyobj)
{
    d->polyobjs.insert(const_cast<Polyobj *>(&polyobj));
}

bool ConvexSubspace::unlink(polyobj_s const &polyobj)
{
    int sizeBefore = d->polyobjs.size();
    d->polyobjs.remove(const_cast<Polyobj *>(&polyobj));
    return d->polyobjs.size() != sizeBefore;
}

ConvexSubspace::Polyobjs const &ConvexSubspace::polyobjs() const
{
    return d->polyobjs;
}

Vector2d const &ConvexSubspace::worldGridOffset() const
{
    return d->worldGridOffset;
}

bool ConvexSubspace::hasCluster() const
{
    return d->cluster != 0;
}

SectorCluster &ConvexSubspace::cluster() const
{
    if(d->cluster)
    {
        return *d->cluster;
    }
    /// @throw MissingClusterError Attempted with no sector cluster attributed.
    throw MissingClusterError("ConvexSubspace::cluster", "No sector cluster is attributed");
}

void ConvexSubspace::setCluster(SectorCluster *newCluster)
{
    d->cluster = newCluster;
}

int ConvexSubspace::validCount() const
{
    return d->validCount;
}

void ConvexSubspace::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}
