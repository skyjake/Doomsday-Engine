/**
 * @file gamemap.c
 * Gamemap. @ingroup map
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_system.h"
#include "de_refresh.h"

#include "blockmap.h"
#include "generators.h"
#include "gamemap.h"

/// Size of Blockmap blocks in map units. Must be an integer power of two.
#define MAPBLOCKUNITS               (128)

const Uri* GameMap_Uri(GameMap* map)
{
    assert(map);
    return map->uri;
}

const char* GameMap_OldUniqueId(GameMap* map)
{
    assert(map);
    return map->uniqueId;
}

void GameMap_Bounds(GameMap* map, coord_t* min, coord_t* max)
{
    assert(map);

    V2d_Copy(min, map->aaBox.min);
    V2d_Copy(max, map->aaBox.max);
}

coord_t GameMap_Gravity(GameMap* map)
{
    assert(map);
    return map->effectiveGravity;
}

GameMap* GameMap_SetGravity(GameMap* map, coord_t gravity)
{
    assert(map);
    map->effectiveGravity = gravity;
    return map;
}

GameMap* GameMap_RestoreGravity(GameMap* map)
{
    assert(map);
    map->effectiveGravity = map->globalGravity;
    return map;
}

const divline_t* GameMap_TraceLOS(GameMap* map)
{
    assert(map);
    return &map->traceLOS;
}

const TraceOpening* GameMap_TraceOpening(GameMap* map)
{
    assert(map);
    return &map->traceOpening;
}

void GameMap_SetTraceOpening(GameMap* map, LineDef* lineDef)
{
    assert(map);
    // Is the linedef part of this map?
    if(!lineDef || GameMap_LineDefIndex(map, lineDef) < 0) return; // Odd...

    LineDef_SetTraceOpening(lineDef, &map->traceOpening);
}

int GameMap_AmbientLightLevel(GameMap* map)
{
    assert(map);
    return map->ambientLightLevel;
}

coord_t GameMap_SkyFix(GameMap* map, boolean ceiling)
{
    planetype_t plane = ceiling? PLN_CEILING : PLN_FLOOR;
    assert(map);
    return map->skyFix[plane].height;
}

GameMap* GameMap_SetSkyFix(GameMap* map, boolean ceiling, coord_t height)
{
    planetype_t plane = ceiling? PLN_CEILING : PLN_FLOOR;
    assert(map);
    map->skyFix[plane].height = height;
    return map;
}

Vertex* GameMap_Vertex(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numVertexes) return NULL;
    return &map->vertexes[idx];
}

int GameMap_VertexIndex(GameMap* map, Vertex* vtx)
{
    assert(map);
    if(!vtx || !(vtx >= map->vertexes && vtx <= &map->vertexes[map->numVertexes])) return -1;
    return vtx - map->vertexes;
}

int GameMap_LineDefIndex(GameMap* map, LineDef* line)
{
    assert(map);
    if(!line || !(line >= map->lineDefs && line <= &map->lineDefs[map->numLineDefs])) return -1;
    return line - map->lineDefs;
}

LineDef* GameMap_LineDef(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numLineDefs) return NULL;
    return &map->lineDefs[idx];
}

int GameMap_SideDefIndex(GameMap* map, SideDef* side)
{
    assert(map);
    if(!side || !(side >= map->sideDefs && side <= &map->sideDefs[map->numSideDefs])) return -1;
    return side - map->sideDefs;
}

SideDef* GameMap_SideDef(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numSideDefs) return NULL;
    return &map->sideDefs[idx];
}

int GameMap_SectorIndex(GameMap* map, Sector* sec)
{
    assert(map);
    if(!sec || !(sec >= map->sectors && sec <= &map->sectors[map->numSectors])) return -1;
    return sec - map->sectors;
}

Sector* GameMap_Sector(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numSectors) return NULL;
    return &map->sectors[idx];
}

Sector* GameMap_SectorByBase(GameMap* map, const void* ddMobjBase)
{
    uint i, k;
    assert(map);
    for(i = 0; i < map->numSectors; ++i)
    {
        Sector* sec = &map->sectors[i];

        if(ddMobjBase == &sec->base)
        {
            return sec;
        }

        // Check the planes of this sector
        for(k = 0; k < sec->planeCount; ++k)
        {
            if(ddMobjBase == &sec->planes[k]->base)
            {
                return sec;
            }
        }
    }
    return NULL;
}

int GameMap_BspLeafIndex(GameMap* map, BspLeaf* leaf)
{
    DENG_UNUSED(map);
    if(!leaf) return -1;
    return leaf->index;
}

BspLeaf* GameMap_BspLeaf(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numBspLeafs) return NULL;
    return map->bspLeafs[idx];
}

int GameMap_HEdgeIndex(GameMap* map, HEdge* hedge)
{
    DENG_UNUSED(map);
    if(hedge) return -1;
    return hedge->index;
}

HEdge* GameMap_HEdge(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numHEdges) return NULL;
    return map->hedges[idx];
}

int GameMap_BspNodeIndex(GameMap* map, BspNode* node)
{
    DENG_UNUSED(map);
    if(!node) return -1;
    return node->index;
}

BspNode* GameMap_BspNode(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numBspNodes) return NULL;
    return map->bspNodes[idx];
}

uint GameMap_VertexCount(GameMap* map)
{
    assert(map);
    return map->numVertexes;
}

uint GameMap_LineDefCount(GameMap* map)
{
    assert(map);
    return map->numLineDefs;
}

uint GameMap_SideDefCount(GameMap* map)
{
    assert(map);
    return map->numSideDefs;
}

uint GameMap_SectorCount(GameMap* map)
{
    assert(map);
    return map->numSectors;
}

uint GameMap_BspLeafCount(GameMap* map)
{
    assert(map);
    return map->numBspLeafs;
}

uint GameMap_HEdgeCount(GameMap* map)
{
    assert(map);
    return map->numHEdges;
}

uint GameMap_BspNodeCount(GameMap* map)
{
    assert(map);
    return map->numBspNodes;
}

uint GameMap_PolyobjCount(GameMap* map)
{
    assert(map);
    return map->numPolyObjs;
}

Polyobj* GameMap_PolyobjByID(GameMap* map, uint id)
{
    assert(map);
    if(id < map->numPolyObjs)
        return map->polyObjs[id];
    return NULL;
}

Polyobj* GameMap_PolyobjByTag(GameMap* map, int tag)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        Polyobj* po = map->polyObjs[i];
        if(po->tag == tag)
        {
            return po;
        }
    }
    return NULL;
}

Polyobj* GameMap_PolyobjByBase(GameMap* map, void* ddMobjBase)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        Polyobj* po = map->polyObjs[i];
        if(po == ddMobjBase)
        {
            return po;
        }
    }
    return NULL;
}

static void initPolyobj(Polyobj* po)
{
    LineDef** lineIter;
    BspLeaf* bspLeaf;
    vec2d_t avg; /// < Used to find a polyobj's center, and hence BSP leaf.

    if(!po) return;

    V2d_Set(avg, 0, 0);
    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        LineDef* line = *lineIter;
        SideDef* front = line->L_frontside;

        front->SW_topinflags |= SUIF_NO_RADIO;
        front->SW_middleinflags |= SUIF_NO_RADIO;
        front->SW_bottominflags |= SUIF_NO_RADIO;

        if(line->L_backside)
        {
            SideDef* back = line->L_backside;

            back->SW_topinflags |= SUIF_NO_RADIO;
            back->SW_middleinflags |= SUIF_NO_RADIO;
            back->SW_bottominflags |= SUIF_NO_RADIO;
        }

        V2d_Sum(avg, avg, line->L_v1origin);
    }
    V2d_Scale(avg, 1.f / po->lineCount);

    bspLeaf = P_BspLeafAtPoint(avg);
    if(bspLeaf)
    {
        if(bspLeaf->polyObj)
        {
            Con_Message("Warning: GameMap::initPolyobj: Multiple polyobjs in a single BSP leaf\n"
                        "  (BSP leaf %lu, sector %lu). Previous polyobj overridden.\n",
                        (unsigned long)GET_BSPLEAF_IDX(bspLeaf), (unsigned long)GET_SECTOR_IDX(bspLeaf->sector));
        }
        bspLeaf->polyObj = po;
        po->bspLeaf = bspLeaf;
    }

    Polyobj_UpdateAABox(po);
    Polyobj_UpdateSurfaceTangents(po);

    P_PolyobjUnlink(po);
    P_PolyobjLink(po);
}

Generators* GameMap_Generators(GameMap* map)
{
    assert(map);
    // Time to initialize a new collection?
    if(!map->generators)
    {
        map->generators = Generators_New(map->numSectors);
    }
    return map->generators;
}

surfacelist_t* GameMap_DecoratedSurfaces(GameMap* map)
{
    assert(map);
    return &map->decoratedSurfaces;
}

surfacelist_t* GameMap_GlowingSurfaces(GameMap* map)
{
    assert(map);
    return &map->glowingSurfaces;
}

surfacelist_t* GameMap_ScrollingSurfaces(GameMap* map)
{
    assert(map);
    return &map->scrollingSurfaces;
}

planelist_t* GameMap_TrackedPlanes(GameMap* map)
{
    assert(map);
    return &map->trackedPlanes;
}

void GameMap_InitPolyobjs(GameMap* map)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        initPolyobj(map->polyObjs[i]);
    }
}

void GameMap_InitNodePiles(GameMap* map)
{
    uint i, starttime = 0;

    assert(map);

    VERBOSE( Con_Message("GameMap::InitNodePiles: Initializing...\n") )
    VERBOSE2( starttime = Sys_GetRealTime() )

    // Initialize node piles and line rings.
    NP_Init(&map->mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&map->lineNodes, map->numLineDefs + 1000);

    // Allocate the rings.
    map->lineLinks = Z_Malloc(sizeof(*map->lineLinks) * map->numLineDefs, PU_MAPSTATIC, 0);

    for(i = 0; i < map->numLineDefs; ++i)
    {
        map->lineLinks[i] = NP_New(&map->lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    VERBOSE2( Con_Message("  Done in %.2f seconds.\n", (Sys_GetRealTime() - starttime) / 1000.0f) )
}

void GameMap_InitLineDefBlockmap(GameMap* map, const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2d_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                 min_[VY] - BLOCKMAP_MARGIN);
    V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                 max_[VY] + BLOCKMAP_MARGIN);

    map->lineDefBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap_InitMobjBlockmap(GameMap* map, const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2d_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                 min_[VY] - BLOCKMAP_MARGIN);
    V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                 max_[VY] + BLOCKMAP_MARGIN);

    map->mobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap_InitPolyobjBlockmap(GameMap* map, const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2d_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                 min_[VY] - BLOCKMAP_MARGIN);
    V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                 max_[VY] + BLOCKMAP_MARGIN);

    map->polyobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap_InitBspLeafBlockmap(GameMap* map, const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2d_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                 min_[VY] - BLOCKMAP_MARGIN);
    V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                 max_[VY] + BLOCKMAP_MARGIN);

    map->bspLeafBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap_LinkMobj(GameMap* map, mobj_t* mo)
{
    Blockmap* blockmap;
    BlockmapCell cell;
    assert(map);

    // Do not link NULL mobjs.
    if(!mo)
    {
        DEBUG_Message(("Warning: GameMap::LinkMobjInBlockmap: Attempted with NULL mobj argument.\n"));
        return;
    }

    blockmap = map->mobjBlockmap;
    Blockmap_Cell(blockmap, cell, mo->origin);
    Blockmap_CreateCellAndLinkObject(blockmap, cell, mo);
}

boolean GameMap_UnlinkMobj(GameMap* map, mobj_t* mo)
{
    boolean unlinked = false;
    assert(map);

    if(mo)
    {
        Blockmap* blockmap = map->mobjBlockmap;
        BlockmapCell cell;
        Blockmap_Cell(blockmap, cell, mo->origin);
        return Blockmap_UnlinkObjectInCell(blockmap, cell, mo);
    }
    return unlinked;
}

typedef struct bmapmoiterparams_s {
    int localValidCount;
    int (*func) (mobj_t*, void *);
    void* param;
} bmapmoiterparams_t;

static int blockmapCellMobjsIterator(void* object, void* context)
{
    mobj_t* mobj = (mobj_t*)object;
    bmapmoiterparams_t* args = (bmapmoiterparams_t*) context;
    if(mobj->validCount != args->localValidCount)
    {
        int result;

        // This mobj has now been processed for the current iteration.
        mobj->validCount = args->localValidCount;

        // Action the callback.
        result = args->func(mobj, args->param);
        if(result) return result; // Stop iteration.
    }
    return false; // Continue iteration.
}

static int GameMap_IterateCellMobjs(GameMap* map, const_BlockmapCell cell,
    int (*callback) (mobj_t*, void*), void* context)
{
    bmapmoiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellObjects(map->mobjBlockmap, cell,
                                       blockmapCellMobjsIterator, (void*)&args);
}

static int GameMap_IterateCellBlockMobjs(GameMap* map, const BlockmapCellBlock* cellBlock,
    int (*callback) (mobj_t*, void*), void* context)
{
    bmapmoiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellBlockObjects(map->mobjBlockmap, cellBlock,
                                            blockmapCellMobjsIterator, (void*) &args);
}

int GameMap_MobjsBoxIterator(GameMap* map, const AABoxd* box,
    int (*callback) (mobj_t*, void*), void* parameters)
{
    BlockmapCellBlock cellBlock;
    assert(map);
    Blockmap_CellBlock(map->mobjBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockMobjs(map, &cellBlock, callback, parameters);
}

void GameMap_LinkLineDef(GameMap* map, LineDef* lineDef)
{
    vec2d_t origin, cellSize, cell, from, to;
    BlockmapCellBlock cellBlock;
    Blockmap* blockmap;
    uint x, y;
    assert(map);

    // Do not link NULL linedefs.
    if(!lineDef)
    {
        DEBUG_Message(("Warning: GameMap::LinkLineDefInBlockmap: Attempted with NULL linedef argument.\n"));
        return;
    }

    // LineDefs of Polyobjs don't get into the blockmap (presently...).
    if(lineDef->inFlags & LF_POLYOBJ) return;

    blockmap = map->lineDefBlockmap;
    V2d_Copy(origin, Blockmap_Origin(blockmap));
    V2d_Copy(cellSize, Blockmap_CellSize(blockmap));

    // Determine the block of cells we'll be working within.
    Blockmap_CellBlock(blockmap, &cellBlock, &lineDef->aaBox);

    for(y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        if(lineDef->slopeType == ST_VERTICAL || lineDef->slopeType == ST_HORIZONTAL)
        {
            Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, lineDef);
            continue;
        }

        // Calculate cell origin.
        V2d_Copy(cell, Blockmap_CellSize(blockmap));
        cell[VX] *= x; cell[VY] *= y;
        V2d_Sum(cell, cell, Blockmap_Origin(blockmap));

        // Choose a cell diagonal to test.
        if(lineDef->slopeType == ST_POSITIVE)
        {
            // LineDef slope / vs \ cell diagonal.
            V2d_Set(from, cell[VX], cell[VY] + cellSize[VY]);
            V2d_Set(to,   cell[VX] + cellSize[VX], cell[VY]);
        }
        else
        {
            // LineDef slope \ vs / cell diagonal.
            V2d_Set(from, cell[VX] + cellSize[VX], cell[VY] + cellSize[VY]);
            V2d_Set(to,   cell[VX], cell[VY]);
        }

        // Would LineDef intersect this?
        if(LineDef_PointOnSide(lineDef, from) < 0 != LineDef_PointOnSide(lineDef, to) < 0)
        {
            Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, lineDef);
        }
    }
}

typedef struct bmapiterparams_s {
    int localValidCount;
    int (*func) (LineDef*, void *);
    void* param;
} bmapiterparams_t;

static int blockmapCellLinesIterator(void* object, void* context)
{
    LineDef* lineDef = (LineDef*)object;
    bmapiterparams_t* args = (bmapiterparams_t*) context;
    if(lineDef->validCount != args->localValidCount)
    {
        int result;

        // This linedef has now been processed for the current iteration.
        lineDef->validCount = args->localValidCount;

        // Action the callback.
        result = args->func(lineDef, args->param);
        if(result) return result; // Stop iteration.
    }
    return false; // Continue iteration.
}

static int GameMap_IterateCellLineDefs(GameMap* map, const_BlockmapCell cell,
    int (*callback) (LineDef*, void*), void* context)
{
    bmapiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellObjects(map->lineDefBlockmap, cell,
                                       blockmapCellLinesIterator, (void*)&args);
}

static int GameMap_IterateCellBlockLineDefs(GameMap* map, const BlockmapCellBlock* cellBlock,
    int (*callback) (LineDef*, void*), void* context)
{
    bmapiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellBlockObjects(map->lineDefBlockmap, cellBlock,
                                            blockmapCellLinesIterator, (void*) &args);
}

int GameMap_LineDefIterator(GameMap* map, int (*callback) (LineDef*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numLineDefs; ++i)
    {
        int result = callback(map->lineDefs + i, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

void GameMap_LinkBspLeaf(GameMap* map, BspLeaf* bspLeaf)
{
    Blockmap* blockmap;
    BlockmapCellBlock cellBlock;
    AABoxd aaBox;
    uint x, y;
    assert(map);

    // Do not link NULL BSP leafs.
    if(!bspLeaf)
    {
        DEBUG_Message(("Warning: GameMap::LinkBspLeafInBlockmap: Attempted with NULL bspLeaf argument.\n"));
        return;
    }

    // BspLeafs without sectors don't get in.
    if(!bspLeaf->sector) return;

    blockmap = map->bspLeafBlockmap;
    aaBox.minX = bspLeaf->aaBox.minX;
    aaBox.minY = bspLeaf->aaBox.minY;
    aaBox.maxX = bspLeaf->aaBox.maxX;
    aaBox.maxY = bspLeaf->aaBox.maxY;
    Blockmap_CellBlock(blockmap, &cellBlock, &aaBox);

    for(y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, bspLeaf);
    }
}

typedef struct subseciterparams_s {
    const AABoxd* box;
    Sector* sector;
    int localValidCount;
    int (*func) (BspLeaf*, void*);
    void* param;
} bmapbspleafiterateparams_t;

static int blockmapCellBspLeafsIterator(void* object, void* context)
{
    BspLeaf* bspLeaf = (BspLeaf*)object;
    bmapbspleafiterateparams_t* args = (bmapbspleafiterateparams_t*) context;
    if(bspLeaf->validCount != args->localValidCount)
    {
        boolean ok = true;

        // This BspLeaf has now been processed for the current iteration.
        bspLeaf->validCount = args->localValidCount;

        // Check the sector restriction.
        if(args->sector && bspLeaf->sector != args->sector)
            ok = false;

        // Check the bounds.
        if(args->box &&
           (bspLeaf->aaBox.maxX < args->box->minX ||
            bspLeaf->aaBox.minX > args->box->maxX ||
            bspLeaf->aaBox.minY > args->box->maxY ||
            bspLeaf->aaBox.maxY < args->box->minY))
            ok = false;

        if(ok)
        {
            // Action the callback.
            int result = args->func(bspLeaf, args->param);
            if(result) return result; // Stop iteration.
        }
    }
    return false; // Continue iteration.
}

static int GameMap_IterateCellBspLeafs(GameMap* map, const_BlockmapCell cell,
    Sector* sector, const AABoxd* box, int localValidCount,
    int (*callback) (BspLeaf*, void*), void* context)
{
    bmapbspleafiterateparams_t args;
    assert(map);

    args.localValidCount = localValidCount;
    args.func = callback;
    args.param = context;
    args.sector = sector;
    args.box = box;

    return Blockmap_IterateCellObjects(map->bspLeafBlockmap, cell,
                                       blockmapCellBspLeafsIterator, (void*)&args);
}

static int GameMap_IterateCellBlockBspLeafs(GameMap* map, const BlockmapCellBlock* cellBlock,
    Sector* sector,  const AABoxd* box, int localValidCount,
    int (*callback) (BspLeaf*, void*), void* context)
{
    bmapbspleafiterateparams_t args;
    assert(map);

    args.localValidCount = localValidCount;
    args.func = callback;
    args.param = context;
    args.sector = sector;
    args.box = box;

    return Blockmap_IterateCellBlockObjects(map->bspLeafBlockmap, cellBlock,
                                            blockmapCellBspLeafsIterator, (void*) &args);
}

int GameMap_BspLeafsBoxIterator(GameMap* map, const AABoxd* box, Sector* sector,
    int (*callback) (BspLeaf*, void*), void* parameters)
{
    static int localValidCount = 0;
    BlockmapCellBlock cellBlock;
    assert(map);

    // This is only used here.
    localValidCount++;

    Blockmap_CellBlock(map->bspLeafBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockBspLeafs(map, &cellBlock, sector, box,
                                              localValidCount, callback, parameters);
}

int GameMap_BspLeafIterator(GameMap* map, int (*callback) (BspLeaf*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numBspLeafs; ++i)
    {
        int result = callback(map->bspLeafs[i], parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

void GameMap_LinkPolyobj(GameMap* map, Polyobj* po)
{
    Blockmap* blockmap;
    BlockmapCellBlock cellBlock;
    uint x, y;
    assert(map);

    // Do not link NULL polyobjs.
    if(!po)
    {
        DEBUG_Message(("Warning: GameMap::LinkPolyobjInBlockmap: Attempted with NULL polyobj argument.\n"));
        return;
    }

    blockmap = map->polyobjBlockmap;
    Blockmap_CellBlock(blockmap, &cellBlock, &po->aaBox);

    for(y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, po);
    }
}

void GameMap_UnlinkPolyobj(GameMap* map, Polyobj* po)
{
    Blockmap* blockmap;
    BlockmapCellBlock cellBlock;
    assert(map);

    if(!po) return;

    blockmap = map->polyobjBlockmap;
    Blockmap_CellBlock(map->polyobjBlockmap, &cellBlock, &po->aaBox);
    Blockmap_UnlinkObjectInCellBlock(blockmap, &cellBlock, po);
}

typedef struct bmappoiterparams_s {
    int localValidCount;
    int (*func) (Polyobj*, void *);
    void* param;
} bmappoiterparams_t;

static int blockmapCellPolyobjsIterator(void* object, void* context)
{
    Polyobj* polyobj = (Polyobj*)object;
    bmappoiterparams_t* args = (bmappoiterparams_t*) context;
    if(polyobj->validCount != args->localValidCount)
    {
        int result;

        // This polyobj has now been processed for the current iteration.
        polyobj->validCount = args->localValidCount;

        // Action the callback.
        result = args->func(polyobj, args->param);
        if(result) return result; // Stop iteration.
    }
    return false; // Continue iteration.
}

static int GameMap_IterateCellPolyobjs(GameMap* map, const_BlockmapCell cell,
    int (*callback) (Polyobj*, void*), void* context)
{
    bmappoiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellObjects(map->polyobjBlockmap, cell,
                                       blockmapCellPolyobjsIterator, (void*)&args);
}

static int GameMap_IterateCellBlockPolyobjs(GameMap* map, const BlockmapCellBlock* cellBlock,
    int (*callback) (Polyobj*, void*), void* context)
{
    bmappoiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellBlockObjects(map->polyobjBlockmap, cellBlock,
                                            blockmapCellPolyobjsIterator, (void*) &args);
}

int GameMap_PolyobjsBoxIterator(GameMap* map, const AABoxd* box,
    int (*callback) (struct polyobj_s*, void*), void* parameters)
{
    BlockmapCellBlock cellBlock;
    assert(map);
    Blockmap_CellBlock(map->polyobjBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockPolyobjs(map, &cellBlock, callback, parameters);
}

int GameMap_PolyobjIterator(GameMap* map, int (*callback) (Polyobj*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        int result = callback(map->polyObjs[i], parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

typedef struct poiterparams_s {
    int (*func) (LineDef*, void*);
    void* param;
} poiterparams_t;

int PTR_PolyobjLines(Polyobj* po, void* context)
{
    poiterparams_t* args = (poiterparams_t*) context;

    return Polyobj_LineIterator(po, args->func, args->param);
}

static int GameMap_IterateCellPolyobjLineDefsIterator(GameMap* map, const_BlockmapCell cell,
    int (*callback) (LineDef*, void*), void* context)
{
    bmappoiterparams_t args;
    poiterparams_t poargs;
    assert(map);

    poargs.func = callback;
    poargs.param = context;

    args.localValidCount = validCount;
    args.func = PTR_PolyobjLines;
    args.param = &poargs;

    return Blockmap_IterateCellObjects(map->polyobjBlockmap, cell,
                                       blockmapCellPolyobjsIterator, &args);
}

static int GameMap_IterateCellBlockPolyobjLineDefs(GameMap* map, const BlockmapCellBlock* cellBlock,
    int (*callback) (LineDef*, void*), void* context)
{
    bmappoiterparams_t args;
    poiterparams_t poargs;
    assert(map);

    poargs.func = callback;
    poargs.param = context;

    args.localValidCount = validCount;
    args.func = PTR_PolyobjLines;
    args.param = &poargs;

    return Blockmap_IterateCellBlockObjects(map->polyobjBlockmap, cellBlock,
                                            blockmapCellPolyobjsIterator, (void*) &args);
}

int GameMap_LineDefsBoxIterator(GameMap* map, const AABoxd* box,
    int (*callback) (LineDef*, void*), void* parameters)
{
    BlockmapCellBlock cellBlock;
    assert(map);
    Blockmap_CellBlock(map->lineDefBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockLineDefs(map, &cellBlock, callback, parameters);
}

int GameMap_PolyobjLinesBoxIterator(GameMap* map, const AABoxd* box,
    int (*callback) (LineDef*, void*), void* parameters)
{
    BlockmapCellBlock cellBlock;
    assert(map);
    Blockmap_CellBlock(map->polyobjBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockPolyobjLineDefs(map, &cellBlock, callback, parameters);
}

/**
 * LineDefs and Polyobj LineDefs (note Polyobj LineDefs are iterated first).
 *
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to GameMap_IterateCellLineDefs(), then make one or more calls to it.
 */
