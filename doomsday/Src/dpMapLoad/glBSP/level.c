//------------------------------------------------------------------------
// LEVEL : Level structure read/write functions.
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
#include "reject.h"
#include "seg.h"
#include "structs.h"
#include "util.h"
#include "wad.h"


#define DEBUG_LOAD      0
#define DEBUG_BSP       0
#define DEBUG_SORTER    0
#define DEBUG_WALLTIPS  0
#define DEBUG_POLYOBJ   0

#define ALLOC_BLKNUM  1024


// per-level variables

static boolean_g normal_exists;
static boolean_g doing_normal;
static boolean_g doing_gl;
static boolean_g doing_hexen;


#define LEVELARRAY(TYPE, BASEVAR, NUMVAR)  \
    static TYPE ** BASEVAR = NULL;  \
    int NUMVAR = 0;


LEVELARRAY(vertex_t,  vertices, num_vertices)
LEVELARRAY(linedef_t, linedefs, num_linedefs)
LEVELARRAY(sidedef_t, sidedefs, num_sidedefs)
LEVELARRAY(sector_t,  sectors,  num_sectors)
LEVELARRAY(seg_t,     segs,     num_segs)
LEVELARRAY(subsec_t,  subsecs,  num_subsecs)
LEVELARRAY(node_t,    nodes,    num_nodes)
LEVELARRAY(wall_tip_t,wall_tips,num_wall_tips)


int num_normal_vert = 0;
int num_gl_vert = 0;
int num_complete_seg = 0;


/* ----- allocation routines ---------------------------- */

#define ALLIGATOR(TYPE, BASEVAR, NUMVAR)  \
{  \
  if ((NUMVAR % ALLOC_BLKNUM) == 0)  \
  {  \
    BASEVAR = UtilRealloc(BASEVAR, (NUMVAR + ALLOC_BLKNUM) *   \
        sizeof(TYPE *));  \
  }  \
  BASEVAR[NUMVAR] = (TYPE *) UtilCalloc(sizeof(TYPE));  \
  NUMVAR += 1;  \
  return BASEVAR[NUMVAR - 1];  \
}


vertex_t *NewVertex(void)
  ALLIGATOR(vertex_t, vertices, num_vertices)

linedef_t *NewLinedef(void)
  ALLIGATOR(linedef_t, linedefs, num_linedefs)

sidedef_t *NewSidedef(void)
  ALLIGATOR(sidedef_t, sidedefs, num_sidedefs)

sector_t *NewSector(void)
  ALLIGATOR(sector_t, sectors, num_sectors)

seg_t *NewSeg(void)
  ALLIGATOR(seg_t, segs, num_segs)

subsec_t *NewSubsec(void)
  ALLIGATOR(subsec_t, subsecs, num_subsecs)

node_t *NewNode(void)
  ALLIGATOR(node_t, nodes, num_nodes)

wall_tip_t *NewWallTip(void)
  ALLIGATOR(wall_tip_t, wall_tips, num_wall_tips)


/* ----- free routines ---------------------------- */

#define FREEMASON(TYPE, BASEVAR, NUMVAR)  \
{  \
  int i;  \
  for (i=0; i < NUMVAR; i++)  \
    UtilFree(BASEVAR[i]);  \
  if (BASEVAR)  \
    UtilFree(BASEVAR);  \
  BASEVAR = NULL; NUMVAR = 0;  \
}


void FreeVertices(void)
  FREEMASON(vertex_t, vertices, num_vertices)

void FreeLinedefs(void)
  FREEMASON(linedef_t, linedefs, num_linedefs)

void FreeSidedefs(void)
  FREEMASON(sidedef_t, sidedefs, num_sidedefs)

void FreeSectors(void)
  FREEMASON(sector_t, sectors, num_sectors)

void FreeSegs(void)
  FREEMASON(seg_t, segs, num_segs)

void FreeSubsecs(void)
  FREEMASON(subsec_t, subsecs, num_subsecs)

void FreeNodes(void)
  FREEMASON(node_t, nodes, num_nodes)

void FreeWallTips(void)
  FREEMASON(wall_tip_t, wall_tips, num_wall_tips)


/* ----- lookup routines ------------------------------ */

#define LOOKERUPPER(BASEVAR, NUMVAR, NAMESTR)  \
{  \
  if (index < 0 || index >= NUMVAR)  \
    FatalError("No such %s number #%d", NAMESTR, index);  \
    \
  return BASEVAR[index];  \
}

vertex_t *LookupVertex(int index)
  LOOKERUPPER(vertices, num_vertices, "vertex")

linedef_t *LookupLinedef(int index)
  LOOKERUPPER(linedefs, num_linedefs, "linedef")
  
sidedef_t *LookupSidedef(int index)
  LOOKERUPPER(sidedefs, num_sidedefs, "sidedef")
  
sector_t *LookupSector(int index)
  LOOKERUPPER(sectors, num_sectors, "sector")
  
seg_t *LookupSeg(int index)
  LOOKERUPPER(segs, num_segs, "seg")
  
subsec_t *LookupSubsec(int index)
  LOOKERUPPER(subsecs, num_subsecs, "subsector")
  
node_t *LookupNode(int index)
  LOOKERUPPER(nodes, num_nodes, "node")


/* ----- reading routines ------------------------------ */


//
// CheckForNormalNodes
//
boolean_g CheckForNormalNodes(void)
{
  lump_t *lump;
  
  // Note: an empty NODES lump can be valid.
  if (FindLevelLump("NODES") == NULL)
    return FALSE;
 
  lump = FindLevelLump("SEGS");
  
  if (! lump || lump->length == 0 || CheckLevelLumpZero(lump))
    return FALSE;

  lump = FindLevelLump("SSECTORS");
  
  if (! lump || lump->length == 0 || CheckLevelLumpZero(lump))
    return FALSE;

  return TRUE;
}

