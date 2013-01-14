/**
 * @file linedef.h
 * Map LineDef. @ingroup map
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_LINEDEF
#define LIBDENG_MAP_LINEDEF

#ifndef __cplusplus
#  error "map/linedef.h requires C++"
#endif

#include <de/binangle.h>
#include <de/mathutil.h> // Divline
#include "resource/r_data.h"
#include "p_mapdata.h"
#include "p_dmu.h"

// Helper macros for accessing linedef data elements.
#define L_v(n)                  v[(n)? 1:0]
#define L_vorigin(n)            v[(n)? 1:0]->origin

#define L_v1                    L_v(0)
#define L_v1origin              L_v(0)->origin

#define L_v2                    L_v(1)
#define L_v2origin              L_v(1)->origin

#define L_vo(n)                 vo[(n)? 1:0]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define L_frontside             sides[0]
#define L_backside              sides[1]
#define L_side(n)               sides[(n)? 1:0]

#define L_sidedef(n)            L_side(n).sideDef
#define L_frontsidedef          L_sidedef(FRONT)
#define L_backsidedef           L_sidedef(BACK)

#define L_sector(n)             L_side(n).sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

// Is this line self-referencing (front sec == back sec)?
#define LINE_SELFREF(l)         ((l)->L_frontsidedef && (l)->L_backsidedef && \
                                 (l)->L_frontsector == (l)->L_backsector)

// Internal flags:
#define LF_POLYOBJ              0x1 ///< Line is part of a polyobject.
#define LF_BSPWINDOW            0x2 ///< Line produced a BSP window. @todo Refactor away.

/**
 * @defgroup sideSectionFlags  Side Section Flags
 * @ingroup map
 */
///@{
#define SSF_MIDDLE          0x1
#define SSF_BOTTOM          0x2
#define SSF_TOP             0x4
///@}

class Sector;
class SideDef;

typedef struct lineside_s {
    Sector* sector; /// Sector on this side.
    SideDef* sideDef; /// SideDef on this side.
    struct hedge_s* hedgeLeft;  /// Left-most HEdge on this side.
    struct hedge_s* hedgeRight; /// Right-most HEdge on this side.
    unsigned short shadowVisFrame; /// Framecount of last time shadows were drawn on this side.
} lineside_t;

class Vertex;

typedef struct linedef_s {
    runtime_mapdata_header_t header;
    Vertex *v[2];
    struct lineowner_s* vo[2]; /// Links to vertex line owner nodes [left, right].
    lineside_t          sides[2];
    int                 flags; /// Public DDLF_* flags.
    byte                inFlags; /// Internal LF_* flags.
    slopetype_t         slopeType;
    int                 validCount;
    binangle_t          angle; /// Calculated from front side's normal.
    coord_t             direction[2];
    coord_t             length; /// Accurate length.
    AABoxd              aaBox;
    boolean             mapped[DDMAXPLAYERS]; /// Whether the line has been mapped by each player yet.
    int                 origIndex; /// Original index in the archived map.
} LineDef;

/**
 * On which side of this LineDef does the specified box lie?
 *
 * @param line  Linedef.
 * @param box   Bounding box.
 *
 * @return  @c <0= bbox is wholly on the left side.
 *          @c  0= line intersects bbox.
 *          @c >0= bbox wholly on the right side.
 */
int LineDef_BoxOnSide(LineDef* lineDef, const AABoxd* box);

/**
 * On which side of this LineDef does the specified box lie? The test is
 * carried out using fixed-point math for behavior compatible with vanilla
 * DOOM. Note that this means there is a maximum size for both the bounding box
 * and the line: neither can exceed the fixed-point 16.16 range (about 65k
 * units).
 *
 * @param line  Linedef.
 * @param box   Bounding box.
 *
 * @return One of the following:
 * - Negative: bbox is entirely on the left side.
 * - Zero: line intersects bbox.
 * - Positive: bbox isentirely on the right side.
 */
