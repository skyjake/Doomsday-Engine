/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_maputil.h: Map Utility Routines
 */

#ifndef LIBDENG_MAP_UTILITIES_H
#define LIBDENG_MAP_UTILITIES_H

#include "m_vector.h"
#include "p_object.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IS_SECTOR_LINKED(mo)    ((mo)->sPrev != NULL)
#define IS_BLOCK_LINKED(mo)     ((mo)->bNext != NULL)

float           P_AccurateDistanceFixed(fixed_t dx, fixed_t dy);
float           P_AccurateDistance(float dx, float dy);
float           P_ApproxDistance(float dx, float dy);
float           P_ApproxDistance3(float dx, float dy, float dz);
float           P_MobjPointDistancef(mobj_t *start, mobj_t *end,
                                     float *fixpoint);

/**
 * Determines on which side of line the point is.
 *
 * @param x  X coordinate of the point.
 * @param y  Y coordinate of the point.
 * @param lX  X coordinate of the line (origin).
 * @param lY  Y coordinate of the line (origin).
 * @param lDX  X axis angle delta (from origin -> out).
 * @param lDY  Y axis angle delta (from origin -> out).
 *
 * @return @c <0 Point is to the left of the line.
 *         @c >0 Point is to the right of the line.
 *         @c =0 Point lies directly on the line.
 */
float P_PointOnLineSide(float x, float y, float lX, float lY, float lDX, float lDY);

/**
 * @return  Non-zero if the point is on the right side of the specified line.
 */
int P_PointOnLineDefSide(float const xy[2], const LineDef* lineDef);
int P_PointXYOnLineDefSide(float x, float y, const LineDef* lineDef);

int             P_PointOnLinedefSide2(double pointX, double pointY,
                                   double lineDX, double lineDY,
                                   double linePerp, double lineLength,
                                   double epsilon);

int             P_BoxOnLineSide(const AABoxf* box, const LineDef* ld);
int             P_BoxOnLineSide2(float xl, float xh, float yl, float yh,
                                 const LineDef *ld);

/**
 * Check the spatial relationship between the given box and a partitioning line.
 *
 * @param bbox          Ptr to the box being tested.
 * @param lineSX        X coordinate of the start of the line.
 * @param lineSY        Y coordinate of the end of the line.
 * @param lineDX        X delta of the line (slope).
 * @param lineDY        Y delta of the line (slope).
 * @param linePerp      Perpendicular d of the line.
 * @param lineLength    Length of the line.
 * @param epsilon       Points within this distance will be considered equal.
 *
 * @return  @c <0= bbox is wholly on the left side.
 *          @c  0= line intersects bbox.
 *          @c >0= bbox wholly on the right side.
 */
int P_BoxOnLineSide3(const AABox* aaBox, double lineSX, double lineSY, double lineDX,
    double lineDY, double linePerp, double lineLength, double epsilon);

void P_MakeDivline(const LineDef* lineDef, divline_t* divline);

int P_PointOnDivlineSide(float x, float y, const divline_t* line);

float P_InterceptVector(const divline_t* v2, const divline_t* v1);

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
BspLeaf* P_BspLeafAtPointXY(float x, float y);

/**
 * Is the point inside the sector, according to the edge lines of the BspLeaf.
 *
 * @param  X coordinate to test.
 * @param  Y coordinate to test.
 * @param  Sector to test.
 *
 * @return  @c true, if the point is inside the sector.
 */
boolean P_IsPointXYInSector(float x, float y, const Sector* sector);

/**
 * Is the point inside the BspLeaf (according to the edges).
 *
 * @algorithm Uses the well-known algorithm described here:
 * http://www.alienryderflex.com/polygon/
 *
 * @param x  X coordinate to test.
 * @param y  Y coordinate to test.
 * @param bspLeaf  BspLeaf to test.
 *
 * @return  @c true, if the point is inside the BspLeaf.
 */
boolean P_IsPointXYInBspLeaf(float x, float y, const BspLeaf* bspLeaf);

void P_MobjLink(mobj_t* mo, byte flags);
int P_MobjUnlink(mobj_t* mo);

/**
 * @important Caller must ensure that the mobj is currently unlinked.
 */
void P_LinkMobjToLineDefs(mobj_t* mo);

/**
 * Unlinks the mobj from all the lines it's been linked to. Can be called
 * without checking that the list does indeed contain lines.
 */
boolean P_UnlinkMobjFromLineDefs(mobj_t* mo);

/**
 * @important The mobj must be currently unlinked.
 */
void P_LinkMobjInBlockmap(mobj_t* mo);

boolean P_UnlinkMobjFromBlockmap(mobj_t* mo);

int PIT_AddLineDefIntercepts(LineDef* ld, void* parameters);
int PIT_AddMobjIntercepts(mobj_t* mobj, void* parameters);

int P_MobjLinesIterator(mobj_t* mo, int (*func) (LineDef*, void*), void* parameters);

int P_MobjSectorsIterator(mobj_t* mo, int (*func) (Sector*, void*), void* parameters);

int P_LineMobjsIterator(LineDef* lineDef, int (*func) (mobj_t*, void*), void* parameters);

int P_SectorTouchingMobjsIterator(Sector* sector, int (*func) (mobj_t*, void*), void *parameters);

int P_MobjsBoxIterator(const AABoxf* box, int (*callback) (mobj_t*, void*), void* parameters);

int P_LinesBoxIterator(const AABoxf* box, int (*callback) (LineDef*, void*), void* parameters);

/**
 * The validCount flags are used to avoid checking polys that are marked in
 * multiple mapblocks, so increment validCount before the first call, then
 * make one or more calls to it.
 */
int P_PolyobjsBoxIterator(const AABoxf* box, int (*callback) (Polyobj*, void*), void* parameters);

int P_PolyobjLinesBoxIterator(const AABoxf* box, int (*callback) (LineDef*, void*), void* parameters);

/**
 * LineDefs and Polyobj LineDefs (note Polyobj LineDefs are iterated first).
 *
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to GameMap_IterateCellLineDefs(), then make one or more calls to it.
 */
int P_AllLinesBoxIterator(const AABoxf* box, int (*callback) (LineDef*, void*), void* parameters);

int P_BspLeafsBoxIterator(const AABoxf* box, Sector* sector, int (*callback) (BspLeaf*, void*), void* parameters);

int P_PathTraverse(float const from[2], float const to[2], int flags, traverser_t callback);
int P_PathTraverse2(float const from[2], float const to[2], int flags, traverser_t callback, void* parameters);

/**
 * Same as P_PathTraverse except 'from' and 'to' arguments are specified
 * as two sets of separate X and Y map space coordinates.
 */
int P_PathXYTraverse(float fromX, float fromY, float toX, float toY, int flags, traverser_t callback);
int P_PathXYTraverse2(float fromX, float fromY, float toX, float toY, int flags, traverser_t callback, void* paramaters);

boolean P_CheckLineSight(const float from[3], const float to[3], float bottomSlope,
    float topSlope, int flags);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_UTILITIES_H
