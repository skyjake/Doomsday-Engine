/**
 * @file superblockmap.h
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

#include "dd_types.h"
//#include "de_platform.h"
//#include "bsp_edge.h"
#include "m_misc.h"

struct bsp_hedge_s;

#ifdef __cplusplus
extern "C" {
#endif

#include "kdtree.h"

struct superblockmap_s; // The SuperBlockmap instance (opaque).
struct superblock_s; // The SuperBlock instance (opaque).

/**
 * SuperBlockmap instance. Created with SuperBlockmap_New().
 */
typedef struct superblockmap_s SuperBlockmap;

/**
 * SuperBlock instance. Created with SuperBlock_New().
 */
typedef struct superblock_s SuperBlock;

/**
 * Constructs a new superBlockmap. The superBlockmap must be destroyed with
 * SuperBlockmap_Delete() when no longer needed.
 */
SuperBlockmap* SuperBlockmap_New(const AABox* bounds);

/**
 * Destroys the superBlockmap.
 *
 * @param superBlockmap  SuperBlockmap instance.
 */
void SuperBlockmap_Delete(SuperBlockmap* superBlockmap);

/**
 * Retrieve the root SuperBlock in this SuperBlockmap.
 *
 * @param superBlockmap  SuperBlockmap instance.
 * @return  Root SuperBlock instance.
 */
SuperBlock* SuperBlockmap_Root(SuperBlockmap* superBlockmap);

/**
 * Find the axis-aligned bounding box defined by the vertices of all
 * HEdges within this superblock. If no HEdges are linked then @a bounds
 * will be set to the "cleared" state (i.e., min[x,y] > max[x,y]).
 *
 * @param superBlockmap SuperBlock instance.
 * @param bounds        Determined bounds are written here.
 */
void SuperBlockmap_FindHEdgeBounds(SuperBlockmap* superBlockmap, AABoxf* bounds);

int SuperBlockmap_PostTraverse2(SuperBlockmap* superBlockmap, int(*callback)(SuperBlock*, void*), void* parameters);
int SuperBlockmap_PostTraverse(SuperBlockmap* superBlockmap, int(*callback)(SuperBlock*, void*)/*, parameters = NULL*/);

/**
 * Retrieve the SuperBlockmap which owns this block.
 * @param superblock  SuperBlock instance.
 * @return  SuperBlockmap instance which owns this.
 */
SuperBlockmap* SuperBlock_Blockmap(SuperBlock* superblock);

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
void SuperBlock_HEdgePush(SuperBlock* superblock, struct bsp_hedge_s* hEdge);

/**
 * Pop (unlink) the next HEdge from the FIFO list of half-edges linked
 * to this superblock.
 *
 * @param superblock    SuperBlock instance.
 *
 * @return  Previous top-most HEdge instance or @c NULL if empty.
 */
struct bsp_hedge_s* SuperBlock_HEdgePop(SuperBlock* superblock);

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
int SuperBlock_IterateHEdges2(SuperBlock* superblock, int (*callback)(struct bsp_hedge_s*, void*), void* parameters);
int SuperBlock_IterateHEdges(SuperBlock* superblock, int (*callback)(struct bsp_hedge_s*, void*)/*, parameters=NULL*/);

/**
 * Retrieve a pointer to a sub-block of this superblock.
 *
 * @param superblock    SuperBlock instance.
 * @param left          non-zero= pick the "left" child.
 *
 * @return  Selected child superblock else @c NULL if none.
 */
SuperBlock* SuperBlock_Child(SuperBlock* superblock, int left);

int SuperBlock_Traverse2(SuperBlock* superblock, int (*callback)(SuperBlock*, void*), void* parameters);
int SuperBlock_Traverse(SuperBlock* superblock, int (*callback)(SuperBlock*, void*)/*, parameters=NULL*/);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_BSP_SUPERBLOCK
