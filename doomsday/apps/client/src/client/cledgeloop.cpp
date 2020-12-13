/** @file cledgeloop.cpp  Client-side world map subsector boundary loop.
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

#include "client/cledgeloop.h"

#include "world/maputil.h"
#include "world/surface.h"
#include "world/convexsubspace.h"

#include "misc/face.h"

#include <doomsday/world/Material>
#include <doomsday/world/Materials>
#include <de/Log>

using namespace de;

namespace world {

DENG2_PIMPL(ClEdgeLoop)
{
    ClientSubsector &owner;
    HEdge *firstHEdge = nullptr;
    dint loopId = 0;

    Impl(Public *i, ClientSubsector &owner)
        : Base(i)
        , owner(owner)
    {}

    /**
     * Look at the neighboring surfaces and pick the best choice of
     * material used on those surfaces to be applied to "this" surface.
     *
     * Material on back neighbor plane has priority. Non-animated materials are preferred.
     * Sky-masked materials are ignored.
     */
    Material *chooseFixMaterial(HEdge &hedge, dint section) const
    {
        Material *choice1 = nullptr, *choice2 = nullptr;

        if (self().hasBackSubsector())
        {
            ClientSubsector &backSubsec = self().backSubsector();

            // Our first choice is the back subsector material in the back subsector.
            switch (section)
            {
            case LineSide::Bottom:
                if (owner.visFloor().height() < backSubsec.visFloor().height())
                {
                    choice1 = backSubsec.visFloor().surface().materialPtr();
                }
                break;

            case LineSide::Top:
                if (owner.visCeiling().height() > backSubsec.visCeiling().height())
                {
                    choice1 = backSubsec.visCeiling().surface().materialPtr();
                }
                break;

            default: break;
            }

            // In the special case of sky mask on the back plane, our best
            // choice is always this material.
            if (choice1 && choice1->isSkyMasked())
            {
                return choice1;
            }
        }
        else
        {
            Sector *frontSec = &owner.sector();
            LineSide &side = hedge.mapElementAs<LineSideSegment>().lineSide();

            // Our first choice is a material on an adjacent wall section.
            // Try the left neighbor first.
            Line *other = R_FindLineNeighbor(side.line(), *side.line().vertexOwner(side.sideId()),
                                             Clockwise, frontSec);
            if (!other)
            {
                // Try the right neighbor.
                other = R_FindLineNeighbor(side.line(), *side.line().vertexOwner(side.sideId()^1),
                                           Anticlockwise, frontSec);
            }

            if (other)
            {
                if (!other->back().hasSector())
                {
                    // Our choice is clear - the middle material.
                    choice1 = other->front().middle().materialPtr();
                }
                else
                {
                    // Compare the relative heights to decide.
                    LineSide &otherSide = other->side(&other->front().sector() == frontSec ? Line::Front : Line::Back);
                    Sector &otherSec    = other->side(&other->front().sector() == frontSec ? Line::Back  : Line::Front).sector();

                    if (otherSec.ceiling().height() <= frontSec->floor().height())
                        choice1 = otherSide.top().materialPtr();
                    else if (otherSec.floor().height() >= frontSec->ceiling().height())
                        choice1 = otherSide.bottom().materialPtr();
                    else if (otherSec.ceiling().height() < frontSec->ceiling().height())
                        choice1 = otherSide.top().materialPtr();
                    else if (otherSec.floor().height() > frontSec->floor().height())
                        choice1 = otherSide.bottom().materialPtr();
                    // else we'll settle for a plane material.
                }
            }
        }

        // Our second choice is a material from this sector.
        choice2 = owner.visPlane(section == LineSide::Bottom ? Sector::Floor : Sector::Ceiling)
                           .surface().materialPtr();

        // Prefer a non-animated, non-masked material.
        if (choice1 && !choice1->hasAnimatedTextureLayers() && !choice1->isSkyMasked())
            return choice1;
        if (choice2 && !choice2->hasAnimatedTextureLayers() && !choice2->isSkyMasked())
            return choice2;

        // Prefer a non-masked material.
        if (choice1 && !choice1->isSkyMasked())
            return choice1;
        if (choice2 && !choice2->isSkyMasked())
            return choice2;

        // At this point we'll accept anything if it means avoiding HOM.
        if (choice1) return choice1;
        if (choice2) return choice2;

        // We'll assign the special "missing" material...
        return &Materials::get().material(de::Uri("System", Path("missing")));
    }

    void fixMissingMaterial(HEdge &hedge, dint section) const
    {
        // Sides without sections need no fixing.
        LineSide &side = hedge.mapElementAs<LineSideSegment>().lineSide();
        if (!side.hasSections()) return;
        // ...nor those of self-referencing lines.
        if (side.line().isSelfReferencing()) return;
        // ...nor those of "one-way window" lines.
        if (!side.back().hasSections() && side.back().hasSector()) return;

        // A material must actually be missing to qualify for fixing.
        Surface &surface = side.surface(section);
        if (!surface.hasMaterial() || surface.hasFixMaterial())
        {
            Material *oldMaterial = surface.materialPtr();

            // Look for and apply a suitable replacement (if found).
            surface.setMaterial(chooseFixMaterial(hedge, section), true/* is missing fix */);

            if (surface.materialPtr() != oldMaterial)
            {
                // We'll need to recalculate reverb.
                /// @todo Use an observer based mechanism in ClientSubsector -ds
                owner.markReverbDirty();
                //owner.markVisPlanesDirty();
            }
        }
    }
};

