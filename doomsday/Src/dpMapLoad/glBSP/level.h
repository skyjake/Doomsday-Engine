//------------------------------------------------------------------------
// LEVEL : Level structures & read/write functions.
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

#ifndef __GLBSP_LEVEL_H__
#define __GLBSP_LEVEL_H__

#include "structs.h"
#include "wad.h"


struct node_s;
struct sector_s;
struct superblock_s;


// a wall_tip is where a wall meets a vertex
typedef struct wall_tip_s
{
  // link in list.  List is kept in ANTI-clockwise order.
  struct wall_tip_s *next;
  struct wall_tip_s *prev;
  
  // angle that line makes at vertex (degrees).
  angle_g angle;

  // sectors on each side of wall.  Left is the side of increasing
  // angles, right is the side of decreasing angles.  Either can be
  // NULL for one sided walls.
  struct sector_s *left;
  struct sector_s *right;
}
wall_tip_t;


typedef struct vertex_s
{
  // coordinates
  float_g x, y;

  // vertex index.  Always valid after loading and pruning of unused
  // vertices has occurred.  For GL vertices, bit 15 will be set.
  int index;

  // reference count.  When building normal node info, unused vertices
  // will be pruned.
  int ref_count;

  // usually NULL, unless this vertex occupies the same location as a
  // previous vertex.  Only used during the pruning phase.
  struct vertex_s *equiv;

  // set of wall_tips
  wall_tip_t *tip_set;

  // non-zero if this vertex is part of a polyobj.  Only valid during
  // the polyobj detection phase.
  int polyobj;

  // contains a duplicate vertex, needed when both normal and V2 GL
  // nodes are being built at the same time (this is the vertex used
  // for the normal segs).  Normally NULL.  Note: the wall tips on
  // this vertex are not created.
  struct vertex_s *normal_dup;
}
vertex_t;


typedef struct sector_s
{
  // sector index.  Always valid after loading & pruning.
  int index;

  // allow segs from other sectors to coexist in a subsector
  int coalesce;

  // reference count.  When building normal nodes, unused sectors will
  // be pruned.
  int ref_count;

  // heights
  int floor_h, ceil_h;

  // textures
  char floor_tex[8];
  char ceil_tex[8];

  // attributes
  int light;
  int special;
  int tag;

  // -JL- non-zero if this sector contains a polyobj.
  int polyobj;

  // used when building REJECT table.  Each set of sectors that are
  // isolated from other sectors will have a different group number.
  // Thus: on every 2-sided linedef, the sectors on both sides will be
  // in the same group.  The rej_next, rej_prev fields are a link in a
  // RING, containing all sectors of the same group.
  int rej_group;

  struct sector_s *rej_next;
  struct sector_s *rej_prev;
}
sector_t;


typedef struct sidedef_s
{
  // adjacent sector.  Can be NULL (invalid sidedef)
  sector_t *sector;

  // offset values
  int x_offset, y_offset;

  // texture names
  char upper_tex[8];
  char lower_tex[8];
  char mid_tex[8];
  
  // sidedef index.  Always valid after loading & pruning.
  int index;

  // reference count.  When building normal nodes, unused sidedefs will
  // be pruned.
  int ref_count;

  // usually NULL, unless this sidedef is exactly the same as a
  // previous one.  Only used during the pruning phase.
  struct sidedef_s *equiv;

  // this is true if the sidedef is on a special line.  We don't merge
  // these sidedefs together, as they might scroll, or change texture
  // when a switch is pressed.
  int on_special;
}
sidedef_t;


typedef struct linedef_s
{
  // link for list
  struct linedef_s *next;

  vertex_t *start;    // from this vertex...
  vertex_t *end;      // ... to this vertex

  sidedef_t *right;   // right sidedef
  sidedef_t *left;    // left sidede, or NULL if none

  // line is marked two-sided
  int two_sided;

  // prefer not to split
  int is_precious;

  // zero length (line should be totally ignored)
  int zero_len;

  int flags;
  int type;
  int tag;

  // Hexen support
  int specials[5];

  // part of a hexen polyobj
  int polyobj;
 
  // linedef index.  Always valid after loading & pruning of zero
  // length lines has occurred.
  int index;
}
linedef_t;


