//------------------------------------------------------------------------
// BLOCKMAP : Generate the blockmap
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



#define DEBUG_BLOCKMAP  0


int block_x, block_y;
int block_w, block_h;
int block_count;

static uint16_g ** block_lines;

static uint16_g *block_ptrs;
static uint16_g *block_dups;

static int block_compression;

#define DUMMY_DUP  0xFFFF


static int CheckLinedefInside(int xmin, int ymin, int xmax, int ymax,
    int x1, int y1, int x2, int y2)
{
  int count = 2;
  int tmp;

  for (;;)
  {
    if (y1 > ymax)
    {
      if (y2 > ymax)
        return FALSE;
        
      x1 = x1 + (x2-x1) * (double)(ymax-y1) / (double)(y2-y1);
      y1 = ymax;
      
      count = 2;
      continue;
    }

    if (y1 < ymin)
    {
      if (y2 < ymin)
        return FALSE;
      
      x1 = x1 + (x2-x1) * (double)(ymin-y1) / (double)(y2-y1);
      y1 = ymin;
      
      count = 2;
      continue;
    }

    if (x1 > xmax)
    {
      if (x2 > xmax)
        return FALSE;
        
      y1 = y1 + (y2-y1) * (double)(xmax-x1) / (double)(x2-x1);
      x1 = xmax;

      count = 2;
      continue;
    }

    if (x1 < xmin)
    {
      if (x2 < xmin)
        return FALSE;
        
      y1 = y1 + (y2-y1) * (double)(xmin-x1) / (double)(x2-x1);
      x1 = xmin;

      count = 2;
      continue;
    }

    count--;

    if (count == 0)
      break;

    // swap end points
    tmp=x1;  x1=x2;  x2=tmp;
    tmp=y1;  y1=y2;  y2=tmp;
  }

  // linedef touches block
  return TRUE;
}


/* ----- create blockmap ------------------------------------ */

#define BK_NUM    0
#define BK_MAX    1
#define BK_XOR    2
#define BK_FIRST  3

#define BK_QUANTUM  32

static void BlockAdd(int blk_num, int line_index)
{
  uint16_g *cur = block_lines[blk_num];

  #if DEBUG_BLOCKMAP
  PrintDebug("Block %d has line %d\n", blk_num, line_index);
  #endif

  if (blk_num < 0 || blk_num >= block_count)
    InternalError("BlockAdd: bad block number %d", blk_num);
    
  if (! cur)
  {
    // create empty block
    block_lines[blk_num] = cur = UtilCalloc(BK_QUANTUM * 
        sizeof(uint16_g));
    cur[BK_NUM] = 0;
    cur[BK_MAX] = BK_QUANTUM;
    cur[BK_XOR] = 0x1234;
  }

  if (BK_FIRST + cur[BK_NUM] == cur[BK_MAX])
  {
    // no more room, so allocate some more...
    cur[BK_MAX] += BK_QUANTUM;

    block_lines[blk_num] = cur = UtilRealloc(cur, cur[BK_MAX] * 
        sizeof(uint16_g));
  }

  // compute new checksum
  cur[BK_XOR] = ((cur[BK_XOR] << 4) | (cur[BK_XOR] >> 12)) ^ line_index;

  cur[BK_FIRST + cur[BK_NUM]] = line_index;
  cur[BK_NUM]++;
}

static void BlockAddLine(linedef_t *L)
{
  int x1 = (int) L->start->x;
  int y1 = (int) L->start->y;
  int x2 = (int) L->end->x;
  int y2 = (int) L->end->y;

  int bx1 = (MIN(x1,x2) - block_x) / 128;
  int by1 = (MIN(y1,y2) - block_y) / 128;
  int bx2 = (MAX(x1,x2) - block_x) / 128;
  int by2 = (MAX(y1,y2) - block_y) / 128;

  int bx, by;
  int line_index = L->index;

  #if DEBUG_BLOCKMAP
  PrintDebug("BlockAddLine: %d (%d,%d) -> (%d,%d)\n", line_index, 
      x1, y1, x2, y2);
  #endif

  // handle truncated blockmaps
  if (bx1 < 0) bx1 = 0;
  if (by1 < 0) by1 = 0;
  if (bx2 >= block_w) bx2 = block_w - 1;
  if (by2 >= block_h) by2 = block_h - 1;

  if (bx2 < bx1 || by2 < by1)
    return;

  // handle simple case #1: completely horizontal
  if (by1 == by2)
  {
    for (bx=bx1; bx <= bx2; bx++)
    {
      int blk_num = by1 * block_w + bx;
      BlockAdd(blk_num, line_index);
    }
    return;
  }

  // handle simple case #2: completely vertical
  if (bx1 == bx2)
  {
    for (by=by1; by <= by2; by++)
    {
      int blk_num = by * block_w + bx1;
      BlockAdd(blk_num, line_index);
    }
    return;
  }

  // handle the rest (diagonals)

  for (by=by1; by <= by2; by++)
  for (bx=bx1; bx <= bx2; bx++)
  {
    int blk_num = by * block_w + bx;
  
    int minx = block_x + bx * 128;
    int miny = block_y + by * 128;
    int maxx = minx + 127;
    int maxy = miny + 127;

    if (CheckLinedefInside(minx, miny, maxx, maxy, x1, y1, x2, y2))
    {
      BlockAdd(blk_num, line_index);
    }
  }
}

