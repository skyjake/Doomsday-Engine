/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_bmap.c: Blockmaps
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct ssecnode_s {
    void       *data;
    struct ssecnode_s *next;
} ssecnode_t;

typedef struct subsecmap_s {
    uint        count;
    ssecnode_t *nodes;
} ssecmap_t;

typedef struct ssecmapblock_s {
    subsector_t **ssecs;
} ssecmapblock_t;

typedef struct bmapblock_s {
    line_t    **lines;
    linkpolyobj_t *polyLinks;
} bmapblock_t;

typedef struct bmap_s {
    vec2_t      bbox[2];
    vec2_t      blockSize;
    uint        dimensions[2]; // In blocks.
    gridmap_t  *gridmap;
} bmap_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte    bmapShowDebug = 0; // 1 = debug linedefs, 2 = debug ssecs.
float   bmapDebugSize = 1.5f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_BoxToBlockmapBlocks(blockmap_t *blockmap, uint blockBox[4],
                           const arvec2_t box)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;
        vec2_t      m[2];

        m[0][VX] = MAX_OF(bmap->bbox[0][VX], box[0][VX]);
        m[1][VX] = MIN_OF(bmap->bbox[1][VX], box[1][VX]);
        m[0][VY] = MAX_OF(bmap->bbox[0][VY], box[0][VY]);
        m[1][VY] = MIN_OF(bmap->bbox[1][VY], box[1][VY]);

        blockBox[BOXLEFT]   = (m[0][VX] - bmap->bbox[0][VX]) / bmap->blockSize[VX];
        blockBox[BOXRIGHT]  = (m[1][VX] - bmap->bbox[0][VX]) / bmap->blockSize[VX];
        blockBox[BOXBOTTOM] = (m[0][VY] - bmap->bbox[0][VY]) / bmap->blockSize[VY];
        blockBox[BOXTOP]    = (m[1][VY] - bmap->bbox[0][VY]) / bmap->blockSize[VY];
    }
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */

boolean P_ToBlockmapBlockIdx(blockmap_t *blockmap, uint dest[2],
                             const pvec2_t pos)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;

        if(!(pos[VX] < bmap->bbox[0][VX] || pos[VX] >= bmap->bbox[1][VX] ||
             pos[VY] < bmap->bbox[0][VY] || pos[VY] >= bmap->bbox[1][VY]))
        {
            dest[VX] = (pos[VX] - bmap->bbox[0][VX]) / bmap->blockSize[VX];
            dest[VY] = (pos[VY] - bmap->bbox[0][VY]) / bmap->blockSize[VY];

            return true;
        }

        return false; // Outside blockmap.
    }

    return false; // hmm...
}

void P_PointToBlock(float x, float y, uint *bx, uint *by)
{
    gamemap_t  *map = P_GetCurrentMap();
    vec2_t      min;

    P_GetBlockmapBounds(map->blockMap, min, NULL);

    *bx = FLT2FIX(x - min[VX]) >> MAPBLOCKSHIFT;
    *by = FLT2FIX(y - min[VY]) >> MAPBLOCKSHIFT;
}

static __inline int xToSSecBlockX(bmap_t *bmap, float x)
{
    if(x >= bmap->bbox[0][VX] && x < bmap->bbox[1][VX])
        return (x - bmap->bbox[0][VX]) / bmap->blockSize[VX];

    return -1;
}

static __inline int yToSSecBlockY(bmap_t *bmap, float y)
{
    if(y >= bmap->bbox[0][VY] && y < bmap->bbox[1][VY])
        return (y - bmap->bbox[0][VY]) / bmap->blockSize[VY];

    return -1;
}

static bmap_t *allocBmap(void)
{
    return Z_Calloc(sizeof(bmap_t), PU_LEVELSTATIC, 0);
}

blockmap_t *P_BlockmapCreate(const pvec2_t min, const pvec2_t max,
                             uint width, uint height)
{
    bmap_t     *bmap = allocBmap();

    V2_Copy(bmap->bbox[0], min);
    V2_Copy(bmap->bbox[1], max);
    bmap->dimensions[VX] = width;
    bmap->dimensions[VY] = height;

    V2_Set(bmap->blockSize,
           (bmap->bbox[1][VX] - bmap->bbox[0][VX]) / bmap->dimensions[VX],
           (bmap->bbox[1][VY] - bmap->bbox[0][VY]) / bmap->dimensions[VY]);

    bmap->gridmap =
        M_GridmapCreate(bmap->dimensions[VX], bmap->dimensions[VY],
                        sizeof(bmapblock_t), PU_LEVELSTATIC);

    VERBOSE(Con_Message
            ("P_BlockMapCreate: w=%i h=%i\n", bmap->dimensions[VX],
             bmap->dimensions[VY]));

    return (blockmap_t *) bmap;
}

void P_SSecBlockmapSetBlock(blockmap_t *blockmap, uint x, uint y,
                            subsector_t **ssecs)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;
        ssecmapblock_t *block = M_GridmapGetBlock(bmap->gridmap, x, y, true);

        if(block)
        {
            block->ssecs = ssecs;
        }
    }
}

