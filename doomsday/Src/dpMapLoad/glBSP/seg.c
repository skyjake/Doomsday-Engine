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
//
// To be able to divide the nodes down, this routine must decide which
// is the best Seg to use as a nodeline. It does this by selecting the
// line with least splits and has least difference of Segs on either
// side of it.
//
// Credit to Raphael Quinet and DEU, this routine is a copy of the
// nodeline picker used in DEU5beta. I am using this method because
// the method I originally used was not so good.
//
// Rewritten by Lee Killough to significantly improve performance,
// while not affecting results one bit in >99% of cases (some tiny
// differences due to roundoff error may occur, but they are
// insignificant).
//
// Rewritten again by Andrew Apted (-AJA-), 1999-2000.
//

#include "system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include "blockmap.h"
#include "level.h"
#include "node.h"
#include "seg.h"
#include "structs.h"
#include "util.h"
#include "wad.h"


#define PRECIOUS_MULTIPLY  64


#define DEBUG_PICKNODE  0
#define DEBUG_SPLIT     0
#define DEBUG_CUTLIST   0


typedef struct eval_info_s
{
  int cost;
  int splits;
  int iffy;
  int near_miss;

  int real_left;
  int real_right;
  int mini_left;
  int mini_right;
}
eval_info_t;


static intersection_t *quick_alloc_cuts = NULL;


//
// ComputeAngle
//
// Translate (dx, dy) into an angle value (degrees)
//
angle_g ComputeAngle(float_g dx, float_g dy)
{
  double angle;

  if (dx == 0)
    return (dy > 0) ? 90.0 : 270.0;

  angle = atan2((double) dy, (double) dx) * 180.0 / M_PI;

  if (angle < 0) 
    angle += 360.0;

  return angle;
}


//
// NewIntersection
//
static intersection_t *NewIntersection(void)
{
  intersection_t *cut;

  if (quick_alloc_cuts)
  {
    cut = quick_alloc_cuts;
    quick_alloc_cuts = cut->next;
  }
  else
  {
    cut = UtilCalloc(sizeof(intersection_t));
  }

  return cut;
}

//
// FreeQuickAllocCuts
//
void FreeQuickAllocCuts(void)
{
  while (quick_alloc_cuts)
  {
    intersection_t *cut = quick_alloc_cuts;
    quick_alloc_cuts = cut->next;

    UtilFree(cut);
  }
}


//
// RecomputeSeg
//
// Fill in the fields `angle', `len', `pdx', `pdy', etc...
//
void RecomputeSeg(seg_t *seg)
{
  seg->psx = seg->start->x;
  seg->psy = seg->start->y;
  seg->pex = seg->end->x;
  seg->pey = seg->end->y;
  seg->pdx = seg->pex - seg->psx;
  seg->pdy = seg->pey - seg->psy;
  
  seg->p_length = ComputeDist(seg->pdx, seg->pdy);
  seg->p_angle = ComputeAngle(seg->pdx, seg->pdy);

  if (seg->p_length <= 0)
    InternalError("Seg %p has zero p_length.", seg);

  seg->p_perp =  seg->psy * seg->pdx - seg->psx * seg->pdy;
  seg->p_para = -seg->psx * seg->pdx - seg->psy * seg->pdy;
}


