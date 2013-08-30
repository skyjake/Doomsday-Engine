/** @file maputil.cpp World map utilities.
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

#ifdef __CLIENT__

#include "BspLeaf"
#include "Plane"
#include "Sector"
#include "world/lineowner.h"

#include "MaterialSnapshot"
#include "MaterialVariantSpec"

#include "render/rend_main.h" // Rend_MapSurfacematerialSpec
#include "WallEdge"

#include "world/maputil.h"

using namespace de;

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
    if(backSec && backSec->floor().visHeight() > frontSec->floor().visHeight())
    {
        bottom = backSec->floor().visHeight();
    }
    else
    {
        bottom = frontSec->floor().visHeight();
    }

    coord_t top;
    if(backSec && backSec->ceiling().visHeight() < frontSec->ceiling().visHeight())
    {
        top = backSec->ceiling().visHeight();
    }
    else
    {
        top = frontSec->ceiling().visHeight();
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

    if(backSec.floor().visHeight()   >= backSec.ceiling().visHeight())  return true;
    if(backSec.ceiling().visHeight() <= frontSec.floor().visHeight())   return true;
    if(backSec.floor().visHeight()   >= frontSec.ceiling().visHeight()) return true;

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
                WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle),
                              *side.leftHEdge(), Line::From);

                return (edge.isValid() && edge.top().z() > edge.bottom().z()
                        && edge.top().z() >= openTop && edge.bottom().z() <= openBottom);
            }
        }
    }

    return false;
}

/// @todo fixme: Should use the visual plane heights of sector clusters.
Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line) return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!((other->isBspWindow()) && other->frontSectorPtr() != sector) &&
       !other->isSelfReferencing())
    {
        if(!other->hasFrontSector()) return other;
        if(!other->hasBackSector()) return other;

        if(other->frontSector().floor().visHeight() >= sector->ceiling().visHeight() ||
           other->frontSector().ceiling().visHeight() <= sector->floor().visHeight() ||
           other->backSector().floor().visHeight() >= sector->ceiling().visHeight() ||
           other->backSector().ceiling().visHeight() <= sector->floor().visHeight() ||
           other->backSector().ceiling().visHeight() <= other->backSector().floor().visHeight())
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