void P_BuildSubsectorBlockMap(gamemap_t *map)
{
#define BLKMARGIN           8
#define BLOCK_WIDTH         128
#define BLOCK_HEIGHT        128

    uint        startTime = Sys_GetRealTime();

    int         xl, xh, yl, yh, x, y;
    int         subMapWidth, subMapHeight;
    uint        i;
    ssecnode_t *iter, *next;
    ssecmap_t  *bmap, *block;
    vec2_t      bounds[2], blockSize, point, dims;
    blockmap_t *ssecBlockMap;

    // Figure out the dimensions of the blockmap.
    for(i = 0; i < map->numvertexes; ++i)
    {
        vertex_t   *vtx = &map->vertexes[i];

        V2_Set(point, vtx->V_pos[VX], vtx->V_pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN, bounds[1][VY] + BLKMARGIN);

    // Select a good size for the blocks.
    V2_Set(blockSize, MAPBLOCKUNITS, MAPBLOCKUNITS);
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    if(dims[VX] <= blockSize[VX])
        subMapWidth = 1;
    else
        subMapWidth = ceil(dims[VX] / blockSize[VX]);

    if(dims[VY] <= blockSize[VY])
        subMapHeight = 1;
    else
        subMapHeight = ceil(dims[VY] / blockSize[VY]);

    // Adjust the max bound so we have whole blocks.
    V2_Set(bounds[1], bounds[0][VX] + subMapWidth  * blockSize[VX],
                      bounds[0][VY] + subMapHeight * blockSize[VY]);

    ssecBlockMap = (blockmap_t*)
        P_BlockmapCreate(bounds[0], bounds[1],
                         subMapWidth, subMapHeight);

    // We'll construct the links using nodes.
    bmap = M_Calloc(sizeof(ssecmap_t) * subMapWidth * subMapHeight);

    // Process all the subsectors in the map.
    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t    *ssec = &map->subsectors[i];

        if(!ssec->sector)
            continue;

        // Blockcoords to link to.
        xl = xToSSecBlockX((bmap_t*)ssecBlockMap, ssec->bbox[0].pos[VX]);
        xh = xToSSecBlockX((bmap_t*)ssecBlockMap, ssec->bbox[1].pos[VX]);
        yl = yToSSecBlockY((bmap_t*)ssecBlockMap, ssec->bbox[0].pos[VY]);
        yh = yToSSecBlockY((bmap_t*)ssecBlockMap, ssec->bbox[1].pos[VY]);

        for(x = xl; x <= xh; ++x)
            for(y = yl; y <= yh; ++y)
            {
                if(x < 0 || y < 0 || x >= subMapWidth || y >= subMapHeight)
                {
                    Con_Printf("sub%i: outside block x=%i, y=%i\n", i, x, y);
                    continue;
                }

                // Create a new node.
                iter = M_Malloc(sizeof(ssecnode_t));
                iter->data = ssec;

                // Link to the temporary map.
                block = &bmap[x + y * subMapWidth];
                iter->next = block->nodes;
                block->nodes = iter;
                block->count++;
            }
    }

    // Create the actual links by 'hardening' the lists into arrays.
    for(y = 0; y < subMapHeight; ++y)
        for(x = 0; x < subMapWidth; ++x)
        {
            block = &bmap[y * subMapWidth + x];

            if(block->count > 0)
            {
                subsector_t **ssecs, **ptr;

                // A NULL-terminated array of pointers to subsectors.
                ssecs = Z_Malloc((block->count + 1) * sizeof(subsector_t *),
                                PU_LEVELSTATIC, NULL);

                // Copy pointers to the array, delete the nodes.
                ptr = ssecs;
                for(iter = block->nodes; iter; iter = next)
                {
                    *ptr++ = (subsector_t *) iter->data;
                    // Kill the node.
                    next = iter->next;
                    M_Free(iter);
                }
                // Terminate.
                *ptr = NULL;

                // Link it into the ssecblockmap.
                P_SSecBlockmapSetBlock(ssecBlockMap, x, y, ssecs);
            }
        }

    map->ssecBlockMap = ssecBlockMap;

    // Free the temporary link map.
    M_Free(bmap);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("P_BuildSubsectorBlockMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

void P_BlockmapSetBlock(blockmap_t *blockmap, uint x, uint y, line_t **lines,
                        linkpolyobj_t *link)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;
        bmapblock_t *block = M_GridmapGetBlock(bmap->gridmap, x, y, true);

        if(block)
        {
            block->lines = lines;
            block->polyLinks = link;
        }
    }
}

boolean unlinkPolyobjInBlock(bmapblock_t *block, void *context)
{
    polyobj_t  *po = (polyobj_t *) context;

    PO_UnlinkPolyobjFromRing(po, &block->polyLinks);
    return true;
}

boolean linkPolyobjInBlock(bmapblock_t *block, void *context)
{
    polyobj_t  *po = (polyobj_t *) context;

    PO_LinkPolyobjToRing(po, &block->polyLinks);
    return true;
}

void P_BlockmapLinkPolyobj(blockmap_t *blockmap, polyobj_t *po)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;
        uint        blockBox[4];

        PO_UpdateBBox(po);
        P_BoxToBlockmapBlocks(blockmap, blockBox, po->box);

        M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                              linkPolyobjInBlock, po);
    }
}