ClEdgeLoop::ClEdgeLoop(ClientSubsector &owner, HEdge &first, dint loopId)
    : d(new Impl(this, owner))
{
    d->firstHEdge = &first;
    d->loopId     = loopId;
}

ClientSubsector &ClEdgeLoop::owner() const
{
    return d->owner;
}

String ClEdgeLoop::description() const
{
    auto desc = String(    _E(l) "Loop: "       _E(.)_E(i) "%1" _E(.)
                       " " _E(l) "Half-edge: "  _E(.)_E(i) "%2" _E(.))
                  .arg(ClientSubsector::edgeLoopIdAsText(loopId()).upperFirstChar())
                  .arg(String("[0x%1]").arg(de::dintptr(d->firstHEdge), 0, 16));
    DENG2_DEBUG_ONLY(
        desc.prepend(String(_E(b) "ClEdgeLoop " _E(.) "[0x%1]\n").arg(de::dintptr(this), 0, 16));
    )
    return desc;
}

dint ClEdgeLoop::loopId() const
{
    return d->loopId;
}

bool ClEdgeLoop::isInner() const
{
    return d->loopId == ClientSubsector::InnerLoop;
}

bool ClEdgeLoop::isOuter() const
{
    return d->loopId == ClientSubsector::OuterLoop;
}

HEdge &ClEdgeLoop::first() const
{
    DENG2_ASSERT(d->firstHEdge);
    return *d->firstHEdge;
}

bool ClEdgeLoop::isSelfReferencing() const
{
    DENG2_ASSERT(d->firstHEdge);
    return d->firstHEdge->mapElementAs<LineSideSegment>().line().isSelfReferencing();
}

bool ClEdgeLoop::hasBackSubsector() const
{
    DENG2_ASSERT(d->firstHEdge);
    return d->firstHEdge->hasTwin()
        && d->firstHEdge->twin().hasFace()
        && d->firstHEdge->twin().face().mapElementAs<ConvexSubspace>().hasSubsector();
}

ClientSubsector &ClEdgeLoop::backSubsector() const
{
    DENG2_ASSERT(d->firstHEdge);
    return d->firstHEdge->twin().face().mapElementAs<ConvexSubspace>().subsector().as<ClientSubsector>();
}

void ClEdgeLoop::fixSurfacesMissingMaterials()
{
    DENG2_ASSERT(d->firstHEdge);
    SubsectorCirculator it(d->firstHEdge);
    do
    {
        if (it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
        {
            LineSide &lineSide = it->mapElementAs<LineSideSegment>().lineSide();
            if (lineSide.hasSections()) // Not a "one-way window" -ds
            {
                if (hasBackSubsector())
                {
                    auto const &backSubsec = backSubsector().as<ClientSubsector>();

                    // Potential bottom section fix?
                    if (!d->owner.hasSkyFloor() && !backSubsec.hasSkyFloor())
                    {
                        if (d->owner.visFloor().height() < backSubsec.visFloor().height())
                        {
                            d->fixMissingMaterial(*it, LineSide::Bottom);
                        }
                        else if (lineSide.bottom().hasFixMaterial())
                        {
                            lineSide.bottom().setMaterial(nullptr);
                        }
                    }

                    // Potential top section fix?
                    if (!d->owner.hasSkyCeiling() && !backSubsec.hasSkyCeiling())
                    {
                        if (d->owner.visCeiling().height() > backSubsec.visCeiling().height())
                        {
                            d->fixMissingMaterial(*it, LineSide::Top);
                        }
                        else if (lineSide.top().hasFixMaterial())
                        {
                            lineSide.top().setMaterial(nullptr);
                        }
                    }
                }
                // Potential middle section fix?
                else if (!lineSide.back().hasSector())
                {
                    d->fixMissingMaterial(*it, LineSide::Middle);
                }
            }
        }
    } while (&it.next() != d->firstHEdge);
}

} // namespace world
