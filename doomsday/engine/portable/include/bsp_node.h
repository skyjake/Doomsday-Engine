/**
 * @file bsp_node.h
 * BSP Builder Node. @ingroup map
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

#ifndef LIBDENG_MAP_BSP_NODE
#define LIBDENG_MAP_BSP_NODE

#include "bsp_edge.h"
#include "bsp_intersection.h"
#include "m_binarytree.h"
#include "p_mapdata.h"

typedef struct bspartition_s {
    double x, y;
    double dX, dY;
    double length;
    LineDef* lineDef; // Not NULL if partition originated from a linedef.
    LineDef* sourceLineDef;

    double pSX, pSY;
    double pDX, pDY;
    double pPara, pPerp;
} bspartition_t;

typedef struct bspnodedata_s {
    partition_t partition;
    float bBox[2][4]; // Bounding box for each child.
    // Node index. Only valid once the NODES or GL_NODES lump has been
    // created.
    int index;
} bspnodedata_t;

/**
 * Takes the half-edge list and determines if it is convex, possibly converting
 * it into a BSP leaf. Otherwise, the list is divided into two halves and recursion
 * will continue on the new sub list.
 *
 * This is done by scanning all of the half-edges and finding the one that does
 * the least splitting and has the least difference in numbers of half-edges on
 * either side.
 *
 * If the ones on the left side make a BspLeaf, then create another BspLeaf
 * else put the half-edges into the left list.
 *
 * If the ones on the right side make a BspLeaf, then create another BspLeaf
 * else put the half-edges into the right list.
 *
 * @param hEdgeList     Ptr to the list of half edges at the current node.
 * @param parent        Ptr to write back the address of any newly created subtree.
 * @param depth         Current tree depth.
 * @param cutList       Ptr to the cutlist to use for storing new intersections (cuts).
 * @return  @c true iff successfull.
 */
boolean BuildNodes(struct superblock_s* hEdgeList, binarytree_t** parent, size_t depth,
    cutlist_t* cutList);

void BSP_AddHEdgeToSuperBlock(struct superblock_s* block, bsp_hedge_t* hEdge);

/**
 * Traverse the BSP tree and put all the half-edges in each BSP leaf into
 * clockwise order, and renumber their indices.
 *
 * @important This cannot be done during BuildNodes() since splitting a half-edge
 * with a twin may insert another half-edge into that twin's list, usually in the
 * wrong place order-wise.
 */
void ClockwiseBspTree(binarytree_t* rootNode);

void SaveMap(GameMap* dest, void* rootNode, Vertex*** vertexes, uint* numVertexes);

typedef struct bspleafdata_s {
    struct bsp_hedge_s* hEdges; // Head ptr to a list of half-edges at this leaf.
} bspleafdata_t;

bspleafdata_t* BSPLeaf_Create(void);

void BSPLeaf_Destroy(bspleafdata_t *leaf);

#endif /// LIBDENG_MAP_BSP_NODE
