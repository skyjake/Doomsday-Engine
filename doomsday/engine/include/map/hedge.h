/**
 * @file hedge.h
 * Map Half-edge. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_HEDGE
#define LIBDENG_MAP_HEDGE

#include "r_data.h"
#include "p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bsphedgeinfo_s;

HEdge* HEdge_New(void);

HEdge* HEdge_NewCopy(const HEdge* other);

void HEdge_Delete(HEdge* hedge);

/**
 * @param offset  Returns the position of the nearest point along the line [0..1].
 */
coord_t HEdge_PointDistance(HEdge* hedge, coord_t const point[2], coord_t* offset);
coord_t HEdge_PointXYDistance(HEdge* hedge, coord_t x, coord_t y, coord_t* offset);

/**
 * On which side of this HEdge does the specified point lie?
 *
 * @param hedge     HEdge instance.
 * @param point     Map space point to test.
 *
 * @return @c <0 Point is to the left/back of the hedge.
 *         @c =0 Point lies directly on the hedge.
 *         @c >0 Point is to the right/front of the hedge.
 */
coord_t HEdge_PointOnSide(const HEdge* hedge, coord_t const point[2]);
coord_t HEdge_PointXYOnSide(const HEdge* hedge, coord_t x, coord_t y);

boolean HEdge_PrepareWallDivs(HEdge* hedge, SideDefSection section,
    Sector* frontSector, Sector* backSector,
    walldivs_t* leftWallDivs, walldivs_t* rightWallDivs, float matOffset[2]);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param hedge  HEdge instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int HEdge_GetProperty(const HEdge* hedge, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param hedge  HEdge instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int HEdge_SetProperty(HEdge* hedge, const setargs_t* args);

#ifdef __cplusplus
}
#endif

#endif /// LIBDENG_MAP_HEDGE
