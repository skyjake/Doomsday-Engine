//------------------------------------------------------------------------
// SEG : Choose the best Seg to use for a node line.
//------------------------------------------------------------------------
//
//  GL-Friendly Node Builder (C) 2000-2002 Andrew Apted
//
//  Based on `BSP 2.3' by Colin Reed, Lee Killough and others.
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

#ifndef __GLBSP_SEG_H__
#define __GLBSP_SEG_H__

#include "structs.h"


#define DEFAULT_FACTOR  7

#define IFFY_LEN  4.0


// smallest distance between two points before being considered equal
#define DIST_EPSILON  (1.0 / 128.0)

// smallest degrees between two angles before being considered equal
#define ANG_EPSILON  (1.0 / 1024.0)


typedef struct intersect_tip_s
{
  // is open ?  Otherwise closed.
  int open;

  // sectors on left & right side of partition line.  Can be NULL.
  sector_t *left;
  sector_t *right;
}
intersect_tip_t;

// an "intersection" remembers the vertex that touches a BSP divider
// line (especially a new vertex that is created at a seg split).

typedef struct intersection_s
{
  // link in list.  The intersection list is kept sorted by
  // `along_dist', in ascending order.
  struct intersection_s *next;
  struct intersection_s *prev;

  // vertex in question
  vertex_t *vertex;

  // how far along the partition line the vertex is.  Zero is at the
  // partition seg's start point, positive values move in the same
  // direction as the partition's direction, and negative values move
  // in the opposite direction.
  float_g along_dist;

  // info for left & right sides of vertex (along the partition)
  intersect_tip_t l;
  intersect_tip_t r;
}
intersection_t;


/* -------- functions ---------------------------- */

// scan all the segs in the list, and choose the best seg to use as a
// partition line, returning it.  If no seg can be used, returns NULL.
// The `depth' parameter is the current depth in the tree, for values
// <= 2 the routine should show 20 progress points.
//
seg_t *PickNode(superblock_t *seg_list, int depth);

// compute angle & distance from (0,0) to (dx,dy)
angle_g ComputeAngle(float_g dx, float_g dy);
#define ComputeDist(dx,dy)  sqrt((dx) * (dx) + (dy) * (dy))

// compute the parallel and perpendicular distances from a partition
// line to a point.
//
#define ComputeParallelDist(part,x,y)  \
    (((x) * (part)->pdx + (y) * (part)->pdy + (part)->p_para)  \
     / (part)->p_length)

#define ComputePerpDist(part,x,y)  \
    (((x) * (part)->pdy - (y) * (part)->pdx + (part)->p_perp)  \
     / (part)->p_length)

// compute the boundary of the list of segs
void FindLimits(superblock_t *seg_list, bbox_t *bbox);

// compute the seg private info (psx/y, pex/y, pdx/y, etc).
void RecomputeSeg(seg_t *seg);

// take the given seg `cur', compare it with the partition line, and
// determine it's fate: moving it into either the left or right lists
// (perhaps both, when splitting it in two).  Handles partners as
// well.  Updates the intersection list if the seg lies on or crosses
// the partition line.
//
void DivideOneSeg(seg_t *cur, seg_t *part, 
    superblock_t *left_list, superblock_t *right_list,
    intersection_t ** cut_list);

// remove all the segs from the list, partitioning them into the left
// or right lists based on the given partition line.  Adds any
// intersections onto the intersection list as it goes.
//
void SeparateSegs(superblock_t *seg_list, seg_t *part,
    superblock_t *left_list, superblock_t *right_list,
    intersection_t ** cut_list);

// analyse the intersection list, and add any needed minisegs to the
// given seg lists (one miniseg on each side).  All the intersection
// structures will be freed back into a quick-alloc list.
//
void AddMinisegs(seg_t *part, 
    superblock_t *left_list, superblock_t *right_list, 
    intersection_t *cut_list);

// free the quick allocation cut list
void FreeQuickAllocCuts(void);


#endif /* __GLBSP_SEG_H__ */