//
// SplitSeg
//
// -AJA- Splits the given seg at the point (x,y).  The new seg is
//       returned.  The old seg is shortened (the original start
//       vertex is unchanged), whereas the new seg becomes the cut-off
//       tail (keeping the original end vertex).
//
//       If the seg has a partner, than that partner is also split.
//       NOTE WELL: the new piece of the partner seg is inserted into
//       the same list as the partner seg (and after it) -- thus ALL
//       segs (except the one we are currently splitting) must exist
//       on a singly-linked list somewhere. 
//
// Note: we must update the count values of any superblock that
//       contains the seg (and/or partner), so that future processing
//       is not fucked up by incorrect counts.
//
static seg_t *SplitSeg(seg_t *old_seg, float_g x, float_g y)
{
  seg_t *new_seg;
  vertex_t *new_vert;

  #if DEBUG_SPLIT
  if (old_seg->linedef)
    PrintDebug("Splitting Linedef %d (%p) at (%1.1f,%1.1f)\n",
        old_seg->linedef->index, old_seg, x, y);
  else
    PrintDebug("Splitting Miniseg %p at (%1.1f,%1.1f)\n", old_seg, x, y);
  #endif

  // update superblock, if needed
  if (old_seg->block)
    SplitSegInSuper(old_seg->block, old_seg);

  new_vert = NewVertexFromSplitSeg(old_seg, x, y);
  new_seg  = NewSeg();

  // copy seg info
  new_seg[0] = old_seg[0];
  new_seg->next = NULL;

  old_seg->end = new_vert;
  RecomputeSeg(old_seg);

  new_seg->start = new_vert;
  RecomputeSeg(new_seg);

  #if DEBUG_SPLIT
  PrintDebug("Splitting Vertex is %04X at (%1.1f,%1.1f)\n",
      new_vert->index, new_vert->x, new_vert->y);
  #endif

  // handle partners

  if (old_seg->partner)
  {
    #if DEBUG_SPLIT
    PrintDebug("Splitting Partner %p\n", old_seg->partner);
    #endif

    // update superblock, if needed
    if (old_seg->partner->block)
      SplitSegInSuper(old_seg->partner->block, old_seg->partner);

    new_seg->partner = NewSeg();

    // copy seg info
    new_seg->partner[0] = old_seg->partner[0];

    // IMPORTANT: keep partner relationship valid.
    new_seg->partner->partner = new_seg;

    old_seg->partner->start = new_vert;
    RecomputeSeg(old_seg->partner);

    new_seg->partner->end = new_vert;
    RecomputeSeg(new_seg->partner);

    // link it into list
    old_seg->partner->next = new_seg->partner;
  }

  return new_seg;
}


//
// ComputeIntersection
//
// -AJA- In the quest for slime-trail annihilation :->, this routine
//       calculates the intersection location between the current seg
//       and the partitioning seg, and takes advantage of some common
//       situations like horizontal/vertical lines.
//
static INLINE_G void ComputeIntersection(seg_t *cur, seg_t *part,
  float_g perp_c, float_g perp_d, float_g *x, float_g *y)
{
  double ds;

  // horizontal partition against vertical seg
  if (part->pdy == 0 && cur->pdx == 0)
  {
    *x = cur->psx;
    *y = part->psy;
    return;
  }
  
  // vertical partition against horizontal seg
  if (part->pdx == 0 && cur->pdy == 0)
  {
    *x = part->psx;
    *y = cur->psy;
    return;
  }
  
  // 0 = start, 1 = end
  ds = perp_c / (perp_c - perp_d);

  if (cur->pdx == 0)
    *x = cur->psx;
  else
    *x = cur->psx + (cur->pdx * ds);

  if (cur->pdy == 0)
    *y = cur->psy;
  else
    *y = cur->psy + (cur->pdy * ds);
}


//
// AddIntersection
//
static void AddIntersection(intersection_t ** cut_list,
    vertex_t *vert, seg_t *part)
{
  intersection_t *cut;
  intersection_t *after;

  // check if vertex already present
  for (cut=(*cut_list); cut; cut=cut->next)
  {
    if (vert == cut->vertex)
      return;
  }

  // create new intersection
  cut = NewIntersection();

  cut->vertex = vert;
  cut->along_dist = ComputeParallelDist(part, vert->x, vert->y);
  
  cut->l.open = VertexCheckOpen(vert, -part->pdx, -part->pdy,
      &cut->l.right, &cut->l.left);

  cut->r.open = VertexCheckOpen(vert, part->pdx, part->pdy,
      &cut->r.left, &cut->r.right);
  
  // enqueue the new intersection into the list

  for (after=(*cut_list); after && after->next; after=after->next)
  { }

  while (after && cut->along_dist < after->along_dist) 
    after = after->prev;
  
  // link it in
  cut->next = after ? after->next : (*cut_list);
  cut->prev = after;

  if (after)
  {
    if (after->next)
      after->next->prev = cut;
    
    after->next = cut;
  }
  else
  {
    if (*cut_list)
      (*cut_list)->prev = cut;
    
    (*cut_list) = cut;
  }
}


