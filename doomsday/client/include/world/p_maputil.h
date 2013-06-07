/** @file world/p_maputil.h World Map Utility Routines.
 *
 * @ingroup map
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

#ifndef DENG_WORLD_P_MAPUTIL_H
#define DENG_WORLD_P_MAPUTIL_H

#include <de/Vector>

#include "p_object.h"

/**
 * Is the point inside the sector, according to the edge lines of the BspLeaf?
 *
 * @param point   XY coordinate to test.
 * @param sector  Sector to test.
 *
 * @return  @c true iff the point lies within @a sector.
 */
bool P_IsPointInSector(de::Vector2d const &point, Sector const &sector);

/**
 * Is the point inside the BspLeaf (according to the edges)?
 *
 * Uses the well-known algorithm described here: http://www.alienryderflex.com/polygon/
 *
 * @param point    XY coordinate to test.
 * @param bspLeaf  BspLeaf to test.
 *
 * @return  @c true iff the point lines within @a bspLeaf.
 */
bool P_IsPointInBspLeaf(de::Vector2d const &point, BspLeaf const &bspLeaf);

/**
 * @note Caller must ensure that the mobj is currently unlinked.
 */
void P_LinkMobjToLines(mobj_t *mo);

/**
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
boolean P_UnlinkMobjFromLines(mobj_t *mo);

/**
 * @note  The mobj must be currently unlinked.
 */
void P_LinkMobjInBlockmap(mobj_t *mo);

boolean P_UnlinkMobjFromBlockmap(mobj_t *mo);

int PIT_AddLineIntercepts(Line *ld, void *parameters);

int PIT_AddMobjIntercepts(mobj_t *mobj, void *parameters);

#endif // DENG_WORLD_P_MAPUTIL_H
