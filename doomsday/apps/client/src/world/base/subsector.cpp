/** @file subsector.cpp  World map subsector.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/subsector.h"

#include "world/map.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "Plane"
#include "Surface"
#include "Face"

#include "dd_main.h"  // App_World()

#include <de/aabox.h>
#include <de/vector1.h>

using namespace de;

namespace world {

DENG2_PIMPL_NOREF(Subsector)
{
    QList<ConvexSubspace *> subspaces;
    std::unique_ptr<AABoxd> bounds;
    Id id;

    /**
     * Calculate the minimum bounding rectangle containing all the subspace geometries.
     */
    AABoxd findBounds() const
    {
        bool inited = false;
        AABoxd bounds;
        for (ConvexSubspace const *subspace : subspaces)
        {
            AABoxd const &subBounds = subspace->poly().bounds();
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

Subsector::Subsector(QList<ConvexSubspace *> const &subspaces) : d(new Impl)
{
    d->subspaces.append(subspaces);
    for (ConvexSubspace *subspace : subspaces)
    {
        // Attribute the subspace to the subsector.
        subspace->setSubsector(this);
    }
}

Subsector::~Subsector()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->subsectorBeingDeleted(*this);
}

String Subsector::description() const
{
    auto desc = String(    _E(l) "Id: "     _E(.) _E(i) "%1" _E(.)
                       " " _E(l) "Sector: " _E(.) _E(i) "%2" _E(.)
                       " " _E(l) "Bounds: " _E(.) _E(i) "%3" _E(.))
                    .arg(d->id.asText())
                    .arg(sector().indexInMap())
                    .arg((Vector2d(bounds().max) - Vector2d(bounds().min)).asText());

    DENG2_DEBUG_ONLY(
        desc.prepend(String("[Subsector 0x%1]\n").arg(de::dintptr(this), 0, 16));
    )
    return desc;
}

Id Subsector::id() const
{
    return d->id;
}

Sector &Subsector::sector()
{
    DENG2_ASSERT(!d->subspaces.isEmpty());
    return *d->subspaces.first()->bspLeaf().sectorPtr();
}

Sector const &Subsector::sector() const
{
    DENG2_ASSERT(!d->subspaces.isEmpty());
    return *d->subspaces.first()->bspLeaf().sectorPtr();
}

dint Subsector::subspaceCount() const
{
    return d->subspaces.count();
}

LoopResult Subsector::forAllSubspaces(std::function<LoopResult (ConvexSubspace &)> func) const
{
    for (ConvexSubspace *sub : d->subspaces)
    {
        if (auto result = func(*sub)) return result;
    }
    return LoopContinue;
}

AABoxd const &Subsector::bounds() const
{
    // If the subsector is comprised of a single subspace we can use the bounding
    // box of the subspace geometry directly.
    if (d->subspaces.count() == 1)
    {
        return d->subspaces.first()->poly().bounds();
    }

    // Time to determine bounds?
    if (!d->bounds)
    {
        d->bounds.reset(new AABoxd(d->findBounds()));
    }

    return *d->bounds;
}

Vector2d Subsector::center() const
{
    return (Vector2d(bounds().min) + Vector2d(bounds().max)) / 2;
}

ddouble Subsector::roughArea() const
{
    AABoxd const &box = bounds();
    return (box.maxX - box.minX) * (box.maxY - box.minY);
}

bool Subsector::isInternalEdge(HEdge *hedge) // static
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

Subsector *SubsectorCirculator::getSubsector(HEdge const &hedge) // static
{
    if (!hedge.hasFace()) return nullptr;
    if (!hedge.face().hasMapElement()) return nullptr;
    if (hedge.face().mapElement().type() != DMU_SUBSPACE) return nullptr;
    return hedge.face().mapElementAs<ConvexSubspace>().subsectorPtr();
}

HEdge &SubsectorCirculator::getNeighbor(HEdge const &hedge, ClockDirection direction,
    Subsector const *subsec) // static
{
    HEdge *neighbor = &hedge.neighbor(direction);
    // Skip over interior edges.
    if (subsec)
    {
        while (neighbor->hasTwin() && subsec == getSubsector(neighbor->twin()))
        {
            neighbor = &neighbor->twin().neighbor(direction);
        }
    }
    return *neighbor;
}

} // namespace world
