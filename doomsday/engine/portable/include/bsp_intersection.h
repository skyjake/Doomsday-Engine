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

struct bspartition_s;
struct superblock_s;

typedef struct bspintersection_s BspIntersection;

BspIntersection* BspIntersection_Next(BspIntersection* intersection);

BspIntersection* BspIntersection_Prev(BspIntersection* intersection);

void* BspIntersection_UserData(BspIntersection* intersection);

/**
 * BspIntersections instance. Created with BspIntersections_New().
 */
typedef struct bspintersections_s BspIntersections;

/**
 * Create a new BspIntersections.
 */
BspIntersections* BspIntersections_New(void);

/**
 * Destroy a BspIntersections.
 */
void BspIntersections_Delete(BspIntersections* bspIntersections);

/**
 * Empty all intersections from the specified BspIntersections.
 */
void BspIntersections_Clear(BspIntersections* bspIntersections);

/**
 * Insert a point at the given intersection into the intersection list.
 */
BspIntersection* BspIntersections_Insert2(BspIntersections* bspIntersections, double distance, void* userData);
BspIntersection* BspIntersections_Insert(BspIntersections* bspIntersections, double distance/*, userData=NULL*/);

int BspIntersections_Iterate2(BspIntersections* bi, int (*callback)(BspIntersection*, void*), void* parameters);
int BspIntersections_Iterate(BspIntersections* bi, int (*callback)(BspIntersection*, void*)/*, parameters=NULL*/);

#if _DEBUG
void BspIntersection_Print(BspIntersections* bspIntersections);
#endif

void BSP_InitIntersectionAllocator(void);
void BSP_ShutdownIntersectionAllocator(void);

/**
 * @todo the following functions do not belong in this module.
 */

void Bsp_MergeIntersections(BspIntersections* intersections);
void Bsp_BuildHEdgesAtIntersectionGaps(BspIntersections* bspIntersections,
    const struct bspartition_s* part, struct superblock_s* rightList, struct superblock_s* leftList);

#endif /// LIBDENG_MAP_BSP_INTERSECTION
