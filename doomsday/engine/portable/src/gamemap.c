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

void GameMap_Bounds(GameMap* map, float* min, float* max)
{
    assert(map);

    min[VX] = map->bBox[BOXLEFT];
    min[VY] = map->bBox[BOXBOTTOM];

    max[VX] = map->bBox[BOXRIGHT];
    max[VY] = map->bBox[BOXTOP];
}

float GameMap_Gravity(GameMap* map)
{
    assert(map);
    return map->effectiveGravity;
}

GameMap* GameMap_SetGravity(GameMap* map, float gravity)
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
    Sector* front, *back;
    assert(map);

    // Is the linedef part of this map?
    if(-1 == GameMap_LineDefIndex(map, lineDef)) return; // Odd...

    if(!lineDef->L_backside)
    {
        // A single-sided linedef.
        map->traceOpening.range = 0;
        return;
    }

    front = lineDef->L_frontsector;
    back = lineDef->L_backsector;

    if(front->SP_ceilheight < back->SP_ceilheight)
        map->traceOpening.top = front->SP_ceilheight;
    else
        map->traceOpening.top = back->SP_ceilheight;

    if(front->SP_floorheight > back->SP_floorheight)
    {
        map->traceOpening.bottom = front->SP_floorheight;
        map->traceOpening.lowFloor = back->SP_floorheight;
    }
    else
    {
        map->traceOpening.bottom = back->SP_floorheight;
        map->traceOpening.lowFloor = front->SP_floorheight;
    }

    map->traceOpening.range = map->traceOpening.top - map->traceOpening.bottom;
}

int GameMap_AmbientLightLevel(GameMap* map)
{
    assert(map);
    return map->ambientLightLevel;
}

vertex_t* GameMap_Vertex(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numVertexes) return NULL;
    return &map->vertexes[idx];
}

int GameMap_VertexIndex(GameMap* map, vertex_t* vtx)
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

int GameMap_BspLeafIndex(GameMap* map, BspLeaf* bspLeaf)
{
    assert(map);
    if(!bspLeaf || !(bspLeaf >= map->bspLeafs && bspLeaf <= &map->bspLeafs[map->numBspLeafs])) return -1;
    return bspLeaf - map->bspLeafs;
}

BspLeaf* GameMap_BspLeaf(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numBspLeafs) return NULL;
    return &map->bspLeafs[idx];
}

int GameMap_HEdgeIndex(GameMap* map, HEdge* hedge)
{
    assert(map);
    if(!hedge || !(hedge >= map->hedges && hedge <= &map->hedges[map->numHEdges])) return -1;
    return hedge - map->hedges;
}

HEdge* GameMap_HEdge(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numHEdges) return NULL;
    return &map->hedges[idx];
}

int GameMap_BspNodeIndex(GameMap* map, BspNode* node)
{
    assert(map);
    if(!node || !(node >= map->bspNodes && node <= &map->bspNodes[map->numBspNodes])) return -1;
    return node - map->bspNodes;
}

BspNode* GameMap_BspNode(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numBspNodes) return NULL;
    return &map->bspNodes[idx];
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

polyobj_t* GameMap_PolyobjByID(GameMap* map, uint id)
{
    assert(map);
    if(id < map->numPolyObjs)
        return map->polyObjs[id];
    return NULL;
}

polyobj_t* GameMap_PolyobjByTag(GameMap* map, int tag)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        if(po->tag == tag)
        {
            return po;
        }
    }
    return NULL;
}

polyobj_t* GameMap_PolyobjByOrigin(GameMap* map, void* ddMobjBase)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        polyobj_t* po = map->polyObjs[i];
        if(po == ddMobjBase)
        {
            return po;
        }
    }
    return NULL;
}

static void initPolyobj(polyobj_t* po)
{
    LineDef** lineIter;
    BspLeaf* bspLeaf;
    vec2_t avg; /// < Used to find a polyobj's center, and hence BSP leaf.

    if(!po) return;

    V2_Set(avg, 0, 0);
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

        V2_Sum(avg, avg, line->L_v1pos);
    }
    V2_Scale(avg, 1.f / po->lineCount);

    bspLeaf = P_BspLeafAtPointXY(avg[VX], avg[VY]);
    if(bspLeaf)
    {
        if(bspLeaf->polyObj)
        {
            Con_Message("Warning: GameMap::initPolyobj: Multiple polyobjs in a single BSP leaf\n"
                        "  (BSP leaf %ld, sector %ld). Previous polyobj overridden.\n",
                        (long)GET_BSPLEAF_IDX(bspLeaf), (long)GET_SECTOR_IDX(bspLeaf->sector));
        }
        bspLeaf->polyObj = po;
        po->bspLeaf = bspLeaf;
    }

    Polyobj_UpdateAABox(po);
    Polyobj_UpdateSideDefOrigins(po);
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

