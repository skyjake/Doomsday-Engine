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

#ifndef __DOOMSDAY_MAP_UTILITIES_H__
#define __DOOMSDAY_MAP_UTILITIES_H__

#include "m_vector.h"
#include "p_object.h"

#define MAXINTERCEPTS   128

#define IS_SECTOR_LINKED(mo)    ((mo)->sPrev != NULL)
#define IS_BLOCK_LINKED(mo)     ((mo)->bNext != NULL)

extern float  opentop, openbottom, openrange, lowfloor;
extern divline_t traceLOS;

float           P_AccurateDistanceFixed(fixed_t dx, fixed_t dy);
float           P_AccurateDistance(float dx, float dy);
float           P_ApproxDistance(float dx, float dy);
float           P_ApproxDistance3(float dx, float dy, float dz);
void            P_LineUnitVector(linedef_t *line, float *unitvec);
float           P_MobjPointDistancef(mobj_t *start, mobj_t *end,
                                     float *fixpoint);

/**
 * @return  Non-zero if the point is on the right side of the specified line.
 */
int P_PointOnLinedefSide(float xy[2], const linedef_t* lineDef);
int P_PointOnLinedefSideXY(float x, float y, const linedef_t* lineDef);

int             P_PointOnLinedefSide2(double pointX, double pointY,
                                   double lineDX, double lineDY,
                                   double linePerp, double lineLength,
                                   double epsilon);

int             P_BoxOnLineSide(const AABoxf* box, const linedef_t* ld);
int             P_BoxOnLineSide2(float xl, float xh, float yl, float yh,
                                 const linedef_t *ld);
int             P_BoxOnLineSide3(const int bbox[4], double lineSX,
                                 double lineSY, double lineDX, double lineDY,
                                 double linePerp, double lineLength,
                                 double epsilon);
void            P_MakeDivline(const linedef_t* li, divline_t* dl);
int             P_PointOnDivlineSide(float x, float y, const divline_t* line);
float           P_InterceptVector(const divline_t* v2, const divline_t* v1);
int             P_PointOnDivLineSidef(fvertex_t *pnt, fdivline_t *dline);
float           P_FloatInterceptVertex(fvertex_t *start, fvertex_t *end,
                                       fdivline_t *fdiv, fvertex_t *inter);
void            P_LineOpening(linedef_t* linedef);

/**
 * Determine the BSP leaf (subsector) on the back side of the BS partition that
 * lies in front of the specified point within the CURRENT map's coordinate space.
 *
 * @note Always returns a valid subsector although the point may not actually lay
 *       within it (however it is on the same side of the space parition)!
 *
 * @param x  X coordinate of the point to test.
 * @param y  Y coordinate of the point to test.
 * @return  Subsector instance for that BSP node's leaf.
 */
subsector_t* P_SubsectorAtPointXY(float x, float y);

/**
 * Is the point inside the sector, according to the edge lines of the subsector.
 *
 * @param  X coordinate to test.
 * @param  Y coordinate to test.
 * @param  Sector to test.
 *
 * @return  @c true, if the point is inside the sector.
 */
boolean P_IsPointXYInSector(float x, float y, const sector_t* sector);

/**
 * Is the point inside the subsector, according to the edge lines of the subsector.
 *
 * @algorithm Uses the well-known algorithm described here:
 * http://www.alienryderflex.com/polygon/
 *
 * @param x  X coordinate to test.
 * @param y  Y coordinate to test.
 * @param subsector Subsector to test.
 *
 * @return  @c true, if the point is inside the subsector.
 */
boolean P_IsPointXYInSubsector(float x, float y, const subsector_t* subsector);

void            P_MobjLink(mobj_t* mo, byte flags);
int             P_MobjUnlink(mobj_t* mo);

int PIT_AddLineDefIntercepts(linedef_t* ld, void* paramaters);
int PIT_AddMobjIntercepts(mobj_t* mobj, void* paramaters);

int             P_MobjLinesIterator(mobj_t *mo,
                                    int (*func) (linedef_t *, void *),
                                    void *);
int             P_MobjSectorsIterator(mobj_t *mo,
                                      int (*func) (sector_t *, void *),
                                      void *data);
int             P_LineMobjsIterator(linedef_t *line,
                                    int (*func) (mobj_t *, void *),
                                    void *data);
int             P_SectorTouchingMobjsIterator(sector_t *sector,
                                              int (*func) (mobj_t *,
                                                               void *),
                                              void *data);

int P_MobjsBoxIterator(const AABoxf* box, int (*callback) (mobj_t*, void*), void* paramaters);

int P_LinesBoxIterator(const AABoxf* box, int (*callback) (linedef_t*, void*), void* paramaters);

int P_PolyobjsBoxIterator(const AABoxf* box, int (*callback) (polyobj_t*, void*), void* paramaters);

int P_PolyobjLinesBoxIterator(const AABoxf* box, int (*callback) (linedef_t*, void*), void* paramaters);

// LineDefs and Polyobj in LineDefs (note Polyobj LineDefs are iterated first).
int P_AllLinesBoxIterator(const AABoxf* box, int (*callback) (linedef_t*, void*), void* paramaters);

int P_SubsectorsBoxIterator(const AABoxf* box, sector_t *sector, int (*callback) (subsector_t*, void*), void* paramaters);

int P_PathTraverse(float const from[2], float const to[2], int flags, traverser_t callback);
int P_PathTraverse2(float const from[2], float const to[2], int flags, traverser_t callback, void* paramaters);

/**
 * Same as P_PathTraverse except 'from' and 'to' arguments are specified
 * as two sets of separate X and Y map space coordinates.
 */
int P_PathTraverseXY(float fromX, float fromY, float toX, float toY, int flags, traverser_t callback);
int P_PathTraverseXY2(float fromX, float fromY, float toX, float toY, int flags, traverser_t callback, void* paramaters);
#endif