//
// GetVertices
//
void GetVertices(void)
{
  int i, count=-1;
  raw_vertex_t *raw;
  lump_t *lump = FindLevelLump("VERTEXES");

  if (lump)
    count = lump->length / sizeof(raw_vertex_t);

  DisplayTicker();

  #if DEBUG_LOAD
  PrintDebug("GetVertices: num = %d\n", count);
  #endif

  if (!lump || count == 0)
    FatalError("Couldn't find any Vertices");

  raw = (raw_vertex_t *) lump->data;

  for (i=0; i < count; i++, raw++)
  {
    vertex_t *vert = NewVertex();

    vert->x = (float_g) SINT16(raw->x);
    vert->y = (float_g) SINT16(raw->y);

    vert->index = i;
  }

  num_normal_vert = num_vertices;
  num_gl_vert = 0;
  num_complete_seg = 0;
}

//
// GetSectors
//
void GetSectors(void)
{
  int i, count=-1;
  raw_sector_t *raw;
  lump_t *lump = FindLevelLump("SECTORS");

  if (lump)
    count = lump->length / sizeof(raw_sector_t);

  if (!lump || count == 0)
    FatalError("Couldn't find any Sectors");

  DisplayTicker();

  #if DEBUG_LOAD
  PrintDebug("GetSectors: num = %d\n", count);
  #endif

  raw = (raw_sector_t *) lump->data;

  for (i=0; i < count; i++, raw++)
  {
    sector_t *sector = NewSector();

    sector->floor_h = SINT16(raw->floor_h);
    sector->ceil_h  = SINT16(raw->ceil_h);

    memcpy(sector->floor_tex, raw->floor_tex, sizeof(sector->floor_tex));
    memcpy(sector->ceil_tex,  raw->ceil_tex,  sizeof(sector->ceil_tex));

    sector->light = UINT16(raw->light);
    sector->special = UINT16(raw->special);
    sector->tag = SINT16(raw->tag);

    sector->coalesce = (sector->tag >= 900 && sector->tag < 1000) ?
        TRUE : FALSE;

    // sector indices never change
    sector->index = i;

    // Note: rej_* fields are handled completely in reject.c
  }
}

//
// GetSidedefs
//
void GetSidedefs(void)
{
  int i, count=-1;
  raw_sidedef_t *raw;
  lump_t *lump = FindLevelLump("SIDEDEFS");

  if (lump)
    count = lump->length / sizeof(raw_sidedef_t);

  if (!lump || count == 0)
    FatalError("Couldn't find any Sidedefs");

  DisplayTicker();

  #if DEBUG_LOAD
  PrintDebug("GetSidedefs: num = %d\n", count);
  #endif

  raw = (raw_sidedef_t *) lump->data;

  for (i=0; i < count; i++, raw++)
  {
    sidedef_t *side = NewSidedef();

    side->sector = (SINT16(raw->sector) == -1) ? NULL :
        LookupSector(UINT16(raw->sector));

    if (side->sector)
      side->sector->ref_count++;

    side->x_offset = SINT16(raw->x_offset);
    side->y_offset = SINT16(raw->y_offset);

    memcpy(side->upper_tex, raw->upper_tex, sizeof(side->upper_tex));
    memcpy(side->lower_tex, raw->lower_tex, sizeof(side->lower_tex));
    memcpy(side->mid_tex,   raw->mid_tex,   sizeof(side->mid_tex));

    // sidedef indices never change
    side->index = i;
  }
}

//
// GetLinedefs
//
void GetLinedefs(void)
{
  int i, count=-1;
  raw_linedef_t *raw;
  lump_t *lump = FindLevelLump("LINEDEFS");

  if (lump)
    count = lump->length / sizeof(raw_linedef_t);

  if (!lump || count == 0)
    FatalError("Couldn't find any Linedefs");

  DisplayTicker();

  #if DEBUG_LOAD
  PrintDebug("GetLinedefs: num = %d\n", count);
  #endif

  raw = (raw_linedef_t *) lump->data;

  for (i=0; i < count; i++, raw++)
  {
    linedef_t *line;

    vertex_t *start = LookupVertex(UINT16(raw->start));
    vertex_t *end   = LookupVertex(UINT16(raw->end));

    start->ref_count++;
    end->ref_count++;

    line = NewLinedef();

    line->start = start;
    line->end   = end;

    // check for zero-length line
    line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) && 
        (fabs(start->y - end->y) < DIST_EPSILON);

    line->flags = UINT16(raw->flags);
    line->type = UINT16(raw->type);
    line->tag  = SINT16(raw->tag);

    line->two_sided = (line->flags & LINEFLAG_TWO_SIDED) ? TRUE : FALSE;
    line->is_precious = (line->tag >= 900 && line->tag < 1000) ? 
        TRUE : FALSE;

    line->right = (SINT16(raw->sidedef1) < 0) ? NULL :
        LookupSidedef(SINT16(raw->sidedef1));

    line->left  = (SINT16(raw->sidedef2) < 0) ? NULL :
        LookupSidedef(SINT16(raw->sidedef2));

    if (line->right)
    {
      line->right->ref_count++;
      line->right->on_special |= (line->type > 0) ? 1 : 0;
    }

    if (line->left)
    {
      line->left->ref_count++;
      line->left->on_special |= (line->type > 0) ? 1 : 0;
    }

    line->index = i;
  }
}

