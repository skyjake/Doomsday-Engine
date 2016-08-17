/** @file cledgeloop.cpp  Client-side world map subsector boundary loop.
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

#include "client/cledgeloop.h"

#include "world/maputil.h"
#include "world/surface.h"

#include "misc/face.h"
#include "dd_main.h"  // verbose

#include <doomsday/world/Material>
#include <doomsday/world/Materials>
#include <doomsday/world/detailtexturemateriallayer.h>
#include <doomsday/world/shinetexturemateriallayer.h>
#include <de/Log>

using namespace de;

namespace world {

DENG2_PIMPL_NOREF(ClEdgeLoop)
{
    ClientSubsector &owner;
    HEdge *firstHEdge = nullptr;
    bool isInner = false;

    Impl(ClientSubsector &owner) : owner(owner)
    {}

    static bool materialHasAnimatedTextureLayers(Material const &mat)
    {
        for (dint i = 0; i < mat.layerCount(); ++i)
        {
            MaterialLayer const &layer = mat.layer(i);
            if (!layer.is<DetailTextureMaterialLayer>() && !layer.is<ShineTextureMaterialLayer>())
            {
                if(layer.isAnimated()) return true;
            }
        }
        return false;
    }

    /**
     * Given a side section, look at the neighboring surfaces and pick the best choice of
     * material used on those surfaces to be applied to "this" surface.
     *
     * Material on back neighbor plane has priority. Non-animated materials are preferred.
     * Sky materials are ignored.
     */
    static Material *chooseFixMaterial(LineSide &side, dint section)
    {
        Material *choice1 = nullptr, *choice2 = nullptr;

        Sector *frontSec = side.sectorPtr();
        Sector *backSec  = side.back().sectorPtr();

        if (backSec)
        {
            // Our first choice is a material in the other sector.
            if (section == LineSide::Bottom)
            {
                if (frontSec->floor().height() < backSec->floor().height())
                {
                    choice1 = backSec->floor().surface().materialPtr();
                }
            }
            else if (section == LineSide::Top)
            {
                if (frontSec->ceiling().height() > backSec->ceiling().height())
                {
                    choice1 = backSec->ceiling().surface().materialPtr();
                }
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
        choice2 = frontSec->plane(section == LineSide::Bottom ? Sector::Floor : Sector::Ceiling)
            .surface().materialPtr();

        // Prefer a non-animated, non-masked material.
        if (choice1 && !materialHasAnimatedTextureLayers(*choice1) && !choice1->isSkyMasked())
            return choice1;
        if (choice2 && !materialHasAnimatedTextureLayers(*choice2) && !choice2->isSkyMasked())
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

    void fixMissingMaterial(LineSide &side, dint section)
    {
        // Sides without sections need no fixing.
        if (!side.hasSections()) return;
        // ...nor those of self-referencing lines.
        if (side.line().isSelfReferencing()) return;
        // ...nor those of "one-way window" lines.
        if (!side.back().hasSections() && side.back().hasSector()) return;

        // A material must actually be missing to qualify for fixing.
        Surface &surface = side.surface(section);
        if (surface.hasMaterial() && !surface.hasFixMaterial())
            return;

        Material *oldMaterial = surface.materialPtr();

        // Look for and apply a suitable replacement (if found).
        surface.setMaterial(chooseFixMaterial(side, section), true/* is missing fix */);

        if (oldMaterial == surface.materialPtr())
            return;

        // We'll need to recalculate reverb.
        if (HEdge *hedge = side.leftHEdge())
        {
            if (hedge->hasFace() && hedge->face().hasMapElement())
            {
                auto &subsec = hedge->face().mapElementAs<ConvexSubspace>()
                    .subsector().as<ClientSubsector>();
                subsec.markReverbDirty();
                subsec.markVisPlanesDirty();
            }
        }

        // During map setup we log missing materials.
        if (::ddMapSetup && ::verbose)
        {
            String const surfaceMaterialPath = surface.hasMaterial() ? surface.material().manifest().composeUri().asText() : "<null>";

            LOG_WARNING(  "%s of Line #%d is missing a material for the %s section."
                        "\n  %s was chosen to complete the definition.")
                << Line::sideIdAsText(side.sideId()).upperFirstChar()
                << side.line().indexInMap()
                << LineSide::sectionIdAsText(section)
                << surfaceMaterialPath;
        }
    }
};

ClEdgeLoop::ClEdgeLoop(ClientSubsector &owner, HEdge &first, dint loopId)
    : d(new Impl(owner))
{
    d->firstHEdge = &first;
    d->isInner    = loopId == ClientSubsector::InnerLoop;
}

ClientSubsector &ClEdgeLoop::owner() const
{
    return d->owner;
}

String ClEdgeLoop::description() const
{
    auto desc = String(    _E(l) "Loop: "       _E(.)_E(i) "%1" _E(.)
                       " " _E(l) "Half edge: "  _E(.)_E(i) "%2" _E(.))
                  .arg(ClientSubsector::edgeLoopIdAsText(loopId()).upperFirstChar())
                  .arg(String("[0x%1]").arg(de::dintptr(&firstHEdge()), 0, 16));

    DENG2_DEBUG_ONLY(
        desc.prepend(String(_E(b) "ClEdgeLoop " _E(.) "[0x%1]\n").arg(de::dintptr(this), 0, 16));
    )
    return desc;
}

dint ClEdgeLoop::loopId() const
{
    return d->isInner ? ClientSubsector::InnerLoop : ClientSubsector::OuterLoop;
}

HEdge &ClEdgeLoop::firstHEdge() const
{
    DENG2_ASSERT(d->firstHEdge);
    return *d->firstHEdge;
}

bool ClEdgeLoop::isSelfReferencing() const
{
    return firstHEdge().mapElementAs<LineSideSegment>().line().isSelfReferencing();
}

bool ClEdgeLoop::hasBackSubsector() const
{
    return firstHEdge().hasTwin()
        && firstHEdge().twin().hasFace()
        && firstHEdge().twin().face().mapElementAs<ConvexSubspace>().hasSubsector();
}

ClientSubsector &ClEdgeLoop::backSubsector() const
{
    return firstHEdge().twin().face().mapElementAs<ConvexSubspace>().subsector().as<ClientSubsector>();
}

/**
 * @todo Optimize: Process only the mapping-affected surfaces -ds
 */
void ClEdgeLoop::fixSurfacesMissingMaterials()
{
    ClientSubsector const &frontSubsec = owner();

    SubsectorCirculator it(d->firstHEdge);
    do
    {
        if (it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
        {
            LineSide &side = it->mapElementAs<LineSideSegment>().lineSide();
            if (hasBackSubsector())
            {
                auto const &backSubsec = backSubsector().as<ClientSubsector>();

                // A potential bottom section fix?
                if (!(frontSubsec.hasSkyFloor() && backSubsec.hasSkyFloor()))
                {
                    if (frontSubsec.visFloor().height() < backSubsec.visFloor().height())
                    {
                        d->fixMissingMaterial(side, LineSide::Bottom);
                    }
                    else if (side.bottom().hasFixMaterial())
                    {
                        side.bottom().setMaterial(0);
                    }
                }

                // A potential top section fix?
                if (!(frontSubsec.hasSkyCeiling() && backSubsec.hasSkyCeiling()))
                {
                    if (frontSubsec.visCeiling().height() > backSubsec.visCeiling().height())
                    {
                        d->fixMissingMaterial(side, LineSide::Top);
                    }
                    else if (side.top().hasFixMaterial())
                    {
                        side.top().setMaterial(0);
                    }
                }
            }
            else if (!side.back().hasSector())
            {
                // A potential middle section fix.
                d->fixMissingMaterial(side, LineSide::Middle);
            }
        }
    } while (&it.next() != d->firstHEdge);
}

} // namespace world
