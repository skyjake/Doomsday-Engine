/** @file world/r_world.h World Setup.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_WORLD_P_MAPUTIL2_H
#define DENG_WORLD_P_MAPUTIL2_H

#include <de/binangle.h>

#include <de/Vector>

#include "Line"
#include "Sector"
#include "Vertex"

void R_SetRelativeHeights(Sector const *front, Sector const *back, int planeIndex,
    coord_t *fz = 0, coord_t *bz = 0, coord_t *bhz = 0);

/**
 * Determine the map space Z coordinates of a wall section.
 *
 * @param side            Map line side to determine Z heights for.
 * @param section         Line::Side section to determine coordinates for.
 *
 * Return values:
 * @param bottom          Z map space coordinate at the bottom of the wall section. Can be @c 0.
 * @param top             Z map space coordinate at the top of the wall section. Can be @c 0.
 * @param materialOrigin  Surface space material coordinate offset. Can be @c 0.
 */
void R_SideSectionCoords(Line::Side const &side, int section, bool skyClip = true,
    coord_t *bottom = 0, coord_t *top = 0, de::Vector2f *materialOrigin = 0);

/**
 * Find the "sharp" Z coordinate range of the opening between sectors @a frontSec
 * and @a backSec. The open range is defined as the gap between foor and ceiling on
 * the front side clipped by the floor and ceiling planes on the back side (if present).
 *
 * @param side      Line side to find the open range for.
 * @param frontSec  Sector on the front side.
 * @param backSec   Sector on the back side. Can be @c 0.
 *
 * Return values:
 * @param bottom    Bottom Z height is written here. Can be @c 0.
 * @param top       Top Z height is written here. Can be @c 0.
 *
 * @return Height of the open range.
 */
coord_t R_OpenRange(Line::Side const &side, Sector const *frontSec, Sector const *backSec,
    coord_t *bottom = 0, coord_t *top = 0);

/**
 * Same as @ref R_OpenRange() except that the sector arguments are taken from the
 * specified line @a side.
 */
inline coord_t R_OpenRange(Line::Side const &side, coord_t *bottom = 0, coord_t *top = 0)
{
    return R_OpenRange(side, side.sectorPtr(), side.back().sectorPtr(), bottom, top);
}

/**
 * Same as @ref R_OpenRange() but works with the "visual" (i.e., smoothed) plane
 * height coordinates rather than the "sharp" coordinates.
 *
 * @param side      Line side to find the open range for.
 * @param frontSec  Sector on the front side.
 * @param backSec   Sector on the back side. Can be @c 0.
 *
 * Return values:
 * @param bottom    Bottom Z height is written here. Can be @c 0.
 * @param top       Top Z height is written here. Can be @c 0.
 *
 * @return Height of the open range.
 */
coord_t R_VisOpenRange(Line::Side const &side, Sector const *frontSec, Sector const *backSec,
    coord_t *bottom = 0, coord_t *top = 0);

/**
 * Same as @ref R_VisOpenRange() except that the sector arguments are taken from
 * the specified line @a side.
 */
inline coord_t R_VisOpenRange(Line::Side const &side, coord_t *bottom = 0, coord_t *top = 0)
{
    return R_VisOpenRange(side, side.sectorPtr(), side.back().sectorPtr(), bottom, top);
}

#ifdef __CLIENT__

/**
 * @param side  Line::Side instance.
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 *
 * @return  @c true if this side is considered "closed" (i.e., there is no opening
 * through which the relative back Sector can be seen). Tests consider all Planes
 * which interface with this and the "middle" Material used on the "this" side.
 */
bool R_SideBackClosed(Line::Side const &side, bool ignoreOpacity = true);

/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
Line *R_FindLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);

Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);

#endif // __CLIENT__

#endif // DENG_WORLD_P_MAPUTIL2_H