int LineDef_BoxOnSide_FixedPrecision(LineDef* line, const AABoxd* box);

/**
 * @param offset  Returns the position of the nearest point along the line [0..1].
 */
coord_t LineDef_PointDistance(LineDef* lineDef, coord_t const point[2], coord_t* offset);
coord_t LineDef_PointXYDistance(LineDef* lineDef, coord_t x, coord_t y, coord_t* offset);

/**
 * On which side of this LineDef does the specified point lie?
 *
 * @param lineDef  LineDef instance.
 * @param xy  Map space point to test.
 *
 * @return @c <0 Point is to the left/back of the line.
 *         @c =0 Point lies directly on the line.
 *         @c >0 Point is to the right/front of the line.
 */
coord_t LineDef_PointOnSide(const LineDef* lineDef, coord_t const point[2]);
coord_t LineDef_PointXYOnSide(const LineDef* lineDef, coord_t x, coord_t y);

/**
 * Configure the specified divline_t by setting the origin point to this LineDef's
 * left (i.e., first) vertex and the vector to this lineDef's parallel vector.
 *
 * @param lineDef  LineDef instance.
 * @param divline  divline_t instance to be configured.
 */
void LineDef_SetDivline(const LineDef* lineDef, divline_t* divline);

/**
 * Find the "sharp" Z coordinate range of the opening on @a side. The open range is
 * defined as the gap between foor and ceiling on @a side clipped by the floor and
 * ceiling planes on the back side (if present).
 *
 * @param line      LineDef instance.
 * @param bottom    Bottom Z height is written here. Can be @c NULL.
 * @param top       Top Z height is written here. Can be @c NULL.
 *
 * @return Height of the open range.
 */
coord_t LineDef_OpenRange(const LineDef* line, int side, coord_t* bottom, coord_t* top);

/// Same as @ref LineDef_OpenRange() but works with the "visual" (i.e., smoothed)
/// plane height coordinates rather than the "sharp" coordinates.
coord_t LineDef_VisOpenRange(const LineDef* line, int side, coord_t* bottom, coord_t* top);

/**
 * Configure the specified TraceOpening according to the opening defined by the
 * inner-minimal plane heights which intercept this LineDef
 *
 * @param lineDef  LineDef instance.
 * @param opening  TraceOpening instance to be configured.
 */
void LineDef_SetTraceOpening(const LineDef* lineDef, TraceOpening* opening);

/**
 * Calculate a unit vector parallel to this LineDef.
 *
 * @todo This is no longer needed (SideDef stores tangent space normals).
 *
 * @param lineDef  LineDef instance.
 * @param unitVector  Unit vector is written here.
 */
void LineDef_UnitVector(LineDef* lineDef, float* unitVec);

/**
 * Update the LineDef's slopetype and map space angle delta according to
 * the points defined by it's vertices.
 *
 * @param lineDef  LineDef instance.
 */
void LineDef_UpdateSlope(LineDef* lineDef);

/**
 * Update the LineDef's map space axis-aligned bounding box to encompass
 * the points defined by it's vertices.
 *
 * @param lineDef  LineDef instance.
 */
void LineDef_UpdateAABox(LineDef* lineDef);

/**
 * The DOOM lighting model applies a sector light level delta when drawing
 * line segments based on their 2D world angle.
 *
 * @param lineDef  LineDef instance.
 * @param side  Side of the LineDef we are interested in.
 * @param deltaL  Light delta for the left edge written here.
 * @param deltaR  Light delta for the right edge written here.
 */
void LineDef_LightLevelDelta(LineDef* lineDef, int side, float* deltaL, float* deltaR);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param lineDef  LineDef instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int LineDef_GetProperty(const LineDef* lineDef, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param lineDef  LineDef instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int LineDef_SetProperty(LineDef* lineDef, const setargs_t* args);

#endif /// LIBDENG_MAP_LINEDEF