static void CreateBlockmap(void)
{
  int i;

  block_lines = UtilCalloc(block_count * sizeof(uint16_g *));

  DisplayTicker();

  for (i=0; i < num_linedefs; i++)
  {
    linedef_t *L = LookupLinedef(i);

    // ignore zero-length lines
    if (L->zero_len)
      continue;

    BlockAddLine(L);
  }
}


static int BlockCompare(const void *p1, const void *p2)
{
  int blk_num1 = ((const uint16_g *) p1)[0];
  int blk_num2 = ((const uint16_g *) p2)[0];

  const uint16_g *A = block_lines[blk_num1];
  const uint16_g *B = block_lines[blk_num2];

  if (A == B)
    return 0;

  if (A == NULL) return -1;
  if (B == NULL) return +1;

  if (A[BK_NUM] != B[BK_NUM])
  {
    return A[BK_NUM] - B[BK_NUM];
  }
 
  if (A[BK_XOR] != B[BK_XOR])
  {
    return A[BK_XOR] - B[BK_XOR];
  }
 
  return memcmp(A+BK_FIRST, B+BK_FIRST, A[BK_NUM] * sizeof(uint16_g));
}

static void CompressBlockmap(void)
{
  int i;
  int cur_offset;
  int dup_count=0;

  int orig_size, new_size;

  block_ptrs = UtilCalloc(block_count * sizeof(uint16_g));
  block_dups = UtilCalloc(block_count * sizeof(uint16_g));

  DisplayTicker();

  // sort duplicate-detecting array.  After the sort, all duplicates
  // will be next to each other.  The duplicate array gives the order
  // of the blocklists in the BLOCKMAP lump.
  
  for (i=0; i < block_count; i++)
    block_dups[i] = i;

  qsort(block_dups, block_count, sizeof(uint16_g), BlockCompare);

  // scan duplicate array and build up offset array

  cur_offset = 4 + block_count + 2;

  orig_size = 4 + block_count;
  new_size  = cur_offset;

  DisplayTicker();

  for (i=0; i < block_count; i++)
  {
    int blk_num = block_dups[i];
    int count;

    // empty block ?
    if (block_lines[blk_num] == NULL)
    {
      block_ptrs[blk_num] = 4 + block_count;
      block_dups[i] = DUMMY_DUP;

      orig_size += 2;
      continue;
    }

    count = 2 + block_lines[blk_num][BK_NUM];

    // duplicate ?  Only the very last one of a sequence of duplicates
    // will update the current offset value.

    if (i+1 < block_count && 
        BlockCompare(block_dups + i, block_dups + i+1) == 0)
    {
      block_ptrs[blk_num] = cur_offset;
      block_dups[i] = DUMMY_DUP;

      // free the memory of the duplicated block
      UtilFree(block_lines[blk_num]);
      block_lines[blk_num] = NULL;
      
      dup_count++;

      orig_size += count;
      continue;
    }

    // OK, this block is either the last of a series of duplicates, or
    // just a singleton.

    block_ptrs[blk_num] = cur_offset;

    cur_offset += count;

    orig_size += count;
    new_size  += count;
  }

  if (cur_offset > 65535)
    PrintWarn("Blockmap has OVERFLOWED!  May cause problems "
        "or even crash\n");

  #if DEBUG_BLOCKMAP
  PrintDebug("Blockmap: Last ptr = %d  duplicates = %d\n", 
      cur_offset, dup_count);
  #endif

  block_compression = (orig_size - new_size) * 100 / orig_size;

  // there's a tiny chance of new_size > orig_size
  if (block_compression < 0)
    block_compression = 0;
}