//
// EvalPartitionWorker
//
// Returns TRUE if a "bad seg" was found early.
//
static int EvalPartitionWorker(superblock_t *seg_list, seg_t *part, 
    int best_cost, eval_info_t *info)
{
  seg_t *check;

  float_g qnty;
  float_g a, b, fa, fb;

  int num;
  int factor = cur_info->factor;

  // -AJA- this is the heart of my superblock idea, it tests the
  //       _whole_ block against the partition line to quickly handle
  //       all the segs within it at once.  Only when the partition
  //       line intercepts the box do we need to go deeper into it.

  num = BoxOnLineSide(seg_list, part);

  if (num < 0)
  {
    // LEFT

    info->real_left += seg_list->real_num;
    info->mini_left += seg_list->mini_num;

    return FALSE;
  }
  else if (num > 0)
  {
    // RIGHT

    info->real_right += seg_list->real_num;
    info->mini_right += seg_list->mini_num;
    
    return FALSE;
  }

  #define ADD_LEFT()  \
      do {  \
        if (check->linedef) info->real_left += 1;  \
        else                info->mini_left += 1;  \
      } while (0)

  #define ADD_RIGHT()  \
      do {  \
        if (check->linedef) info->real_right += 1;  \
        else                info->mini_right += 1;  \
      } while (0)

  // check partition against all Segs

  for (check=seg_list->segs; check; check=check->next)
  { 
    // This is the heart of my pruning idea - it catches
    // bad segs early on. Killough

    if (info->cost > best_cost)
      return TRUE;

    // get state of lines' relation to each other
    if (check->source_line == part->source_line)
    {
      a = b = fa = fb = 0;
    }
    else
    {
      a = ComputePerpDist(part, check->psx, check->psy);
      b = ComputePerpDist(part, check->pex, check->pey);

      fa = fabs(a);
      fb = fabs(b);
    }

    // check for being on the same line
    if (fa <= DIST_EPSILON && fb <= DIST_EPSILON)
    {
      // this seg runs along the same line as the partition.  Check
      // whether it goes in the same direction or the opposite.

      if (check->pdx*part->pdx + check->pdy*part->pdy < 0)
      {
        ADD_LEFT();
      }
      else
      {
        ADD_RIGHT();
      }

      continue;
    }

    // check for right side
    if (a > -DIST_EPSILON && b > -DIST_EPSILON)
    {
      ADD_RIGHT();

      // check for a near miss
      if ((a >= IFFY_LEN && b >= IFFY_LEN) ||
          (a <= DIST_EPSILON && b >= IFFY_LEN) ||
          (b <= DIST_EPSILON && a >= IFFY_LEN))
      {
        continue;
      }
      
      info->near_miss++;

      // -AJA- near misses are bad, since they have the potential to
      //       cause really short minisegs to be created in future
      //       processing.  Thus the closer the near miss, the higher
      //       the cost.

      if (a <= DIST_EPSILON || b <= DIST_EPSILON)
        qnty = IFFY_LEN / MAX(a, b);
      else
        qnty = IFFY_LEN / MIN(a, b);

      info->cost += (int) (100 * factor * (qnty * qnty - 1.0));
      continue;
    }

    // check for left side
    if (a < DIST_EPSILON && b < DIST_EPSILON)
    {
      ADD_LEFT();

      // check for a near miss
      if ((a <= -IFFY_LEN && b <= -IFFY_LEN) ||
          (a >= -DIST_EPSILON && b <= -IFFY_LEN) ||
          (b >= -DIST_EPSILON && a <= -IFFY_LEN))
      {
        continue;
      }

      info->near_miss++;

      // the closer the miss, the higher the cost (see note above)
      if (a >= -DIST_EPSILON || b >= -DIST_EPSILON)
        qnty = IFFY_LEN / -MIN(a, b);
      else
        qnty = IFFY_LEN / -MAX(a, b);

      info->cost += (int) (100 * factor * (qnty * qnty - 1.0));
      continue;
    }

    // When we reach here, we have a and b non-zero and opposite sign,
    // hence this seg will be split by the partition line.

    info->splits++;

    // If the linedef associated with this seg has a tag >= 900, treat
    // it as precious; i.e. don't split it unless all other options
    // are exhausted. This is used to protect deep water and invisible
    // lifts/stairs from being messed up accidentally by splits.

    if (check->linedef && check->linedef->is_precious)
      info->cost += 100 * factor * PRECIOUS_MULTIPLY;
    else
      info->cost += 100 * factor;

    // -AJA- check if the split point is very close to one end, which
    //       is quite an undesirable situation (producing really short
    //       segs).  This is perhaps is _one_ source of those darn
    //       slime trails.  Hence the name "IFFY segs", and a rather
    //       hefty surcharge :->.

    if (fa < IFFY_LEN || fb < IFFY_LEN)
    {
      info->iffy++;

      // the closer to the end, the higher the cost
      qnty = IFFY_LEN / MIN(fa, fb);
      info->cost += (int) (100 * factor * (qnty * qnty - 1.0));
    }
  }

  // handle sub-blocks recursively

  for (num=0; num < 2; num++)
  {
    if (! seg_list->subs[num])
      continue;

    if (EvalPartitionWorker(seg_list->subs[num], part, best_cost, info))
      return TRUE;
  }

  // no "bad seg" was found.
  return FALSE;
}

