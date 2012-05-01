/**
 * @file bspleaf.h
 * Map BSP leaf. @ingroup map
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

#ifndef LIBDENG_MAP_BSPLEAF
#define LIBDENG_MAP_BSPLEAF

#include "r_data.h"
#include "p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

BspLeaf* BspLeaf_New(void);

void BspLeaf_Delete(BspLeaf* bspLeaf);

/**
 * Determine the HEdge from @a bspleaf whose vertex is suitable for use as the
 * center point of a trifan primitive (clockwise).
 *
 * Note that we do not want any overlapping or zero-area (degenerate) triangles.
 *
 * We are assured by the node build process that BspLeaf->hedges has been ordered
 * by angle, clockwise starting from the smallest angle.
 *
 * @algorithm:
 * For each vertex
 *    For each triangle
 *        if area is not greater than minimum bound, move to next vertex
 *    Vertex is suitable
 *
 * If a vertex exists which results in no zero-area triangles it is suitable for
 * use as the center of our trifan. If a suitable vertex is not found then the
 * center of BSP leaf should be selected instead (it will always be valid as
 * BSP leafs are convex).
 */
void BspLeaf_ChooseFanBase(BspLeaf* leaf);

/**
 * Number of vertices needed for this leaf's trifan.
 */
uint BspLeaf_NumFanVertices(const BspLeaf* bspLeaf);

/**
 * Prepare the trifan rvertex_t buffer specified according to the edges of this
 * BSP leaf. If a fan base HEdge has been chosen it will be used as the center of
 * the trifan, else the mid point of this leaf will be used instead.
 *
 * @param bspLeaf  BspLeaf instance.
 * @param antiClockwise  @c true= wind vertices in anticlockwise order (else clockwise).
 * @param height  Z map space height coordinate to be set for each vertex.
 * @param rvertices  The rvertex_t buffer to be populated.
 * @param numVertices  Number of rvertex_ts in @a rvertices.
 */
void BspLeaf_PrepareFan(const BspLeaf* bspLeaf, boolean antiClockwise, float height,
    rvertex_t* rvertices, uint numVertices);

/**
 * Update the BspLeaf's map space axis-aligned bounding box to encompass
 * the points defined by it's vertices.
 *
 * @param bspLeaf  BspLeaf instance.
 */
void BspLeaf_UpdateAABox(BspLeaf* bspLeaf);

/**
 * Update the mid point in the map coordinate space.
 *
 * @pre Axis-aligned bounding box must have been initialized.
 *
 * @param bspLeaf  BspLeaf instance.
 */
void BspLeaf_UpdateMidPoint(BspLeaf* bspLeaf);

/**
 * Update the world grid offset.
 *
 * @pre Axis-aligned bounding box must have been initialized.
 *
 * @param bspLeaf  BspLeaf instance.
 */
void BspLeaf_UpdateWorldGridOffset(BspLeaf* bspLeaf);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param bspLeaf  BspLeaf instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int BspLeaf_GetProperty(const BspLeaf* bspLeaf, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param bspLeaf  BspLeaf instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int BspLeaf_SetProperty(BspLeaf* bspLeaf, const setargs_t* args);

#ifdef __cplusplus
}
#endif

#endif /// LIBDENG_MAP_BSPLEAF
