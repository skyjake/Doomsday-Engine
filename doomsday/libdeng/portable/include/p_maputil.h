/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
int             P_PointOnLinedefSide(float x, float y, const linedef_t* line);
int             P_PointOnLinedefSide2(double pointX, double pointY,
                                   double lineDX, double lineDY,
                                   double linePerp, double lineLength,
                                   double epsilon);
int             P_BoxOnLineSide(const float* tmbox, const linedef_t* ld);
int             P_BoxOnLineSide2(float xl, float xh, float yl, float yh,
                                 const linedef_t *ld);
int             P_BoxOnLineSide3(const int bbox[4], double lineSX,
                                 double lineSY, double lineDX, double lineDY,
                                 double linePerp, double lineLength,
                                 double epsilon);
int             P_PointOnDivlineSide(float x, float y, const divline_t* line);
int             P_PointOnDivLineSidef(fvertex_t *pnt, fdivline_t *dline);
float           P_FloatInterceptVertex(fvertex_t *start, fvertex_t *end,
                                       fdivline_t *fdiv, fvertex_t *inter);
void            P_LineOpening(linedef_t* linedef);
void            P_MobjLink(mobj_t* mo, byte flags);
int             P_MobjUnlink(mobj_t* mo);
boolean         P_MobjUnlinkFromRing(mobj_t* mo, linkmobj_t** list);
void            P_MobjLinkToRing(mobj_t* mo, linkmobj_t** link);

boolean         PIT_AddLineIntercepts(linedef_t *ld, void *data);
boolean         PIT_AddMobjIntercepts(mobj_t *mo, void *data);

boolean         P_MobjLinesIterator(mobj_t *mo,
                                    boolean (*func) (linedef_t *, void *),
                                    void *);
boolean         P_MobjSectorsIterator(mobj_t *mo,
                                      boolean (*func) (sector_t *, void *),
                                      void *data);
boolean         P_LineMobjsIterator(linedef_t *line,
                                    boolean (*func) (mobj_t *, void *),
                                    void *data);
boolean         P_SectorTouchingMobjsIterator(sector_t *sector,
                                              boolean (*func) (mobj_t *,
                                                               void *),
                                              void *data);

// Mobjs in bounding box iterators.
boolean         P_MobjsBoxIterator(const float box[4],
                                   boolean (*func) (mobj_t *, void *),
                                   void *data);

boolean         P_MobjsBoxIteratorv(const arvec2_t box,
                                    boolean (*func) (mobj_t *, void *),
                                    void *data);

// Lines in bounding box iterators.
boolean         P_LinesBoxIterator(const float box[4],
                                   boolean (*func) (linedef_t *, void *),
                                   void *data);
boolean         P_LinesBoxIteratorv(const arvec2_t box,
                                    boolean (*func) (linedef_t *, void *),
                                    void *data);

// Polyobj in bounding box iterators.
boolean         P_PolyobjsBoxIterator(const float box[4],
                                     boolean (*func) (polyobj_t *, void *),
                                     void *data);
boolean         P_PolyobjsBoxIteratorv(const arvec2_t box,
                                      boolean (*func) (polyobj_t *, void *),
                                      void *data);

// (Polyobj in bounding box)->lineDefs iterators.
boolean         P_PolyobjLinesBoxIterator(const float box[4],
                                          boolean (*func) (linedef_t *, void *),
                                          void *data);
boolean         P_PolyobjLinesBoxIteratorv(const arvec2_t box,
                                           boolean (*func) (linedef_t *, void *),
                                           void *data);

// Lines and (Polyobj in bounding box)->lineDefs iterators.
// Polyobj lines are iterated first.
boolean         P_AllLinesBoxIterator(const float box[4],
                                      boolean (*func) (linedef_t *, void *),
                                      void *data);
boolean         P_AllLinesBoxIteratorv(const arvec2_t box,
                                       boolean (*func) (linedef_t *, void *),
                                       void *data);

// Subsectors in bounding box iterators.
boolean         P_SubsectorsBoxIterator(const float box[4], sector_t *sector,
                                       boolean (*func) (subsector_t *, void *),
                                       void *parm);
boolean         P_SubsectorsBoxIteratorv(const arvec2_t box, sector_t *sector,
                                        boolean (*func) (subsector_t *,
                                                         void *), void *data);

boolean         P_PathTraverse(float x1, float y1, float x2, float y2,
                               int flags, boolean (*trav) (intercept_t *));
#endif
