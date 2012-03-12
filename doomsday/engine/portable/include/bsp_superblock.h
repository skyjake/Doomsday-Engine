/**
 * @file bsp_superblock.h
 * BSP Builder Superblock. @ingroup map
 *
 * Design is effectively that of a 2-dimensional kd-tree.
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

#ifndef LIBDENG_MAP_BSP_SUPERBLOCK
#define LIBDENG_MAP_BSP_SUPERBLOCK

#include "de_platform.h"
#include "bsp_intersection.h"

typedef struct superblock_s {
    // Parent of this block, or NULL for a top-level block.
    struct superblock_s* parent;

    // Coordinates on map for this block, from lower-left corner to
    // upper-right corner. Pseudo-inclusive, i.e (x,y) is inside block
    // if and only if minX <= x < maxX and minY <= y < maxY.
    AABox aaBox;

    // Sub-blocks. NULL when empty. [0] has the lower coordinates, and
    // [1] has the higher coordinates. Division of a square always
    // occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
    struct superblock_s* subs[2];

    // Number of real half-edges and minihedges contained by this block
    // (including all sub-blocks below it).
    int realNum;
    int miniNum;

    // List of half-edges completely contained by this block.
    struct bsp_hedge_s* hEdges;
} superblock_t;

/**
 * Init the superblock allocator.
 */
void BSP_InitSuperBlockAllocator(void);

/**
 * Free all the superblocks on the quick-alloc list.
 */
void BSP_ShutdownSuperBlockAllocator(void);

superblock_t* BSP_SuperBlockCreate(void);

void BSP_SuperBlockDestroy(superblock_t* block);

#if _DEBUG
void BSP_SuperBlockPrintHEdges(superblock_t* superblock);
#endif

superblock_t* CreateHEdges(void);

/**
 * Link the given half-edge into the given superblock.
 */
void BSP_LinkHEdgeToSuperBlock(superblock_t* superblock, bsp_hedge_t* hEdge);

/**
 * Increase the counts within the superblock, to account for the given
 * half-edge being split.
 */
void BSP_IncSuperBlockHEdgeCounts(superblock_t* block, boolean lineLinked);

/**
 * Find the best half-edge in the list to use as a partition.
 *
 * @param hEdgeList     List of half-edges to choose from.
 * @param depth         Current node depth.
 * @param partition     Ptr to partition to be updated with the results.
 *
 * @return  @c true= A suitable partition was found.
 */
boolean BSP_PickPartition(const superblock_t* hEdgeList, size_t depth, bspartition_t* partition);

/**
 * Find the extremes of a box containing all half-edges.
 */
void BSP_FindNodeBounds(bspnodedata_t* node, superblock_t* hEdgeListRight, superblock_t* hEdgeListLeft);

/**
 * Partition the given edge and perform any further necessary action (moving it into
 * either the left list, right list, or splitting it).
 *
 * Take the given half-edge 'cur', compare it with the partition line, and determine it's
 * fate: moving it into either the left or right lists (perhaps both, when splitting it
 * in two). Handles the twin as well. Updates the intersection list if the half-edge lies
 * on or crosses the partition line.
 *
 * @note AJA: I have rewritten this routine based on evalPartition() (which I've also
 *       reworked, heavily). I think it is important that both these routines follow the
 *       exact same logic.
 */
void BSP_DivideOneHEdge(bsp_hedge_t* hEdge, const bspartition_t* part, superblock_t* rightList,
    superblock_t* leftList, cutlist_t* cutList);

/**
 * Remove all the half-edges from the list, partitioning them into the left or right lists
 * based on the given partition line. Adds any intersections onto the intersection list as
 * it goes.
 */
void BSP_PartitionHEdges(superblock_t* hEdgeList, const bspartition_t* part, superblock_t* rightList,
    superblock_t* leftList, cutlist_t* cutList);

/**
 * Analyze the intersection list, and add any needed minihedges to the given half-edge lists
 * (one minihedge on each side).
 *
 * @note All the intersections in the cutList will be free'd back into the quick-alloc list.
 */
void BSP_AddMiniHEdges(const bspartition_t* part, superblock_t* rightList,
    superblock_t* leftList, cutlist_t* cutList);

#endif /// LIBDENG_MAP_BSP_SUPERBLOCK