//
// EvalPartition
//
// -AJA- Evaluate a partition seg & determine the cost, taking into
//       account the number of splits, difference between left &
//       right, and linedefs that are tagged `precious'.
//
// Returns the computed cost, or a negative value if the seg should be
// skipped altogether.
//
static int EvalPartition(superblock_t *seg_list, seg_t *part, 
    int best_cost)
{
  eval_info_t info;

  // initialise info structure
  info.cost   = 0;
  info.splits = 0;
  info.iffy   = 0;
  info.near_miss  = 0;

  info.real_left  = 0;
  info.real_right = 0;
  info.mini_left  = 0;
  info.mini_right = 0;
  
  if (EvalPartitionWorker(seg_list, part, best_cost, &info))
    return -1;
  
  // make sure there is at least one real seg on each side
  if (!info.real_left || !info.real_right)
  {
    #if DEBUG_PICKNODE
    PrintDebug("Eval : No real segs on %s%sside\n", 
        info.real_left  ? "" : "left ", 
        info.real_right ? "" : "right ");
    #endif

    return -1;
  }

  // increase cost by the difference between left & right
  info.cost += 100 * ABS(info.real_left - info.real_right);

  // -AJA- allow miniseg counts to affect the outcome, but only to a
  //       lesser degree than real segs.
  
  info.cost += 50 * ABS(info.mini_left - info.mini_right);

  // -AJA- Another little twist, here we show a slight preference for
  //       partition lines that lie either purely horizontally or
  //       purely vertically.
  
  if (part->pdx != 0 && part->pdy != 0)
    info.cost += 25;

  #if DEBUG_PICKNODE
  PrintDebug("Eval %p: splits=%d iffy=%d near=%d left=%d+%d right=%d+%d "
      "cost=%d.%02d\n", part, info.splits, info.iffy, info.near_miss, 
      info.real_left, info.mini_left, info.real_right, info.mini_right, 
      info.cost / 100, info.cost % 100);
  #endif
 
  return info.cost;
}


