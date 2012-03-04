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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_system.h"
#include "de_refresh.h"

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

int GameMap_LineDefIndex(GameMap* map, linedef_t* line)
{
    assert(map);
    if(!line || !(line >= map->lineDefs && line <= &map->lineDefs[map->numLineDefs])) return -1;
    return line - map->lineDefs;
}

linedef_t* GameMap_LineDef(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numLineDefs) return NULL;
    return &map->lineDefs[idx];
}

int GameMap_SideDefIndex(GameMap* map, sidedef_t* side)
{
    assert(map);
    if(!side || !(side >= map->sideDefs && side <= &map->sideDefs[map->numSideDefs])) return -1;
    return side - map->sideDefs;
}

sidedef_t* GameMap_SideDef(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numSideDefs) return NULL;
    return &map->sideDefs[idx];
}

int GameMap_SectorIndex(GameMap* map, sector_t* sec)
{
    assert(map);
    if(!sec || !(sec >= map->sectors && sec <= &map->sectors[map->numSectors])) return -1;
    return sec - map->sectors;
}

sector_t* GameMap_Sector(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numSectors) return NULL;
    return &map->sectors[idx];
}

int GameMap_SubsectorIndex(GameMap* map, subsector_t* ssec)
{
    assert(map);
    if(!ssec || !(ssec >= map->subsectors && ssec <= &map->subsectors[map->numSubsectors])) return -1;
    return ssec - map->subsectors;
}

subsector_t* GameMap_Subsector(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numSubsectors) return NULL;
    return &map->subsectors[idx];
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

int GameMap_NodeIndex(GameMap* map, node_t* node)
{
    assert(map);
    if(!node || !(node >= map->nodes && node <= &map->nodes[map->numNodes])) return -1;
    return node - map->nodes;
}

node_t* GameMap_Node(GameMap* map, uint idx)
{
    assert(map);
    if(idx >= map->numNodes) return NULL;
    return &map->nodes[idx];
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

uint GameMap_SubsectorCount(GameMap* map)
{
    assert(map);
    return map->numSubsectors;
}

uint GameMap_HEdgeCount(GameMap* map)
{
    assert(map);
    return map->numHEdges;
}

uint GameMap_NodeCount(GameMap* map)
{
    assert(map);
    return map->numNodes;
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
    linedef_t** lineIter;
    subsector_t* ssec;
    vec2_t avg; // < Used to find a polyobj's center, and hence subsector.

    if(!po) return;

    V2_Set(avg, 0, 0);
    for(lineIter = po->lines; *lineIter; lineIter++)
    {
        linedef_t* line = *lineIter;
        sidedef_t* front = line->L_frontside;

        front->SW_topinflags |= SUIF_NO_RADIO;
        front->SW_middleinflags |= SUIF_NO_RADIO;
        front->SW_bottominflags |= SUIF_NO_RADIO;

        if(line->L_backside)
        {
            sidedef_t* back = line->L_backside;

            back->SW_topinflags |= SUIF_NO_RADIO;
            back->SW_middleinflags |= SUIF_NO_RADIO;
            back->SW_bottominflags |= SUIF_NO_RADIO;
        }

        V2_Sum(avg, avg, line->L_v1pos);
    }
    V2_Scale(avg, 1.f / po->lineCount);

    ssec = P_SubsectorAtPointXY(avg[VX], avg[VY]);
    if(ssec)
    {
        if(ssec->polyObj)
        {
            Con_Message("Warning: GameMap::initPolyobj: Multiple polyobjs in a single subsector\n"
                        "  (subsector %ld, sector %ld). Previous polyobj overridden.\n",
                        (long)GET_SUBSECTOR_IDX(ssec), (long)GET_SECTOR_IDX(ssec->sector));
        }
        ssec->polyObj = po;
        po->subsector = ssec;
    }

    Polyobj_UpdateAABox(po);
    Polyobj_UpdateSurfaceTangents(po);

    P_PolyobjUnlink(po);
    P_PolyobjLink(po);
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

void GameMap_InitSubsectorBlockmap(GameMap* map, const_pvec2_t min_, const_pvec2_t max_)
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

    map->subsectorBlockmap = Blockmap_New(min, max, CELL_SIZE, CELL_SIZE);

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

void GameMap_LinkLineDefInBlockmap(GameMap* map, linedef_t* lineDef)
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
    int (*func) (linedef_t*, void *);
    void* param;
} bmapiterparams_t;

static int blockmapCellLinesIterator(void* object, void* context)
{
    linedef_t* lineDef = (linedef_t*)object;
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
    int (*callback) (linedef_t*, void*), void* context)
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
    int (*callback) (linedef_t*, void*), void* context)
{
    bmapiterparams_t args;
    assert(map);

    args.localValidCount = validCount;
    args.func = callback;
    args.param = context;

    return Blockmap_IterateCellBlockObjects(map->lineDefBlockmap, blockCoords,
                                            blockmapCellLinesIterator, (void*) &args);
}