void P_BlockmapUnlinkPolyobj(blockmap_t *blockmap, polyobj_t *po)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;
        uint        blockBox[4];

        PO_UpdateBBox(po);
        P_BoxToBlockmapBlocks(BlockMap, blockBox, po->box);

        M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                              unlinkPolyobjInBlock, po);
    }
}

void P_GetBlockmapBounds(blockmap_t *blockmap, pvec2_t min, pvec2_t max)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;

        if(min)
            V2_Copy(min, bmap->bbox[0]);
        if(max)
            V2_Copy(max, bmap->bbox[1]);
    }
}

void P_GetBlockmapDimensions(blockmap_t *blockmap, uint v[2])
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;

        v[VX] = bmap->dimensions[VX];
        v[VY] = bmap->dimensions[VY];
    }
}

void P_InitMapBlockRings(gamemap_t *map)
{
    uint        i;
    size_t      size;
    uint        bmapSize[2];

    P_GetBlockmapDimensions(map->blockMap, bmapSize);

    // Clear out mobj rings.
    size = sizeof(*map->blockrings) * bmapSize[VX] * bmapSize[VY];
    map->blockrings = Z_Calloc(size, PU_LEVEL, 0);

    for(i = 0; i < bmapSize[VX] * bmapSize[VY]; ++i)
        map->blockrings[i].next =
            map->blockrings[i].prev = (mobj_t *) &map->blockrings[i];
}

typedef struct bmapiterparams_s {
    int         localValidCount;
    boolean   (*func) (line_t *, void *);
    void       *param;
} bmapiterparams_t;

static boolean bmapBlockLinesIterator(bmapblock_t *block, void *context)
{
    if(block->lines)
    {
        line_t    **iter;
        bmapiterparams_t *args = (bmapiterparams_t*) context;

        iter = block->lines;
        while(*iter)
        {
            line_t     *line = *iter;

            if(line->validCount != args->localValidCount)
            {
                line->validCount = args->localValidCount;

                if(!args->func(line, args->param))
                    return false;
            }

            *iter++;
        }
    }

    return true;
}

boolean P_BlockmapLinesIterator(blockmap_t *blockmap, const uint block[2],
                                boolean (*func) (line_t *, void *),
                                void *data)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t *) blockmap;
        bmapblock_t *bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock)
        {
            bmapiterparams_t args;

            args.localValidCount = validCount;
            args.func = func;
            args.param = data;

            return bmapBlockLinesIterator(bmapBlock, &args);
        }
    }

    return true;
}

boolean P_BlockBoxLinesIterator(blockmap_t *blockmap, const uint blockBox[4],
                                boolean (*func) (line_t *, void *),
                                void *data)
{
    bmap_t     *bmap = (bmap_t*) blockmap;
    bmapiterparams_t args;

    args.localValidCount = validCount;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockLinesIterator, &args);
}

typedef struct bmappoiterparams_s {
    int         localValidCount;
    boolean   (*func) (polyobj_t *, void *);
    void       *param;
} bmappoiterparams_t;

static boolean bmapBlockPolyobjsIterator(bmapblock_t *block, void *context)
{
    bmappoiterparams_t *args = (bmappoiterparams_t*) context;
    linkpolyobj_t *link;

    link = block->polyLinks;
    while(link)
    {
        linkpolyobj_t *next = link->next;

        if(link->polyobj)
            if(link->polyobj->validCount != args->localValidCount)
            {
                link->polyobj->validCount = args->localValidCount;

                if(!args->func(link->polyobj, args->param))
                    return false;
            }

        link = next;
    }

    return true;
}

boolean P_BlockmapPolyobjsIterator(blockmap_t *blockmap, const uint block[2],
                                   boolean (*func) (void *, void*),
                                   void *data)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t *) blockmap;
        bmapblock_t *bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock)
        {
            bmappoiterparams_t args;

            args.localValidCount = validCount;
            args.func = func;
            args.param = data;

            return bmapBlockPolyobjsIterator(bmapBlock, &args);
        }
    }

    return true;
}

boolean P_BlockBoxPolyobjsIterator(blockmap_t *blockmap, const uint blockBox[4],
                                   boolean (*func) (void *, void*),
                                   void *data)
{
    bmap_t     *bmap = (bmap_t*) blockmap;
    bmappoiterparams_t args;

    args.localValidCount = validCount;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockPolyobjsIterator, &args);
}

typedef struct sseciterparams_s {
    arvec2_t    box;
    sector_t   *sector;
    int         localValidCount;
    boolean   (*func) (subsector_t *, void *);
    void       *param;
} sseciterparams_t;

static boolean ssecBlockIterator(ssecmapblock_t *block, void *context)
{
    if(block->ssecs)
    {
        subsector_t **iter;
        sseciterparams_t *args = (sseciterparams_t*) context;

        iter = block->ssecs;

        while(*iter)
        {
            subsector_t *ssec = *iter;

            if(ssec->validCount != args->localValidCount)
            {
                boolean     ok = true;

                ssec->validCount = args->localValidCount;

                // Check the sector restriction.
                if(args->sector && ssec->sector != args->sector)
                    ok = false;

                // Check the bounds.
                if(args->box &&
                   (ssec->bbox[1].pos[VX] < args->box[0][VX] ||
                   ssec->bbox[0].pos[VX] > args->box[1][VX] ||
                   ssec->bbox[1].pos[VY] < args->box[0][VY] ||
                   ssec->bbox[0].pos[VY] > args->box[1][VY]))
                   ok = false;

                if(ok)
                {
                    if(!args->func(ssec, args->param))
                        return false;
                }
            }

            *iter++;
        }
    }

    return true;
}