// returns FALSE if cancelled
static boolean_g PickNodeWorker(superblock_t *part_list, 
    superblock_t *seg_list, seg_t ** best, int *best_cost,
    int *progress, int prog_step)
{
  seg_t *part;

  int num;
  int cost;

  // use each Seg as partition
  for (part=part_list->segs; part; part = part->next)
  {
    if (cur_comms->cancelled)
      return FALSE;

    #if DEBUG_PICKNODE
    PrintDebug("PickNode:   %sSEG %p  sector=%d  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
      part->linedef ? "" : "MINI", part, 
      part->sector ? part->sector->index : -1,
      part->start->x, part->start->y, part->end->x, part->end->y);
    #endif

    // something for the user to look at
    (*progress) += 1;

    if ((*progress % prog_step) == 0)
    {
      cur_build_pos++;
      DisplaySetBar(1, cur_build_pos);
      DisplaySetBar(2, cur_file_pos + cur_build_pos / 10);
    }

    DisplayTicker();

    // ignore minisegs as partition candidates
    if (! part->linedef)
      continue;
    
    cost = EvalPartition(seg_list, part, *best_cost);

    // seg unsuitable or too costly ?
    if (cost < 0 || cost >= *best_cost)
      continue;

    // we have a new better choice
    (*best_cost) = cost;

    // remember which Seg
    (*best) = part;
  }

  // recursively handle sub-blocks

  for (num=0; num < 2; num++)
  {
    if (part_list->subs[num])
      PickNodeWorker(part_list->subs[num], seg_list, best, best_cost,
        progress, prog_step);
  }

  return TRUE;
}

//
// PickNode
//
// Find the best seg in the seg_list to use as a partition line.
//
seg_t *PickNode(superblock_t *seg_list, int depth)
{
  seg_t *best=NULL;

  int best_cost=INT_MAX;

  int progress=0;
  int prog_step=1<<24;

  #if DEBUG_PICKNODE
  PrintDebug("PickNode: BEGUN\n");
  #endif

  // compute info for showing progress
  if (depth <= 3)
  {
    static int depth_counts[4] = { 28, 12, 6, 3 };
    
    int total = seg_list->real_num + seg_list->mini_num;
    int want  = depth_counts[depth];

    prog_step = 1 + ((total - 1) / want);

    if (total / prog_step < want)
    {
      cur_build_pos += want - total / prog_step;
      DisplaySetBar(1, cur_build_pos);
      DisplaySetBar(2, cur_file_pos + cur_build_pos / 10);
    }
  }
 
  if (FALSE == PickNodeWorker(seg_list, seg_list, &best, &best_cost, 
      &progress, prog_step))
  {
    // hack here : BuildNodes will detect the cancellation
    return NULL;
  }

  #if DEBUG_PICKNODE
  if (! best)
  {
    PrintDebug("PickNode: NO BEST FOUND !\n");
  }
  else
  {
    PrintDebug("PickNode: Best has score %d.%02d  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n", 
      best_cost / 100, best_cost % 100, best->start->x, best->start->y,
      best->end->x, best->end->y);
  }
  #endif

  // all finished, return best Seg
  return best;
}


//
// DivideOneSeg
//
// Apply the partition line to the given seg, taking the necessary
// action (moving it into either the left list, right list, or
// splitting it).
//
// -AJA- I have rewritten this routine based on the EvalPartition
//       routine above (which I've also reworked, heavily).  I think
//       it is important that both these routines follow the exact
//       same logic when determining which segs should go left, right
//       or be split.
//
void DivideOneSeg(seg_t *cur, seg_t *part, 
    superblock_t *left_list, superblock_t *right_list,
    intersection_t ** cut_list)
{
  seg_t *new_seg;

  float_g x, y;

  // get state of lines' relation to each other
  float_g a = ComputePerpDist(part, cur->psx, cur->psy);
  float_g b = ComputePerpDist(part, cur->pex, cur->pey);

  if (cur->source_line == part->source_line)
    a = b = 0;

  // check for being on the same line
  if (fabs(a) <= DIST_EPSILON && fabs(b) <= DIST_EPSILON)
  {
    AddIntersection(cut_list, cur->start, part);
    AddIntersection(cut_list, cur->end, part);

    // this seg runs along the same line as the partition.  check
    // whether it goes in the same direction or the opposite.

    if (cur->pdx*part->pdx + cur->pdy*part->pdy < 0)
    {
      AddSegToSuper(left_list, cur);
    }
    else
    {
      AddSegToSuper(right_list, cur);
    }

    return;
  }

  // check for right side
  if (a > -DIST_EPSILON && b > -DIST_EPSILON)
  {
    if (a < DIST_EPSILON)
      AddIntersection(cut_list, cur->start, part);
    else if (b < DIST_EPSILON)
      AddIntersection(cut_list, cur->end, part);

    AddSegToSuper(right_list, cur);
    return;
  }
 
  // check for left side
  if (a < DIST_EPSILON && b < DIST_EPSILON)
  {
    if (a > -DIST_EPSILON)
      AddIntersection(cut_list, cur->start, part);
    else if (b > -DIST_EPSILON)
      AddIntersection(cut_list, cur->end, part);

    AddSegToSuper(left_list, cur);
    return;
  }
 
  // when we reach here, we have a and b non-zero and opposite sign,
  // hence this seg will be split by the partition line.
  
  ComputeIntersection(cur, part, a, b, &x, &y);

  new_seg = SplitSeg(cur, x, y);

  AddIntersection(cut_list, cur->end, part);

  if (a < 0)
  {
    AddSegToSuper(left_list,  cur);
    AddSegToSuper(right_list, new_seg);
  }
  else
  {
    AddSegToSuper(right_list, cur);
    AddSegToSuper(left_list,  new_seg);
  }
}

