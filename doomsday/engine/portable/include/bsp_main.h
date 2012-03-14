/**
 * @file bsp_main.h
 * BSP Builder. @ingroup map
 *
 * @algorithm High-level description (courtesy of Raphael Quinet)
 *   1 - Create one Seg for each SideDef: pick each LineDef in turn.  If it
 *       has a "first" SideDef, then create a normal Seg.  If it has a
 *       "second" SideDef, then create a flipped Seg.
 *   2 - Call CreateNodes with the current list of Segs.  The list of Segs is
 *       the only argument to CreateNodes.
 *   3 - Save the Nodes, Segs and BspLeafs to disk.  Start with the leaves of
 *       the Nodes tree and continue up to the root (last Node).
 *
 * CreateNodes does the following:
 *   1 - Pick a nodeline amongst the Segs (minimize the number of splits and
 *       keep the tree as balanced as possible).
 *   2 - Move all Segs on the right of the nodeline in a list (segs1) and do
 *       the same for all Segs on the left of the nodeline (in segs2).
 *   3 - If the first list (segs1) contains references to more than one
 *       Sector or if the angle between two adjacent Segs is greater than
 *       180 degrees, then call CreateNodes with this (smaller) list.
 *       Else, create a BspLeaf with all these Segs.
 *   4 - Do the same for the second list (segs2).
 *   5 - Return the new node (its two children are already OK).
 *
 * Each time CreateBspLeaf is called, the Segs are put in a global list.
 * When there is no more Seg in CreateNodes' list, then they are all in the
 * global list and ready to be saved to disk.
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 * @authors Copyright © 1997-1998 Raphael.Quinet <raphael.quinet@eed.ericsson.se>
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

#ifndef LIBDENG_MAP_BSP_BUILDER
#define LIBDENG_MAP_BSP_BUILDER

#include "de_edit.h"

// Degrees, 0 is E, 90 is N
typedef double angle_g;

struct bspintersections_s;
struct superblock_s;
struct bspartition_s;
struct bsp_hedge_s;

/**
 * An "intercept" remembers the vertex that touches a BS partition line (especially
 * a new vertex that is created at a twin-edge split).
 */
typedef struct hedgeintercept_s {
    // Vertex in question.
    Vertex* vertex;

    // True if this intersection was on a self-referencing linedef.
    boolean selfRef;

    // Sector on each side of the vertex (along the partition),
    // or NULL when that direction isn't OPEN.
    Sector* before;
    Sector* after;
} HEdgeIntercept;

/**
 * Create a new intersection.
 */
HEdgeIntercept* Bsp_NewHEdgeIntercept(Vertex* vertex, const struct bspartition_s* partition,
    boolean lineDefIsSelfReferencing);

/**
 * Destroy the specified intersection.
 *
 * @param inter  Ptr to the intersection to be destroyed.
 */
void Bsp_DeleteHEdgeIntercept(HEdgeIntercept* intercept);

#if _DEBUG
void Bsp_PrintHEdgeIntercept(HEdgeIntercept* intercept);
#endif

/**
 * Search the given list for an intersection, if found; return it.
 *
 * @param list  The list to be searched.
 * @param vert  Ptr to the intersection vertex to look for.
 *
 * @return  Ptr to the found intersection, else @c NULL;
 */
HEdgeIntercept* Bsp_HEdgeInterceptByVertex(struct bspintersections_s* bspIntersections, Vertex* v);

/**
 * Analyze the intersection list, and add any needed minihedges to the given half-edge lists
 * (one minihedge on each side).
 *
 * @note All the intersections in the bspIntersections will be free'd back into the quick-alloc list.
 */
void BSP_AddMiniHEdges(const struct bspartition_s* part, struct superblock_s* rightList,
    struct superblock_s* leftList, struct bspintersections_s* bspIntersections);

void Bsp_BuildHEdgesBetweenIntersections(const struct bspartition_s* part, HEdgeIntercept* start,
    HEdgeIntercept* end, struct bsp_hedge_s** right, struct bsp_hedge_s** left);

// CVar for tuning the BSP edge split cost factor.
extern int bspFactor;

/**
 * Register the ccmds and cvars of the BSP builder. Called during engine startup
 */
void BSP_Register(void);

/**
 * Build the BSP for the given map.
 *
 * @param map           The map to build the BSP for.
 * @param vertexes      Editable vertex (ptr) array.
 * @param numVertexes   Number of vertexes in the array.
 *
 * @return  @c true= iff completed successfully.
 */
boolean BSP_Build(GameMap* map, Vertex*** vertexes, uint* numVertexes);

#endif /// LIBDENG_MAP_BSP_BUILDER
