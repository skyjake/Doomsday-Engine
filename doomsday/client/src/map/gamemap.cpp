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
    std::memset(_oldUniqueId, 0, sizeof(_oldUniqueId));
    std::memset(&aaBox, 0, sizeof(aaBox));
    std::memset(&thinkers, 0, sizeof(thinkers));
    _generators = 0;
    std::memset(&clMobjHash, 0, sizeof(clMobjHash));
    std::memset(&clActivePlanes, 0, sizeof(clActivePlanes));
    std::memset(&clActivePolyobjs, 0, sizeof(clActivePolyobjs));
    _bspRoot = 0;
    entityDatabase = 0;
    mobjBlockmap = 0;
    polyobjBlockmap = 0;
    lineBlockmap = 0;
    bspLeafBlockmap = 0;
    std::memset(&mobjNodes, 0, sizeof(mobjNodes));
    std::memset(&lineNodes, 0, sizeof(lineNodes));
    lineLinks = 0;
    _globalGravity = 0;
    _effectiveGravity = 0;
    _ambientLightLevel = 0;
    std::memset(_skyFix, 0, sizeof(_skyFix));
    std::memset(&traceOpening, 0, sizeof(traceOpening));
    std::memset(&traceLOS, 0, sizeof(traceLOS));
}

GameMap::~GameMap()
{}

MapElement *GameMap::bspRoot() const
{
    return _bspRoot;
}

void GameMap::updateBounds()
{
    bool isFirst = true;
    foreach(Sector *sector, _sectors)
    {
        // Sectors with no lines have invalid bounds; skip them.
        if(!sector->lineCount()) continue;

        if(isFirst)
        {
            // The first sector is used as is.
            V2d_CopyBox(aaBox.arvec2, sector->aaBox().arvec2);
            isFirst = false;
        }
        else
        {
            // Expand the bounding box.
            V2d_UniteBox(aaBox.arvec2, sector->aaBox().arvec2);
        }
    }
}

#ifdef __CLIENT__

SurfaceSet &GameMap::decoratedSurfaces()
{
    return _decoratedSurfaces;
}

SurfaceSet &GameMap::glowingSurfaces()
{
    return _glowingSurfaces;
}

void GameMap::addSurfaceToLists(Surface &suf)
{
    if(!suf.hasMaterial()) return;

    if(suf.material().hasGlow())
    {
        _glowingSurfaces.insert(&suf);
    }

    if(suf.material().isDecorated())
    {
        _decoratedSurfaces.insert(&suf);
    }
}

#endif // __CLIENT__

de::Uri GameMap::uri() const
{
    return _uri;
}

char const *GameMap::oldUniqueId() const
{
    return _oldUniqueId;
}

void GameMap::bounds(coord_t *min, coord_t *max) const
{
    if(min) V2d_Copy(min, aaBox.min);
    if(max) V2d_Copy(max, aaBox.max);
}

coord_t GameMap::gravity() const
{
    return _effectiveGravity;
}

void GameMap::setGravity(coord_t newGravity)
{
    _effectiveGravity = newGravity;
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
    if(!line || map->lineIndex(line) < 0) return; // Odd...

    line->configureTraceOpening(map->traceOpening);
}

int GameMap::ambientLightLevel() const
{
    return _ambientLightLevel;
}

coord_t GameMap::skyFix(bool ceiling) const
{
    Plane::Type plane = ceiling? Plane::Ceiling : Plane::Floor;
    return _skyFix[plane].height;
}

void GameMap::setSkyFix(bool ceiling, coord_t height)
{
    Plane::Type plane = ceiling? Plane::Ceiling : Plane::Floor;
    _skyFix[plane].height = height;
}

int GameMap::vertexIndex(Vertex const *vtx) const
{
    if(!vtx) return -1;
    return _vertexes.indexOf(const_cast<Vertex *>(vtx)); // Bad performance: O(n)
}

int GameMap::lineIndex(LineDef const *line) const
{
    if(!line) return -1;
    return _lines.indexOf(const_cast<LineDef *>(line)); // Bad performance: O(n)
}

