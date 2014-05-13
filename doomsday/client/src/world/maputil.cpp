/** @file maputil.cpp  World map utilities.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "world/maputil.h"

#include "Line"
#include "Plane"
#include "Sector"

#ifdef __CLIENT__

#include "Surface"
#include "world/lineowner.h"

#include "MaterialSnapshot"
#include "MaterialVariantSpec"

#include "render/rend_main.h" // Rend_MapSurfacematerialSpec
#include "WallEdge"
#endif

using namespace de;

lineopening_s::lineopening_s(Line const &line)
{
    if(!line.hasBackSector())
    {
        top = bottom = range = lowFloor = 0;
        return;
    }

    Sector const *frontSector = line.front().sectorPtr();
    Sector const *backSector  = line.back().sectorPtr();
    DENG_ASSERT(frontSector != 0);

    if(backSector && backSector->ceiling().height() < frontSector->ceiling().height())
    {
        top = backSector->ceiling().height();
    }
    else
    {
        top = frontSector->ceiling().height();
    }

    if(backSector && backSector->floor().height() > frontSector->floor().height())
    {
        bottom = backSector->floor().height();
    }
    else
    {
        bottom = frontSector->floor().height();
    }

    range = top - bottom;

    // Determine the "low floor".
    if(backSector && frontSector->floor().height() > backSector->floor().height())
    {
        lowFloor = float( backSector->floor().height() );
    }
    else
    {
        lowFloor = float( frontSector->floor().height() );
    }
}

lineopening_s &lineopening_s::operator = (lineopening_s const &other)
{
    top      = other.top;
    bottom   = other.bottom;
    range    = other.range;
    lowFloor = other.lowFloor;
    return *this;
}

#ifdef __CLIENT__

/**
 * Same as @ref R_OpenRange() but works with the "visual" (i.e., smoothed) plane
 * height coordinates rather than the "sharp" coordinates.
 *
 * @param side      Line side to find the open range for.
 *
 * Return values:
 * @param bottom    Bottom Z height is written here. Can be @c 0.
 * @param top       Top Z height is written here. Can be @c 0.
 *
 * @return Height of the open range.
 *
 * @todo fixme: Should use the visual plane heights of sector clusters.
 */
static coord_t visOpenRange(LineSide const &side, coord_t *retBottom = 0, coord_t *retTop = 0)
{
    Sector const *frontSec = side.sectorPtr();
    Sector const *backSec  = side.back().sectorPtr();

    coord_t bottom;
    if(backSec && backSec->floor().heightSmoothed() > frontSec->floor().heightSmoothed())
    {
        bottom = backSec->floor().heightSmoothed();
    }
    else
    {
        bottom = frontSec->floor().heightSmoothed();
    }

    coord_t top;
    if(backSec && backSec->ceiling().heightSmoothed() < frontSec->ceiling().heightSmoothed())
    {
        top = backSec->ceiling().heightSmoothed();
    }
    else
    {
        top = frontSec->ceiling().heightSmoothed();
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

/// @todo fixme: Should use the visual plane heights of sector clusters.
bool R_SideBackClosed(LineSide const &side, bool ignoreOpacity)
{
    if(!side.hasSections()) return false;
    if(!side.hasSector()) return false;
    if(side.line().isSelfReferencing()) return false; // Never.

    if(side.considerOneSided()) return true;

    Sector const &frontSec = side.sector();
    Sector const &backSec  = side.back().sector();

    if(backSec.floor().heightSmoothed()   >= backSec.ceiling().heightSmoothed())  return true;
    if(backSec.ceiling().heightSmoothed() <= frontSec.floor().heightSmoothed())   return true;
    if(backSec.floor().heightSmoothed()   >= frontSec.ceiling().heightSmoothed()) return true;

    // Perhaps a middle material completely covers the opening?
    if(side.middle().hasMaterial())
    {
        // Ensure we have up to date info about the material.
        MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

        if(ignoreOpacity || (ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1))
        {
            // Stretched middles always cover the opening.
            if(side.isFlagged(SDF_MIDDLE_STRETCH))
                return true;

            if(side.leftHEdge()) // possibility of degenerate BSP leaf
            {
                coord_t openRange, openBottom, openTop;
                openRange = visOpenRange(side, &openBottom, &openTop);
                if(ms.height() >= openRange)
                {
                    // Possibly; check the placement.
                    WallEdge left(*side.leftHEdge(), Line::From);
                    WallEdgeSection &sectionLeft = left.wallMiddle();

                    return (sectionLeft.isValid() && sectionLeft.top().z() > sectionLeft.bottom().z()
                            && sectionLeft.top().z() >= openTop && sectionLeft.bottom().z() <= openBottom);
                }
            }
        }
    }

    return false;
}

Line *R_FindLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line)
        return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!other->hasBackSector() || !other->isSelfReferencing())
    {
        if(sector) // Must one of the sectors match?
        {
            if(other->frontSectorPtr() == sector ||
               (other->hasBackSector() && other->backSectorPtr() == sector))
                return other;
        }
        else
        {
            return other;
        }
    }

    // Not suitable, try the next.
    return R_FindLineNeighbor(sector, line, cown, antiClockwise, diff);
}

/**
 * @param side  Line side for which to determine covered opening status.
 *
 * @return  @c true iff there is a "middle" material on @a side which
 * completely covers the open range.
 *
 * @todo fixme: Should use the visual plane heights of sector clusters.
 */
static bool middleMaterialCoversOpening(LineSide const &side)
{
    if(!side.hasSector()) return false; // Never.

    if(!side.hasSections()) return false;
    if(!side.middle().hasMaterial()) return false;

    // Stretched middles always cover the opening.
    if(side.isFlagged(SDF_MIDDLE_STRETCH))
        return true;

    // Ensure we have up to date info about the material.
    MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

    if(ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1)
    {
        if(side.leftHEdge()) // possibility of degenerate BSP leaf
        {
            coord_t openRange, openBottom, openTop;
            openRange = visOpenRange(side, &openBottom, &openTop);
            if(ms.height() >= openRange)
            {
                // Possibly; check the placement.
                WallEdge left(*side.leftHEdge(), Line::From);
                WallEdgeSection &sectionLeft = left.wallMiddle();

                return (sectionLeft.isValid() && sectionLeft.top().z() > sectionLeft.bottom().z()
                        && sectionLeft.top().z() >= openTop && sectionLeft.bottom().z() <= openBottom);
            }
        }
    }

    return false;
}

/// @todo fixme: Should use the visual plane heights of sector clusters.
Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    DENG_ASSERT(sector);

    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line) return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!((other->isBspWindow()) && other->frontSectorPtr() != sector) &&
       !other->isSelfReferencing())
    {
        if(!other->hasFrontSector()) return other;
        if(!other->hasBackSector()) return other;

        if(other->frontSector().floor().heightSmoothed() >= sector->ceiling().heightSmoothed() ||
           other->frontSector().ceiling().heightSmoothed() <= sector->floor().heightSmoothed() ||
           other->backSector().floor().heightSmoothed() >= sector->ceiling().heightSmoothed() ||
           other->backSector().ceiling().heightSmoothed() <= sector->floor().heightSmoothed() ||
           other->backSector().ceiling().heightSmoothed() <= other->backSector().floor().heightSmoothed())
            return other;

        // Both front and back MUST be open by this point.

        // Perhaps a middle material completely covers the opening?
        // We should not give away the location of false walls (secrets).
        LineSide &otherSide = other->side(other->frontSectorPtr() == sector? Line::Front : Line::Back);
        if(middleMaterialCoversOpening(otherSide))
            return other;
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

#endif // __CLIENT__