//
// GetLinedefsHexen
//
void GetLinedefsHexen(void)
{
  int i, j, count=-1;
  raw_hexen_linedef_t *raw;
  lump_t *lump = FindLevelLump("LINEDEFS");

  if (lump)
    count = lump->length / sizeof(raw_hexen_linedef_t);

  if (!lump || count == 0)
    FatalError("Couldn't find any Linedefs");

  DisplayTicker();

  #if DEBUG_LOAD
  PrintDebug("GetLinedefs: num = %d\n", count);
  #endif

  raw = (raw_hexen_linedef_t *) lump->data;

  for (i=0; i < count; i++, raw++)
  {
    linedef_t *line;

    vertex_t *start = LookupVertex(UINT16(raw->start));
    vertex_t *end   = LookupVertex(UINT16(raw->end));

    start->ref_count++;
    end->ref_count++;

    line = NewLinedef();

    line->start = start;
    line->end   = end;

    // check for zero-length line
    line->zero_len = (fabs(start->x - end->x) < DIST_EPSILON) && 
        (fabs(start->y - end->y) < DIST_EPSILON);

    line->flags = UINT16(raw->flags);
    line->type = UINT8(raw->type);
    line->tag  = 0;

    // read specials
    for (j=0; j < 5; j++)
      line->specials[j] = UINT8(raw->specials[j]);

    line->right = (SINT16(raw->sidedef1) < 0) ? NULL :
        LookupSidedef(SINT16(raw->sidedef1));

    line->left  = (SINT16(raw->sidedef2) < 0) ? NULL :
        LookupSidedef(SINT16(raw->sidedef2));

    // -JL- Added missing sidedef handling that caused all sidedefs to be
    //      pruned.
    if (line->right)
    {
      line->right->ref_count++;
      line->right->on_special |= (line->type > 0) ? 1 : 0;
    }

    if (line->left)
    {
      line->left->ref_count++;
      line->left->on_special |= (line->type > 0) ? 1 : 0;
    }

    line->index = i;

    // handle polyobj lines
    if (line->type == HEXTYPE_POLY_START)
    {
      line->polyobj = 1;
      start->polyobj = end->polyobj = 1;
    }
    else if (line->type == HEXTYPE_POLY_EXPLICIT)
    {
      line->polyobj = 1;
    }
  }
}

static void MarkPolyobjSector(sector_t *sector)
{
  int i;
    
  if (! sector)
    return;

  #if DEBUG_POLYOBJ
  PrintDebug("  Marking SECTOR %d\n", sector->index);
  #endif

  // mark all lines of this sector as precious, to prevent the sector
  // from being split.
 
  sector->polyobj = TRUE;

  for (i = 0; i < num_linedefs; i++)
  {
    linedef_t *L = linedefs[i];

    if ((L->right && L->right->sector == sector) ||
        (L->left && L->left->sector == sector))
    {
      L->is_precious = TRUE;
    }
  }
}

static void MarkPolyobjPoint(float_g x, float_g y)
{
  int i;
  
  float_g best_dist = 999999;
  linedef_t *best_match = NULL;
  sector_t *sector = NULL;

  float_g x1, y1;
  float_g x2, y2;

  // -AJA- Algorithm is just like in DEU: we cast a line horizontally
  // from the given (x,y) position and find all linedefs that
  // intersect it, choosing the one with the closest distance.  If the
  // point is sitting directly on a (two-sided) line, then we mark the
  // sectors on both sides.

  for (i = 0; i < num_linedefs; i++)
  {
    linedef_t *L = linedefs[i];

    float_g x_cut;

    x1 = L->start->x;  y1 = L->start->y;
    x2 = L->end->x;    y2 = L->end->y;

    // check vertical range
    if (fabs(y2 - y1) < DIST_EPSILON)
      continue;

    if ((y > (y1 + DIST_EPSILON) && y > (y2 + DIST_EPSILON)) || 
        (y < (y1 - DIST_EPSILON) && y < (y2 - DIST_EPSILON)))
      continue;

    x_cut = x1 + (x2 - x1) * (y - y1) / (y2 - y1) - x;

    if (fabs(x_cut) < fabs(best_dist))
    {
      // found a closer linedef

      best_match = L;
      best_dist = x_cut;
    }
  }

  if (! best_match)
  {
    PrintWarn("Bad polyobj thing at (%1.0f,%1.0f).\n", x, y);
    return;
  }

  y1 = best_match->start->y;
  y2 = best_match->end->y;

  #if DEBUG_POLYOBJ
  PrintDebug("  Closest line was %d Y=%1.0f..%1.0f (dist=%1.1f)\n",
      best_match->index, y1, y2, best_dist);
  #endif

  // directly on the line ?
  if (fabs(best_dist) < DIST_EPSILON)
  {
    if (best_match->left)
      MarkPolyobjSector(best_match->left->sector);

    if (best_match->right)
      MarkPolyobjSector(best_match->right->sector);

    return;
  }
      
  // check orientation of line, to determine which side the polyobj is
  // actually on.

  if ((y1 > y2) == (best_dist > 0))
    sector = best_match->right ? best_match->right->sector : NULL;
  else
    sector = best_match->left ? best_match->left->sector : NULL;

  #if DEBUG_POLYOBJ
  PrintDebug("  Sector %d contains the polyobj.\n", 
      sector ? sector->index : -1);
  #endif

  if (! sector)
  {
    PrintWarn("Invalid Polyobj thing at (%1.0f,%1.0f).\n", x, y);
    return;
  }

  MarkPolyobjSector(sector);
}

//
// FindPolyobjSectors
//
// Based on code courtesy of Janis Legzdinsh.
//
void FindPolyobjSectors(void)
{
  int i, count=-1;
  raw_hexen_thing_t *raw;
  lump_t *lump = FindLevelLump("THINGS");

  if (lump)
    count = lump->length / sizeof(raw_hexen_thing_t);

  // Note: no error if no things exist, even though technically a map
  // will be unplayable without the player starts.

  if (!lump || count==0)
    return;

  raw = (raw_hexen_thing_t*) lump->data;

  for (i = 0; i < count; i++, raw++)
  {
    float_g x = (float_g) SINT16(raw->x);
    float_g y = (float_g) SINT16(raw->y);

    int type = UINT16(raw->type);

    // not a polyobj start spot
    if (type != PO_SPAWN_TYPE && type != PO_SPAWNCRUSH_TYPE)
      continue;

    #if DEBUG_POLYOBJ
    PrintDebug("Thing %d at (%1.0f,%1.0f) is a polyobj spawner.\n",
        i, x, y);
    #endif
    
    MarkPolyobjPoint(x, y);
  }
}


/* ----- analysis routines ----------------------------- */

static int VertexCompare(const void *p1, const void *p2)
{
  int vert1 = ((const uint16_g *) p1)[0];
  int vert2 = ((const uint16_g *) p2)[0];

  vertex_t *A = vertices[vert1];
  vertex_t *B = vertices[vert2];

  if (vert1 == vert2)
    return 0;

  if ((int)A->x != (int)B->x)
    return (int)A->x - (int)B->x; 
  
  return (int)A->y - (int)B->y;
}