void GameMap_InitLineDefBlockmap(GameMap* map, const_pvec2_t min_, const_pvec2_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                min_[VY] - BLOCKMAP_MARGIN);
    V2_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                max_[VY] + BLOCKMAP_MARGIN);

    map->lineDefBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap_InitMobjBlockmap(GameMap* map, const_pvec2_t min_, const_pvec2_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                min_[VY] - BLOCKMAP_MARGIN);
    V2_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                max_[VY] + BLOCKMAP_MARGIN);

    map->mobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap_InitPolyobjBlockmap(GameMap* map, const_pvec2_t min_, const_pvec2_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                min_[VY] - BLOCKMAP_MARGIN);
    V2_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                max_[VY] + BLOCKMAP_MARGIN);

    map->polyobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap_InitBspLeafBlockmap(GameMap* map, const_pvec2_t min_, const_pvec2_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2_t min, max;
    assert(map && min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    V2_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                min_[VY] - BLOCKMAP_MARGIN);
    V2_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                max_[VY] + BLOCKMAP_MARGIN);

    map->bspLeafBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap_LinkMobjInBlockmap(GameMap* map, mobj_t* mo)
{
    Blockmap* blockmap;
    uint coords[2];
    assert(map);

    // Do not link NULL mobjs.
    if(!mo)
    {
        DEBUG_Message(("Warning: GameMap::LinkMobjInBlockmap: Attempted with NULL mobj argument.\n"));
        return;
    }

    blockmap = map->mobjBlockmap;
    Blockmap_CellCoords(blockmap, coords, mo->pos);
    Blockmap_CreateCellAndLinkObject(blockmap, coords, mo);
}