int GameMap::sideDefIndex(SideDef const *side) const
{
    if(!side) return -1;
    return _sideDefs.indexOf(const_cast<SideDef *>(side)); // Bad performance: O(n)
}

int GameMap::sectorIndex(Sector const *sec) const
{
    if(!sec) return -1;
    return _sectors.indexOf(const_cast<Sector *>(sec)); // Bad performance: O(n)
}

Sector *GameMap::sectorBySoundEmitter(ddmobj_base_t const &soundEmitter) const
{
    foreach(Sector *sector, _sectors)
    {
        if(&soundEmitter == &sector->soundEmitter())
            return sector;
    }
    return 0; // Not found.
}

/// @todo Optimize: All sound emitters in a sector are linked together forming
/// a chain. Make use of the chains instead of iterating the sidedefs.
Surface *GameMap::surfaceBySoundEmitter(ddmobj_base_t const &soundEmitter) const
{
    // First try plane surfaces.
    foreach(Sector *sector, _sectors)
    foreach(Plane *plane, sector->planes())
    {
        if(&soundEmitter == &plane->surface().soundEmitter())
        {
            return &plane->surface();
        }
    }

    // Perhaps a wall surface?
    foreach(SideDef *sideDef, _sideDefs)
    {
        if(&soundEmitter == &sideDef->middle().soundEmitter())
        {
            return &sideDef->middle();
        }
        if(&soundEmitter == &sideDef->bottom().soundEmitter())
        {
            return &sideDef->bottom();
        }
        if(&soundEmitter == &sideDef->top().soundEmitter())
        {
            return &sideDef->top();
        }
    }

    return 0; // Not found.
}

int GameMap::bspLeafIndex(BspLeaf const *bspLeaf) const
{
    if(!bspLeaf) return -1;
    return bspLeaf->origIndex();
}

int GameMap::hedgeIndex(HEdge const *hedge) const
{
    if(!hedge) return -1;
    return hedge->origIndex();
}

int GameMap::bspNodeIndex(BspNode const *bspLeaf) const
{
    if(!bspLeaf) return -1;
    return bspLeaf->origIndex();
}

Polyobj *GameMap::polyobjByTag(int tag) const
{
    foreach(Polyobj *polyobj, _polyobjs)
    {
        if(polyobj->tag == tag)
            return polyobj;
    }
    return 0;
}

Polyobj *GameMap::polyobjByBase(ddmobj_base_t const &ddMobjBase) const
{
    foreach(Polyobj *polyobj, _polyobjs)
    {
        if(reinterpret_cast<ddmobj_base_t *>(polyobj) == &ddMobjBase)
            return polyobj;
    }
    return 0;
}

static void initPolyobj(Polyobj *po)
{
    if(!po) return;

    vec2d_t avg; /// < Used to find a polyobj's center, and hence BSP leaf.
    V2d_Set(avg, 0, 0);

    for(LineDef **lineIter = po->lines; *lineIter; lineIter++)
    {
        LineDef *line = *lineIter;
        V2d_Sum(avg, avg, line->v1Origin());
    }
    V2d_Scale(avg, 1.f / po->lineCount);

    if(BspLeaf *bspLeaf = P_BspLeafAtPoint(avg))
    {
        if(bspLeaf->hasPolyobj())
        {
            Con_Message("Warning: GameMap::initPolyobj: Multiple polyobjs in a single BSP leaf\n"
                        "  (BSP leaf %lu, sector %lu). Previous polyobj overridden.",
                        ulong( theMap->bspLeafIndex(bspLeaf) ),
                        ulong( theMap->sectorIndex(bspLeaf->sectorPtr()) ));
        }
        bspLeaf->_polyObj = po;
        po->bspLeaf = bspLeaf;
    }

    po->updateAABox();
    po->updateSurfaceTangents();

    P_PolyobjUnlink(po);
    P_PolyobjLink(po);
}

void GameMap::initPolyobjs()
{
    foreach(Polyobj *polyobj, _polyobjs)
    {
        initPolyobj(polyobj);
    }
}

Generators *GameMap::generators()
{
    // Time to initialize a new collection?
    if(!_generators)
    {
        _generators = Generators_New(sectorCount());
    }
    return _generators;
}

