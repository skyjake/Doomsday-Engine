/**
 * @file bsp_superblock.h
 * BSP Builder SuperBlock. @ingroup map
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
#include "bsp_edge.h"

struct superblock_s; // The SuperBlock instance (opaque).

/**
 * SuperBlock instance. Created with SuperBlock_New().
 */
typedef struct superblock_s SuperBlock;

/**
 * Constructs a new superblock. The superblock must be destroyed with
 * SuperBlock_Delete() when no longer needed.
 */
SuperBlock* SuperBlock_New(const AABox* bounds);

/**
 * Destroys the superblock.
 */
void SuperBlock_Delete(SuperBlock* superblock);

/**
 * Retrieve the axis-aligned bounding box defined for this superblock
 * during instantiation. Note that this is NOT the bounds defined by
 * the linked HEdges' vertices (@see SuperBlock_FindHEdgeListBounds()).
 *
 * @param superblock    SuperBlock instance.
 * @return  Axis-aligned bounding box.
 */
const AABox* SuperBlock_Bounds(SuperBlock* superblock);

/**
 * Push (link) the given HEdge onto the FIFO list of half-edges linked
 * to this superblock.
 *
 * @param superblock    SuperBlock instance.
 * @param hEdge  HEdge instance to add.
 */
void SuperBlock_HEdgePush(SuperBlock* superblock, bsp_hedge_t* hEdge);

/**
 * Pop (unlink) the next HEdge from the FIFO list of half-edges linked
 * to this superblock.
 *
 * @param superblock    SuperBlock instance.
 *
 * @return  Previous top-most HEdge instance or @c NULL if empty.
 */
bsp_hedge_t* SuperBlock_HEdgePop(SuperBlock* superblock);

/**
 * Retrieve the total number of HEdges linked in this superblock (including
 * any within child superblocks).
 *
 * @param superblock    SuperBlock instance.
 * @param addReal       Include the "real" half-edges in the total.
 * @param addMini       Include the "mini" half-edges in the total.
 *
 * @return  Total HEdge count.
 */
uint SuperBlock_HEdgeCount(SuperBlock* superblock, boolean addReal, boolean addMini);

/// Convenience macros for retrieving the HEdge totals:
#define SuperBlock_MiniHEdgeCount(sb)  SuperBlock_HEdgeCount((sb), false, true)
#define SuperBlock_RealHEdgeCount(sb)  SuperBlock_HEdgeCount((sb), true, false)
#define SuperBlock_TotalHEdgeCount(sb) SuperBlock_HEdgeCount((sb), true, true)

/**
 * Iterate over all HEdges linked in this superblock. Iteration ends
 * when all HEdges have been visited or a callback returns non-zero.
 *
 * @param superblock    SuperBlock instance.
 * @param callback      Callback function ptr.
 * @param parameters    Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int SuperBlock_IterateHEdges2(SuperBlock* superblock, int (*callback)(bsp_hedge_t*, void*), void* parameters);
int SuperBlock_IterateHEdges(SuperBlock* superblock, int (*callback)(bsp_hedge_t*, void*)/*, parameters=NULL*/);

/**
 * Retrieve a pointer to a sub-block of this superblock.
 *
 * @param superblock    SuperBlock instance.
 * @param left          @c true= pick the "left" child.
 *
 * @return  Selected child superblock else @c NULL if none.
 */
SuperBlock* SuperBlock_Child(SuperBlock* superblock, boolean left);

int SuperBlock_Traverse2(SuperBlock* superblock, int (*callback)(SuperBlock*, void*), void* parameters);
int SuperBlock_Traverse(SuperBlock* superblock, int (*callback)(SuperBlock*, void*)/*, parameters=NULL*/);

/**
 * Find the axis-aligned bounding box defined by the vertices of all
 * HEdges within this superblock. If no HEdges are linked then @a bounds
 * will be set to the "cleared" state (i.e., min[x,y] > max[x,y]).
 *
 * @param superblock    SuperBlock instance.
 * @param bounds        Determined bounds are written here.
 */
void SuperBlock_FindHEdgeListBounds(SuperBlock* superblock, AABoxf* bounds);

/**
 * @todo The following functions do not belong in this module.
 */

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