int GameMap_AllLineDefsBoxIterator(GameMap* map, const AABoxd* box,
    int (*callback) (LineDef*, void*), void* parameters)
{
    assert(map);
    if(map->numPolyObjs > 0)
    {
        int result = P_PolyobjLinesBoxIterator(box, callback, parameters);
        if(result) return result;
    }
    return P_LinesBoxIterator(box, callback, parameters);
}

int GameMap_VertexIterator(GameMap* map, int (*callback) (Vertex*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numVertexes; ++i)
    {
        int result = callback(map->vertexes + i, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int GameMap_SideDefIterator(GameMap* map, int (*callback) (SideDef*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numSideDefs; ++i)
    {
        int result = callback(map->sideDefs + i, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int GameMap_SectorIterator(GameMap* map, int (*callback) (Sector*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numSectors; ++i)
    {
        int result = callback(map->sectors + i, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int GameMap_HEdgeIterator(GameMap* map, int (*callback) (HEdge*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numHEdges; ++i)
    {
        int result = callback(map->hedges[i], parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int GameMap_BspNodeIterator(GameMap* map, int (*callback) (BspNode*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numBspNodes; ++i)
    {
        int result = callback(map->bspNodes[i], parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

static int traverseCellPath2(Blockmap* bmap, uint const fromBlock[2],
    uint const toBlock[2], coord_t const from[2], coord_t const to[2],
    int (*callback) (uint const block[2], void* parameters), void* parameters)
{
    int result = false; // Continue iteration.
    coord_t intercept[2], delta[2], partial;
    uint count, block[2];
    int stepDir[2];
    assert(bmap);

    if(toBlock[VX] > fromBlock[VX])
    {
        stepDir[VX] = 1;
        partial = from[VX] / Blockmap_CellWidth(bmap);
        partial = 1 - (partial - (int) partial);
        delta[VY] = (to[VY] - from[VY]) / fabs(to[VX] - from[VX]);
    }
    else if(toBlock[VX] < fromBlock[VX])
    {
        stepDir[VX] = -1;
        partial = from[VX] / Blockmap_CellWidth(bmap);
        partial = (partial - (int) partial);
        delta[VY] = (to[VY] - from[VY]) / fabs(to[VX] - from[VX]);
    }
    else
    {
        stepDir[VX] = 0;
        partial = 1;
        delta[VY] = 256;
    }
    intercept[VY] = from[VY] / Blockmap_CellHeight(bmap) + partial * delta[VY];

    if(toBlock[VY] > fromBlock[VY])
    {
        stepDir[VY] = 1;
        partial = from[VY] / Blockmap_CellHeight(bmap);
        partial = 1 - (partial - (int) partial);
        delta[VX] = (to[VX] - from[VX]) / fabs(to[VY] - from[VY]);
    }
    else if(toBlock[VY] < fromBlock[VY])
    {
        stepDir[VY] = -1;
        partial = from[VY] / Blockmap_CellHeight(bmap);
        partial = (partial - (int) partial);
        delta[VX] = (to[VX] - from[VX]) / fabs(to[VY] - from[VY]);
    }
    else
    {
        stepDir[VY] = 0;
        partial = 1;
        delta[VX] = 256;
    }
    intercept[VX] = from[VX] / Blockmap_CellWidth(bmap) + partial * delta[VX];

    //
    // Step through map blocks.
    //

    // Count is present to prevent a round off error from skipping the
    // break and ending up in an infinite loop..
    block[VX] = fromBlock[VX];
    block[VY] = fromBlock[VY];
    for(count = 0; count < 64; ++count)
    {
        result = callback(block, parameters);
        if(result) return result; // Early out.

        if(block[VX] == toBlock[VX] && block[VY] == toBlock[VY])
            break;

        /// @todo Replace incremental translation?
        if((uint)intercept[VY] == block[VY])
        {
            block[VX] += stepDir[VX];
            intercept[VY] += delta[VY];
        }
        else if((uint)intercept[VX] == block[VX])
        {
            block[VY] += stepDir[VY];
            intercept[VX] += delta[VX];
        }
    }

    return false; // Continue iteration.
}

static int traverseCellPath(GameMap* map, Blockmap* bmap, coord_t const from_[2],
    coord_t const to_[2], int (*callback) (uint const block[2], void* parameters),
    void* parameters)
{
    // Constant terms implicitly defined by DOOM's original version of this
    // algorithm (we must honor these fudge factors for compatibility).
    const coord_t epsilon    = FIX2FLT(FRACUNIT);
    const coord_t unitOffset = FIX2FLT(FRACUNIT);
    uint fromBlock[2], toBlock[2];
    vec2d_t from, to, min, max;
    coord_t dX, dY;
    assert(bmap);

    V2d_Copy(min, Blockmap_Bounds(bmap)->min);
    V2d_Copy(max, Blockmap_Bounds(bmap)->max);

    // We may need to clip and/or fudge these points.
    V2d_Copy(from, from_);
    V2d_Copy(to, to_);

    if(!(from[VX] >= min[VX] && from[VX] <= max[VX] &&
         from[VY] >= min[VY] && from[VY] <= max[VY]))
    {
        // 'From' is outside the blockmap (really? very unusual...)
        return true;
    }

    // Check the easy case of a path that lies completely outside the bmap.
    if((from[VX] < min[VX] && to[VX] < min[VX]) ||
       (from[VX] > max[VX] && to[VX] > max[VX]) ||
       (from[VY] < min[VY] && to[VY] < min[VY]) ||
       (from[VY] > max[VY] && to[VY] > max[VY]))
    {
        // Nothing intercepts outside the blockmap!
        return true;
    }

    // Lines should not be perfectly parallel to a blockmap axis.
    // We honor these so-called fudge factors for compatible behavior
    // with DOOM's algorithm.
    dX = (from[VX] - Blockmap_Origin(bmap)[VX]) / Blockmap_CellWidth(bmap);
    dY = (from[VY] - Blockmap_Origin(bmap)[VY]) / Blockmap_CellHeight(bmap);
    if(INRANGE_OF(dX, 0, epsilon)) from[VX] += unitOffset;
    if(INRANGE_OF(dY, 0, epsilon)) from[VY] += unitOffset;

    map->traceLOS.origin[VX] = FLT2FIX(from[VX]);
    map->traceLOS.origin[VY] = FLT2FIX(from[VY]);
    map->traceLOS.direction[VX] = FLT2FIX(to[VX] - from[VX]);
    map->traceLOS.direction[VY] = FLT2FIX(to[VY] - from[VY]);

    /**
     * It is possible that one or both points are outside the blockmap.
     * Clip path so that 'to' is within the AABB of the blockmap (note we
     * would have already abandoned if 'from' lay outside..
     */
    if(!(to[VX] >= min[VX] && to[VX] <= max[VX] &&
         to[VY] >= min[VY] && to[VY] <= max[VY]))
    {
        // 'to' is outside the blockmap.
        vec2d_t bounds[4], point;
        coord_t ab;

        V2d_Set(bounds[0], min[VX], min[VY]);
        V2d_Set(bounds[1], min[VX], max[VY]);
        V2d_Set(bounds[2], max[VX], max[VY]);
        V2d_Set(bounds[3], max[VX], min[VY]);

        ab = V2d_Intercept(from, to, bounds[0], bounds[1], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[1], bounds[2], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[2], bounds[3], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);

        ab = V2d_Intercept(from, to, bounds[3], bounds[0], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(to, point);
    }

    // Clipping already applied above, so we don't need to check it again...
    Blockmap_Cell(bmap, fromBlock, from);
    Blockmap_Cell(bmap, toBlock, to);

    V2d_Subtract(from, from, min);
    V2d_Subtract(to, to, min);
    return traverseCellPath2(bmap, fromBlock, toBlock, from, to, callback, parameters);
}

typedef struct {
    int (*callback) (LineDef*, void*);
    void* parameters;
} iteratepolyobjlinedefs_params_t;

static int iteratePolyobjLineDefs(Polyobj* po, void* parameters)
{
    const iteratepolyobjlinedefs_params_t* p = (iteratepolyobjlinedefs_params_t*)parameters;
    return Polyobj_LineIterator(po, p->callback, p->parameters);
}

static int collectPolyobjLineDefIntercepts(uint const block[2], void* parameters)
{
    GameMap* map = (GameMap*)parameters;
    iteratepolyobjlinedefs_params_t iplParams;
    iplParams.callback = PIT_AddLineDefIntercepts;
    iplParams.parameters = NULL;
    return GameMap_IterateCellPolyobjs(map, block, iteratePolyobjLineDefs, (void*)&iplParams);
}

static int collectLineDefIntercepts(uint const block[2], void* parameters)
{
    GameMap* map = (GameMap*)parameters;
    return GameMap_IterateCellLineDefs(map, block, PIT_AddLineDefIntercepts, NULL);
}

static int collectMobjIntercepts(uint const block[2], void* parameters)
{
    GameMap* map = (GameMap*)parameters;
    return GameMap_IterateCellMobjs(map, block, PIT_AddMobjIntercepts, NULL);
}

int GameMap_PathTraverse2(GameMap* map, const coord_t from[], const coord_t to[],
    int flags, traverser_t callback, void* parameters)
{
    assert(map);

    // A new intercept trace begins...
    P_ClearIntercepts();
    validCount++;

    // Step #1: Collect intercepts.
    if(flags & PT_ADDLINES)
    {
        if(NUM_POLYOBJS != 0)
        {
            traverseCellPath(map, map->polyobjBlockmap, from, to, collectPolyobjLineDefIntercepts, (void*)map);
        }
        traverseCellPath(map, map->lineDefBlockmap, from, to, collectLineDefIntercepts, (void*)map);
    }
    if(flags & PT_ADDMOBJS)
    {
        traverseCellPath(map, map->mobjBlockmap, from, to, collectMobjIntercepts, (void*)map);
    }

    // Step #2: Process sorted intercepts.
    return P_TraverseIntercepts(callback, parameters);
}

int GameMap_PathTraverse(GameMap* map, const coord_t from[], const coord_t to[],
    int flags, traverser_t callback)
{
    return GameMap_PathTraverse2(map, from, to, flags, callback, NULL/*no parameters*/);
}

int GameMap_PathXYTraverse2(GameMap* map, coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
    int flags, traverser_t callback, void* parameters)
{
    vec2d_t from, to;
    V2d_Set(from, fromX, fromY);
    V2d_Set(to, toX, toY);
    return GameMap_PathTraverse2(map, from, to, flags, callback, parameters);
}

int GameMap_PathXYTraverse(GameMap* map, coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
    int flags, traverser_t callback)
{
    return GameMap_PathXYTraverse2(map, fromX, fromY, toX, toY, flags, callback, NULL/*no parameters*/);
}

BspLeaf* GameMap_BspLeafAtPoint(GameMap* map, coord_t const point_[])
{
    runtime_mapdata_header_t* node;
    vec2d_t point;

    V2d_Set(point, point_? point_[VX] : 0,
                   point_? point_[VY] : 0);

    node = map->bsp;
    while(node->type != DMU_BSPLEAF)
    {
        BspNode* bspNode = (BspNode*)node;
        node = bspNode->children[Partition_PointOnSide(&bspNode->partition, point)];
    }
    return (BspLeaf*)node;
}

BspLeaf* GameMap_BspLeafAtPointXY(GameMap* map, coord_t x, coord_t y)
{
    vec2d_t point;
    V2d_Set(point, x, y);
    return GameMap_BspLeafAtPoint(map, point);
}