static int SidedefCompare(const void *p1, const void *p2)
{
  int comp;

  int side1 = ((const uint16_g *) p1)[0];
  int side2 = ((const uint16_g *) p2)[0];

  sidedef_t *A = sidedefs[side1];
  sidedef_t *B = sidedefs[side2];

  if (side1 == side2)
    return 0;

  // don't merge sidedefs on special lines
  if (A->on_special || B->on_special)
    return side1 - side2;

  if (A->sector != B->sector)
  {
    if (A->sector == NULL) return -1;
    if (B->sector == NULL) return +1;

    return (A->sector->index - B->sector->index);
  }

  if ((int)A->x_offset != (int)B->x_offset)
    return A->x_offset - (int)B->x_offset;

  if ((int)A->y_offset != B->y_offset)
    return (int)A->y_offset - (int)B->y_offset;

  // compare textures

  comp = memcmp(A->upper_tex, B->upper_tex, sizeof(A->upper_tex));
  if (comp) return comp;
  
  comp = memcmp(A->lower_tex, B->lower_tex, sizeof(A->lower_tex));
  if (comp) return comp;
  
  comp = memcmp(A->mid_tex, B->mid_tex, sizeof(A->mid_tex));
  if (comp) return comp;

  // sidedefs must be the same
  return 0;
}

static void DetectDuplicateVertices(void)
{
  int i;
  uint16_g *array = UtilCalloc(num_vertices * sizeof(uint16_g));

  DisplayTicker();

  // sort array of indices
  for (i=0; i < num_vertices; i++)
    array[i] = i;
  
  qsort(array, num_vertices, sizeof(uint16_g), VertexCompare);

  // now mark them off
  for (i=0; i < num_vertices - 1; i++)
  {
    // duplicate ?
    if (VertexCompare(array + i, array + i+1) == 0)
    {
      vertex_t *A = vertices[array[i]];
      vertex_t *B = vertices[array[i+1]];

      // found a duplicate !
      if (A->equiv)
        B->equiv = A->equiv;
      else
        B->equiv = A;

      B->equiv->polyobj |= B->polyobj;
    }
  }

  UtilFree(array);
}

static void DetectDuplicateSidedefs(void)
{
  int i;
  uint16_g *array = UtilCalloc(num_sidedefs * sizeof(uint16_g));

  DisplayTicker();

  // sort array of indices
  for (i=0; i < num_sidedefs; i++)
    array[i] = i;
  
  qsort(array, num_sidedefs, sizeof(uint16_g), SidedefCompare);

  // now mark them off
  for (i=0; i < num_sidedefs - 1; i++)
  {
    // duplicate ?
    if (SidedefCompare(array + i, array + i+1) == 0)
    {
      sidedef_t *A = sidedefs[array[i]];
      sidedef_t *B = sidedefs[array[i+1]];

      // found a duplicate !
      if (A->equiv)
        B->equiv = A->equiv;
      else
        B->equiv = A;
    }
  }

  UtilFree(array);
}

static void PruneLinedefs(void)
{
  int i;
  int new_num;

  DisplayTicker();

  // scan all linedefs
  for (i=0, new_num=0; i < num_linedefs; i++)
  {
    linedef_t *L = linedefs[i];

    // handle duplicated vertices
    while (L->start->equiv)
    {
      L->start->ref_count--;
      L->start = L->start->equiv;
      L->start->ref_count++;
    }

    while (L->end->equiv)
    {
      L->end->ref_count--;
      L->end = L->end->equiv;
      L->end->ref_count++;
    }

    // handle duplicated sidedefs
    while (L->right && L->right->equiv)
    {
      L->right->ref_count--;
      L->right = L->right->equiv;
      L->right->ref_count++;
    }

    while (L->left && L->left->equiv)
    {
      L->left->ref_count--;
      L->left = L->left->equiv;
      L->left->ref_count++;
    }

    // remove zero length lines
    if (L->zero_len)
    {
      L->start->ref_count--;
      L->end->ref_count--;

      UtilFree(L);
      continue;
    }

    L->index = new_num;
    linedefs[new_num++] = L;
  }

  if (new_num < num_linedefs)
  {
    PrintMsg("Pruned %d zero-length linedefs\n", num_linedefs - new_num);
    num_linedefs = new_num;
  }

  if (new_num == 0)
    FatalError("Couldn't find any Linedefs");
}

static void PruneVertices(void)
{
  int i;
  int new_num;
  int unused = 0;

  DisplayTicker();

  // scan all vertices
  for (i=0, new_num=0; i < num_vertices; i++)
  {
    vertex_t *V = vertices[i];

    if (V->ref_count < 0)
      InternalError("Vertex %d ref_count is %d", i, V->ref_count);
    
    if (V->ref_count == 0)
    {
      if (V->equiv == NULL)
        unused++;

      UtilFree(V);
      continue;
    }

    V->index = new_num;
    vertices[new_num++] = V;
  }

  if (new_num < num_vertices)
  {
    int dup_num = num_vertices - new_num - unused;

    if (unused > 0)
      PrintMsg("Pruned %d unused vertices "
        "(this is normal if the nodes were built before)\n", unused);

    if (dup_num > 0)
      PrintMsg("Pruned %d duplicate vertices\n", dup_num);

    num_vertices = new_num;
  }

  if (new_num == 0)
    FatalError("Couldn't find any Vertices");
 
  num_normal_vert = num_vertices;
}

static void PruneSidedefs(void)
{
  int i;
  int new_num;
  int unused = 0;

  DisplayTicker();

  // scan all sidedefs
  for (i=0, new_num=0; i < num_sidedefs; i++)
  {
    sidedef_t *S = sidedefs[i];

    if (S->ref_count < 0)
      InternalError("Sidedef %d ref_count is %d", i, S->ref_count);
    
    if (S->ref_count == 0)
    {
      if (S->sector)
        S->sector->ref_count--;

      if (S->equiv == NULL)
        unused++;

      UtilFree(S);
      continue;
    }

    S->index = new_num;
    sidedefs[new_num++] = S;
  }

  if (new_num < num_sidedefs)
  {
    int dup_num = num_sidedefs - new_num - unused;

    if (unused > 0)
      PrintMsg("Pruned %d unused sidedefs\n", unused);

    if (dup_num > 0)
      PrintMsg("Pruned %d duplicate sidedefs\n", dup_num);

    num_sidedefs = new_num;
  }

  if (new_num == 0)
    FatalError("Couldn't find any Sidedefs");
}