void GameMap_LinkSubsectorInBlockmap(GameMap* map, subsector_t* ssec)
{
    Blockmap* blockmap;
    GridmapBlock blockCoords;
    AABoxf aaBox;
    uint x, y;
    assert(map);

    // Do not link NULL subsectors.
    if(!ssec)
    {
        DEBUG_Message(("Warning: GameMap::LinkSubsectorInBlockmap: Attempted with NULL subsector argument.\n"));
        return;
    }

    // Subsectors without sectors don't get in.
    if(!ssec->sector) return;

    blockmap = map->subsectorBlockmap;
    aaBox.minX = ssec->aaBox.minX;
    aaBox.minY = ssec->aaBox.minY;
    aaBox.maxX = ssec->aaBox.maxX;
    aaBox.maxY = ssec->aaBox.maxY;
    Blockmap_CellBlockCoords(blockmap, &blockCoords, &aaBox);

    for(y = blockCoords.minY; y <= blockCoords.maxY; ++y)
    for(x = blockCoords.minX; x <= blockCoords.maxX; ++x)
    {
        Blockmap_CreateCellAndLinkObjectXY(blockmap, x, y, ssec);
    }
}

typedef struct subseciterparams_s {
    const AABoxf* box;
    sector_t* sector;
    int localValidCount;
    int (*func) (subsector_t*, void*);
    void* param;
} bmapsubsectoriterateparams_t;

static int blockmapCellSubsectorsIterator(void* object, void* context)
{
    subsector_t* ssec = (subsector_t*)object;
    bmapsubsectoriterateparams_t* args = (bmapsubsectoriterateparams_t*) context;
    if(ssec->validCount != args->localValidCount)
    {
        boolean ok = true;

        // This subsector has now been processed for the current iteration.
        ssec->validCount = args->localValidCount;

        // Check the sector restriction.
        if(args->sector && ssec->sector != args->sector)
            ok = false;

        // Check the bounds.
        if(args->box &&
           (ssec->aaBox.maxX < args->box->minX ||
            ssec->aaBox.minX > args->box->maxX ||
            ssec->aaBox.minY > args->box->maxY ||
            ssec->aaBox.maxY < args->box->minY))
            ok = false;

        if(ok)
        {
            // Action the callback.
            int result = args->func(ssec, args->param);
            if(result) return result; // Stop iteration.
        }
    }
    return false; // Continue iteration.
}

int GameMap_IterateCellSubsectors(GameMap* map, const uint coords[2],
    sector_t* sector, const AABoxf* box, int localValidCount,
    int (*callback) (subsector_t*, void*), void* context)
{
    bmapsubsectoriterateparams_t args;
    assert(map);

    args.localValidCount = localValidCount;
    args.func = callback;
    args.param = context;
    args.sector = sector;
    args.box = box;

    return Blockmap_IterateCellObjects(map->subsectorBlockmap, coords,
                                       blockmapCellSubsectorsIterator, (void*)&args);
}

int GameMap_IterateCellBlockSubsectors(GameMap* map, const GridmapBlock* blockCoords,
    sector_t* sector,  const AABoxf* box, int localValidCount,
    int (*callback) (subsector_t*, void*), void* context)
{
    bmapsubsectoriterateparams_t args;
    assert(map);

    args.localValidCount = localValidCount;
    args.func = callback;
    args.param = context;
    args.sector = sector;
    args.box = box;

    return Blockmap_IterateCellBlockObjects(map->subsectorBlockmap, blockCoords,
                                            blockmapCellSubsectorsIterator, (void*) &args);
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

typedef struct poiterparams_s {
    int (*func) (linedef_t*, void*);
    void* param;
} poiterparams_t;

int PTR_PolyobjLines(void* object, void* context)
{
    polyobj_t* po = (polyobj_t*)object;
    poiterparams_t* args = (poiterparams_t*) context;

    return Polyobj_LineIterator(po, args->func, args->param);
}

int GameMap_IterateCellPolyobjLineDefsIterator(GameMap* map, const uint coords[2],
    int (*callback) (linedef_t*, void*), void* context)
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
    int (*callback) (linedef_t*, void*), void* context)
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

subsector_t* GameMap_SubsectorAtPoint(GameMap* map, float point_[2])
{
    node_t* node = 0;
    uint nodenum = 0;
    float point[2];

    point[0] = point_? point_[0] : 0;
    point[1] = point_? point_[1] : 0;

    // single subsector is a special case
    if(!map->numNodes)
    {
        return (subsector_t*) map->subsectors;
    }

    nodenum = map->numNodes - 1;
    while(!(nodenum & NF_SUBSECTOR))
    {
        node = map->nodes + nodenum;
        ASSERT_DMU_TYPE(node, DMU_NODE);
        nodenum = node->children[P_PointOnPartitionSide(point[0], point[1], &node->partition)];
    }

    return map->subsectors + (nodenum & ~NF_SUBSECTOR);
}

subsector_t* GameMap_SubsectorAtPointXY(GameMap* map, float x, float y)
{
    float point[2];
    point[0] = x;
    point[1] = y;
    return GameMap_SubsectorAtPoint(map, point);
}
