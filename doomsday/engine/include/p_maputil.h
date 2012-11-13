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

#include "m_vector.h"
#include "p_object.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @return  @c 0 if point is in front of the line, else @c 1.
 */
int Divline_PointOnSide(const divline_t* line, coord_t const point[2]);
int Divline_PointXYOnSide(const divline_t* line, coord_t x, coord_t y);

/**
 * @return  Fractional intercept point along the first divline.
 */
fixed_t Divline_Intersection(const divline_t* v1, const divline_t* v2);

/**
 * Retrieve an immutable copy of the LOS trace line for the CURRENT map.
 *
 * @note Always returns a valid divline_t even if there is no current map.
 */
const divline_t* P_TraceLOS(void);

/**
 * Retrieve an immutable copy of the TraceOpening state for the CURRENT map.
 *
 * @note Always returns a valid TraceOpening even if there is no current map.
 */
const TraceOpening* P_TraceOpening(void);

/**
 * Update the TraceOpening state for the CURRENT map according to the opening
 * defined by the inner-minimal planes heights which intercept @a linedef
 */
void P_SetTraceOpening(LineDef* linedef);

/**
 * Determine the BSP leaf on the back side of the BS partition that lies in
 * front of the specified point within the CURRENT map's coordinate space.
 *
 * @note Always returns a valid BspLeaf although the point may not actually lay
 *       within it (however it is on the same side of the space partition)!
 *
 * @param x  X coordinate of the point to test.
 * @param y  Y coordinate of the point to test.
 * @return  BspLeaf instance for that BSP node's leaf.
 */
BspLeaf* P_BspLeafAtPoint(coord_t const point[2]);
BspLeaf* P_BspLeafAtPointXY(coord_t x, coord_t y);

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

void P_MobjLink(mobj_t* mo, byte flags);
int P_MobjUnlink(mobj_t* mo);

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

int P_MobjLinesIterator(mobj_t* mo, int (*callback) (LineDef*, void*), void* parameters);

int P_MobjSectorsIterator(mobj_t* mo, int (*callback) (Sector*, void*), void* parameters);

int P_LineMobjsIterator(LineDef* lineDef, int (*callback) (mobj_t*, void*), void* parameters);

int P_SectorTouchingMobjsIterator(Sector* sector, int (*callback) (mobj_t*, void*), void *parameters);

int P_MobjsBoxIterator(const AABoxd* box, int (*callback) (mobj_t*, void*), void* parameters);

int P_LinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters);

/**
 * The validCount flags are used to avoid checking polys that are marked in
 * multiple mapblocks, so increment validCount before the first call, then
 * make one or more calls to it.
 */
int P_PolyobjsBoxIterator(const AABoxd* box, int (*callback) (Polyobj*, void*), void* parameters);

int P_PolyobjLinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters);

/**
 * LineDefs and Polyobj LineDefs (note Polyobj LineDefs are iterated first).
 *
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to GameMap_IterateCellLineDefs(), then make one or more calls to it.
 */
int P_AllLinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters);

int P_BspLeafsBoxIterator(const AABoxd* box, Sector* sector, int (*callback) (BspLeaf*, void*), void* parameters);

int P_PathTraverse2(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback, void* parameters);
int P_PathTraverse(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback/*parameters=NULL*/);

/**
 * Same as P_PathTraverse except 'from' and 'to' arguments are specified
 * as two sets of separate X and Y map space coordinates.
 */
int P_PathXYTraverse2(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags, traverser_t callback, void* paramaters);
int P_PathXYTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags, traverser_t callback/*parameters=NULL*/);

boolean P_CheckLineSight(coord_t const from[3], coord_t const to[3], coord_t bottomSlope, coord_t topSlope, int flags);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_UTILITIES_H