static void PruneSectors(void)
{
  int i;
  int new_num;

  DisplayTicker();

  // scan all sectors
  for (i=0, new_num=0; i < num_sectors; i++)
  {
    sector_t *S = sectors[i];

    if (S->ref_count < 0)
      InternalError("Sector %d ref_count is %d", i, S->ref_count);
    
    if (S->ref_count == 0)
    {
      UtilFree(S);
      continue;
    }

    S->index = new_num;
    sectors[new_num++] = S;
  }

  if (new_num < num_sectors)
  {
    PrintMsg("Pruned %d unused sectors\n", num_sectors - new_num);
    num_sectors = new_num;
  }

  if (new_num == 0)
    FatalError("Couldn't find any Sectors");
}

static INLINE_G int TransformSegDist(const seg_t *seg)
{
  float_g sx = seg->side ? seg->linedef->end->x : seg->linedef->start->x;
  float_g sy = seg->side ? seg->linedef->end->y : seg->linedef->start->y;

  return (int) ceil(ComputeDist(seg->start->x - sx, seg->start->y - sy));
}

static INLINE_G int TransformAngle(angle_g angle)
{
  int result;
  
  result = (int)(angle * 65536.0 / 360.0);
  
  if (result < 0)
    result += 65536;

  return (result & 0xFFFF);
}

static int SegCompare(const void *p1, const void *p2)
{
  const seg_t *A = ((const seg_t **) p1)[0];
  const seg_t *B = ((const seg_t **) p2)[0];

  if (A->index < 0)
    InternalError("Seg %p never reached a subsector !", A);

  if (B->index < 0)
    InternalError("Seg %p never reached a subsector !", B);

  return (A->index - B->index);
}


/* ----- vertex routines ------------------------------- */

static void VertexAddWallTip(vertex_t *vert, float_g dx, float_g dy,
  sector_t *left, sector_t *right)
{
  wall_tip_t *tip = NewWallTip();
  wall_tip_t *after;

  tip->angle = ComputeAngle(dx, dy);
  tip->left  = left;
  tip->right = right;

  // find the correct place (order is increasing angle)
  for (after=vert->tip_set; after && after->next; after=after->next)
  { }

  while (after && tip->angle + ANG_EPSILON < after->angle) 
    after = after->prev;
  
  // link it in
  tip->next = after ? after->next : vert->tip_set;
  tip->prev = after;

  if (after)
  {
    if (after->next)
      after->next->prev = tip;
    
    after->next = tip;
  }
  else
  {
    if (vert->tip_set)
      vert->tip_set->prev = tip;
    
    vert->tip_set = tip;
  }
}

static void CalculateWallTips(void)
{
  int i;

  DisplayTicker();

  for (i=0; i < num_linedefs; i++)
  {
    linedef_t *line = linedefs[i];

    float_g x1 = line->start->x;
    float_g y1 = line->start->y;
    float_g x2 = line->end->x;
    float_g y2 = line->end->y;

    sector_t *left  = (line->left)  ? line->left->sector  : NULL;
    sector_t *right = (line->right) ? line->right->sector : NULL;
    
    VertexAddWallTip(line->start, x2-x1, y2-y1, left, right);
    VertexAddWallTip(line->end,   x1-x2, y1-y2, right, left);
  }
 
  #if DEBUG_WALLTIPS
  for (i=0; i < num_vertices; i++)
  {
    vertex_t *vert = vertices[i];
    wall_tip_t *tip;

    PrintDebug("WallTips for vertex %d:\n", i);

    for (tip=vert->tip_set; tip; tip=tip->next)
    {
      PrintDebug("  Angle=%1.1f left=%d right=%d\n", tip->angle,
        tip->left ? tip->left->index : -1,
        tip->right ? tip->right->index : -1);
    }
  }
  #endif
}

//
// NewVertexFromSplitSeg
//
vertex_t *NewVertexFromSplitSeg(seg_t *seg, float_g x, float_g y)
{
  vertex_t *vert = NewVertex();

  vert->x = x;
  vert->y = y;

  vert->ref_count = seg->partner ? 4 : 2;

  if (doing_gl && (!cur_info->v1_vert || !doing_normal))
  {
    vert->index = num_gl_vert | 0x8000;
    num_gl_vert++;
  }
  else
  {
    vert->index = num_normal_vert;
    num_normal_vert++;
  }

  // compute wall_tip info

  VertexAddWallTip(vert, -seg->pdx, -seg->pdy, seg->sector, 
      seg->partner ? seg->partner->sector : NULL);

  VertexAddWallTip(vert, seg->pdx, seg->pdy,
      seg->partner ? seg->partner->sector : NULL, seg->sector);

  // create a duplex vertex if needed

  if (doing_normal && doing_gl && !cur_info->v1_vert)
  {
    vert->normal_dup = NewVertex();

    vert->normal_dup->x = x;
    vert->normal_dup->y = y;
    vert->normal_dup->ref_count = vert->ref_count;

    vert->normal_dup->index = num_normal_vert;
    num_normal_vert++;
  }

  return vert;
}

//
// NewVertexDegenerate
//
vertex_t *NewVertexDegenerate(vertex_t *start, vertex_t *end)
{
  float_g dx = end->x - start->x;
  float_g dy = end->y - start->y;

  float_g dlen = ComputeDist(dx, dy);

  vertex_t *vert = NewVertex();

  vert->ref_count = start->ref_count;

  if (doing_normal)
  {
    vert->index = num_normal_vert;
    num_normal_vert++;
  }
  else
  {
    vert->index = num_gl_vert | 0x8000;
    num_gl_vert++;
  }

  // compute new coordinates

  vert->x = start->x;
  vert->y = start->x;

  if (dlen == 0)
    InternalError("NewVertexDegenerate: bad delta !");

  dx /= dlen;
  dy /= dlen;

  while ((int)vert->x == (int)start->x && 
         (int)vert->y == (int)start->y)
  {
    vert->x += dx;
    vert->y += dy;
  }

  return vert;
}

