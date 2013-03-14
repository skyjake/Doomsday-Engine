/** @file gamemap.cpp Gamemap.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cmath>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_system.h"

#include "map/blockmap.h"
#include "map/generators.h"
#include "map/gamemap.h"
#include "render/r_main.h" // validCount
#include "r_util.h"

/// Size of Blockmap blocks in map units. Must be an integer power of two.
#define MAPBLOCKUNITS               (128)

using namespace de;

GameMap::GameMap()
{
    uri = 0;
    memset(uniqueId, 0, sizeof(uniqueId));
    memset(&aaBox, 0, sizeof(aaBox));
    memset(&thinkers, 0, sizeof(thinkers));
    generators = 0;
    memset(&clMobjHash, 0, sizeof(clMobjHash));
    memset(&clActivePlanes, 0, sizeof(clActivePlanes));
    memset(&clActivePolyobjs, 0, sizeof(clActivePolyobjs));
    numPolyObjs = 0;
    polyObjs = 0;
    bsp = 0;
    numHEdges = 0;
    hedges = 0;
    numBspLeafs = 0;
    bspLeafs = 0;
    numBspNodes = 0;
    bspNodes = 0;
    entityDatabase = 0;
    mobjBlockmap = 0;
    polyobjBlockmap = 0;
    lineDefBlockmap = 0;
    bspLeafBlockmap = 0;
    memset(&mobjNodes, 0, sizeof(mobjNodes));
    memset(&lineNodes, 0, sizeof(lineNodes));
    lineLinks = 0;
    globalGravity = 0;
    effectiveGravity = 0;
    ambientLightLevel = 0;
    memset(skyFix, 0, sizeof(skyFix));
    memset(&traceOpening, 0, sizeof(traceOpening));
    memset(&traceLOS, 0, sizeof(traceLOS));
}

GameMap::~GameMap()
{}

SurfaceSet &GameMap::scrollingSurfaces()
{
    return scrollingSurfaces_;
}

#ifdef __CLIENT__
SurfaceSet &GameMap::decoratedSurfaces()
{
    return decoratedSurfaces_;
}

SurfaceSet &GameMap::glowingSurfaces()
{
    return glowingSurfaces_;
}

#endif // __CLIENT__

uri_s const *GameMap_Uri(GameMap *map)
{
    DENG2_ASSERT(map);
    return map->uri;
}

char const *GameMap_OldUniqueId(GameMap *map)
{
    DENG2_ASSERT(map);
    return map->uniqueId;
}

void GameMap_Bounds(GameMap *map, coord_t *min, coord_t *max)
{
    DENG2_ASSERT(map);

    V2d_Copy(min, map->aaBox.min);
    V2d_Copy(max, map->aaBox.max);
}

coord_t GameMap_Gravity(GameMap *map)
{
    DENG2_ASSERT(map);
    return map->effectiveGravity;
}

GameMap *GameMap_SetGravity(GameMap *map, coord_t gravity)
{
    DENG2_ASSERT(map);
    map->effectiveGravity = gravity;
    return map;
}

GameMap *GameMap_RestoreGravity(GameMap *map)
{
    DENG2_ASSERT(map);
    map->effectiveGravity = map->globalGravity;
    return map;
}

divline_t const *GameMap_TraceLOS(GameMap *map)
{
    DENG2_ASSERT(map);
    return &map->traceLOS;
}

TraceOpening const *GameMap_TraceOpening(GameMap *map)
{
    DENG2_ASSERT(map);
    return &map->traceOpening;
}

void GameMap_SetTraceOpening(GameMap *map, LineDef *line)
{
    DENG2_ASSERT(map);
    // Is the linedef part of this map?
    if(!line || GameMap_LineDefIndex(map, line) < 0) return; // Odd...

    line->configureTraceOpening(map->traceOpening);
}

int GameMap_AmbientLightLevel(GameMap *map)
{
    DENG2_ASSERT(map);
    return map->ambientLightLevel;
}

coord_t GameMap_SkyFix(GameMap *map, boolean ceiling)
{
    DENG2_ASSERT(map);
    Plane::Type plane = ceiling? Plane::Ceiling : Plane::Floor;
    return map->skyFix[plane].height;
}

GameMap* GameMap_SetSkyFix(GameMap* map, boolean ceiling, coord_t height)
{
    DENG2_ASSERT(map);
    Plane::Type plane = ceiling? Plane::Ceiling : Plane::Floor;
    map->skyFix[plane].height = height;
    return map;
}

Vertex* GameMap_Vertex(GameMap* map, uint idx)
{
    DENG2_ASSERT(map);
    if(idx >= (uint)map->vertexes.size()) return NULL;
    return &map->vertexes[idx];
}

int GameMap_VertexIndex(GameMap* map, Vertex const *vtx)
{
    DENG2_ASSERT(map);
    if(!vtx) return -1;
    return map->vertexes.indexOf(vtx); // Note: Bad performance!
}

int GameMap_LineDefIndex(GameMap* map, const LineDef *line)
{
    DENG2_ASSERT(map);
    if(!line) return -1;
    return map->lineDefs.indexOf(line);
}

LineDef* GameMap_LineDef(GameMap* map, uint idx)
{
    DENG2_ASSERT(map);
    if(idx >= (uint)map->lineDefs.size()) return NULL;
    return &map->lineDefs[idx];
}

int GameMap_SideDefIndex(GameMap* map, SideDef const *side)
{
    DENG2_ASSERT(map);
    if(!side) return -1;
    return map->sideDefs.indexOf(side); // Note: Bad performance!
}

SideDef* GameMap_SideDef(GameMap* map, uint idx)
{
    DENG2_ASSERT(map);
    if(idx >= (uint)map->sideDefs.size()) return NULL;
    return &map->sideDefs[idx];
}

int GameMap_SectorIndex(GameMap *map, Sector const *sec)
{
    DENG2_ASSERT(map);
    if(!sec) return -1;
    return map->sectors.indexOf(sec); // Note: Bad performance!
}

Sector* GameMap_Sector(GameMap* map, uint idx)
{
    DENG2_ASSERT(map);
    if(idx >= map->sectorCount()) return NULL;
    return &map->sectors[idx];
}

Sector* GameMap_SectorByBase(GameMap* map, const void* ddMobjBase)
{
    DENG2_ASSERT(map);
    for(int i = 0; i < map->sectors.size(); ++i)
    {
        Sector* sec = &map->sectors[i];
        if(ddMobjBase == &sec->base)
        {
            return sec;
        }
    }
    return NULL;
}

Surface *GameMap_SurfaceByBase(GameMap *map, void const *ddMobjBase)
{
    DENG2_ASSERT(map);

    // First try plane surfaces.
    for(uint i = 0; i < map->sectorCount(); ++i)
    {
        Sector *sec = &map->sectors[i];
        for(uint k = 0; k < sec->planeCount(); ++k)
        {
            Plane *pln = sec->SP_plane(k);
            if(ddMobjBase == &pln->surface().base)
            {
                return &pln->surface();
            }
        }
    }

    // Perhaps a sidedef surface?
    for(uint i = 0; i < map->sideDefCount(); ++i)
    {
        SideDef *side = &map->sideDefs[i];
        if(ddMobjBase == &side->SW_middlesurface.base)
        {
            return &side->SW_middlesurface;
        }
        if(ddMobjBase == &side->SW_bottomsurface.base)
        {
            return &side->SW_bottomsurface;
        }
        if(ddMobjBase == &side->SW_topsurface.base)
        {
            return &side->SW_topsurface;
        }
    }

    return NULL;
}

int GameMap_BspLeafIndex(GameMap *map, BspLeaf const *leaf)
{
    DENG_UNUSED(map);
    if(!leaf) return -1;
    return leaf->origIndex();
}

BspLeaf* GameMap_BspLeaf(GameMap* map, uint idx)
{
    DENG2_ASSERT(map);
    if(idx >= map->numBspLeafs) return NULL;
    return map->bspLeafs[idx];
}

int GameMap_HEdgeIndex(GameMap* map, HEdge const *hedge)
{
    DENG_UNUSED(map);
    if(hedge) return -1;
    return hedge->index;
}

HEdge* GameMap_HEdge(GameMap* map, uint idx)
{
    DENG2_ASSERT(map);
    if(idx >= map->numHEdges) return NULL;
    return map->hedges[idx];
}

int GameMap_BspNodeIndex(GameMap* map, BspNode const *node)
{
    DENG_UNUSED(map);
    if(!node) return -1;
    return node->origIndex();
}

BspNode* GameMap_BspNode(GameMap* map, uint idx)
{
    DENG2_ASSERT(map);
    if(idx >= map->numBspNodes) return NULL;
    return map->bspNodes[idx];
}

uint GameMap_VertexCount(GameMap* map)
{
    DENG2_ASSERT(map);
    return map->vertexes.size();
}

uint GameMap_LineDefCount(GameMap* map)
{
    DENG2_ASSERT(map);
    return map->lineDefCount();
}

uint GameMap_SideDefCount(GameMap* map)
{
    DENG2_ASSERT(map);
    return map->sideDefCount();
}

uint GameMap_SectorCount(GameMap* map)
{
    DENG2_ASSERT(map);
    return map->sectorCount();
}

uint GameMap_BspLeafCount(GameMap* map)
{
    DENG2_ASSERT(map);
    return map->numBspLeafs;
}

uint GameMap_HEdgeCount(GameMap* map)
{
    DENG2_ASSERT(map);
    return map->numHEdges;
}

uint GameMap_BspNodeCount(GameMap* map)
{
    DENG2_ASSERT(map);
    return map->numBspNodes;
}

uint GameMap_PolyobjCount(GameMap* map)
{
    DENG2_ASSERT(map);
    return map->numPolyObjs;
}

Polyobj* GameMap_PolyobjByID(GameMap* map, uint id)
{
    DENG2_ASSERT(map);
    if(id < map->numPolyObjs)
        return map->polyObjs[id];
    return NULL;
}

Polyobj* GameMap_PolyobjByTag(GameMap* map, int tag)
{
    uint i;
    DENG2_ASSERT(map);
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
    DENG2_ASSERT(map);
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

static void initPolyobj(Polyobj *po)
{
    if(!po) return;

    vec2d_t avg; /// < Used to find a polyobj's center, and hence BSP leaf.
    V2d_Set(avg, 0, 0);

    for(LineDef **lineIter = po->lines; *lineIter; lineIter++)
    {
        LineDef *line = *lineIter;
        SideDef &frontDef = line->frontSideDef();

        frontDef.SW_topinflags |= SUIF_NO_RADIO;
        frontDef.SW_middleinflags |= SUIF_NO_RADIO;
        frontDef.SW_bottominflags |= SUIF_NO_RADIO;

        if(line->hasBackSideDef())
        {
            SideDef &backDef = line->backSideDef();

            backDef.SW_topinflags |= SUIF_NO_RADIO;
            backDef.SW_middleinflags |= SUIF_NO_RADIO;
            backDef.SW_bottominflags |= SUIF_NO_RADIO;
        }

        V2d_Sum(avg, avg, line->v1Origin());
    }
    V2d_Scale(avg, 1.f / po->lineCount);

    if(BspLeaf *bspLeaf = P_BspLeafAtPoint(avg))
    {
        if(bspLeaf->hasPolyobj())
        {
            Con_Message("Warning: GameMap::initPolyobj: Multiple polyobjs in a single BSP leaf\n"
                        "  (BSP leaf %lu, sector %lu). Previous polyobj overridden.",
                        (unsigned long) GET_BSPLEAF_IDX(bspLeaf), (unsigned long) GET_SECTOR_IDX(bspLeaf->sectorPtr()));
        }
        bspLeaf->_polyObj = po;
        po->bspLeaf = bspLeaf;
    }

    Polyobj_UpdateAABox(po);
    Polyobj_UpdateSurfaceTangents(po);

    P_PolyobjUnlink(po);
    P_PolyobjLink(po);
}

Generators* GameMap_Generators(GameMap* map)
{
    DENG2_ASSERT(map);
    // Time to initialize a new collection?
    if(!map->generators)
    {
        map->generators = Generators_New(map->sectorCount());
    }
    return map->generators;
}

PlaneSet* GameMap_TrackedPlanes(GameMap* map)
{
    DENG2_ASSERT(map);
    return &map->trackedPlanes;
}

void GameMap_InitPolyobjs(GameMap* map)
{
    uint i;
    DENG2_ASSERT(map);
    for(i = 0; i < map->numPolyObjs; ++i)
    {
        initPolyobj(map->polyObjs[i]);
    }
}

void GameMap_InitNodePiles(GameMap* map)
{
    uint i, starttime = 0;

    DENG2_ASSERT(map);

    VERBOSE( Con_Message("GameMap::InitNodePiles: Initializing...") )
    VERBOSE2( starttime = Timer_RealMilliseconds() )

    // Initialize node piles and line rings.
    NP_Init(&map->mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&map->lineNodes, map->lineDefCount() + 1000);

    // Allocate the rings.
    map->lineLinks = (nodeindex_t *) Z_Malloc(sizeof(*map->lineLinks) * map->lineDefCount(), PU_MAPSTATIC, 0);

    for(i = 0; i < map->lineDefCount(); ++i)
    {
        map->lineLinks[i] = NP_New(&map->lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    VERBOSE2( Con_Message("  Done in %.2f seconds.", (Timer_RealMilliseconds() - starttime) / 1000.0f) )
}

void GameMap_InitLineDefBlockmap(GameMap* map, const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    vec2d_t min, max;
    DENG2_ASSERT(map && min_ && max_);

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
    DENG2_ASSERT(map && min_ && max_);

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
    DENG2_ASSERT(map && min_ && max_);

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
    DENG2_ASSERT(map && min_ && max_);

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
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);
    Blockmap_CellBlock(map->mobjBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockMobjs(map, &cellBlock, callback, parameters);
}

void GameMap_LinkLineDef(GameMap* map, LineDef* lineDef)
{
    vec2d_t origin, cellSize, cell, from, to;
    BlockmapCellBlock cellBlock;
    Blockmap* blockmap;
    uint x, y;
    DENG2_ASSERT(map);

    // Do not link NULL linedefs.
    if(!lineDef)
    {
        DEBUG_Message(("Warning: GameMap::LinkLineDefInBlockmap: Attempted with NULL linedef argument.\n"));
        return;
    }

    // LineDefs of Polyobjs don't get into the blockmap (presently...).
    if(lineDef->isFromPolyobj()) return;

    blockmap = map->lineDefBlockmap;
    V2d_Copy(origin, Blockmap_Origin(blockmap));
    V2d_Copy(cellSize, Blockmap_CellSize(blockmap));

    // Determine the block of cells we'll be working within.
    Blockmap_CellBlock(blockmap, &cellBlock, &lineDef->aaBox());

    for(y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        if(lineDef->slopeType() == ST_VERTICAL ||
           lineDef->slopeType() == ST_HORIZONTAL)
        {
            Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, lineDef);
            continue;
        }

        // Calculate cell origin.
        V2d_Copy(cell, Blockmap_CellSize(blockmap));
        cell[VX] *= x; cell[VY] *= y;
        V2d_Sum(cell, cell, Blockmap_Origin(blockmap));

        // Choose a cell diagonal to test.
        if(lineDef->slopeType() == ST_POSITIVE)
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
        if(lineDef->pointOnSide(from) < 0 != lineDef->pointOnSide(to) < 0)
        {
            Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, lineDef);
        }
    }
}

typedef struct bmapiterparams_s {
    int localValidCount;
    int (*callback) (LineDef *, void *);
    void *context;
} bmapiterparams_t;

static int blockmapCellLinesIterator(void *mapElement, void *context)
{
    LineDef *line = static_cast<LineDef *>(mapElement);
    bmapiterparams_t *parms = static_cast<bmapiterparams_t *>(context);

    if(line->validCount() != parms->localValidCount)
    {
        int result;

        // This linedef has now been processed for the current iteration.
        line->_validCount = parms->localValidCount;

        // Action the callback.
        result = parms->callback(line, parms->context);
        if(result) return result; // Stop iteration.
    }

    return false; // Continue iteration.
}

static int GameMap_IterateCellLineDefs(GameMap *map, const_BlockmapCell cell,
    int (*callback) (LineDef *, void *), void *context)
{
    DENG2_ASSERT(map);

    bmapiterparams_t parms;
    parms.localValidCount = validCount;
    parms.callback        = callback;
    parms.context         = context;

    return Blockmap_IterateCellObjects(map->lineDefBlockmap, cell,
                                       blockmapCellLinesIterator, (void *)&parms);
}

static int GameMap_IterateCellBlockLineDefs(GameMap *map, BlockmapCellBlock const *cellBlock,
    int (*callback) (LineDef *, void *), void *context)
{
    DENG2_ASSERT(map);

    bmapiterparams_t parms;
    parms.localValidCount = validCount;
    parms.callback        = callback;
    parms.context          = context;

    return Blockmap_IterateCellBlockObjects(map->lineDefBlockmap, cellBlock,
                                            blockmapCellLinesIterator, (void *) &parms);
}

int GameMap_LineDefIterator(GameMap *map, int (*callback) (LineDef *, void *), void *context)
{
    DENG2_ASSERT(map);
    for(uint i = 0; i < map->lineDefCount(); ++i)
    {
        int result = callback(&map->lineDefs[i], context);
        if(result) return result;
    }
    return false; // Continue iteration.
}

void GameMap_LinkBspLeaf(GameMap *map, BspLeaf *bspLeaf)
{
    DENG2_ASSERT(map);

    // Do not link NULL BSP leafs.
    if(!bspLeaf)
    {
        DEBUG_Message(("Warning: GameMap::LinkBspLeafInBlockmap: Attempted with NULL bspLeaf argument.\n"));
        return;
    }

    // BspLeafs without sectors don't get in.
    if(!bspLeaf->hasSector()) return;

    Blockmap *blockmap = map->bspLeafBlockmap;

    AABoxd aaBox(bspLeaf->aaBox().minX, bspLeaf->aaBox().minY,
                 bspLeaf->aaBox().maxX, bspLeaf->aaBox().maxY);

    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(blockmap, &cellBlock, &aaBox);

    for(uint y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(uint x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, bspLeaf);
    }
}

typedef struct subseciterparams_s {
    AABoxd const *box;
    Sector *sector;
    int localValidCount;
    int (*func) (BspLeaf *, void *);
    void *param;
} bmapbspleafiterateparams_t;

static int blockmapCellBspLeafsIterator(void *object, void *context)
{
    BspLeaf *bspLeaf = (BspLeaf *)object;
    bmapbspleafiterateparams_t *args = (bmapbspleafiterateparams_t *) context;
    if(bspLeaf->validCount() != args->localValidCount)
    {
        boolean ok = true;

        // This BspLeaf has now been processed for the current iteration.
        bspLeaf->_validCount = args->localValidCount;

        // Check the sector restriction.
        if(args->sector && bspLeaf->sectorPtr() != args->sector)
            ok = false;

        // Check the bounds.
        if(args->box &&
           (bspLeaf->aaBox().maxX < args->box->minX ||
            bspLeaf->aaBox().minX > args->box->maxX ||
            bspLeaf->aaBox().minY > args->box->maxY ||
            bspLeaf->aaBox().maxY < args->box->minY))
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

/*
static int GameMap_IterateCellBspLeafs(GameMap* map, const_BlockmapCell cell,
    Sector* sector, const AABoxd* box, int localValidCount,
    int (*callback) (BspLeaf*, void*), void* context)
{
    bmapbspleafiterateparams_t args;
    DENG2_ASSERT(map);

    args.localValidCount = localValidCount;
    args.func = callback;
    args.param = context;
    args.sector = sector;
    args.box = box;

    return Blockmap_IterateCellObjects(map->bspLeafBlockmap, cell,
                                       blockmapCellBspLeafsIterator, (void*)&args);
}
*/

