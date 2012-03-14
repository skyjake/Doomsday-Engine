/**
 * @file bsp_intersection.h
 * BSP Builder Intersections. @ingroup map
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

#ifndef LIBDENG_MAP_BSP_INTERSECTION
#define LIBDENG_MAP_BSP_INTERSECTION

#include "bsp_edge.h"

struct hplanepartition_s;
struct superblock_s;

typedef struct hplanepartition_s {
    double x, y;
    double dX, dY;
    double length;
    LineDef* lineDef; // Not NULL if partition originated from a linedef.
    LineDef* sourceLineDef;

    double pSX, pSY;
    double pDX, pDY;
    double pPara, pPerp;
} HPlanePartition;

typedef struct hplaneintercept_s HPlaneIntercept;

HPlaneIntercept* HPlaneIntercept_Next(HPlaneIntercept* intersection);

HPlaneIntercept* HPlaneIntercept_Prev(HPlaneIntercept* intersection);

void* HPlaneIntercept_UserData(HPlaneIntercept* intersection);

/**
 * HPlane instance. Created with HPlane_New().
 */
typedef struct hplane_s HPlane;

/**
 * Create a new HPlane.
 */
HPlane* HPlane_New(void);

HPlanePartition* HPlane_Partition(HPlane* hPlane);

/**
 * Destroy a HPlane.
 */
void HPlane_Delete(HPlane* hPlane);

/**
 * Empty all intersections from the specified HPlane.
 */
void HPlane_Clear(HPlane* hPlane);

/**
 * Insert a point at the given intersection into the intersection list.
 */
HPlaneIntercept* HPlane_NewIntercept2(HPlane* hPlane, double distance, void* userData);
HPlaneIntercept* HPlane_NewIntercept(HPlane* hPlane, double distance/*, userData=NULL*/);

int HPlane_IterateIntercepts2(HPlane* bi, int (*callback)(HPlaneIntercept*, void*), void* parameters);
int HPlane_IterateIntercepts(HPlane* bi, int (*callback)(HPlaneIntercept*, void*)/*, parameters=NULL*/);

#if _DEBUG
void HPlaneIntercept_Print(HPlane* hPlane);
#endif

void BSP_InitHPlaneInterceptAllocator(void);
void BSP_ShutdownIntersectionAllocator(void);

/**
 * @todo the following functions do not belong in this module.
 */

void Bsp_MergeIntersections(HPlane* intersections);
void Bsp_BuildHEdgesAtIntersectionGaps(HPlane* hPlane,
    struct superblock_s* rightList, struct superblock_s* leftList);

#endif /// LIBDENG_MAP_BSP_INTERSECTION