//
// VertexCheckOpen
//
int VertexCheckOpen(vertex_t *vert, float_g dx, float_g dy,
    sector_t ** left_sec, sector_t ** right_sec)
{
  wall_tip_t *tip;

  angle_g angle = ComputeAngle(dx, dy);

  *left_sec = *right_sec = NULL;

  // first check whether there's a wall_tip that lies in the exact
  // direction of the given direction (which is relative to the
  // vertex).

  for (tip=vert->tip_set; tip; tip=tip->next)
  {
    if (fabs(tip->angle - angle) < ANG_EPSILON)
    {
      // yes, found one

      *left_sec  = tip->left;
      *right_sec = tip->right;

      return FALSE;
    }
  }

  // OK, now just find the first wall_tip whose angle is greater than
  // the angle we're interested in.  Therefore we'll be on the RIGHT
  // side of that wall_tip.

  for (tip=vert->tip_set; tip; tip=tip->next)
  {
    if (angle + ANG_EPSILON < tip->angle)
    {
      // found it
      *left_sec = *right_sec = tip->right;

      return (tip->right != NULL);
    }

    if (! tip->next)
    {
      // no more tips, thus we must be on the LEFT side of the tip
      // with the largest angle.

      *left_sec = *right_sec = tip->left;

      return (tip->left != NULL);
    }
  }
  
  InternalError("Vertex %d has no tips !", vert->index);
  return FALSE;
}

static void GroupPolyobjLinedefs(void)
{
  int count;
  int i;

  DisplayTicker();

  do
  {
    count = 0;

    for (i=0; i < num_linedefs; i++)
    {
      linedef_t *line = linedefs[i];

      if (line->polyobj)
        continue;
      
      if (line->start->polyobj || line->end->polyobj)
      {
        line->polyobj = 1;
        line->start->polyobj = 1;
        line->end->polyobj = 1;

        count++;
      }
    }
  } while (count > 0);

  #if DEBUG_POLYOBJ
  PrintDebug("\n");
  for (i=0; i < num_linedefs; i++)
  {
    linedef_t *line = linedefs[i];

    if (line->polyobj)
      PrintDebug("Linedef #%d belongs to a polyobj\n", i);
  }
  #endif
}


/* ----- writing routines ------------------------------ */

void PutVertices(char *name, int do_gl)
{
  int count, i;
  lump_t *lump;

  DisplayTicker();

  if (do_gl)
    lump = CreateGLLump(name);
  else
    lump = CreateLevelLump(name);

  for (i=0, count=0; i < num_vertices; i++)
  {
    raw_vertex_t raw;
    vertex_t *vert = vertices[i];

    if ((do_gl && !(vert->index & 0x8000)) ||
        (!do_gl && (vert->index & 0x8000)))
    {
      continue;
    }

    raw.x = SINT16((int)vert->x);
    raw.y = SINT16((int)vert->y);

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;
  }

  if (count >= 32768)
    PrintWarn("Number of %svertices (%d) has OVERFLOWED the "
        "normal limit!\n", do_gl ? "GL " : "", count);

  if (count != (do_gl ? num_gl_vert : num_normal_vert))
    InternalError("PutVertices miscounted (%d != %d)", count,
      do_gl ? num_gl_vert : num_normal_vert);
}

void PutV2Vertices(void)
{
  int count, i;
  lump_t *lump;

  static uint8_g v2_magic[4] = "gNd2";
 
  DisplayTicker();

  lump = CreateGLLump("GL_VERT");

  AppendLevelLump(lump, v2_magic, 4);

  for (i=0, count=0; i < num_vertices; i++)
  {
    raw_v2_vertex_t raw;
    vertex_t *vert = vertices[i];

    if (! (vert->index & 0x8000))
      continue;

    raw.x = SINT32((int)(vert->x * 65536.0));
    raw.y = SINT32((int)(vert->y * 65536.0));

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;
  }

  if (count >= 32768)
    PrintWarn("Number of GL vertices (%d) has OVERFLOWED the "
        "normal limit!\n", count);

  if (count != num_gl_vert)
    InternalError("PutV2Vertices miscounted (%d != %d)", count,
      num_gl_vert);
}

void PutSectors(void)
{
  int i;
  lump_t *lump = CreateLevelLump("SECTORS");

  DisplayTicker();

  for (i=0; i < num_sectors; i++)
  {
    raw_sector_t raw;
    sector_t *sector = sectors[i];

    raw.floor_h = SINT16(sector->floor_h);
    raw.ceil_h  = SINT16(sector->ceil_h);

    memcpy(raw.floor_tex, sector->floor_tex, sizeof(raw.floor_tex));
    memcpy(raw.ceil_tex,  sector->ceil_tex,  sizeof(raw.ceil_tex));

    raw.light = UINT16(sector->light);
    raw.special = UINT16(sector->special);
    raw.tag   = SINT16(sector->tag);

    AppendLevelLump(lump, &raw, sizeof(raw));
  }

  if (num_sectors >= 32768)
    PrintWarn("Number of sectors (%d) has OVERFLOWED the "
        "normal limit!\n", num_sectors);
}

void PutSidedefs(void)
{
  int i;
  lump_t *lump = CreateLevelLump("SIDEDEFS");

  DisplayTicker();

  for (i=0; i < num_sidedefs; i++)
  {
    raw_sidedef_t raw;
    sidedef_t *side = sidedefs[i];

    raw.sector = (side->sector == NULL) ? SINT16(-1) :
        UINT16(side->sector->index);

    raw.x_offset = SINT16(side->x_offset);
    raw.y_offset = SINT16(side->y_offset);

    memcpy(raw.upper_tex, side->upper_tex, sizeof(raw.upper_tex));
    memcpy(raw.lower_tex, side->lower_tex, sizeof(raw.lower_tex));
    memcpy(raw.mid_tex,   side->mid_tex,   sizeof(raw.mid_tex));
 
    AppendLevelLump(lump, &raw, sizeof(raw));
  }

  if (num_sidedefs >= 32768)
    PrintWarn("Number of sidedefs (%d) has OVERFLOWED the "
        "normal limit!\n", num_sidedefs);
}

