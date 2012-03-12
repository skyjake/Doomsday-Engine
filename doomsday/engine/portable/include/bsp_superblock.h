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

struct bspartition_s;

struct superblock_s; // The SuperBlock instance (opaque).

/**
 * SuperBlock instance. Created with Uri_New() or one of the other constructors.
 */
typedef struct superblock_s SuperBlock;

SuperBlock* SuperBlock_New(const AABox* bounds);

void SuperBlock_Delete(SuperBlock* superblock);

const AABox* SuperBlock_Bounds(SuperBlock* superblock);

void SuperBlock_HEdgePush(SuperBlock* superblock, bsp_hedge_t* hEdge);

bsp_hedge_t* SuperBlock_HEdgePop(SuperBlock* superblock);

/**
 * Increase the counts within the superblock, to account for the given
 * half-edge being split.
 */
void SuperBlock_IncrementHEdgeCounts(SuperBlock* block, boolean lineLinked);

uint SuperBlock_HEdgeCount(SuperBlock* superblock, boolean addReal, boolean addMini);

#define SuperBlock_MiniHEdgeCount(sb)  SuperBlock_HEdgeCount((sb), false, true)
#define SuperBlock_RealHEdgeCount(sb)  SuperBlock_HEdgeCount((sb), true, false)
#define SuperBlock_TotalHEdgeCount(sb) SuperBlock_HEdgeCount((sb), true, true)

int SuperBlock_IterateHEdges(SuperBlock* superblock, int (*callback)(bsp_hedge_t*, void*), void* parameters);

#if _DEBUG
void SuperBlock_PrintHEdges(SuperBlock* superblock);
#endif

SuperBlock* SuperBlock_Child(SuperBlock* superblock, boolean left);

int SuperBlock_Traverse(SuperBlock* superblock, int (*callback)(SuperBlock*, void*), void* parameters);

/**
 * Find the extremes of a box containing all half-edges.
 */
void SuperBlock_FindHEdgeListBounds(SuperBlock* superblock, AABoxf* bounds);

/**
 * Init the superblock allocator.
 */
void BSP_InitSuperBlockAllocator(void);

/**
 * Free all the superblocks on the quick-alloc list.
 */
void BSP_ShutdownSuperBlockAllocator(void);

SuperBlock* BSP_NewSuperBlock(const AABox* bounds);
void BSP_RecycleSuperBlock(SuperBlock* superblock);

#endif /// LIBDENG_MAP_BSP_SUPERBLOCK
