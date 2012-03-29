/**
 * @file hedges.hh
 * BSP Builder Half-edges. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#ifndef LIBDENG_BSPBUILDER_HEDGES_HH
#define LIBDENG_BSPBUILDER_HEDGES_HH

#include "dd_types.h"
#include "p_mapdata.h"

#define IFFY_LEN            4.0

// Smallest distance between two points before being considered equal.
#define DIST_EPSILON        (1.0 / 128.0)

// Smallest degrees between two angles before being considered equal.
#define ANG_EPSILON         (1.0 / 1024.0)

/**
 * BspHEdgeInfo. Plain old data structure storing additional information about
 * a half-edge produced by BspBuilder.
 */
typedef struct bsphedgeinfo_s {
    // Precomputed data for faster calculations.
    double pSX, pSY;
    double pEX, pEY;
    double pDX, pDY;

    double pLength;
    double pAngle;
    double pPara;
    double pPerp;

    // Linedef that this half-edge goes along, or NULL if miniseg.
    struct linedef_s* lineDef;

    // Linedef that this half-edge initially comes from.
    // For "real" half-edges, this is just the same as the 'linedef' field
    // above. For "miniedges", this is the linedef of the partition line.
    struct linedef_s* sourceLineDef;
} BspHEdgeInfo;

typedef struct bsp_hedge_s {
    struct vertex_s* v[2]; // [Start, End] of the half-edge..

    // Half-edge on the other side, or NULL if one-sided. This relationship
    // is always one-to-one -- if one of the half-edges is split, the twin
    // must also be split.
    struct bsp_hedge_s* twin;

    //struct bsp_hedge_s* nextInBlock;
    struct bsp_hedge_s* nextInLeaf;
    struct bsp_hedge_s* nextOnSide;
    struct bsp_hedge_s* prevOnSide;

    // Index of the half-edge. Only valid once the half-edge has been added
    // to a polygon. A negative value means it is invalid -- there
    // shouldn't be any of these once the BSP tree has been built.
    int index;

    // The superblock that contains this half-edge, or NULL if the half-edge
    // is no longer in any superblock (e.g. now in a leaf).
    void* block;

    BspHEdgeInfo info;

    struct sector_s* sector; // Adjacent sector, or NULL if invalid sidedef or minihedge.
    byte side; // 0 for right, 1 for left.
} bsp_hedge_t;

/**
 * Plain-old-data structure containing additional information for a half-edge
 * half-plane intercept point where the geometry intersects (an incident vertex
 * can be found here (or at there will be upon insertion.)).
 *
 * There is always a corresponding HPlaneIntercept in the owning HPlane.
 */
typedef struct hedgeintercept_s {
    // Vertex in question.
    struct vertex_s* vertex;

    // True if this intersection was on a self-referencing linedef.
    boolean selfRef;

    // Sector on each side of the vertex (along the partition),
    // or NULL when that direction isn't OPEN.
    struct sector_s* before;
    struct sector_s* after;
} HEdgeIntercept;

#if _DEBUG
void Bsp_PrintHEdgeIntercept(HEdgeIntercept* intercept);
#endif

#endif /// LIBDENG_BSPBUILDER_HEDGES_HH
