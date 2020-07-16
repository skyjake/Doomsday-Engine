/** @file subsector.cpp  World map subsector.
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

#include "doomsday/world/subsector.h"

#include "doomsday/world/map.h"
#include "doomsday/world/bspleaf.h"
#include "doomsday/world/convexsubspace.h"
#include "doomsday/world/plane.h"
#include "doomsday/world/surface.h"
#include "doomsday/mesh/face.h"

#include <de/legacy/aabox.h>
#include <de/legacy/vector1.h>
#include <de/rectangle.h>

using namespace de;

namespace world {

DE_PIMPL_NOREF(Subsector)
{
    List<world::ConvexSubspace *> subspaces;
    std::unique_ptr<AABoxd> bounds;
    Id id;
    Sector *sector = nullptr;

    /**
     * Calculate the minimum bounding rectangle containing all the subspace geometries.
     */
    AABoxd findBounds() const
    {
        bool inited = false;
        AABoxd bounds;
        for (const auto *subspace : subspaces)
        {
            const AABoxd &subBounds = subspace->poly().bounds();
            if (inited)
            {
                V2d_UniteBox(bounds.arvec2, subBounds.arvec2);
            }
            else
            {
                bounds = subBounds;
                inited = true;
            }
        }
        return bounds;
    }
};

Subsector::Subsector(const List<world::ConvexSubspace *> &subspaces) : d(new Impl)
{
    d->subspaces += subspaces;
    for (auto *subspace : subspaces)
    {
        // Attribute the subspace to the subsector.
        subspace->setSubsector(this);
    }
}

Subsector::~Subsector()
{
    DE_NOTIFY_VAR(Deletion, i) i->subsectorBeingDeleted(*this);
}

String Subsector::description() const
{
    auto desc = Stringf(_E(l) "Id: " _E(.) _E(i) "%s" _E(.) " " _E(l) "Sector: " _E(.)
                            _E(i) "%i" _E(.) " " _E(l) "Bounds: " _E(.) _E(i) "%s" _E(.),
                        d->id.asText().c_str(),
                        sector().indexInMap(),
                        Rectangled(Vec2d(bounds().min), Vec2d(bounds().max)).asText().c_str());

    DE_DEBUG_ONLY(
        desc.prepend(Stringf(_E(b) "Subsector " _E(.) "[%p]\n", this));
    )
    return desc;
}

Id Subsector::id() const
{
    return d->id;
}

Sector &Subsector::sector()
{
    if (!d->sector)
    {
        d->sector = firstSubspace().bspLeaf().sectorPtr();
        DE_ASSERT(d->sector);
    }
    return *d->sector;
}

const Sector &Subsector::sector() const
{
    if (!d->sector)
    {
        d->sector = firstSubspace().bspLeaf().sectorPtr();
        DE_ASSERT(d->sector);
    }
    return *d->sector;
}

dint Subsector::subspaceCount() const
{
    return d->subspaces.count();
}

ConvexSubspace &Subsector::firstSubspace() const
{
    DE_ASSERT(!d->subspaces.isEmpty());
    return *d.getConst()->subspaces.first();
}

LoopResult Subsector::forAllSubspaces(const std::function<LoopResult (ConvexSubspace &)>& func) const
{
    for (ConvexSubspace *sub : d->subspaces)
    {
        if (auto result = func(*sub)) return result;
    }
    return LoopContinue;
}

List<mesh::HEdge *> Subsector::listUniqueBoundaryEdges() const
{
    List<mesh::HEdge *> list;
    for (const ConvexSubspace *sub : d->subspaces)
    {
        auto *hedge = sub->poly().hedge();
        do
        {
            if (hedge->hasMapElement() && !isInternalEdge(hedge))
            {
                bool unique = true;
                for (auto *base : list)
                {
                    SubsectorCirculator subIt(base);
                    do
                    {
                        auto &other = *subIt;
                        if (&other == hedge)
                        {
                            unique = false;
                            break;
                        }
                    } while (&subIt.next() != base);
                }
                if (unique)
                {
                    list << hedge;
                }
            }
        } while ((hedge = &hedge->next()) != sub->poly().hedge());
    }
    return list;
}

const AABoxd &Subsector::bounds() const
{
    // If the subsector is comprised of a single subspace we can use the bounding
    // box of the subspace geometry directly.
    if (subspaceCount() == 1)
    {
        return firstSubspace().poly().bounds();
    }

    // Time to determine bounds?
    if (!d->bounds)
    {
        d->bounds.reset(new AABoxd(d->findBounds()));
    }

    return *d->bounds;
}

Vec2d Subsector::center() const
{
    return (Vec2d(bounds().min) + Vec2d(bounds().max)) / 2;
}

ddouble Subsector::roughArea() const
{
    const AABoxd &box = bounds();
    return (box.maxX - box.minX) * (box.maxY - box.minY);
}

bool Subsector::isInternalEdge(mesh::HEdge *hedge) // static
{
    if (!hedge) return false;
    if (!hedge->hasFace() || !hedge->twin().hasFace()) return false;
    if (!hedge->face().hasMapElement() || hedge->face().mapElement().type() != DMU_SUBSPACE) return false;
    if (!hedge->twin().face().hasMapElement() || hedge->twin().face().mapElement().type() != DMU_SUBSPACE) return false;

    Subsector *frontSubsector = hedge->face().mapElementAs<ConvexSubspace>().subsectorPtr();
    if (!frontSubsector) return false;
    return frontSubsector == hedge->twin().face().mapElementAs<ConvexSubspace>().subsectorPtr();
}

//- SubsectorCirculator -----------------------------------------------------------------

#if 0
Subsector *SubsectorCirculator::getSubsector(const HEdge &hedge) // static
{
    if (!hedge.hasFace()) return nullptr;
    if (!hedge.face().hasMapElement()) return nullptr;
    if (hedge.face().mapElement().type() != DMU_SUBSPACE) return nullptr;
    return hedge.face().mapElementAs<ConvexSubspace>().subsectorPtr();
}
#endif

mesh::HEdge &SubsectorCirculator::getNeighbor(const mesh::HEdge &hedge,
                                              ClockDirection     direction,
                                              const Subsector *  subsec) // static
{
    auto *neighbor = &hedge.neighbor(direction);
    // Skip over interior edges.
    if (subsec)
    {
        while (neighbor->hasTwin() && subsec == neighbor->twin().subsector())
        {
            neighbor = &neighbor->twin().neighbor(direction);
        }
    }
    return *neighbor;
}

} // namespace world
