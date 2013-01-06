/**
 * @file p_maputil.h
 * Map Utility Routines.
 *
 * @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_UTILITIES_H
#define LIBDENG_MAP_UTILITIES_H

#include <de/mathutil.h>
#include <de/vector1.h>
#include "p_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Is the point inside the sector, according to the edge lines of the BspLeaf?
 *
 * @param point   XY coordinate to test.
 * @param sector  Sector to test.
 *
 * @return  @c true, if the point is inside the sector.
 */
boolean P_IsPointInSector(coord_t const point[2], const Sector* sector);

/**
 * Is the point inside the sector, according to the edge lines of the BspLeaf?
 *
 * @param x       X coordinate to test.
 * @param y       Y coordinate to test.
 * @param sector  Sector to test.
 *
 * @return  @c true, if the point is inside the sector.
 */
boolean P_IsPointXYInSector(coord_t x, coord_t y, const Sector* sector);

/**
 * Is the point inside the BspLeaf (according to the edges)?
 *
 * Uses the well-known algorithm described here: http://www.alienryderflex.com/polygon/
 *
 * @param point    XY coordinate to test.
 * @param bspLeaf  BspLeaf to test.
 *
 * @return  @c true, if the point is inside the BspLeaf.
 */
boolean P_IsPointInBspLeaf(coord_t const point[2], const BspLeaf* bspLeaf);

/**
 * Is the point inside the BspLeaf (according to the edges)?
 *
 * Uses the well-known algorithm described here: http://www.alienryderflex.com/polygon/
 *
 * @param x        X coordinate to test.
 * @param y        Y coordinate to test.
 * @param bspLeaf  BspLeaf to test.
 *
 * @return  @c true, if the point is inside the BspLeaf.
 */
boolean P_IsPointXYInBspLeaf(coord_t x, coord_t y, const BspLeaf* bspLeaf);

/**
 * @note Caller must ensure that the mobj is currently unlinked.
 */
void P_LinkMobjToLineDefs(mobj_t* mo);

/**
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
boolean P_UnlinkMobjFromLineDefs(mobj_t* mo);

/**
 * @note  The mobj must be currently unlinked.
 */
void P_LinkMobjInBlockmap(mobj_t* mo);

boolean P_UnlinkMobjFromBlockmap(mobj_t* mo);

int PIT_AddLineDefIntercepts(LineDef* ld, void* parameters);

int PIT_AddMobjIntercepts(mobj_t* mobj, void* parameters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_UTILITIES_H
