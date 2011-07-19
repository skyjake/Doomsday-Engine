/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 *\author Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 *\author Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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
 * bsp_intersection.h: Segment intersections.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

#ifndef __BSP_INTERSECTION_H__
#define __BSP_INTERSECTION_H__

#include "bsp_edge.h"

typedef void* cutlist_t;

struct bspartition_s;

/**
 * An "intersection" remembers the vertex that touches a BSP divider
 * line (especially a new vertex that is created at a twin-edge split).
 */
typedef struct intersection_s {
    // Vertex in question.
    vertex_t*   vertex;

    // How far along the partition line the vertex is. Zero is at the
    // partition half-edge's start point, positive values move in the same
    // direction as the partition's direction, and negative values move
    // in the opposite direction.
    double      alongDist;

    // True if this intersection was on a self-referencing linedef.
    boolean     selfRef;

    // Sector on each side of the vertex (along the partition),
    // or NULL when that direction isn't OPEN.
    sector_t*   before;
    sector_t*   after;
} intersection_t;

void        BSP_InitIntersectionAllocator(void);
void        BSP_ShutdownIntersectionAllocator(void);

intersection_t* BSP_IntersectionCreate(vertex_t* vert,
                                       const struct bspartition_s* part,
                                       boolean selfRef);
void        BSP_IntersectionDestroy(intersection_t* cut);
#if _DEBUG
void        BSP_IntersectionPrint(intersection_t* cut);
#endif

// Cutlist functions:
cutlist_t*  BSP_CutListCreate(void);
void        BSP_CutListDestroy(cutlist_t* cutList);

void        BSP_CutListEmpty(cutlist_t* cutList);
void        BSP_CutListPrint(cutlist_t* cutList);

boolean     BSP_CutListInsertIntersection(cutlist_t* cutList,
                                          intersection_t* cut);
intersection_t* BSP_CutListFindIntersection(cutlist_t* cutList, vertex_t* v);
#endif