boolean P_BlockmapSubsectorsIterator(blockmap_t *blockmap, const uint block[2],
                                     sector_t *sector, const arvec2_t box,
                                     int localValidCount,
                                     boolean (*func) (subsector_t *, void*),
                                     void *data)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t *) blockmap;
        ssecmapblock_t *ssecBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(ssecBlock && ssecBlock->ssecs)
        {
            sseciterparams_t args;

            args.box = box;
            args.localValidCount = localValidCount;
            args.sector = sector;
            args.func = func;
            args.param = data;

            return ssecBlockIterator(ssecBlock, &args);
        }
    }

    return true;
}

boolean P_BlockBoxSubsectorsIterator(blockmap_t *blockmap, const uint blockBox[4],
                                     sector_t *sector,  const arvec2_t box,
                                     int localValidCount,
                                     boolean (*func) (subsector_t *, void*),
                                     void *data)
{
    bmap_t     *bmap = (bmap_t *) blockmap;
    sseciterparams_t args;

    args.box = box;
    args.localValidCount = localValidCount;
    args.sector = sector;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 ssecBlockIterator, &args);
}

boolean P_BlockmapMobjsIterator(blockmap_t *blockmap, const uint block[2],
                                boolean (*func) (mobj_t *, void *),
                                void *data)
{
    return P_BlockRingMobjsIterator(block[VX], block[VY], func, data);
}

boolean P_BlockBoxMobjsIterator(blockmap_t *blockmap, const uint blockBox[4],
                                boolean (*func) (mobj_t *, void *),
                                void *data)
{
    uint        x, y;

    for(y = blockBox[BOXBOTTOM]; y <= blockBox[BOXTOP]; ++y)
        for(x = blockBox[BOXLEFT]; x <= blockBox[BOXRIGHT]; ++x)
        {
            if(!P_BlockRingMobjsIterator(x, y, func, data))
                return false;
        }

    return true;
}

typedef struct poiterparams_s {
    boolean   (*func) (line_t *, void *);
    void       *param;
} poiterparams_t;

boolean PTR_PolyobjLines(polyobj_t *po, void *data)
{
    poiterparams_t *args = (poiterparams_t*) data;

    return PO_PolyobjLineIterator(po, args->func, args->param);
}

boolean P_BlockmapPolyobjLinesIterator(blockmap_t *blockmap, const uint block[2],
                                       boolean (*func) (line_t *, void *),
                                       void *data)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t *) blockmap;
        bmapblock_t *bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock && bmapBlock->polyLinks)
        {
            bmappoiterparams_t args;
            poiterparams_t poargs;

            poargs.func = func;
            poargs.param = data;

            args.localValidCount = validCount;
            args.func = PTR_PolyobjLines;
            args.param = &poargs;

            return bmapBlockPolyobjsIterator(bmapBlock, &args);
        }
    }

    return true;
}

boolean P_BlockBoxPolyobjLinesIterator(blockmap_t *blockmap, const uint blockBox[4],
                                       boolean (*func) (line_t *, void *),
                                       void *data)
{
    bmap_t     *bmap = (bmap_t *) blockmap;
    bmappoiterparams_t args;
    poiterparams_t poargs;

    poargs.func = func;
    poargs.param = data;

    args.localValidCount = validCount;
    args.func = PTR_PolyobjLines;
    args.param = &poargs;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockPolyobjsIterator, &args);
}

