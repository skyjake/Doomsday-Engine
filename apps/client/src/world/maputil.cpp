/** @file maputil.cpp  World map utilities.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/maputil.h"
#include "world/line.h"
#include "world/plane.h"
#include "world/surface.h"
#include "resource/materialvariantspec.h"
#include "resource/materialanimator.h"
#include "render/walledge.h"
#include "render/rend_main.h" // Rend_MapSurfacematerialSpec

#include <doomsday/world/sector.h>
#include <doomsday/world/lineowner.h>

/**
 * Same as @ref R_OpenRange() but works with the "visual" (i.e., smoothed) plane
 * height coordinates rather than the "sharp" coordinates.
 *
 * @param side      Line side to find the open range for.
 *
 * Return values:
 * @param bottom    Bottom Z height is written here. Can be @c nullptr.
 * @param top       Top Z height is written here. Can be @c nullptr.
 *
 * @return Height of the open range.
 *
 * @todo fixme: Should use the visual plane heights of subsectors.
 */
static coord_t visOpenRange(const LineSide &side, coord_t *retBottom = nullptr, coord_t *retTop = nullptr)
{
    const auto *frontSec = side.sectorPtr();
    const auto *backSec  = side.back().sectorPtr();

    coord_t bottom;
    if(backSec && backSec->floor().as<Plane>().heightSmoothed() > frontSec->floor().as<Plane>().heightSmoothed())
    {
        bottom = backSec->floor().as<Plane>().heightSmoothed();
    }
    else
    {
        bottom = frontSec->floor().as<Plane>().heightSmoothed();
    }

    coord_t top;
    if(backSec && backSec->ceiling().as<Plane>().heightSmoothed() < frontSec->ceiling().as<Plane>().heightSmoothed())
    {
        top = backSec->ceiling().as<Plane>().heightSmoothed();
    }
    else
    {
        top = frontSec->ceiling().as<Plane>().heightSmoothed();
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

/// @todo fixme: Should use the visual plane heights of subsectors.
bool R_SideBackClosed(const LineSide &side, bool ignoreOpacity)
{
    if (!side.hasSections()) return false;
    if (!side.hasSector()) return false;
    if (side.line().isSelfReferencing()) return false; // Never.

    if (side.considerOneSided()) return true;

    const auto &frontSec = side.sector();
    const auto &backSec  = side.back().sector();

    if (backSec.floor().as<Plane>().heightSmoothed()   >= backSec.ceiling().as<Plane>().heightSmoothed())  return true;
    if (backSec.ceiling().as<Plane>().heightSmoothed() <= frontSec.floor().as<Plane>().heightSmoothed())   return true;
    if (backSec.floor().as<Plane>().heightSmoothed()   >= frontSec.ceiling().as<Plane>().heightSmoothed()) return true;

    // Perhaps a middle material completely covers the opening?
    //if(side.middle().hasMaterial())

    if (MaterialAnimator *matAnimator = side.middle().as<Surface>().materialAnimator())
    {
        //MaterialAnimator &matAnimator = static_cast<ClientMaterial &>(side.middle().material())
        //        .getAnimator(Rend_MapSurfaceMaterialSpec());

        // Ensure we have up to date info about the material.
        matAnimator->prepare();

        if(ignoreOpacity || (matAnimator->isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1))
        {
            // Stretched middles always cover the opening.
            if(side.isFlagged(SDF_MIDDLE_STRETCH))
                return true;

            if(side.leftHEdge()) // possibility of degenerate BSP leaf
            {
                coord_t openRange, openBottom, openTop;
                openRange = visOpenRange(side, &openBottom, &openTop);
                if(matAnimator->dimensions().y >= openRange)
                {
                    // Possibly; check the placement.
                    WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle),
                                  *side.leftHEdge(), Line::From);

                    return (edge.isValid() && edge.top().z() > edge.bottom().z()
                            && edge.top().z() >= openTop && edge.bottom().z() <= openBottom);
                }
            }
        }
    }

    return false;
}

