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

#include "BspLeaf"
#include "ConvexSubspace"
#include "Plane"
#include "Surface"
#include "Face"

#include <de/aabox.h>
#include <de/vector1.h>

using namespace de;

namespace world {

DENG2_PIMPL_NOREF(Subsector)
{
    QList<ConvexSubspace *> subspaces;
    std::unique_ptr<AABoxd> aaBox;
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

Plane &Subsector::plane(dint planeIndex)
{
    return sector().plane(planeIndex);
}

Plane const &Subsector::plane(dint planeIndex) const
{
    return sector().plane(planeIndex);
}

AABoxd const &Subsector::aaBox() const
{
    // If the subsector is comprised of a single subspace we can use the bounding
    // box of the subspace geometry directly.
    if (d->subspaces.count() == 1)
    {
        return d->subspaces.first()->poly().aaBox();
    }

    // Time to determine bounds?
    if (!d->aaBox)
    {
        // Unite the geometry bounding boxes of all subspaces in the subsector.
        for (ConvexSubspace const *subspace : d->subspaces)
        {
            AABoxd const &leafAABox = subspace->poly().aaBox();
            if (d->aaBox)
            {
                V2d_UniteBox((*d->aaBox).arvec2, leafAABox.arvec2);
            }
            else
            {
                d->aaBox.reset(new AABoxd(leafAABox));
            }
        }
    }

    return *d->aaBox;
}

ddouble Subsector::roughArea() const
{
    AABoxd const &bounds = aaBox();
    return (bounds.maxX - bounds.minX) * (bounds.maxY - bounds.minY);
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