static int GameMap_IterateCellBlockBspLeafs(GameMap* map, const BlockmapCellBlock* cellBlock,
    Sector* sector,  const AABoxd* box, int localValidCount,
    int (*callback) (BspLeaf*, void*), void* context)
{
    bmapbspleafiterateparams_t args;
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);

    // This is only used here.
    localValidCount++;

    Blockmap_CellBlock(map->bspLeafBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockBspLeafs(map, &cellBlock, sector, box,
                                              localValidCount, callback, parameters);
}

int GameMap_BspLeafIterator(GameMap* map, int (*callback) (BspLeaf*, void*), void* parameters)
{
    uint i;
    DENG2_ASSERT(map);
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
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);
    Blockmap_CellBlock(map->polyobjBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockPolyobjs(map, &cellBlock, callback, parameters);
}

int GameMap_PolyobjIterator(GameMap* map, int (*callback) (Polyobj*, void*), void* parameters)
{
    uint i;
    DENG2_ASSERT(map);
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

/*
static int GameMap_IterateCellPolyobjLineDefsIterator(GameMap* map, const_BlockmapCell cell,
    int (*callback) (LineDef*, void*), void* context)
{
    bmappoiterparams_t args;
    poiterparams_t poargs;
    DENG2_ASSERT(map);

    poargs.func = callback;
    poargs.param = context;

    args.localValidCount = validCount;
    args.func = PTR_PolyobjLines;
    args.param = &poargs;

    return Blockmap_IterateCellObjects(map->polyobjBlockmap, cell,
                                       blockmapCellPolyobjsIterator, &args);
}
*/

static int GameMap_IterateCellBlockPolyobjLineDefs(GameMap* map, const BlockmapCellBlock* cellBlock,
    int (*callback) (LineDef*, void*), void* context)
{
    bmappoiterparams_t args;
    poiterparams_t poargs;
    DENG2_ASSERT(map);

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
    DENG2_ASSERT(map);
    Blockmap_CellBlock(map->lineDefBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockLineDefs(map, &cellBlock, callback, parameters);
}

int GameMap_PolyobjLinesBoxIterator(GameMap* map, const AABoxd* box,
    int (*callback) (LineDef*, void*), void* parameters)
{
    BlockmapCellBlock cellBlock;
    DENG2_ASSERT(map);
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
    DENG2_ASSERT(map);
    if(map->numPolyObjs > 0)
    {
        int result = P_PolyobjLinesBoxIterator(box, callback, parameters);
        if(result) return result;
    }
    return P_LinesBoxIterator(box, callback, parameters);
}

int GameMap_VertexIterator(GameMap* map, int (*callback) (Vertex*, void*), void* parameters)
{
    DENG2_ASSERT(map);
    for(int i = 0; i < map->vertexes.size(); ++i)
    {
        int result = callback(&map->vertexes[i], parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int GameMap_SideDefIterator(GameMap* map, int (*callback) (SideDef*, void*), void* parameters)
{
    uint i;
    DENG2_ASSERT(map);
    for(i = 0; i < map->sideDefCount(); ++i)
    {
        int result = callback(&map->sideDefs[i], parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int GameMap_SectorIterator(GameMap* map, int (*callback) (Sector*, void*), void* parameters)
{
    uint i;
    DENG2_ASSERT(map);
    for(i = 0; i < map->sectorCount(); ++i)
    {
        int result = callback(&map->sectors[i], parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

int GameMap_HEdgeIterator(GameMap* map, int (*callback) (HEdge*, void*), void* parameters)
{
    uint i;
    DENG2_ASSERT(map);
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
    DENG2_ASSERT(map);
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
    DENG2_ASSERT(bmap);

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
    DENG2_ASSERT(bmap);

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
    DENG2_ASSERT(map);

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

int GameMap_PathXYTraverse2(GameMap *map, coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
    int flags, traverser_t callback, void *parameters)
{
    vec2d_t from; V2d_Set(from, fromX, fromY);
    vec2d_t to;   V2d_Set(to, toX, toY);
    return GameMap_PathTraverse2(map, from, to, flags, callback, parameters);
}

int GameMap_PathXYTraverse(GameMap *map, coord_t fromX, coord_t fromY, coord_t toX, coord_t toY,
    int flags, traverser_t callback)
{
    return GameMap_PathXYTraverse2(map, fromX, fromY, toX, toY, flags, callback, NULL/*no parameters*/);
}

BspLeaf *GameMap_BspLeafAtPoint(GameMap *map, coord_t const point_[])
{
    vec2d_t point; V2d_Set(point, point_? point_[VX] : 0,
                                  point_? point_[VY] : 0);

    MapElement *bspElement = map->bsp;
    while(bspElement->type() != DMU_BSPLEAF)
    {
        BspNode const &node = *bspElement->castTo<BspNode>();
        int side = node.partition().pointOnSide(point);

        // Decend to the next child subspace.
        bspElement = node.childPtr(side);
    }
    return bspElement->castTo<BspLeaf>();
}

BspLeaf *GameMap_BspLeafAtPointXY(GameMap *map, coord_t x, coord_t y)
{
    vec2d_t point; V2d_Set(point, x, y);
    return GameMap_BspLeafAtPoint(map, point);
}