Line *R_FindLineNeighbor(const Line &line, const world::LineOwner &own, de::ClockDirection direction,
    const world::Sector *sector, binangle_t *diff)
{
    const world::LineOwner *cown = (direction == de::CounterClockwise ? own.prev() : own.next());
    Line *other = &cown->line().as<Line>();

    if(other == &line)
        return nullptr;

    if(diff)
    {
        *diff += (direction == de::CounterClockwise ? cown->angle() : own.angle());
    }

    if(!other->back().hasSector() || !other->isSelfReferencing())
    {
        if(sector)  // Must one of the sectors match?
        {
            if(other->front().sectorPtr() == sector ||
               (other->back().hasSector() && other->back().sectorPtr() == sector))
                return other;
        }
        else
        {
            return other;
        }
    }

    // Not suitable, try the next.
    DE_ASSERT(cown);
    return R_FindLineNeighbor(line, *cown, direction, sector, diff);
}

/**
 * @param side  Line side for which to determine covered opening status.
 *
 * @return  @c true iff there is a "middle" material on @a side which
 * completely covers the open range.
 *
 * @todo fixme: Should use the visual plane heights of subsectors.
 */
static bool middleMaterialCoversOpening(const world::LineSide &side)
{
    if (!side.hasSector()) return false; // Never.

    if (!side.hasSections()) return false;
    //if(!side.middle().hasMaterial()) return false;

    // Stretched middles always cover the opening.
    if (side.isFlagged(SDF_MIDDLE_STRETCH))
        return true;

    MaterialAnimator *matAnimator = side.middle().as<Surface>().materialAnimator();
            //.as<ClientMaterial>().getAnimator(Rend_MapSurfaceMaterialSpec());
    if (!matAnimator) return false;

    // Ensure we have up to date info about the material.
    matAnimator->prepare();

    if (matAnimator->isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1)
    {
        if (side.leftHEdge())  // possibility of degenerate BSP leaf.
        {
            coord_t openRange, openBottom, openTop;
            openRange = visOpenRange(side.as<LineSide>(), &openBottom, &openTop);
            if (matAnimator->dimensions().y >= openRange)
            {
                // Possibly; check the placement.
                WallEdge edge(WallSpec::fromMapSide(side.as<LineSide>(), LineSide::Middle),
                              *side.leftHEdge(), Line::From);

                return (edge.isValid()
                        && edge.top   ().z() > edge.bottom().z()
                        && edge.top   ().z() >= openTop
                        && edge.bottom().z() <= openBottom);
            }
        }
    }

    return false;
}

/// @todo fixme: Should use the visual plane heights of subsectors.
Line *R_FindSolidLineNeighbor(const Line &line, const world::LineOwner &own, de::ClockDirection direction,
    const world::Sector *sector, binangle_t *diff)
{
    DE_ASSERT(sector);

    const auto *cown = (direction == de::CounterClockwise ? own.prev() : own.next());
    Line *other = &cown->line().as<Line>();

    if (other == &line) return nullptr;

    if (diff)
    {
        *diff += (direction == de::CounterClockwise ? cown->angle() : own.angle());
    }

    if (!((other->isBspWindow()) && other->front().sectorPtr() != sector)
        && !other->isSelfReferencing())
    {
        if (!other->front().hasSector() || !other->back().hasSector())
            return other;

        if (   other->front().sector().floor  ().as<Plane>().heightSmoothed() >= sector->ceiling().as<Plane>().heightSmoothed()
            || other->front().sector().ceiling().as<Plane>().heightSmoothed() <= sector->floor  ().as<Plane>().heightSmoothed()
            || other->back().sector().floor  ().as<Plane>().heightSmoothed() >= sector->ceiling().as<Plane>().heightSmoothed()
            || other->back().sector().ceiling().as<Plane>().heightSmoothed() <= sector->floor  ().as<Plane>().heightSmoothed()
            || other->back().sector().ceiling().as<Plane>().heightSmoothed() <= other->back().sector().floor().as<Plane>().heightSmoothed())
            return other;

        // Both front and back MUST be open by this point.

        // Perhaps a middle material completely covers the opening?
        // We should not give away the location of false walls (secrets).
        auto &otherSide = other->side(other->front().sectorPtr() == sector ? Line::Front : Line::Back);
        if (middleMaterialCoversOpening(otherSide))
            return other;
    }

    // Not suitable, try the next.
    DE_ASSERT(cown);
    return R_FindSolidLineNeighbor(line, *cown, direction, sector, diff);
}