void GameMap::initNodePiles()
{
    uint starttime = 0;

    VERBOSE( Con_Message("GameMap::InitNodePiles: Initializing...") )
    VERBOSE2( starttime = Timer_RealMilliseconds() )

    // Initialize node piles and line rings.
    NP_Init(&mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&lineNodes, lineCount() + 1000);

    // Allocate the rings.
    lineLinks = (nodeindex_t *) Z_Malloc(sizeof(*lineLinks) * lineCount(), PU_MAPSTATIC, 0);

    for(uint i = 0; i < lineCount(); ++i)
    {
        lineLinks[i] = NP_New(&lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    VERBOSE2( Con_Message("  Done in %.2f seconds.", (Timer_RealMilliseconds() - starttime) / 1000.0f) )
}

void GameMap::initLineBlockmap(const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    DENG2_ASSERT(min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    vec2d_t min; V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                              min_[VY] - BLOCKMAP_MARGIN);
    vec2d_t max; V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                              max_[VY] + BLOCKMAP_MARGIN);

    lineBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap::initMobjBlockmap(const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    DENG2_ASSERT(min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    vec2d_t min; V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                              min_[VY] - BLOCKMAP_MARGIN);
    vec2d_t max; V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                              max_[VY] + BLOCKMAP_MARGIN);

    mobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap::initPolyobjBlockmap(const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    DENG2_ASSERT(min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    vec2d_t min; V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                              min_[VY] - BLOCKMAP_MARGIN);
    vec2d_t max; V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                              max_[VY] + BLOCKMAP_MARGIN);

    polyobjBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap::initBspLeafBlockmap(const_pvec2d_t min_, const_pvec2d_t max_)
{
#define BLOCKMAP_MARGIN      8 // size guardband around map
#define CELL_SIZE            MAPBLOCKUNITS

    DENG2_ASSERT(min_ && max_);

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap cell).
    vec2d_t min; V2d_Set(min, min_[VX] - BLOCKMAP_MARGIN,
                              min_[VY] - BLOCKMAP_MARGIN);
    vec2d_t max; V2d_Set(max, max_[VX] + BLOCKMAP_MARGIN,
                              max_[VY] + BLOCKMAP_MARGIN);

    bspLeafBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

#undef CELL_SIZE
#undef BLOCKMAP_MARGIN
}

void GameMap::linkMobj(mobj_t &mo)
{
    BlockmapCell cell;
    Blockmap_Cell(mobjBlockmap, cell, mo.origin);
    Blockmap_CreateCellAndLinkObject(mobjBlockmap, cell, &mo);
}

bool GameMap::unlinkMobj(mobj_t &mo)
{
    BlockmapCell cell;
    Blockmap_Cell(mobjBlockmap, cell, mo.origin);
    return Blockmap_UnlinkObjectInCell(mobjBlockmap, cell, &mo);
}

typedef struct bmapmoiterparams_s {
    int localValidCount;
    int (*func) (mobj_t *, void *);
    void *param;
} bmapmoiterparams_t;

static int blockmapCellMobjsIterator(void *object, void *context)
{
    mobj_t *mobj = (mobj_t *)object;
    bmapmoiterparams_t *args = (bmapmoiterparams_t *) context;
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

static int GameMap_IterateCellMobjs(GameMap *map, const_BlockmapCell cell,
    int (*callback) (mobj_t *, void *), void *context)
{
    DENG2_ASSERT(map);

    bmapmoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.param           = context;

    return Blockmap_IterateCellObjects(map->mobjBlockmap, cell,
                                       blockmapCellMobjsIterator, (void*)&args);
}

static int GameMap_IterateCellBlockMobjs(GameMap *map, BlockmapCellBlock const *cellBlock,
    int (*callback) (mobj_t *, void *), void *context)
{
    DENG2_ASSERT(map);

    bmapmoiterparams_t args;
    args.localValidCount = validCount;
    args.func            = callback;
    args.param           = context;

    return Blockmap_IterateCellBlockObjects(map->mobjBlockmap, cellBlock,
                                            blockmapCellMobjsIterator, (void*) &args);
}

int GameMap_MobjsBoxIterator(GameMap *map, AABoxd const *box,
    int (*callback) (mobj_t *, void *), void *parameters)
{
    DENG2_ASSERT(map);
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(map->mobjBlockmap, &cellBlock, box);
    return GameMap_IterateCellBlockMobjs(map, &cellBlock, callback, parameters);
}

void GameMap::linkLine(LineDef &line)
{
    // LineDefs of Polyobjs don't get into the blockmap (presently...).
    if(line.isFromPolyobj()) return;

    vec2d_t origin; V2d_Copy(origin, Blockmap_Origin(lineBlockmap));
    vec2d_t cellSize; V2d_Copy(cellSize, Blockmap_CellSize(lineBlockmap));

    // Determine the block of cells we'll be working within.
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(lineBlockmap, &cellBlock, &line.aaBox());

    vec2d_t cell, from, to;

    for(uint y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(uint x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        if(line.slopeType() == ST_VERTICAL ||
           line.slopeType() == ST_HORIZONTAL)
        {
            Blockmap_CreateCellAndLinkObjectXY(lineBlockmap, x, y, &line);
            continue;
        }

        // Calculate cell origin.
        V2d_Copy(cell, Blockmap_CellSize(lineBlockmap));
        cell[VX] *= x; cell[VY] *= y;
        V2d_Sum(cell, cell, Blockmap_Origin(lineBlockmap));

        // Choose a cell diagonal to test.
        if(line.slopeType() == ST_POSITIVE)
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
        if((line.pointOnSide(from) < 0) != (line.pointOnSide(to) < 0))
        {
            Blockmap_CreateCellAndLinkObjectXY(lineBlockmap, x, y, &line);
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

static int GameMap_IterateCellLines(GameMap *map, const_BlockmapCell cell,
    int (*callback) (LineDef *, void *), void *context)
{
    DENG2_ASSERT(map);

    bmapiterparams_t parms;
    parms.localValidCount = validCount;
    parms.callback        = callback;
    parms.context         = context;

    return Blockmap_IterateCellObjects(map->lineBlockmap, cell,
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

    return Blockmap_IterateCellBlockObjects(map->lineBlockmap, cellBlock,
                                            blockmapCellLinesIterator, (void *) &parms);
}

void GameMap::linkBspLeaf(BspLeaf &bspLeaf)
{
    // BspLeafs without sectors don't get in.
    if(!bspLeaf.hasSector()) return;

    AABoxd aaBox(bspLeaf.aaBox().minX, bspLeaf.aaBox().minY,
                 bspLeaf.aaBox().maxX, bspLeaf.aaBox().maxY);

    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(bspLeafBlockmap, &cellBlock, &aaBox);

    for(uint y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(uint x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(bspLeafBlockmap, x, y, &bspLeaf);
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

static int GameMap_IterateCellBlockBspLeafs(GameMap *map, BlockmapCellBlock const *cellBlock,
    Sector *sector,  AABoxd const *box, int localValidCount,
    int (*callback) (BspLeaf *, void *), void *context)
{
    DENG2_ASSERT(map);

    bmapbspleafiterateparams_t args;
    args.localValidCount = localValidCount;
    args.func   = callback;
    args.param  = context;
    args.sector = sector;
    args.box    = box;

    return Blockmap_IterateCellBlockObjects(map->bspLeafBlockmap, cellBlock,
                                            blockmapCellBspLeafsIterator, (void*) &args);
}

int GameMap_BspLeafsBoxIterator(GameMap *map, AABoxd const *box, Sector *sector,
    int (*callback) (BspLeaf *, void *), void *parameters)
{
    DENG2_ASSERT(map);

    static int localValidCount = 0;

    // This is only used here.
    localValidCount++;

    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(map->bspLeafBlockmap, &cellBlock, box);

    return GameMap_IterateCellBlockBspLeafs(map, &cellBlock, sector, box,
                                              localValidCount, callback, parameters);
}

void GameMap::linkPolyobj(Polyobj &polyobj)
{
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(polyobjBlockmap, &cellBlock, &polyobj.aaBox);

    for(uint y = cellBlock.minY; y <= cellBlock.maxY; ++y)
    for(uint x = cellBlock.minX; x <= cellBlock.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(polyobjBlockmap, x, y, &polyobj);
    }
}

void GameMap::unlinkPolyobj(Polyobj &polyobj)
{
    BlockmapCellBlock cellBlock;
    Blockmap_CellBlock(polyobjBlockmap, &cellBlock, &polyobj.aaBox);
    Blockmap_UnlinkObjectInCellBlock(polyobjBlockmap, &cellBlock, &polyobj);
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

typedef struct poiterparams_s {
    int (*func) (LineDef *, void *);
    void *param;
} poiterparams_t;

int PTR_PolyobjLines(Polyobj *po, void* context)
{
    poiterparams_t *args = (poiterparams_t *) context;

    return po->lineIterator(args->func, args->param);
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

int GameMap_LinesBoxIterator(GameMap* map, const AABoxd* box,
    int (*callback) (LineDef*, void*), void* parameters)
{
    BlockmapCellBlock cellBlock;
    DENG2_ASSERT(map);
    Blockmap_CellBlock(map->lineBlockmap, &cellBlock, box);
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
int GameMap_AllLinesBoxIterator(GameMap *map, AABoxd const *box,
    int (*callback) (LineDef *, void *), void *parameters)
{
    DENG2_ASSERT(map);
    if(map->polyobjCount() != 0)
    {
        int result = P_PolyobjLinesBoxIterator(box, callback, parameters);
        if(result) return result;
    }
    return P_LinesBoxIterator(box, callback, parameters);
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
    int (*callback) (LineDef *, void *);
    void *parameters;
} iteratepolyobjlines_params_t;

static int iteratePolyobjLines(Polyobj *po, void *parameters)
{
    iteratepolyobjlines_params_t const *p = (iteratepolyobjlines_params_t *)parameters;
    return po->lineIterator(p->callback, p->parameters);
}

static int collectPolyobjLineIntercepts(uint const block[2], void* parameters)
{
    GameMap* map = (GameMap*)parameters;
    iteratepolyobjlines_params_t iplParams;
    iplParams.callback = PIT_AddLineDefIntercepts;
    iplParams.parameters = NULL;
    return GameMap_IterateCellPolyobjs(map, block, iteratePolyobjLines, (void*)&iplParams);
}

static int collectLineIntercepts(uint const block[2], void* parameters)
{
    GameMap* map = (GameMap*)parameters;
    return GameMap_IterateCellLines(map, block, PIT_AddLineDefIntercepts, NULL);
}

static int collectMobjIntercepts(uint const block[2], void* parameters)
{
    GameMap* map = (GameMap*)parameters;
    return GameMap_IterateCellMobjs(map, block, PIT_AddMobjIntercepts, NULL);
}

int GameMap_PathTraverse(GameMap *map, const_pvec2d_t from, const_pvec2d_t to,
    int flags, traverser_t callback, void *parameters)
{
    DENG2_ASSERT(map);

    // A new intercept trace begins...
    P_ClearIntercepts();
    validCount++;

    // Step #1: Collect intercepts.
    if(flags & PT_ADDLINES)
    {
        if(theMap->polyobjCount() != 0)
        {
            traverseCellPath(map, map->polyobjBlockmap, from, to, collectPolyobjLineIntercepts, (void *)map);
        }
        traverseCellPath(map, map->lineBlockmap, from, to, collectLineIntercepts, (void *)map);
    }
    if(flags & PT_ADDMOBJS)
    {
        traverseCellPath(map, map->mobjBlockmap, from, to, collectMobjIntercepts, (void *)map);
    }

    // Step #2: Process sorted intercepts.
    return P_TraverseIntercepts(callback, parameters);
}

BspLeaf *GameMap::bspLeafAtPoint(const_pvec2d_t const point)
{
    MapElement *bspElement = _bspRoot;
    while(bspElement->type() != DMU_BSPLEAF)
    {
        BspNode const &node = *bspElement->castTo<BspNode>();
        int side = node.partition().pointOnSide(point);

        // Decend to the next child subspace.
        bspElement = node.childPtr(side);
    }
    return bspElement->castTo<BspLeaf>();
}