typedef struct seg_s
{
  // link for list
  struct seg_s *next;

  vertex_t *start;   // from this vertex...
  vertex_t *end;     // ... to this vertex

  // linedef that this seg goes along, or NULL if miniseg
  linedef_t *linedef;

  // adjacent sector, or NULL if invalid sidedef or miniseg
  sector_t *sector;

  // 0 for right, 1 for left
  int side;

  // seg on other side, or NULL if one-sided.  This relationship is
  // always one-to-one -- if one of the segs is split, the partner seg
  // must also be split.
  struct seg_s *partner;

  // seg index.  Only valid once the seg has been added to a
  // subsector.  A negative value means it is invalid -- there
  // shouldn't be any of these once the BSP tree has been built.
  int index;

  // when 1, this seg has become zero length (integer rounding of the
  // start and end vertices produces the same location).  It should be
  // ignored when writing the SEGS or V1 GL_SEGS lumps.  [Note: there
  // won't be any of these when writing the V2 GL_SEGS lump].
  int degenerate;
 
  // the superblock that contains this seg, or NULL if the seg is no
  // longer in any superblock (e.g. now in a subsector).
  struct superblock_s *block;

  // precomputed data for fast calculations
  float_g psx, psy;
  float_g pex, pey;
  float_g pdx, pdy;

  float_g p_length;
  float_g p_angle;
  float_g p_para;
  float_g p_perp;

  // linedef that this seg initially comes from.  For "real" segs,
  // this is just the same as the `linedef' field above.  For
  // "minisegs", this is the linedef of the partition line.
  linedef_t *source_line;
}
seg_t;


typedef struct subsec_s
{
  // list of segs
  seg_t *seg_list;

  // count of segs
  int seg_count;

  // subsector index.  Always valid, set when the subsector is
  // initially created.
  int index;

  // approximate middle point
  float_g mid_x;
  float_g mid_y;
}
subsec_t;


typedef struct bbox_s
{
  int minx, miny;
  int maxx, maxy;
}
bbox_t;


typedef struct child_s
{
  // child node or subsector (one must be NULL)
  struct node_s *node;
  subsec_t *subsec;

  // child bounding box
  bbox_t bounds;
}
child_t;


typedef struct node_s
{
  int x, y;     // starting point
  int dx, dy;   // offset to ending point

  // right & left children
  child_t r;
  child_t l;

  // node index.  Only valid once the NODES or GL_NODES lump has been
  // created.
  int index;

  // the node is too long, and the (dx,dy) values should be halved
  // when writing into the NODES lump.
  int too_long;
}
node_t;


typedef struct superblock_s
{
  // parent of this block, or NULL for a top-level block
  struct superblock_s *parent;

  // coordinates on map for this block, from lower-left corner to
  // upper-right corner.  Pseudo-inclusive, i.e (x,y) is inside block
  // if and only if x1 <= x < x2 and y1 <= y < y2.
  int x1, y1;
  int x2, y2;

  // sub-blocks.  NULL when empty.  [0] has the lower coordinates, and
  // [1] has the higher coordinates.  Division of a square always
  // occurs horizontally (e.g. 512x512 -> 256x512 -> 256x256).
  struct superblock_s *subs[2];

  // number of real segs and minisegs contained by this block
  // (including all sub-blocks below it).
  int real_num;
  int mini_num;

  // list of segs completely contained by this block.
  seg_t *segs;
}
superblock_t;

#define SUPER_IS_LEAF(s)  \
    ((s)->x2-(s)->x1 <= 256 && (s)->y2-(s)->y1 <= 256)


/* ----- Level data arrays ----------------------- */

extern int num_vertices;
extern int num_linedefs;
extern int num_sidedefs;
extern int num_sectors;
extern int num_segs;
extern int num_subsecs;
extern int num_nodes;

extern int num_normal_vert;
extern int num_gl_vert;
extern int num_complete_seg;


/* ----- function prototypes ----------------------- */

// allocation routines
vertex_t *NewVertex(void);
linedef_t *NewLinedef(void);
sidedef_t *NewSidedef(void);
sector_t *NewSector(void);
seg_t *NewSeg(void);
subsec_t *NewSubsec(void);
node_t *NewNode(void);

// lookup routines
vertex_t *LookupVertex(int index);
linedef_t *LookupLinedef(int index);
sidedef_t *LookupSidedef(int index);
sector_t *LookupSector(int index);
seg_t *LookupSeg(int index);
subsec_t *LookupSubsec(int index);
node_t *LookupNode(int index);

// check whether the current level already has normal nodes
int CheckForNormalNodes(void);

// load all level data for the current level
void LoadLevel(void);

// free all level data
void FreeLevel(void);

// save the newly computed NODE info etc..
void SaveLevel(node_t *root_node);

// return a new vertex (with correct wall_tip info) for the split that
// happens along the given seg at the given location.
//
vertex_t *NewVertexFromSplitSeg(seg_t *seg, float_g x, float_g y);

// return a new end vertex to compensate for a seg that would end up
// being zero-length (after integer rounding).  Doesn't compute the
// wall_tip info (thus this routine should only be used _after_ node
// building).
//
vertex_t *NewVertexDegenerate(vertex_t *start, vertex_t *end);

// check whether a line with the given delta coordinates and beginning
// at this vertex is open.  Returns 1 if open, or 0 if closed.  The
// sectors that lie on the left & right side of the given line are
// also determined (NULL if the area is void space).
//
int VertexCheckOpen(vertex_t *vert, float_g dx, float_g dy,
    sector_t ** left_sec, sector_t ** right_sec);


#endif /* __GLBSP_LEVEL_H__ */