static void WriteBlockmap(void)
{
  int i;
  
  raw_blockmap_header_t header;

  lump_t *lump = CreateLevelLump("BLOCKMAP");

  uint16_g null_block[2] = { 0x0000, 0xFFFF };
  uint16_g m_zero = 0x0000;
  uint16_g m_neg1 = 0xFFFF;
  
  // fill in header
  header.x_origin = UINT16(block_x);
  header.y_origin = UINT16(block_y);
  header.x_blocks = UINT16(block_w);
  header.y_blocks = UINT16(block_h);
  
  AppendLevelLump(lump, &header, sizeof(header));

  // handle pointers
  for (i=0; i < block_count; i++)
  {
    uint16_g ptr = UINT16(block_ptrs[i]);

    if (ptr == 0)
      InternalError("WriteBlockmap: offset %d not set.", i);

    AppendLevelLump(lump, &ptr, sizeof(uint16_g));
  }

  // add the null block which _all_ empty blocks will use
  AppendLevelLump(lump, null_block, sizeof(null_block));

  // handle each block list
  for (i=0; i < block_count; i++)
  {
    int blk_num = block_dups[i];
    uint16_g *blk;

    // ignore duplicate or empty blocks
    if (blk_num == DUMMY_DUP)
      continue;

    blk = block_lines[blk_num];

    if (blk == NULL)
      InternalError("WriteBlockmap: block %d is NULL !", i);

    AppendLevelLump(lump, &m_zero, sizeof(uint16_g));
    AppendLevelLump(lump, blk + BK_FIRST, blk[BK_NUM] * sizeof(uint16_g));
    AppendLevelLump(lump, &m_neg1, sizeof(uint16_g));
  }
}


static void FreeBlockmap(void)
{
  int i;

  for (i=0; i < block_count; i++)
  {
    if (block_lines[i])
      UtilFree(block_lines[i]);
  }

  UtilFree(block_lines);
  UtilFree(block_ptrs);
  UtilFree(block_dups);
}


/* ----- top level funcs ------------------------------------ */

static void FindBlockmapLimits(bbox_t *bbox)
{
  int i;

  bbox->minx = bbox->miny = SHRT_MAX;
  bbox->maxx = bbox->maxy = SHRT_MIN;

  for (i=0; i < num_linedefs; i++)
  {
    linedef_t *L = LookupLinedef(i);

    if (! L->zero_len)
    {
      float_g x1 = L->start->x;
      float_g y1 = L->start->y;
      float_g x2 = L->end->x;
      float_g y2 = L->end->y;

      int lx = floor(MIN(x1, x2));
      int ly = floor(MIN(y1, y2));
      int hx = ceil(MAX(x1, x2));
      int hy = ceil(MAX(y1, y2));

      if (lx < bbox->minx) bbox->minx = lx;
      if (ly < bbox->miny) bbox->miny = ly;
      if (hx > bbox->maxx) bbox->maxx = hx;
      if (hy > bbox->maxy) bbox->maxy = hy;
    }
  }
}

//
// TruncateBlockmap
//
static void TruncateBlockmap(void)
{
  int orig_w = block_w;
  int orig_h = block_h;

  while (block_w * block_h > cur_info->block_limit)
  {
    block_w -= block_w / 8;
    block_h -= block_h / 8;
  }

  block_count = block_w * block_h;

  PrintWarn("Blockmap too large!  Truncated to %dx%d blocks\n",
      block_w, block_h);

  // center the truncated blockmap
  block_x += (block_w - orig_w) * 128 / 2;
  block_y += (block_h - orig_h) * 128 / 2;
}

//
// InitBlockmap
//
void InitBlockmap(void)
{
  bbox_t map_bbox;

  // find limits of linedefs, and store as map limits
  FindBlockmapLimits(&map_bbox);

  PrintMsg("Map goes from (%d,%d) to (%d,%d)\n",
      map_bbox.minx, map_bbox.miny, map_bbox.maxx, map_bbox.maxy);

  block_x = map_bbox.minx - (map_bbox.minx & 0x7);
  block_y = map_bbox.miny - (map_bbox.miny & 0x7);

  block_w = ((map_bbox.maxx - block_x) / 128) + 1;
  block_h = ((map_bbox.maxy - block_y) / 128) + 1;
  block_count = block_w * block_h;

}

//
// PutBlockmap
//
void PutBlockmap(void)
{
  // truncate blockmap if too large.  We're limiting the number of
  // blocks to around 44000 (user changeable), this leaves about 20K
  // of shorts for the actual line lists.
 
  if (block_count > cur_info->block_limit)
    TruncateBlockmap();

  // initial phase: create internal blockmap containing the index of
  // all lines in each block.
  
  CreateBlockmap();

  // -AJA- second phase: compress the blockmap.  We do this by sorting
  //       the blocks, which is a typical way (as I now know) to
  //       detect duplicates in a large list.

  CompressBlockmap();
 
  // final phase: write it out in the correct format

  WriteBlockmap();

  PrintMsg("Completed blockmap building (compression: %d%%)\n",
      block_compression);

  FreeBlockmap();
}