//
// SeparateSegs
//
void SeparateSegs(superblock_t *seg_list, seg_t *part,
    superblock_t *lefts, superblock_t *rights,
    intersection_t ** cut_list)
{
  int num;

  DisplayTicker();

  while (seg_list->segs)
  {
    seg_t *cur = seg_list->segs;
    seg_list->segs = cur->next;

    cur->block = NULL;

    DivideOneSeg(cur, part, lefts, rights, cut_list);
  }

  // recursively handle sub-blocks
  for (num=0; num < 2; num++)
  {
    superblock_t *A = seg_list->subs[num];

    if (A)
    {
      SeparateSegs(A, part, lefts, rights, cut_list);

      if (A->real_num + A->mini_num > 0)
        InternalError("SeparateSegs: child %d not empty !", num);

      FreeSuper(A);
      seg_list->subs[num] = NULL;
    }
  }

  seg_list->real_num = seg_list->mini_num = 0;
}


static void FindLimitWorker(superblock_t *block, bbox_t *bbox)
{
  seg_t *cur;
  int num;

  for (cur=block->segs; cur; cur=cur->next)
  {
    float_g x1 = cur->start->x;
    float_g y1 = cur->start->y;
    float_g x2 = cur->end->x;
    float_g y2 = cur->end->y;

    int lx = floor(MIN(x1, x2));
    int ly = floor(MIN(y1, y2));
    int hx = ceil(MAX(x1, x2));
    int hy = ceil(MAX(y1, y2));

    if (lx < bbox->minx) bbox->minx = lx;
    if (ly < bbox->miny) bbox->miny = ly;
    if (hx > bbox->maxx) bbox->maxx = hx;
    if (hy > bbox->maxy) bbox->maxy = hy;
  }

  // recursive handle sub-blocks

  for (num=0; num < 2; num++)
  {
    if (block->subs[num])
      FindLimitWorker(block->subs[num], bbox);
  }
}

//
// FindLimits
//
// Find the limits from a list of segs, by stepping through the segs
// and comparing the vertices at both ends.
//
void FindLimits(superblock_t *seg_list, bbox_t *bbox)
{
  bbox->minx = bbox->miny = SHRT_MAX;
  bbox->maxx = bbox->maxy = SHRT_MIN;

  FindLimitWorker(seg_list, bbox);
}