void PutLinedefs(void)
{
  int i;
  lump_t *lump = CreateLevelLump("LINEDEFS");

  DisplayTicker();

  for (i=0; i < num_linedefs; i++)
  {
    raw_linedef_t raw;
    linedef_t *line = linedefs[i];

    raw.start = UINT16(line->start->index);
    raw.end   = UINT16(line->end->index);

    raw.flags = UINT16(line->flags);
    raw.type  = UINT16(line->type);
    raw.tag   = SINT16(line->tag);

    raw.sidedef1 = line->right ? UINT16(line->right->index) : 0xFFFF;
    raw.sidedef2 = line->left  ? UINT16(line->left->index)  : 0xFFFF;

    AppendLevelLump(lump, &raw, sizeof(raw));
  }

  if (num_linedefs >= 32768)
    PrintWarn("Number of linedefs (%d) has OVERFLOWED the "
        "normal limit!\n", num_linedefs);
}

void PutLinedefsHexen(void)
{
  int i, j;
  lump_t *lump = CreateLevelLump("LINEDEFS");

  DisplayTicker();

  for (i=0; i < num_linedefs; i++)
  {
    raw_hexen_linedef_t raw;
    linedef_t *line = linedefs[i];

    raw.start = UINT16(line->start->index);
    raw.end   = UINT16(line->end->index);

    raw.flags = UINT16(line->flags);
    raw.type  = UINT8(line->type);

    // write specials
    for (j=0; j < 5; j++)
      raw.specials[j] = UINT8(line->specials[j]);

    raw.sidedef1 = line->right ? UINT16(line->right->index) : 0xFFFF;
    raw.sidedef2 = line->left  ? UINT16(line->left->index)  : 0xFFFF;

    AppendLevelLump(lump, &raw, sizeof(raw));
  }

  if (num_linedefs >= 32768)
    PrintWarn("Number of linedefs (%d) has OVERFLOWED the "
        "normal limit!\n", num_linedefs);
}