boolean GameMap_UnlinkMobjInBlockmap(GameMap* map, mobj_t* mo)
{
    boolean unlinked = false;
    assert(map);

    if(mo)
    {
        Blockmap* blockmap = map->mobjBlockmap;
        uint coords[2];
        Blockmap_CellCoords(blockmap, coords, mo->pos);
        return Blockmap_UnlinkObjectInCell(blockmap, coords, mo);
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

int GameMap_IterateCellMobjs(GameMap* map, const uint coords[2],
    int (*callback) (mobj_t*, void*), void* context)
{
    bmapmoiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellObjects(map->mobjBlockmap, coords,
                                       blockmapCellMobjsIterator, (void*)&args);
}

int GameMap_IterateCellBlockMobjs(GameMap* map, const GridmapBlock* blockCoords,
    int (*callback) (mobj_t*, void*), void* context)
{
    bmapmoiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellBlockObjects(map->mobjBlockmap, blockCoords,
                                            blockmapCellMobjsIterator, (void*) &args);
}

int GameMap_MobjsBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (mobj_t*, void*), void* parameters)
{
    GridmapBlock blockCoords;
    assert(map);
    Blockmap_CellBlockCoords(map->mobjBlockmap, &blockCoords, box);
    return GameMap_IterateCellBlockMobjs(map, &blockCoords, callback, parameters);
}

void GameMap_LinkLineDefInBlockmap(GameMap* map, LineDef* lineDef)
{
    vec2_t origin, cellSize, cell, from, to;
    GridmapBlock blockCoords;
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
    V2_Copy(origin, Blockmap_Origin(blockmap));
    V2_Copy(cellSize, Blockmap_CellSize(blockmap));

    // Determine the block of cells we'll be working within.
    Blockmap_CellBlockCoords(blockmap, &blockCoords, &lineDef->aaBox);

    for(y = blockCoords.minY; y <= blockCoords.maxY; ++y)
    for(x = blockCoords.minX; x <= blockCoords.maxX; ++x)
    {
        if(lineDef->slopeType == ST_VERTICAL || lineDef->slopeType == ST_HORIZONTAL)
        {
            Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, lineDef);
            continue;
        }

        // Calculate cell origin.
        V2_Copy(cell, Blockmap_CellSize(blockmap));
        cell[VX] *= x; cell[VY] *= y;
        V2_Sum(cell, cell, Blockmap_Origin(blockmap));

        // Choose a cell diagonal to test.
        if(lineDef->slopeType == ST_POSITIVE)
        {
            // LineDef slope / vs \ cell diagonal.
            V2_Set(from, cell[VX], cell[VY] + cellSize[VY]);
            V2_Set(to,   cell[VX] + cellSize[VX], cell[VY]);
        }
        else
        {
            // LineDef slope \ vs / cell diagonal.
            V2_Set(from, cell[VX] + cellSize[VX], cell[VY] + cellSize[VY]);
            V2_Set(to,   cell[VX], cell[VY]);
        }

        // Would LineDef intersect this?
        if(P_PointOnLinedefSide(from, lineDef) != P_PointOnLinedefSide(to, lineDef))
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

int GameMap_IterateCellLineDefs(GameMap* map, const uint coords[2],
    int (*callback) (LineDef*, void*), void* context)
{
    bmapiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellObjects(map->lineDefBlockmap, coords,
                                       blockmapCellLinesIterator, (void*)&args);
}

int GameMap_IterateCellBlockLineDefs(GameMap* map, const GridmapBlock* blockCoords,
    int (*callback) (LineDef*, void*), void* context)
{
    bmapiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellBlockObjects(map->lineDefBlockmap, blockCoords,
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

void GameMap_LinkBspLeafInBlockmap(GameMap* map, BspLeaf* bspLeaf)
{
    Blockmap* blockmap;
    GridmapBlock blockCoords;
    AABoxf aaBox;
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
    Blockmap_CellBlockCoords(blockmap, &blockCoords, &aaBox);

    for(y = blockCoords.minY; y <= blockCoords.maxY; ++y)
    for(x = blockCoords.minX; x <= blockCoords.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, bspLeaf);
    }
}

typedef struct subseciterparams_s {
    const AABoxf* box;
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

int GameMap_IterateCellBspLeafs(GameMap* map, const uint coords[2],
    Sector* sector, const AABoxf* box, int localValidCount,
    int (*callback) (BspLeaf*, void*), void* context)
{
    bmapbspleafiterateparams_t args;
    assert(map);

    args.localValidCount = localValidCount;
    args.func = callback;
    args.param = context;
    args.sector = sector;
    args.box = box;

    return Blockmap_IterateCellObjects(map->bspLeafBlockmap, coords,
                                       blockmapCellBspLeafsIterator, (void*)&args);
}

int GameMap_IterateCellBlockBspLeafs(GameMap* map, const GridmapBlock* blockCoords,
    Sector* sector,  const AABoxf* box, int localValidCount,
    int (*callback) (BspLeaf*, void*), void* context)
{
    bmapbspleafiterateparams_t args;
    assert(map);

    args.localValidCount = localValidCount;
    args.func = callback;
    args.param = context;
    args.sector = sector;
    args.box = box;

    return Blockmap_IterateCellBlockObjects(map->bspLeafBlockmap, blockCoords,
                                            blockmapCellBspLeafsIterator, (void*) &args);
}

int GameMap_BspLeafsBoxIterator(GameMap* map, const AABoxf* box, Sector* sector,
    int (*callback) (BspLeaf*, void*), void* parameters)
{
    static int localValidCount = 0;
    GridmapBlock blockCoords;
    assert(map);

    // This is only used here.
    localValidCount++;

    Blockmap_CellBlockCoords(map->bspLeafBlockmap, &blockCoords, box);
    return GameMap_IterateCellBlockBspLeafs(map, &blockCoords, sector, box,
                                              localValidCount, callback, parameters);
}

int GameMap_BspLeafIterator(GameMap* map, int (*callback) (BspLeaf*, void*), void* parameters)
{
    uint i;
    assert(map);
    for(i = 0; i < map->numBspLeafs; ++i)
    {
        int result = callback(map->bspLeafs + i, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

void GameMap_LinkPolyobjInBlockmap(GameMap* map, polyobj_t* po)
{
    Blockmap* blockmap;
    GridmapBlock blockCoords;
    uint x, y;
    assert(map);

    // Do not link NULL polyobjs.
    if(!po)
    {
        DEBUG_Message(("Warning: GameMap::LinkPolyobjInBlockmap: Attempted with NULL polyobj argument.\n"));
        return;
    }

    blockmap = map->polyobjBlockmap;
    Blockmap_CellBlockCoords(blockmap, &blockCoords, &po->aaBox);

    for(y = blockCoords.minY; y <= blockCoords.maxY; ++y)
    for(x = blockCoords.minX; x <= blockCoords.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, po);
    }
}

void GameMap_UnlinkPolyobjInBlockmap(GameMap* map, polyobj_t* po)
{
    Blockmap* blockmap;
    GridmapBlock blockCoords;
    assert(map);

    if(!po) return;

    blockmap = map->polyobjBlockmap;
    Blockmap_CellBlockCoords(map->polyobjBlockmap, &blockCoords, &po->aaBox);
    Blockmap_UnlinkObjectInCellBlock(blockmap, &blockCoords, po);
}

typedef struct bmappoiterparams_s {
    int localValidCount;
    int (*func) (polyobj_t*, void *);
    void* param;
} bmappoiterparams_t;

static int blockmapCellPolyobjsIterator(void* object, void* context)
{
    polyobj_t* polyobj = (polyobj_t*)object;
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

int GameMap_IterateCellPolyobjs(GameMap* map, const uint coords[2],
    int (*callback) (polyobj_t*, void*), void* context)
{
    bmappoiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellObjects(map->polyobjBlockmap, coords,
                                       blockmapCellPolyobjsIterator, (void*)&args);
}

int GameMap_IterateCellBlockPolyobjs(GameMap* map, const GridmapBlock* blockCoords,
    int (*callback) (polyobj_t*, void*), void* context)
{
    bmappoiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellBlockObjects(map->polyobjBlockmap, blockCoords,
                                            blockmapCellPolyobjsIterator, (void*) &args);
}

int GameMap_PolyobjsBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (struct polyobj_s*, void*), void* parameters)
{
    GridmapBlock blockCoords;
    assert(map);
    Blockmap_CellBlockCoords(map->polyobjBlockmap, &blockCoords, box);
    return GameMap_IterateCellBlockPolyobjs(map, &blockCoords, callback, parameters);
}

int GameMap_PolyobjIterator(GameMap* map, int (*callback) (polyobj_t*, void*), void* parameters)
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

int PTR_PolyobjLines(void* object, void* context)
{
    polyobj_t* po = (polyobj_t*)object;
    poiterparams_t* args = (poiterparams_t*) context;

    return Polyobj_LineIterator(po, args->func, args->param);
}

int GameMap_IterateCellPolyobjLineDefsIterator(GameMap* map, const uint coords[2],
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

    return Blockmap_IterateCellObjects(map->polyobjBlockmap, coords,
                                       blockmapCellPolyobjsIterator, &args);
}

int GameMap_IterateCellBlockPolyobjLineDefs(GameMap* map, const GridmapBlock* blockCoords,
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

    return Blockmap_IterateCellBlockObjects(map->polyobjBlockmap, blockCoords,
                                            blockmapCellPolyobjsIterator, (void*) &args);
}

int GameMap_LineDefsBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (LineDef*, void*), void* parameters)
{
    GridmapBlock blockCoords;
    assert(map);
    Blockmap_CellBlockCoords(map->lineDefBlockmap, &blockCoords, box);
    return GameMap_IterateCellBlockLineDefs(map, &blockCoords, callback, parameters);
}

int GameMap_PolyobjLinesBoxIterator(GameMap* map, const AABoxf* box,
    int (*callback) (LineDef*, void*), void* parameters)
{
    GridmapBlock blockCoords;
    assert(map);
    Blockmap_CellBlockCoords(map->polyobjBlockmap, &blockCoords, box);
    return GameMap_IterateCellBlockPolyobjLineDefs(map, &blockCoords, callback, parameters);
}

/**
 * LineDefs and Polyobj LineDefs (note Polyobj LineDefs are iterated first).
 *
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to GameMap_IterateCellLineDefs(), then make one or more calls to it.
 */
int GameMap_AllLineDefsBoxIterator(GameMap* map, const AABoxf* box,
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

int GameMap_VertexIterator(GameMap* map, int (*callback) (vertex_t*, void*), void* parameters)
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
        int result = callback(map->hedges + i, parameters);
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
        int result = callback(map->bspNodes + i, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

static int traverseCellPath2(Blockmap* bmap, uint const fromBlock[2],
    uint const toBlock[2], float const from[2], float const to[2],
    int (*callback) (uint const block[2], void* parameters), void* parameters)
{
    int result = false; // Continue iteration.
    float intercept[2], delta[2], partial;
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

        /// \todo Replace incremental translation?
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

static int traverseCellPath(GameMap* map, Blockmap* bmap, float const from_[2],
    float const to_[2], int (*callback) (uint const block[2], void* parameters),
    void* parameters)
{
    // Constant terms implicitly defined by DOOM's original version of this
    // algorithm (we must honor these fudge factors for compatibility).
    const float epsilon    = FIX2FLT(FRACUNIT);
    const float unitOffset = FIX2FLT(FRACUNIT);
    uint fromBlock[2], toBlock[2];
    vec2_t from, to, min, max;
    float dX, dY;
    assert(bmap);

    V2_Copy(min, Blockmap_Bounds(bmap)->min);
    V2_Copy(max, Blockmap_Bounds(bmap)->max);

    // We may need to clip and/or fudge these points.
    V2_Copy(from, from_);
    V2_Copy(to, to_);

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

    map->traceLOS.pos[VX] = FLT2FIX(from[VX]);
    map->traceLOS.pos[VY] = FLT2FIX(from[VY]);
    map->traceLOS.dX = FLT2FIX(to[VX] - from[VX]);
    map->traceLOS.dY = FLT2FIX(to[VY] - from[VY]);

    /**
     * It is possible that one or both points are outside the blockmap.
     * Clip path so that 'to' is within the AABB of the blockmap (note we
     * would have already abandoned if 'from' lay outside..
     */
    if(!(to[VX] >= min[VX] && to[VX] <= max[VX] &&
         to[VY] >= min[VY] && to[VY] <= max[VY]))
    {
        // 'to' is outside the blockmap.
        vec2_t bounds[4], point;
        float ab;

        V2_Set(bounds[0], min[VX], min[VY]);
        V2_Set(bounds[1], min[VX], max[VY]);
        V2_Set(bounds[2], max[VX], max[VY]);
        V2_Set(bounds[3], max[VX], min[VY]);

        ab = V2_Intercept(from, to, bounds[0], bounds[1], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(to, point);

        ab = V2_Intercept(from, to, bounds[1], bounds[2], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(to, point);

        ab = V2_Intercept(from, to, bounds[2], bounds[3], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(to, point);

        ab = V2_Intercept(from, to, bounds[3], bounds[0], point);
        if(ab >= 0 && ab <= 1)
            V2_Copy(to, point);
    }

    // Clipping already applied above, so we don't need to check it again...
    Blockmap_CellCoords(bmap, fromBlock, from);
    Blockmap_CellCoords(bmap, toBlock, to);

    V2_Subtract(from, from, min);
    V2_Subtract(to, to, min);
    return traverseCellPath2(bmap, fromBlock, toBlock, from, to, callback, parameters);
}

typedef struct {
    int (*callback) (LineDef*, void*);
    void* parameters;
} iteratepolyobjlinedefs_params_t;

static int iteratePolyobjLineDefs(polyobj_t* po, void* parameters)
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

int GameMap_PathTraverse2(GameMap* map, float const from[2], float const to[2],
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

int GameMap_PathTraverse(GameMap* map, float const from[2], float const to[2],
    int flags, traverser_t callback)
{
    return GameMap_PathTraverse2(map, from, to, flags, callback, NULL/*no parameters*/);
}

int GameMap_PathXYTraverse2(GameMap* map, float fromX, float fromY, float toX, float toY,
    int flags, traverser_t callback, void* parameters)
{
    vec2_t from, to;
    V2_Set(from, fromX, fromY);
    V2_Set(to, toX, toY);
    return GameMap_PathTraverse2(map, from, to, flags, callback, parameters);
}

int GameMap_PathXYTraverse(GameMap* map, float fromX, float fromY, float toX, float toY,
    int flags, traverser_t callback)
{
    return GameMap_PathXYTraverse2(map, fromX, fromY, toX, toY, flags, callback, NULL/*no parameters*/);
}

BspLeaf* GameMap_BspLeafAtPoint(GameMap* map, float point_[2])
{
    BspNode* node = 0;
    uint nodenum = 0;
    float point[2];

    point[0] = point_? point_[0] : 0;
    point[1] = point_? point_[1] : 0;

    // A single BSP leaf is a special case
    if(!map->numBspNodes)
    {
        return (BspLeaf*) map->bspLeafs;
    }

    nodenum = map->numBspNodes - 1;
    while(!(nodenum & NF_LEAF))
    {
        node = map->bspNodes + nodenum;
        ASSERT_DMU_TYPE(node, DMU_BSPNODE);
        nodenum = node->children[P_PointOnPartitionSide(point[0], point[1], &node->partition)];
    }

    return map->bspLeafs + (nodenum & ~NF_LEAF);
}

BspLeaf* GameMap_BspLeafAtPointXY(GameMap* map, float x, float y)
{
    float point[2];
    point[0] = x;
    point[1] = y;
    return GameMap_BspLeafAtPoint(map, point);
}
