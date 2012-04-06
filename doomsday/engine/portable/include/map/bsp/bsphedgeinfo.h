/**
 * @file bsphedgeinfo.h
 * BSP Builder half-edge info. @ingroup bsp
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

#ifndef LIBDENG_BSPBUILDER_HEDGEINFO
#define LIBDENG_BSPBUILDER_HEDGEINFO

#include "dd_types.h"
#include "p_mapdata.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Plain old data (POD) structure storing additional information about a
 * half-edge produced by BspBuilder.
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

    HEdge* nextOnSide;
    HEdge* prevOnSide;

    // The superblock that contains this half-edge, or NULL if the half-edge
    // is no longer in any superblock (e.g. now in a leaf).
    void* block;

    // Linedef that this half-edge goes along, or NULL if miniseg.
    LineDef* lineDef;

    // Linedef that this half-edge initially comes from.
    // For "real" half-edges, this is just the same as the 'linedef' field
    // above. For "miniedges", this is the linedef of the partition line.
    LineDef* sourceLineDef;
} BspHEdgeInfo;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_BSPBUILDER_HEDGEINFO