void PutSegs(void)
{
  int i, count;
  lump_t *lump = CreateLevelLump("SEGS");

  DisplayTicker();

  // sort segs into ascending index
  qsort(segs, num_segs, sizeof(seg_t *), SegCompare);

  for (i=0, count=0; i < num_segs; i++)
  {
    raw_seg_t raw;
    seg_t *seg = segs[i];

    // ignore minisegs and degenerate segs
    if (! seg->linedef || seg->degenerate)
      continue;

    raw.start   = UINT16(seg->start->index);
    raw.end     = UINT16(seg->end->index);
    raw.angle   = UINT16(TransformAngle(seg->p_angle));
    raw.linedef = UINT16(seg->linedef->index);
    raw.flip    = UINT16(seg->side);
    raw.dist    = UINT16(TransformSegDist(seg));

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;

    #if DEBUG_BSP
    PrintDebug("PUT SEG: %04X  Vert %04X->%04X  Line %04X %s  "
        "Angle %04X  (%1.1f,%1.1f) -> (%1.1f,%1.1f)\n", seg->index,
        UINT16(raw.start), UINT16(raw.end), UINT16(raw.linedef), 
        seg->side ? "L" : "R", UINT16(raw.angle), 
        seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    #endif
  }

  if (count >= 32768)
    PrintWarn("Number of segs (%d) has OVERFLOWED the "
        "normal limit!\n", count);

  if (count != num_complete_seg)
    InternalError("PutSegs miscounted (%d != %d)", count,
      num_complete_seg);
}

void PutGLSegs(void)
{
  int i, count;
  lump_t *lump = CreateGLLump("GL_SEGS");

  DisplayTicker();

  // sort segs into ascending index
  qsort(segs, num_segs, sizeof(seg_t *), SegCompare);

  for (i=0, count=0; i < num_segs; i++)
  {
    raw_gl_seg_t raw;
    seg_t *seg = segs[i];

    // ignore degenerate segs
    if (seg->degenerate)
      continue;

    raw.start = UINT16(seg->start->index);
    raw.end   = UINT16(seg->end->index);
    raw.side  = UINT16(seg->side);

    if (seg->linedef)
      raw.linedef = UINT16(seg->linedef->index);
    else
      raw.linedef = UINT16(0xFFFF);

    if (seg->partner)
      raw.partner = UINT16(seg->partner->index);
    else
      raw.partner = UINT16(0xFFFF);

    AppendLevelLump(lump, &raw, sizeof(raw));

    count++;

    #if DEBUG_BSP
    PrintDebug("PUT GL SEG: %04X  Line %04X %s  Partner %04X  "
      "(%1.1f,%1.1f) -> (%1.1f,%1.1f)\n", seg->index, UINT16(raw.linedef), 
      seg->side ? "L" : "R", UINT16(raw.partner),
      seg->start->x, seg->start->y, seg->end->x, seg->end->y);
    #endif
  }

  if (count >= 32768)
    PrintWarn("Number of GL segs (%d) has OVERFLOWED the "
        "normal limit!\n", count);

  if (count != num_complete_seg)
    InternalError("PutGLSegs miscounted (%d != %d)", count,
      num_complete_seg);
}

void PutSubsecs(char *name, int do_gl)
{
  int i;
  lump_t *lump;

  DisplayTicker();

  if (do_gl)
    lump = CreateGLLump(name);
  else
    lump = CreateLevelLump(name);

  for (i=0; i < num_subsecs; i++)
  {
    raw_subsec_t raw;
    subsec_t *sub = subsecs[i];

    raw.first = UINT16(sub->seg_list->index);
    raw.num   = UINT16(sub->seg_count);

    AppendLevelLump(lump, &raw, sizeof(raw));

    #if DEBUG_BSP
    PrintDebug("PUT SUBSEC %04X  First %04X  Num %04X\n",
      sub->index, UINT16(raw.first), UINT16(raw.num));
    #endif
  }

  if (num_subsecs >= 32768)
    PrintWarn("Number of %ssubsectors (%d) has OVERFLOWED the "
        "normal limit!\n", do_gl ? "GL " : "", num_subsecs);
}

static int node_cur_index;

static void PutOneNode(node_t *node, lump_t *lump)
{
  raw_node_t raw;

  if (node->r.node)
    PutOneNode(node->r.node, lump);

  if (node->l.node)
    PutOneNode(node->l.node, lump);

  node->index = node_cur_index++;

  raw.x  = SINT16(node->x);
  raw.y  = SINT16(node->y);
  raw.dx = SINT16(node->dx / (node->too_long ? 2 : 1));
  raw.dy = SINT16(node->dy / (node->too_long ? 2 : 1));

  raw.b1.minx = SINT16(node->r.bounds.minx);
  raw.b1.miny = SINT16(node->r.bounds.miny);
  raw.b1.maxx = SINT16(node->r.bounds.maxx);
  raw.b1.maxy = SINT16(node->r.bounds.maxy);

  raw.b2.minx = SINT16(node->l.bounds.minx);
  raw.b2.miny = SINT16(node->l.bounds.miny);
  raw.b2.maxx = SINT16(node->l.bounds.maxx);
  raw.b2.maxy = SINT16(node->l.bounds.maxy);

  if (node->r.node)
    raw.right = UINT16(node->r.node->index);
  else if (node->r.subsec)
    raw.right = UINT16(node->r.subsec->index | 0x8000);
  else
    InternalError("Bad right child in node %d", node->index);

  if (node->l.node)
    raw.left = UINT16(node->l.node->index);
  else if (node->l.subsec)
    raw.left = UINT16(node->l.subsec->index | 0x8000);
  else
    InternalError("Bad left child in node %d", node->index);

  AppendLevelLump(lump, &raw, sizeof(raw));

  #if DEBUG_BSP
  PrintDebug("PUT NODE %04X  Left %04X  Right %04X  "
    "(%d,%d) -> (%d,%d)\n", node->index, UINT16(raw.left),
    UINT16(raw.right), node->x, node->y,
    node->x + node->dx, node->y + node->dy);
  #endif
}

void PutNodes(char *name, int do_gl, node_t *root)
{
  lump_t *lump;

  DisplayTicker();

  if (do_gl)
    lump = CreateGLLump(name);
  else
    lump = CreateLevelLump(name);

  node_cur_index = 0;

  if (root)
    PutOneNode(root, lump);
  
  if (node_cur_index >= 32768)
    PrintWarn("Number of %snodes (%d) has OVERFLOWED the "
        "normal limit!\n", do_gl ? "GL " : "", node_cur_index);

  if (node_cur_index != num_nodes)
    InternalError("PutNodes miscounted (%d != %d)",
      node_cur_index, num_nodes);
}


/* ----- whole-level routines --------------------------- */

//
// LoadLevel
//
void LoadLevel(void)
{
  char message[256];

  const char *level_name = GetLevelName();

  normal_exists = CheckForNormalNodes();

  doing_normal = !cur_info->gwa_mode && (cur_info->force_normal || 
    (!cur_info->no_normal && !normal_exists));

  doing_gl = cur_info->gwa_mode || !cur_info->no_gl;

  if (doing_normal && doing_gl)
    sprintf(message, "Building normal and GL nodes on %s", level_name);
  else if (doing_normal)
    sprintf(message, "Building normal nodes only on %s", level_name);
  else if (doing_gl)
    sprintf(message, "Building GL nodes on %s", level_name);
  else
    sprintf(message, "Building _nothing_ on %s", level_name);
  
  DisplaySetBarText(1, message);
  PrintMsg("\n\n%s\n\n", message);

  doing_hexen = cur_info->force_hexen;

  // -JL- Identify Hexen mode by presence of BEHAVIOR lump
  if (!doing_hexen && FindLevelLump("BEHAVIOR") != NULL)
  {
    PrintMsg("Hexen level detected.\n");
    doing_hexen = TRUE;
  }
  
  GetVertices();
  GetSectors();
  GetSidedefs();

  if (doing_hexen)
    GetLinedefsHexen();
  else
    GetLinedefs();

  PrintMsg("Loaded %d vertices, %d sectors, %d sides, %d lines\n", 
      num_vertices, num_sectors, num_sidedefs, num_linedefs);

  if (doing_normal && !cur_info->no_prune)
  {
    DetectDuplicateVertices();

    if (cur_info->pack_sides)
      DetectDuplicateSidedefs();

    PruneLinedefs();
    PruneVertices();
    PruneSidedefs();
    
    if (!cur_info->keep_sect)
      PruneSectors();
  }

  if (doing_hexen)
  {
    GroupPolyobjLinedefs();

    // -JL- Find sectors containing polyobjs
    FindPolyobjSectors();
  }
  
  CalculateWallTips();
}

//
// FreeLevel
//
void FreeLevel(void)
{
  FreeVertices();
  FreeSidedefs();
  FreeLinedefs();
  FreeSectors();
  FreeSegs();
  FreeSubsecs();
  FreeNodes();
  FreeWallTips();
}

//
// SaveLevel
//
void SaveLevel(node_t *root_node)
{
  if (cur_info->v1_vert)
    RoundOffBspTree(root_node);
 
  if (doing_gl)
  {
    if (cur_info->v1_vert)
      PutVertices("GL_VERT", TRUE);
    else
      PutV2Vertices();

    PutGLSegs();
    PutSubsecs("GL_SSECT", TRUE);
    PutNodes("GL_NODES", TRUE, root_node);
    
    // -JL- Add empty PVS lump
    CreateGLLump("GL_PVS");
  }

  if (doing_normal)
  {
    if (! cur_info->v1_vert)
      RoundOffBspTree(root_node);
 
    NormaliseBspTree(root_node);

    PutVertices("VERTEXES", FALSE);
    PutSectors();
    PutSidedefs();

    if (doing_hexen)
      PutLinedefsHexen();
    else
      PutLinedefs();
 
    PutSegs();
    PutSubsecs("SSECTORS", FALSE);
    PutNodes("NODES", FALSE, root_node);
  }

  if (!cur_info->gwa_mode)
  {
    PutBlockmap();

    if (!cur_info->no_reject || !FindLevelLump("REJECT"))
      PutReject();
  }
}