//
// AddMinisegs
//
void AddMinisegs(seg_t *part, 
    superblock_t *left_list, superblock_t *right_list, 
    intersection_t *cut_list)
{
  intersection_t *cur, *next;
  seg_t *seg, *buddy;

  #if DEBUG_CUTLIST
  PrintDebug("CUT LIST:\n");
  for (cur=cut_list; cur; cur=cur->next)
  {
    PrintDebug("  %s-%s  Vertex %04X  Along %1.2f  Left %d/%d  Right %d/%d\n", 
        cur->l.open ? "O" : "C", cur->r.open ? "O" : "C", 
        cur->vertex->index, cur->along_dist,
        cur->l.left ? cur->l.left->index : -1,
        cur->l.right ? cur->l.right->index : -1,
        cur->r.left ? cur->r.left->index : -1,
        cur->r.right ? cur->r.right->index : -1);
  }
  #endif

  // scan the intersection list...

  for (cur=cut_list; cur && cur->next; cur=cur->next)
  {
    next = cur->next;
    
    if (!cur->r.open && !next->l.open)
      continue;
    
    // ignore identical intersection points, which can happen when two
    // or more segs originate from the same vertex.

    if (fabs(cur->along_dist - next->along_dist) < DIST_EPSILON)
      continue;

    // check for cuts that are too short.
    if (fabs(cur->along_dist - next->along_dist) < 0.2)
    {
      PrintMiniWarn("Skipping very short seg (len=%1.3f) near (%1.1f,%1.1f)\n",
          ABS(cur->along_dist - next->along_dist),
          cur->vertex->x, cur->vertex->y);
      continue;
    }
 
    // check for some nasty OPEN/CLOSED or CLOSED/OPEN cases
    if ((cur->r.open && !next->l.open) ||
        (!cur->r.open && next->l.open))
    {
      if (cur->r.open && cur->r.right)
      {
        PrintMiniWarn("Sector #%d is unclosed near (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            cur->r.right->index, cur->vertex->x, cur->vertex->y,
            next->vertex->x, next->vertex->y);
      }
      else if (cur->r.open && cur->r.left)
      {
        PrintMiniWarn("Sector #%d is unclosed near (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            cur->r.left->index, cur->vertex->x, cur->vertex->y,
            next->vertex->x, next->vertex->y);
      }
      else if (next->l.open && next->l.right)
      {
        PrintMiniWarn("Sector #%d is unclosed near (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            next->l.right->index, next->vertex->x, next->vertex->y,
            cur->vertex->x, cur->vertex->y);
      }
      else if (next->l.open && next->l.left)
      {
        PrintMiniWarn("Sector #%d is unclosed near (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            next->l.left->index, next->vertex->x, next->vertex->y,
            cur->vertex->x, cur->vertex->y);
      }
      else
      {
        PrintMiniWarn("Unclosed sector found between (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
            cur->vertex->x, cur->vertex->y,
            next->vertex->x, next->vertex->y);
      }

      continue;
    }

    // righteo, here we have definite open space.  Do a sanity check
    // on the sectors (just for good measure).

    if (cur->r.right != next->l.right || cur->r.left != next->l.left)
    {
      PrintMiniWarn("Sector mismatch between (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
          cur->vertex->x, cur->vertex->y,
          next->vertex->x, next->vertex->y);
    }

    // create the miniseg pair
    seg = NewSeg();
    buddy = NewSeg();

    seg->partner = buddy;
    buddy->partner = seg;

    seg->start = cur->vertex;
    seg->end   = next->vertex;

    buddy->start = next->vertex;
    buddy->end   = cur->vertex;

    // leave `linedef' field as NULL.
    // leave `side' as zero too (not needed for minisegs).

    seg->sector = cur->r.right;
    buddy->sector = next->l.left;

    seg->index = buddy->index = -1;

    seg->source_line = buddy->source_line = part->linedef;

    RecomputeSeg(seg);
    RecomputeSeg(buddy);

    // add the new segs to the appropriate lists
    AddSegToSuper(right_list, seg);
    AddSegToSuper(left_list, buddy);

    #if DEBUG_CUTLIST
    PrintDebug("AddMiniseg: %p RIGHT  sector %d  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
        seg, seg->sector ? seg->sector->index : -1, 
        seg->start->x, seg->start->y, seg->end->x, seg->end->y);

    PrintDebug("AddMiniseg: %p LEFT   sector %d  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n",
        buddy, buddy->sector ? buddy->sector->index : -1, 
        buddy->start->x, buddy->start->y, buddy->end->x, buddy->end->y);
    #endif
  }

  // free intersection structures into quick-alloc list
  while (cut_list)
  {
    cur = cut_list;
    cut_list = cur->next;

    cur->next = quick_alloc_cuts;
    quick_alloc_cuts = cur;
  }
}