boolean P_BlockPathTraverse(blockmap_t *bmap, const uint originBlock[2],
                            const uint destBlock[2],
                            const float origin[2], const float dest[2],
                            int flags, boolean (*func) (intercept_t *))
{
    uint        count;
    uint        block[2];
    float       delta[2], partial;
    fixed_t     intercept[2], step[2];
    int         stepDir[2];

    if(destBlock[VX] > originBlock[VX])
    {
        stepDir[VX] = 1;
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else if(destBlock[VX] < originBlock[VX])
    {
        stepDir[VX] = -1;
        partial = FIX2FLT((FLT2FIX(origin[VX]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VY] = (dest[VY] - origin[VY]) / fabs(dest[VX] - origin[VX]);
    }
    else
    {
        stepDir[VX] = 0;
        partial = 1;
        delta[VY] = 256;
    }
    intercept[VY] = (FLT2FIX(origin[VY]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[VY]);

    if(destBlock[VY] > originBlock[VY])
    {
        stepDir[VY] = 1;
        partial = 1 - FIX2FLT((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else if(destBlock[VY] < originBlock[VY])
    {
        stepDir[VY] = -1;
        partial = FIX2FLT((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1));
        delta[VX] = (dest[VX] - origin[VX]) / fabs(dest[VY] - origin[VY]);
    }
    else
    {
        stepDir[VY] = 0;
        partial = 1;
        delta[VX] = 256;
    }
    intercept[VX] = (FLT2FIX(origin[VX]) >> MAPBTOFRAC) +
        FLT2FIX(partial * delta[VX]);

    //
    // Step through map blocks.
    //

    // Count is present to prevent a round off error from skipping the
    // break and ending up in an infinite loop..
    block[VX] = originBlock[VX];
    block[VY] = originBlock[VY];
    step[VX] = FLT2FIX(delta[VX]);
    step[VY] = FLT2FIX(delta[VY]);
    for(count = 0; count < 64; ++count)
    {
        if(flags & PT_ADDLINES)
        {
            if(po_NumPolyobjs > 0)
            {
                if(!P_BlockmapPolyobjLinesIterator(BlockMap, block,
                                                   PIT_AddLineIntercepts, 0))
                    return false; // Early out.
            }

            if(!P_BlockmapLinesIterator(BlockMap, block,
                                        PIT_AddLineIntercepts, 0))
                return false; // Early out
        }

        if(flags & PT_ADDMOBJS)
        {
            if(!P_BlockmapMobjsIterator(BlockMap, block,
                                      PIT_AddMobjIntercepts, 0))
                return false; // Early out.
        }

        if(block[VX] == destBlock[VX] && block[VY] == destBlock[VY])
            break;

        if(intercept[VY] >> FRACBITS == block[VY])
        {
            intercept[VY] += step[VY];
            block[VX] += stepDir[VX];
        }
        else if(intercept[VX] >> FRACBITS == block[VX])
        {
            intercept[VX] += step[VX];
            block[VY] += stepDir[VY];
        }
    }

    return true;
}

/**
 * Part of the public Doomsday API.
 */
boolean P_BlockMobjsIterator(uint x, uint y,
                             boolean (*func) (mobj_t *, void *),
                             void *data)
{
    uint        block[2];
    block[VX] = x, block[VY] = y;
    return P_BlockmapMobjsIterator(BlockMap, block, func, data);
}

/**
 * Part of the public Doomsday API.
 */
boolean P_BlockLinesIterator(uint x, uint y,
                             boolean (*func) (line_t *, void *),
                             void *data)
{
    uint        block[2];
    block[VX] = x, block[VY] = y;

    // First polyobj lines.
    if(po_NumPolyobjs > 0)
    {
        if(!P_BlockmapPolyobjLinesIterator(BlockMap, block, func, data))
            return false;
    }

    // Now 'normal' lines.
    return P_BlockmapLinesIterator(BlockMap, block, func, data);
}

/**
 * Part of the public Doomsday API.
 */
boolean P_BlockPolyobjsIterator(uint x, uint y,
                                boolean (*func) (void *, void*),
                                void *data)
{
    if(po_NumPolyobjs > 0)
    {
        uint        block[2];
        block[VX] = x, block[VY] = y;

        return P_BlockmapPolyobjsIterator(BlockMap, block, func, data);
    }

    return true;
}

static boolean rendBlockLinedef(line_t *line, void *data)
{
    vec2_t      start, end;
    arvec2_t    bbox = data;

    V2_Set(start,
           line->L_v1pos[VX] - bbox[0][VX], line->L_v1pos[VY] - bbox[0][VY]);

    V2_Set(end,
           line->L_v2pos[VX] - bbox[0][VX], line->L_v2pos[VY] - bbox[0][VY]);

    gl.Vertex2fv(start);
    gl.Vertex2fv(end);
    return true; // Continue iteration.
}

static boolean rendBlockSubsector(subsector_t *ssec, void *data)
{
    vec2_t      start, end;
    arvec2_t    bbox = data;
    seg_t     **segs = ssec->segs;

    while(*segs)
    {
        seg_t      *seg = *segs;

        V2_Set(start,
               seg->SG_v1pos[VX] - bbox[0][VX], seg->SG_v1pos[VY] - bbox[0][VY]);

        V2_Set(end,
               seg->SG_v2pos[VX] - bbox[0][VX], seg->SG_v2pos[VY] - bbox[0][VY]);

        gl.Begin(DGL_LINES);
        gl.Vertex2fv(start);
        gl.Vertex2fv(end);
        gl.End();

        {
        float       length, dx, dy;
        float       normal[2], unit[2];
        float       scale = MAX_OF(bmapDebugSize, 1);
        float       width = (theWindow->width / 16) / scale;

        dx = end[VX] - start[VX];
        dy = end[VY] - start[VY];
        length = sqrt(dx * dx + dy * dy);
        if(length > 0)
        {
            unit[VX] = dx / length;
            unit[VY] = dy / length;
            normal[VX] = -unit[VY];
            normal[VY] = unit[VX];

            gl.Bind(GL_PrepareLSTexture(LST_DYNAMIC, NULL));

            gl.Enable(DGL_TEXTURING);
            gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
            gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);

            gl.Begin(DGL_QUADS);
            gl.TexCoord2f(0.75f, 0.5f);
            gl.Vertex2fv(start);
            gl.TexCoord2f(0.75f, 0.5f);
            gl.Vertex2fv(end);
            gl.TexCoord2f(0.75f, 1);
            gl.Vertex2f(end[VX] - normal[VX] * width,
                        end[VY] - normal[VY] * width);
            gl.TexCoord2f(0.75f, 1);
            gl.Vertex2f(start[VX] - normal[VX] * width,
                        start[VY] - normal[VY] * width);
            gl.End();

            gl.Disable(DGL_TEXTURING);
            gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
            gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
            }
        }

        // Draw the bounding box.
        V2_Set(start, ssec->bbox[0].pos[VX] - bbox[0][VX],
                      ssec->bbox[0].pos[VY] - bbox[0][VY]);
        V2_Set(end, ssec->bbox[1].pos[VX] - bbox[0][VX],
                    ssec->bbox[1].pos[VY] - bbox[0][VY]);

        gl.Begin(DGL_LINES);
        gl.Vertex2f(start[VX], start[VY]);
        gl.Vertex2f(end[VX], start[VY]);
        gl.Vertex2f(end[VX], start[VY]);
        gl.Vertex2f(end[VX], end[VY]);
        gl.Vertex2f(end[VX], end[VY]);
        gl.Vertex2f(start[VX], end[VY]);
        gl.Vertex2f(start[VX], end[VY]);
        gl.Vertex2f(start[VX], start[VY]);
        gl.End();
        *segs++;
    }
    return true; // Continue iteration.
}

void rendBlockLinedefs(void *blockPtr, void *param,
                       float r, float g, float b, float a)
{
    bmapblock_t *block = blockPtr;

    // Lines?
    if(block->lines)
    {
        bmapiterparams_t args;

        args.localValidCount = validCount;
        args.func = rendBlockLinedef;
        args.param = param;

        gl.Color4f(r, g, b, a);
        gl.Begin(DGL_LINES);
        bmapBlockLinesIterator(block, &args);
        gl.End();
    }

    // Polyobj lines?
    if(block->polyLinks)
    {
        bmappoiterparams_t args;
        poiterparams_t poargs;

        poargs.func = rendBlockLinedef;
        poargs.param = param;

        args.localValidCount = validCount;
        args.func = PTR_PolyobjLines;
        args.param = &poargs;

        gl.Color4f(r, g, b, a);
        gl.Begin(DGL_LINES);
        bmapBlockPolyobjsIterator(block, &args);
        gl.End();
    }
}

void rendBlockSubsectors(void *blockPtr, void *param,
                         float r, float g, float b, float a)
{
    ssecmapblock_t *block = blockPtr;

    if(block->ssecs)
    {
        sseciterparams_t args;

        args.box = NULL;
        args.localValidCount = validCount;
        args.sector = NULL;
        args.func = rendBlockSubsector;
        args.param = param;

        gl.Color4f(r, g, b, a);
        ssecBlockIterator(block, &args);
    }
}

static void drawInfoBox(int x, int y, long blockIdx, uint blockX,
                        uint blockY, int lineCount, int poCount)
{
    int         w, h;
    char        buf[160];

    sprintf(buf, "Block: %li [%u, %u] Lines: #%i Polyobjs: #%i",
            blockIdx, blockX, blockY, lineCount, poCount);
    w = FR_TextWidth(buf) + 16;
    h = FR_TextHeight(buf) + 16;
    x -= w / 2;
    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM),
                  UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx(buf, x + 8, y + h / 2, false, true, UI_Color(UIC_TITLE), 1);
}

static void drawInfoBox2(float minX, float minY, float maxX, float maxY,
                         float blockWidth, float blockHeight,
                         uint width, uint height)
{
    int         w = 16 + FR_TextWidth("(+000.0,+000.0)(+000.0,+000.0)");
    int         th = FR_TextHeight("a"), h = th * 4 + 16;
    int         x, y;
    char        buf[80];

    x = theWindow->width - 10 - w;
    y = theWindow->height - 10 - h;

    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM),
                  UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    x += 8;
    y += 8 + th/2;

    UI_TextOutEx("Blockmap", x, y, false, true, UI_Color(UIC_TITLE), 1);
    y += th;

    sprintf(buf, "Dimensions:[%u,%u] #%li", width, height,
            width * (long) height);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;

    sprintf(buf, "Blksize:[%.2f,%.2f]", blockWidth, blockHeight);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;

    sprintf(buf, "(%+06.0f,%+06.0f)(%+06.0f,%+06.0f)",
            minX, minY, maxX, maxY);
    UI_TextOutEx(buf, x, y, false, true, UI_Color(UIC_TEXT), 1);
    y += th;
}

static void drawBlockInfoBox(uint vBlock[2])
{
    int         lineCount = -1;
    int         poCount = -1;
    long        blockIdx = -1;
    bmap_t     *bmap = (bmap_t*) BlockMap;
    bmapblock_t *block;

    block = M_GridmapGetBlock(bmap->gridmap, vBlock[VX], vBlock[VY], false);
    if(block)
    {
        blockIdx = vBlock[VY] * bmap->dimensions[VY] + vBlock[VX];

        // Count the number of lines linked to this block.
        lineCount = 0;
        if(block->lines)
        {
            line_t    **iter = block->lines;
            while(*iter)
            {
                lineCount++;
                *iter++;
            }
        }

        // Count the number of polyobjs linked to this block.
        poCount = 0;
        if(block->polyLinks)
        {
            linkpolyobj_t *link = block->polyLinks;
            while(link)
            {
                linkpolyobj_t *next = link->next;
                if(link->polyobj)
                    poCount++;
                link = next;
            }
        }
    }

    drawInfoBox(theWindow->width / 2, 30,
                blockIdx, vBlock[VX], vBlock[VY], lineCount, poCount);
}

/**
 * Draw the blockmap in 2D HUD mode.
 */
static void blockmapDebug(blockmap_t *blockmap, mobj_t *followMobj,
                          void (*func) (void *blockPtr, void *param,
                                        float r, float g, float b, float a))
{
    bmap_t         *bmap = (bmap_t*) blockmap;
    void           *block;
    uint            x, y, vBlock[2], vBlockBox[4];
    float           scale, radius;
    vec2_t          start, end, box[2];

    scale = bmapDebugSize / MAX_OF(theWindow->height / 100, 1);

    if(followMobj)
    {
        // Determine the mobj's block.
        P_ToBlockmapBlockIdx(blockmap, vBlock, followMobj->pos);

        // Determine the mobj's collision blockbox.
        radius = followMobj->radius + 64; // MAXRADIUS
        V2_Set(start, followMobj->pos[VX] - radius, followMobj->pos[VY] - radius);
        V2_Set(end,   followMobj->pos[VX] + radius, followMobj->pos[VY] + radius);
        V2_InitBox(box, start);
        V2_AddToBox(box, end);
        P_BoxToBlockmapBlocks(blockmap, vBlockBox, box);
    }

    // Go into screen projection mode.
    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, theWindow->width, theWindow->height, -1, 1);

    gl.Translatef((theWindow->width / 2), (theWindow->height / 2), 0);
    gl.Scalef(scale, -scale, 1);

    if(followMobj)
    {   // Offset relatively to center on the location of the mobj.
        V2_Set(start,
               (vBlock[VX] * bmap->blockSize[VX]),
               (vBlock[VY] * bmap->blockSize[VY]));
        gl.Translatef(-start[VX], -start[VY], 0);
    }
    else
    {   // Offset to center the blockmap on the screen.
        gl.Translatef(-(bmap->blockSize[VX] * bmap->dimensions[VX] / 2),
                      -(bmap->blockSize[VY] * bmap->dimensions[VY] / 2), 0);
    }

    gl.Disable(DGL_TEXTURING);

    // Draw a background.
    V2_Set(start, 0, 0);
    V2_Set(end, bmap->blockSize[VX] * bmap->dimensions[VX],
                bmap->blockSize[VY] * bmap->dimensions[VY]);

    gl.Color4f(.25f, .25f, .25f, .66f);
    gl.Begin(DGL_QUADS);
    gl.Vertex2f(start[VX], start[VY]);
    gl.Vertex2f(end[VX], start[VY]);
    gl.Vertex2f(end[VX], end[VY]);
    gl.Vertex2f(start[VX], end[VY]);
    gl.End();

    /**
     * Draw the blocks.
     */

    for(y = 0; y < bmap->dimensions[VY]; ++y)
        for(x = 0; x < bmap->dimensions[VX]; ++x)
        {
            boolean     draw = false;
            bmapblock_t *block =
                M_GridmapGetBlock(bmap->gridmap, x, y, false);

            if(followMobj)
            {
                if(x == vBlock[VX] && y == vBlock[VY])
                {   // The block the viewPlayer is in.
                    gl.Color4f(.66f, .66f, 1, .66f);
                    draw = true;
                }
                else if(x >= vBlockBox[BOXLEFT]   && x <= vBlockBox[BOXRIGHT] &&
                        y >= vBlockBox[BOXBOTTOM] && y <= vBlockBox[BOXTOP])
                {   // In the viewPlayer's extended collision range.
                    gl.Color4f(.33f, .33f, .66f, .33f);
                    draw = true;
                }
            }

            if(!draw && !block)
            {   // NULL block.
                gl.Color4f(0, 0, 0, .95f);
                draw = true;
            }

            if(draw)
            {
                V2_Set(start, x * bmap->blockSize[VX],
                              y * bmap->blockSize[VY]);
                V2_Set(end, bmap->blockSize[VX],
                            bmap->blockSize[VY]);
                V2_Sum(end, end, start);

                gl.Begin(DGL_QUADS);
                gl.Vertex2f(start[VX], start[VY]);
                gl.Vertex2f(end[VX], start[VY]);
                gl.Vertex2f(end[VX], end[VY]);
                gl.Vertex2f(start[VX], end[VY]);
                gl.End();
            }
        }

    /**
     * Draw the grid lines
     */

    gl.Color4f(.5f, .5f, .5f, .125f);
    // Vertical lines:
    gl.Begin(DGL_LINES);
    for(x = 1; x < bmap->dimensions[VX]; ++x)
    {
        gl.Vertex2f(x * bmap->blockSize[VX],  0);
        gl.Vertex2f(x * bmap->blockSize[VX],
                    bmap->blockSize[VY] * bmap->dimensions[VY]);
    }
    gl.End();

    // Horizontal lines;
    gl.Begin(DGL_LINES);
    for(y = 1; y < bmap->dimensions[VY]; ++y)
    {
        gl.Vertex2f(0, y * bmap->blockSize[VY]);
        gl.Vertex2f(bmap->blockSize[VX] * bmap->dimensions[VX],
                    y * bmap->blockSize[VY]);
    }
    gl.End();

    /**
     * Draw the blockmap-linked data.
     */

    validCount++;

    if(followMobj)
    {
        // First, the blocks outside the viewPlayer's range.
        for(y = 0; y < bmap->dimensions[VY]; ++y)
            for(x = 0; x < bmap->dimensions[VX]; ++x)
            {
                if(x >= vBlockBox[BOXLEFT]   && x <= vBlockBox[BOXRIGHT] &&
                   y >= vBlockBox[BOXBOTTOM] && y <= vBlockBox[BOXTOP])
                    continue;

                block = M_GridmapGetBlock(bmap->gridmap, x, y, false);
                if(block)
                {
                    func(block, bmap->bbox, .33f, 0, 0, .75f);
                }
            }

        validCount++;

        // Next, the blocks within the viewPlayer's extended collision range.
        for(y = vBlockBox[BOXBOTTOM]; y <= vBlockBox[BOXTOP]; ++y)
            for(x = vBlockBox[BOXLEFT]; x <= vBlockBox[BOXRIGHT]; ++x)
            {
                if(x == vBlock[VX] && y == vBlock[VY])
                    continue;

                block = M_GridmapGetBlock(bmap->gridmap, x, y, false);
                if(block)
                {
                    func(block, bmap->bbox, 1, .5f, 0, 1);
                }
            }

        validCount++;

        // Lastly, the block the viewPlayer is in.
        block = M_GridmapGetBlock(bmap->gridmap, vBlock[VX], vBlock[VY], false);
        if(block)
        {
            func(block, bmap->bbox, 1, 1, 0, 1);
        }

        /**
         * Draw the viewPlayer.
         */

        radius = viewPlayer->mo->radius;
        V2_Set(start,
               viewPlayer->mo->pos[VX] - bmap->bbox[0][VX] - radius,
               viewPlayer->mo->pos[VY] - bmap->bbox[0][VY] - radius);
        V2_Set(end,
               viewPlayer->mo->pos[VX] - bmap->bbox[0][VX] + radius,
               viewPlayer->mo->pos[VY] - bmap->bbox[0][VY] + radius);

        gl.Color4f(0, 1, 0, 1);
        gl.Begin(DGL_QUADS);
        gl.Vertex2f(start[VX], start[VY]);
        gl.Vertex2f(end[VX], start[VY]);
        gl.Vertex2f(end[VX], end[VY]);
        gl.Vertex2f(start[VX], end[VY]);
        gl.End();
    }
    else
    {   // Just draw the lot.
        for(y = 0; y < bmap->dimensions[VY]; ++y)
            for(x = 0; x < bmap->dimensions[VX]; ++x)
            {
                block = M_GridmapGetBlock(bmap->gridmap, x, y, false);
                if(block)
                {
                    func(block, bmap->bbox, .33f, 0, 0, .75f);
                }
            }
    }

    /**
     * Draw the blockmap bounds.
     */

    V2_Set(start, -1, -1);
    V2_Set(end, 1 + bmap->blockSize[VX] * bmap->dimensions[VX],
           1 + bmap->blockSize[VY] * bmap->dimensions[VY]);

    gl.Color4f(1, .5f, .5f, 1);

    gl.Begin(DGL_LINES);
    gl.Vertex2f(start[VX], start[VY]);
    gl.Vertex2f(end[VX], start[VY]);

    gl.Vertex2f(end[VX], start[VY]);
    gl.Vertex2f(end[VX], end[VY]);

    gl.Vertex2f(end[VX], end[VY]);
    gl.Vertex2f(start[VX], end[VY]);

    gl.Vertex2f(start[VX], end[VY]);
    gl.Vertex2f(start[VX], start[VY]);
    gl.End();

    gl.PopMatrix();
    gl.Enable(DGL_TEXTURING);
}

void P_BlockmapDebug(void)
{
    blockmap_t *blockmap;
    bmap_t     *bmap;
    mobj_t     *followMobj = NULL;
    void      (*func) (void *, void *, float r, float g, float b, float a);

    if(!bmapShowDebug)
        return;

    if(bmapShowDebug == 2) // Subsectors.
    {
        if(!SSecBlockMap)
            return;

        blockmap = SSecBlockMap;
        func = rendBlockSubsectors;
    }
    else // Linedefs.
    {
        if(!BlockMap)
            return;

        blockmap = BlockMap;
        func = rendBlockLinedefs;
    }

    // If possible, we'll tailor what we draw relative to the viewPlayer.
    if(viewPlayer && viewPlayer->mo)
        followMobj = viewPlayer->mo;

    blockmapDebug(blockmap, followMobj, func);

    gl.MatrixMode(DGL_PROJECTION);
    gl.PushMatrix();
    gl.LoadIdentity();
    gl.Ortho(0, 0, theWindow->width, theWindow->height, -1, 1);

    if(followMobj && bmapShowDebug == 1)
    {
        uint            vBlock[2];

        // Determine the block mobj is in.
        P_ToBlockmapBlockIdx(blockmap, vBlock, followMobj->pos);

        // Draw info about the block mobj is in.
        drawBlockInfoBox(vBlock);
    }

    // Draw info about the blockmap.
    bmap = (bmap_t*) blockmap;
    drawInfoBox2(bmap->bbox[0][VX], bmap->bbox[0][VY],
                 bmap->bbox[1][VX], bmap->bbox[1][VY],
                 bmap->blockSize[VX], bmap->blockSize[VY],
                 bmap->dimensions[VX], bmap->dimensions[VY]);

    gl.MatrixMode(DGL_PROJECTION);
    gl.PopMatrix();
}
