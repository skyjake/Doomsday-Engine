/** @file convexsubspace.cpp  Map convex subspace.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/convexsubspace.h"
#include "doomsday/world/bspleaf.h"
#include "doomsday/world/subsector.h"
#include "doomsday/world/polyobj.h"
#include "doomsday/world/surface.h"
#include "doomsday/world/factory.h"
#include "doomsday/mesh/face.h"
#include "doomsday/mesh/hedge.h"

#include <de/log.h>
#include <de/set.h>

using namespace de;

namespace world {

DE_PIMPL(ConvexSubspace)
{
    mesh::Face *poly = nullptr;            // Convex polygon geometry (not owned).

    typedef Set<mesh::Mesh *> Meshes;
    Meshes extraMeshes;                    // Additional meshes (owned).

    typedef Set<polyobj_s *> Polyobjs;
    Polyobjs polyobjs;                     // Linked polyobjs (not owned).

    dint validCount = 0;                   // Used to prevent repeated processing.

    Impl(Public * i)
        : Base(i)
    {}

    ~Impl() { deleteAll(extraMeshes); }
};

ConvexSubspace::ConvexSubspace(mesh::Face &convexPolygon, BspLeaf *bspLeaf)
    : MapElement(DMU_SUBSPACE)
    , d(new Impl(this))
{
    d->poly = &convexPolygon;
    poly().setMapElement(this);
    setBspLeaf(bspLeaf);
}

ConvexSubspace *ConvexSubspace::newFromConvexPoly(mesh::Face &poly, BspLeaf *bspLeaf) // static
{
    if (!poly.isConvex())
    {
        /// @throw InvalidPolyError  Attempted to attribute a non-convex polygon.
        throw InvalidPolyError("ConvexSubspace::newFromConvexPoly", "Source is non-convex");
    }
    return Factory::newConvexSubspace(poly, bspLeaf);
}

void ConvexSubspace::setBspLeaf(BspLeaf *newBspLeaf)
{
    _bspLeaf = newBspLeaf;
}

mesh::Face &ConvexSubspace::poly() const
{
    DE_ASSERT(d->poly);
    return *d->poly;
}

bool ConvexSubspace::contains(const Vec2d &point) const
{
    const mesh::HEdge *hedge = poly().hedge();
    do
    {
        const world::Vertex &va = hedge->vertex();
        const world::Vertex &vb = hedge->next().vertex();

        if(((va.origin().y - point.y) * (vb.origin().x - va.origin().x) -
            (va.origin().x - point.x) * (vb.origin().y - va.origin().y)) < 0)
        {
            // Outside the BSP leaf's edges.
            return false;
        }

    } while((hedge = &hedge->next()) != poly().hedge());

    return true;
}

void ConvexSubspace::assignExtraMesh(mesh::Mesh &newMesh)
{
    LOG_AS("ConvexSubspace");

    const dint sizeBefore = d->extraMeshes.size();

    d->extraMeshes.insert(&newMesh);

    if (d->extraMeshes.size() != sizeBefore)
    {
        LOG_DEBUG("Assigned extra mesh to subspace %p") << this;

        // Attribute all faces to "this" subspace.
        for (auto*face : newMesh.faces())
        {
            face->setMapElement(this);
        }
    }
}

LoopResult ConvexSubspace::forAllExtraMeshes(const std::function<LoopResult (mesh::Mesh &)>& func) const
{
    for (auto *mesh : d->extraMeshes)
    {
        if (auto result = func(*mesh)) return result;
    }
    return LoopContinue;
}

dint ConvexSubspace::polyobjCount() const
{
    return d->polyobjs.size();
}

LoopResult ConvexSubspace::forAllPolyobjs(const std::function<LoopResult (Polyobj &)>& func) const
{
    for(Polyobj *pob : d->polyobjs)
    {
        if(auto result = func(*pob)) return result;
    }
    return LoopContinue;
}

void ConvexSubspace::link(const Polyobj &polyobj)
{
    d->polyobjs.insert(const_cast<Polyobj *>(&polyobj));
}

bool ConvexSubspace::unlink(const Polyobj &polyobj)
{
    dint sizeBefore = d->polyobjs.size();
    d->polyobjs.remove(const_cast<Polyobj *>(&polyobj));
    return d->polyobjs.size() != sizeBefore;
}

/*Subsector &ConvexSubspace::subsector() const
{
    if(d->subsector) return *d->subsector;
    /// @throw MissingSubsectorError  Attempted with no subsector attributed.
    throw MissingSubsectorError("ConvexSubspace::subsector", "No subsector is attributed");
}

Subsector *ConvexSubspace::subsectorPtr() const
{
    return hasSubsector() ? &subsector() : nullptr;
}*/

void ConvexSubspace::setSubsector(Subsector *newSubsector)
{
    _subsector = newSubsector;
}

dint ConvexSubspace::validCount() const
{
    return d->validCount;
}

void ConvexSubspace::setValidCount(dint newValidCount)
{
    d->validCount = newValidCount;
}

} // namespace world
