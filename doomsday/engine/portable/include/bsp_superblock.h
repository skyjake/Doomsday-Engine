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
 * bsp_superblock.h: Blockmap for half-edges
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 */

#ifndef __BSP_SUPERBLOCK_H__
#define __BSP_SUPERBLOCK_H__

#include "bsp_intersection.h"

typedef struct superblock_s {
    // Parent of this block, or NULL for a top-level block.
    struct superblock_s* parent;

    // Coordinates on map for this block, from lower-left corner to
    // upper-right corner. Pseudo-inclusive, i.e (x,y) is inside block
    // if and only if BOXLEFT <= x < BOXRIGHT and BOXBOTTOM <= y < BOXTOP.
    int         bbox[4];

    // Sub-blocks. NULL when empty. [0] has the lower coordinates, and
    // [1] has the higher coordinates. Division of a square always
    // occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
    struct superblock_s* subs[2];

    // Number of real half-edges and minihedges contained by this block
    // (including all sub-blocks below it).
    int         realNum;
    int         miniNum;

    // List of half-edges completely contained by this block.
    struct hedge_s* hEdges;
} superblock_t;

void        BSP_InitSuperBlockAllocator(void);
void        BSP_ShutdownSuperBlockAllocator(void);

superblock_t* BSP_SuperBlockCreate(void);
void        BSP_SuperBlockDestroy(superblock_t* block);

#if _DEBUG
void        BSP_SuperBlockPrintHEdges(superblock_t* superblock);
#endif

superblock_t* CreateHEdges(void);

void        BSP_LinkHEdgeToSuperBlock(superblock_t* superblock,
                                      hedge_t* hEdge);
void        BSP_IncSuperBlockHEdgeCounts(superblock_t* block,
                                         boolean lineLinked);

boolean     BSP_PickPartition(const superblock_t* hEdgeList, size_t depth,
                              bspartition_t* partition);
void        BSP_FindNodeBounds(bspnodedata_t* node, superblock_t* hEdgeListRight,
                               superblock_t* hEdgeListLeft);
void        BSP_DivideOneHEdge(hedge_t* hEdge, const bspartition_t* part,
                               superblock_t* rightList, superblock_t* leftList,
                               cutlist_t* cutList);
void        BSP_PartitionHEdges(superblock_t* hEdgeList,
                                const bspartition_t* part,
                                superblock_t* rightList, superblock_t* leftList,
                                cutlist_t* cutList);
void        BSP_AddMiniHEdges(const bspartition_t* part, superblock_t* rightList,
                              superblock_t* leftList, cutlist_t* cutList);
#endif
