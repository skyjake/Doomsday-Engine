//------------------------------------------------------------------------
// NODE : Recursively create nodes and return the pointers.
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2007 Andrew Apted
//
//  Based on 'BSP 2.3' by Colin Reed, Lee Killough and others.
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#ifndef __GLBSP_NODE_H__
#define __GLBSP_NODE_H__

#include "structs.h"

// check the relationship between the given box and the partition
// line.  Returns -1 if box is on left side, +1 if box is on right
// size, or 0 if the line intersects the box.
//
int BoxOnLineSide(superblock_t *box, seg_t *part);

// add the seg to the given list
void AddSegToSuper(superblock_t *block, seg_t *seg);

// increase the counts within the superblock, to account for the given
// seg being split.
//
void SplitSegInSuper(superblock_t *block, seg_t *seg);

// scan all the linedef of the level and convert each sidedef into a
// seg (or seg pair).  Returns the list of segs.
//
superblock_t *CreateSegs(void);

// free a super block.
void FreeSuper(superblock_t *block);

// takes the seg list and determines if it is convex.  When it is, the
// segs are converted to a subsector, and '*S' is the new subsector
// (and '*N' is set to NULL).  Otherwise the seg list is divided into
// two halves, a node is created by calling this routine recursively,
// and '*N' is the new node (and '*S' is set to NULL).  Normally
// returns GLBSP_E_OK, or GLBSP_E_Cancelled if user stopped it.
//
glbsp_ret_e BuildNodes(superblock_t *seg_list,
    node_t ** N, subsec_t ** S, int depth, node_t *stale_nd);

// compute the height of the bsp tree, starting at 'node'.
int ComputeBspHeight(node_t *node);

// traverse the BSP tree and put all the segs in each subsector into
// clockwise order, and renumber the seg indices.  This cannot be done
// DURING BuildNodes() since splitting a seg with a partner may insert
// another seg into that partner's list -- usually in the wrong place
// order-wise.
//
void ClockwiseBspTree(node_t *root);

// traverse the BSP tree and do whatever is necessary to convert the
// node information from GL standard to normal standard (for example,
// removing minisegs).
//
void NormaliseBspTree(node_t *root);

// traverse the BSP tree, doing whatever is necessary to round
// vertices to integer coordinates (for example, removing segs whose
// rounded coordinates degenerate to the same point).
//
void RoundOffBspTree(node_t *root);

// free all the superblocks on the quick-alloc list
void FreeQuickAllocSupers(void);


#endif /* __GLBSP_NODE_H__ */
