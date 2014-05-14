/** @file maputil.h  World map utilities.
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

#ifdef __CLIENT__
#ifndef DENG_WORLD_MAPUTIL_H
#define DENG_WORLD_MAPUTIL_H

#include <de/binangle.h>
#include "Line"

class Sector;
class LineOwner;

/// @deprecated
coord_t R_VisOpenRange(LineSide const &side, coord_t *retBottom = 0, coord_t *retTop = 0);

/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
Line *R_FindLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);

Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff = 0);

#endif // DENG_WORLD_MAPUTIL_H
#endif // __CLIENT__
