/**\file p_bmap.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * Blockmaps.
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

typedef struct ssecmapblock_s {
    subsector_t**   ssecs;
} ssecmapblock_t;

typedef struct bmapblock_s {
    linedef_t**     lineDefs;
    linkmobj_t*     mobjLinks;
    linkpolyobj_t*  polyLinks;
} bmapblock_t;

typedef struct bmap_s {
    vec2_t          bBox[2];
    vec2_t          blockSize;
    uint            dimensions[2]; // In blocks.
    gridmap_t*      gridmap;
} bmap_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte bmapShowDebug = 0; // 1 = mobjs, 2 = linedefs, 3 = ssecs.
float bmapDebugSize = 1.5f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void P_BoxToBlockmapBlocks(blockmap_t* blockmap, uint blockBox[4],
                           const arvec2_t box)
{
    if(blockmap)
    {
        bmap_t*         bmap = (bmap_t*) blockmap;
        vec2_t          m[2];

        m[0][VX] = MAX_OF(bmap->bBox[0][VX], box[0][VX]);
        m[1][VX] = MIN_OF(bmap->bBox[1][VX], box[1][VX]);
        m[0][VY] = MAX_OF(bmap->bBox[0][VY], box[0][VY]);
        m[1][VY] = MIN_OF(bmap->bBox[1][VY], box[1][VY]);

        blockBox[BOXLEFT] =
            MINMAX_OF(0, (m[0][VX] - bmap->bBox[0][VX]) /
                            bmap->blockSize[VX], bmap->dimensions[0]);
        blockBox[BOXRIGHT] =
            MINMAX_OF(0, (m[1][VX] - bmap->bBox[0][VX]) /
                            bmap->blockSize[VX], bmap->dimensions[0]);
        blockBox[BOXBOTTOM] =
            MINMAX_OF(0, (m[0][VY] - bmap->bBox[0][VY]) /
                            bmap->blockSize[VY], bmap->dimensions[1]);
        blockBox[BOXTOP] =
            MINMAX_OF(0, (m[1][VY] - bmap->bBox[0][VY]) /
                            bmap->blockSize[VY], bmap->dimensions[1]);
    }
}

/**
 * Given a world coordinate, output the blockmap block[x, y] it resides in.
 */

boolean P_ToBlockmapBlockIdx(blockmap_t* blockmap, uint dest[2],
                             const pvec2_t pos)
{
    if(blockmap)
    {
        bmap_t*         bmap = (bmap_t*) blockmap;

        if(!(pos[VX] < bmap->bBox[0][VX] || pos[VX] >= bmap->bBox[1][VX] ||
             pos[VY] < bmap->bBox[0][VY] || pos[VY] >= bmap->bBox[1][VY]))
        {
            dest[VX] = (pos[VX] - bmap->bBox[0][VX]) / bmap->blockSize[VX];
            dest[VY] = (pos[VY] - bmap->bBox[0][VY]) / bmap->blockSize[VY];

            return true;
        }

        return false; // Outside blockmap.
    }

    return false; // hmm...
}

static __inline int xToSSecBlockX(bmap_t* bmap, float x)
{
    if(x >= bmap->bBox[0][VX] && x < bmap->bBox[1][VX])
        return (x - bmap->bBox[0][VX]) / bmap->blockSize[VX];

    return -1;
}

static __inline int yToSSecBlockY(bmap_t* bmap, float y)
{
    if(y >= bmap->bBox[0][VY] && y < bmap->bBox[1][VY])
        return (y - bmap->bBox[0][VY]) / bmap->blockSize[VY];

    return -1;
}

static bmap_t* allocBmap(void)
{
    return Z_Calloc(sizeof(bmap_t), PU_MAPSTATIC, 0);
}

blockmap_t* P_BlockmapCreate(const pvec2_t min, const pvec2_t max,
                             uint width, uint height)
{
    bmap_t*         bmap = allocBmap();

    V2_Copy(bmap->bBox[0], min);
    V2_Copy(bmap->bBox[1], max);
    bmap->dimensions[VX] = width;
    bmap->dimensions[VY] = height;

    V2_Set(bmap->blockSize,
           (bmap->bBox[1][VX] - bmap->bBox[0][VX]) / bmap->dimensions[VX],
           (bmap->bBox[1][VY] - bmap->bBox[0][VY]) / bmap->dimensions[VY]);

    bmap->gridmap =
        M_GridmapCreate(bmap->dimensions[VX], bmap->dimensions[VY],
                        sizeof(bmapblock_t), PU_MAPSTATIC);

    VERBOSE(Con_Message
            ("P_BlockMapCreate: w=%i h=%i\n", bmap->dimensions[VX],
             bmap->dimensions[VY]));

    return (blockmap_t *) bmap;
}

void P_SSecBlockmapSetBlock(blockmap_t* blockmap, uint x, uint y,
                            subsector_t** ssecs)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        ssecmapblock_t*     block =
            M_GridmapGetBlock(bmap->gridmap, x, y, true);

        if(block)
        {
            block->ssecs = ssecs;
        }
    }
}

void P_BuildSubsectorBlockMap(gamemap_t* map)
{
#define BLKMARGIN       8
#define BLOCK_WIDTH     128
#define BLOCK_HEIGHT    128

typedef struct ssecnode_s {
    void*           data;
    struct ssecnode_s* next;
} ssecnode_t;

typedef struct subsecmap_s {
    uint            count;
    ssecnode_t*     nodes;
} ssecmap_t;

    uint                startTime = Sys_GetRealTime();

    int                 xl, xh, yl, yh, x, y;
    int                 subMapWidth, subMapHeight;
    uint                i;
    ssecnode_t*         iter, *next;
    ssecmap_t*          bmap, *block;
    vec2_t              bounds[2], blockSize, dims;
    blockmap_t*         ssecBlockMap;
    subsector_t**       ssecLinks;
    size_t              totalLinks;

    // Setup the blockmap area to enclose the whole map, plus a margin
    // (margin is needed for a map that fits entirely inside one blockmap
    // cell).
    V2_Set(bounds[0], map->bBox[BOXLEFT] - BLKMARGIN,
                      map->bBox[BOXBOTTOM] - BLKMARGIN);
    V2_Set(bounds[1], map->bBox[BOXRIGHT] + BLKMARGIN,
                      map->bBox[BOXTOP] + BLKMARGIN);

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
        P_BlockmapCreate(bounds[0], bounds[1], subMapWidth, subMapHeight);

    // We'll construct the temporary links using nodes.
    bmap = M_Calloc(sizeof(ssecmap_t) * subMapWidth * subMapHeight);

    // Process all the subsectors in the map.
    totalLinks = 0;
    for(i = 0; i < map->numSSectors; ++i)
    {
        subsector_t*        ssec = &map->ssectors[i];

        if(!ssec->sector)
            continue;

        // Blockcoords to link to.
        xl = xToSSecBlockX((bmap_t*)ssecBlockMap, ssec->bBox[0].pos[VX]);
        xh = xToSSecBlockX((bmap_t*)ssecBlockMap, ssec->bBox[1].pos[VX]);
        yl = yToSSecBlockY((bmap_t*)ssecBlockMap, ssec->bBox[0].pos[VY]);
        yh = yToSSecBlockY((bmap_t*)ssecBlockMap, ssec->bBox[1].pos[VY]);

        for(x = xl; x <= xh; ++x)
            for(y = yl; y <= yh; ++y)
            {
                if(x < 0 || y < 0 || x >= subMapWidth || y >= subMapHeight)
                {
                    Con_Printf("Warning: Subsector %i outside block [%i, %i]\n", i, x, y);
                    continue;
                }

                // Create a new node.
                iter = M_Malloc(sizeof(ssecnode_t));
                iter->data = ssec;

                // Link to the temporary map.
                block = &bmap[x + y * subMapWidth];
                iter->next = block->nodes;
                block->nodes = iter;
                totalLinks++;
                if(!block->count)
                    totalLinks++; // +1 for terminating NULL.
                block->count++;
            }
    }

    ssecLinks = Z_Malloc(totalLinks * sizeof(subsector_t*), PU_MAP, NULL);

    // Create the actual links by 'hardening' the lists into arrays.
    for(y = 0; y < subMapHeight; ++y)
        for(x = 0; x < subMapWidth; ++x)
        {
            block = &bmap[y * subMapWidth + x];

            if(block->count > 0)
            {
                subsector_t**       ssecs, **ptr;

                // A NULL-terminated array of pointers to subsectors.
                ssecs = ssecLinks;

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

                ssecLinks += block->count + 1;
            }
        }

    map->ssecBlockMap = ssecBlockMap;

    // Free the temporary link map.
    M_Free(bmap);

    VERBOSE2( Con_Message("P_BuildSubsectorBlockMap: Done in %.2f seconds.\n", (Sys_GetRealTime() - startTime) / 1000.0f) )

#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

void P_BlockmapSetBlock(blockmap_t* blockmap, uint x, uint y,
                        linedef_t** lines, linkmobj_t* moLink,
                        linkpolyobj_t* poLink)
{
    if(blockmap)
    {
        bmap_t*         bmap = (bmap_t*) blockmap;
        bmapblock_t*    block = M_GridmapGetBlock(bmap->gridmap, x, y, true);

        if(block)
        {
            block->lineDefs = lines;
            block->mobjLinks = moLink;
            block->polyLinks = poLink;
        }
    }
}

boolean unlinkPolyobjInBlock(void* ptr, void* context)
{
    bmapblock_t*        block = (bmapblock_t*) ptr;
    polyobj_t*          po = (polyobj_t*) context;

    P_PolyobjUnlinkFromRing(po, &block->polyLinks);
    return true;
}

boolean linkPolyobjInBlock(void* ptr, void* context)
{
    bmapblock_t*        block = (bmapblock_t*) ptr;
    polyobj_t*          po = (polyobj_t *) context;

    P_PolyobjLinkToRing(po, &block->polyLinks);
    return true;
}

void P_BlockmapLinkPolyobj(blockmap_t* blockmap, polyobj_t* po)
{
    if(blockmap)
    {
        bmap_t* bmap = (bmap_t*) blockmap;
        uint blockBox[4], x, y;

        P_PolyobjUpdateBBox(po);
        P_BoxToBlockmapBlocks(blockmap, blockBox, po->box);

        for(y = blockBox[BOXBOTTOM]; y <= blockBox[BOXTOP]; ++y)
            for(x = blockBox[BOXLEFT]; x <= blockBox[BOXRIGHT]; ++x)
            {
                bmapblock_t* block = (bmapblock_t*) M_GridmapGetBlock(bmap->gridmap, x, y, true);
                linkPolyobjInBlock((void*)block, po);
            }
    }
}

void P_BlockmapUnlinkPolyobj(blockmap_t* blockmap, polyobj_t* po)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        uint                blockBox[4];

        P_PolyobjUpdateBBox(po);
        P_BoxToBlockmapBlocks(BlockMap, blockBox, po->box);

        M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                              unlinkPolyobjInBlock, (void*) po);
    }
}

void P_BlockmapLinkMobj(blockmap_t* blockmap, mobj_t* mo)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        uint                blockXY[2];
        bmapblock_t*        block;

        P_ToBlockmapBlockIdx(BlockMap, blockXY, mo->pos);
        block = (bmapblock_t*)
            M_GridmapGetBlock(bmap->gridmap, blockXY[0], blockXY[1], true);
        if(block)
            P_MobjLinkToRing(mo, &block->mobjLinks);
    }
}

boolean P_BlockmapUnlinkMobj(blockmap_t* blockmap, mobj_t* mo)
{
    boolean             unlinked = false;

    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        uint                blockXY[2];
        bmapblock_t*        block;

        P_ToBlockmapBlockIdx(BlockMap, blockXY, mo->pos);
        block = (bmapblock_t*)
            M_GridmapGetBlock(bmap->gridmap, blockXY[0], blockXY[1], false);
        if(block)
            unlinked = P_MobjUnlinkFromRing(mo, &block->mobjLinks);
    }

    return unlinked;
}

void P_GetBlockmapBounds(blockmap_t* blockmap, pvec2_t min, pvec2_t max)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;

        if(min)
            V2_Copy(min, bmap->bBox[0]);
        if(max)
            V2_Copy(max, bmap->bBox[1]);
    }
}

void P_GetBlockmapDimensions(blockmap_t* blockmap, uint v[2])
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;

        v[VX] = bmap->dimensions[VX];
        v[VY] = bmap->dimensions[VY];
    }
}

typedef struct bmapiterparams_s {
    int             localValidCount;
    boolean       (*func) (linedef_t*, void *);
    void*           param;
} bmapiterparams_t;

static boolean bmapBlockLinesIterator(void* ptr, void* context)
{
    bmapblock_t* block = (bmapblock_t*) ptr;
    if(block->lineDefs)
    {
        bmapiterparams_t* args = (bmapiterparams_t*) context;
        linedef_t** iter = block->lineDefs;
        while(*iter)
        {
            {linedef_t* line = *iter;
            if(line->validCount != args->localValidCount)
            {
                line->validCount = args->localValidCount;
                if(!args->func(line, args->param))
                    return false;
            }}
            iter++;
        }
    }

    return true;
}

boolean P_BlockmapLinesIterator(blockmap_t* blockmap, const uint block[2],
                                boolean (*func) (linedef_t*, void*),
                                void* data)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        bmapblock_t*        bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock)
        {
            bmapiterparams_t    args;

            args.localValidCount = validCount;
            args.func = func;
            args.param = data;

            return bmapBlockLinesIterator(bmapBlock, &args);
        }
    }

    return true;
}

boolean P_BlockBoxLinesIterator(blockmap_t* blockmap, const uint blockBox[4],
                                boolean (*func) (linedef_t*, void*),
                                void* data)
{
    bmap_t*             bmap = (bmap_t*) blockmap;
    bmapiterparams_t    args;

    args.localValidCount = validCount;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockLinesIterator, (void*) &args);
}

typedef struct bmappoiterparams_s {
    int             localValidCount;
    boolean       (*func) (polyobj_t*, void *);
    void*           param;
} bmappoiterparams_t;

static boolean bmapBlockPolyobjsIterator(void* ptr, void* context)
{
    bmapblock_t*        block = (bmapblock_t*) ptr;
    bmappoiterparams_t* args = (bmappoiterparams_t*) context;
    linkpolyobj_t*      link;

    link = block->polyLinks;
    while(link)
    {
        linkpolyobj_t*      next = link->next;

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

boolean P_BlockmapPolyobjsIterator(blockmap_t* blockmap, const uint block[2],
                                   boolean (*func) (polyobj_t*, void*),
                                   void* data)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        bmapblock_t*        bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock)
        {
            bmappoiterparams_t  args;

            args.localValidCount = validCount;
            args.func = func;
            args.param = data;

            return bmapBlockPolyobjsIterator(bmapBlock, (void*) &args);
        }
    }

    return true;
}

boolean P_BlockBoxPolyobjsIterator(blockmap_t* blockmap, const uint blockBox[4],
                                   boolean (*func) (polyobj_t*, void*),
                                   void* data)
{
    bmap_t*             bmap = (bmap_t*) blockmap;
    bmappoiterparams_t  args;

    args.localValidCount = validCount;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockPolyobjsIterator, (void*) &args);
}

typedef struct bmapmoiterparams_s {
    int             localValidCount;
    boolean       (*func) (mobj_t*, void *);
    void*           param;
} bmapmoiterparams_t;

static boolean bmapBlockMobjsIterator(void* ptr, void* context)
{
    bmapblock_t*        block = (bmapblock_t*) ptr;
    bmapmoiterparams_t* args = (bmapmoiterparams_t*) context;
    linkmobj_t*         link;

    link = block->mobjLinks;
    while(link)
    {
        linkmobj_t*         next = link->next;

        if(link->mobj)
            if(link->mobj->validCount != args->localValidCount)
            {
                link->mobj->validCount = args->localValidCount;

                if(!args->func(link->mobj, args->param))
                    return false;
            }

        link = next;
    }

    return true;
}

boolean P_BlockmapMobjsIterator(blockmap_t* blockmap, const uint block[2],
                                boolean (*func) (mobj_t*, void*),
                                void* data)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        bmapblock_t*        bmapBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(bmapBlock)
        {
            bmapmoiterparams_t  args;

            args.localValidCount = validCount;
            args.func = func;
            args.param = data;

            return bmapBlockMobjsIterator(bmapBlock, (void*) &args);
        }
    }

    return true;
}

boolean P_BlockBoxMobjsIterator(blockmap_t *blockmap, const uint blockBox[4],
                                boolean (*func) (mobj_t*, void*),
                                void* data)
{
    bmap_t*             bmap = (bmap_t*) blockmap;
    bmapmoiterparams_t  args;

    args.localValidCount = validCount;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockMobjsIterator, (void*) &args);
}

typedef struct sseciterparams_s {
    arvec2_t        box;
    sector_t*       sector;
    int             localValidCount;
    boolean       (*func) (subsector_t*, void*);
    void*           param;
} sseciterparams_t;

static boolean ssecBlockIterator(void* ptr, void* context)
{
    ssecmapblock_t*     block = (ssecmapblock_t*) ptr;

    if(block->ssecs)
    {
        subsector_t**       iter;
        sseciterparams_t*   args = (sseciterparams_t*) context;

        iter = block->ssecs;

        while(*iter)
        {
            subsector_t* ssec = *iter;

            if(ssec->validCount != args->localValidCount)
            {
                boolean ok = true;

                ssec->validCount = args->localValidCount;

                // Check the sector restriction.
                if(args->sector && ssec->sector != args->sector)
                    ok = false;

                // Check the bounds.
                if(args->box &&
                   (ssec->bBox[1].pos[VX] < args->box[0][VX] ||
                    ssec->bBox[0].pos[VX] > args->box[1][VX] ||
                    ssec->bBox[0].pos[VY] > args->box[1][VY] ||
                    ssec->bBox[1].pos[VY] < args->box[0][VY]))
                   ok = false;

                if(ok)
                {
                    if(!args->func(ssec, args->param))
                        return false;
                }
            }

            iter++;
        }
    }

    return true;
}

boolean P_BlockmapSubsectorsIterator(blockmap_t* blockmap, const uint block[2],
                                     sector_t* sector, const arvec2_t box,
                                     int localValidCount,
                                     boolean (*func) (subsector_t*, void*),
                                     void* data)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        ssecmapblock_t*     ssecBlock =
            M_GridmapGetBlock(bmap->gridmap, block[VX], block[VY], false);

        if(ssecBlock && ssecBlock->ssecs)
        {
            sseciterparams_t    args;

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

boolean P_BlockBoxSubsectorsIterator(blockmap_t* blockmap, const uint blockBox[4],
                                     sector_t* sector,  const arvec2_t box,
                                     int localValidCount,
                                     boolean (*func) (subsector_t*, void*),
                                     void* data)
{
    bmap_t*             bmap = (bmap_t *) blockmap;
    sseciterparams_t    args;

    args.box = box;
    args.localValidCount = localValidCount;
    args.sector = sector;
    args.func = func;
    args.param = data;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 ssecBlockIterator, (void*) &args);
}

typedef struct poiterparams_s {
    boolean       (*func) (linedef_t*, void*);
    void*           param;
} poiterparams_t;

boolean PTR_PolyobjLines(polyobj_t* po, void* data)
{
    poiterparams_t*     args = (poiterparams_t*) data;

    return P_PolyobjLinesIterator(po, args->func, args->param);
}

boolean P_BlockmapPolyobjLinesIterator(blockmap_t* blockmap, const uint block[2],
                                       boolean (*func) (linedef_t*, void*),
                                       void* data)
{
    if(blockmap)
    {
        bmap_t*             bmap = (bmap_t*) blockmap;
        bmapblock_t*        bmapBlock =
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

boolean P_BlockBoxPolyobjLinesIterator(blockmap_t* blockmap, const uint blockBox[4],
                                       boolean (*func) (linedef_t*, void*),
                                       void* data)
{
    bmap_t*             bmap = (bmap_t*) blockmap;
    bmappoiterparams_t  args;
    poiterparams_t      poargs;

    poargs.func = func;
    poargs.param = data;

    args.localValidCount = validCount;
    args.func = PTR_PolyobjLines;
    args.param = &poargs;

    return M_GridmapBoxIteratorv(bmap->gridmap, blockBox,
                                 bmapBlockPolyobjsIterator, (void*) &args);
}

boolean P_BlockPathTraverse(blockmap_t* bmap, const uint originBlock[2],
                            const uint destBlock[2],
                            const float origin[2], const float dest[2],
                            int flags, boolean (*func) (intercept_t*))
{
    uint                count, block[2];
    float               delta[2], partial;
    fixed_t             intercept[2], step[2];
    int                 stepDir[2];

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
        partial = FIX2FLT(FRACUNIT - ((FLT2FIX(origin[VY]) >> MAPBTOFRAC) & (FRACUNIT - 1)));
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
            if(numPolyObjs > 0)
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

        if((unsigned) (intercept[VY] >> FRACBITS) == block[VY])
        {
            intercept[VY] += step[VY];
            block[VX] += stepDir[VX];
        }
        else if((unsigned) (intercept[VX] >> FRACBITS) == block[VX])
        {
            intercept[VX] += step[VX];
            block[VY] += stepDir[VY];
        }
    }

    return true;
}

static boolean rendBlockMobj(mobj_t* mo, void* data)
{
    vec2_t              start, end;
    arvec2_t            bbox = data;

    V2_Set(start,
           mo->pos[VX] - bbox[0][VX] - mo->radius,
           mo->pos[VY] - bbox[0][VY] - mo->radius);
    V2_Set(end,
           mo->pos[VX] - bbox[0][VX] + mo->radius,
           mo->pos[VY] - bbox[0][VY] + mo->radius);

    glVertex2f(start[VX], start[VY]);
    glVertex2f(end[VX], start[VY]);
    glVertex2f(end[VX], end[VY]);
    glVertex2f(start[VX], end[VY]);
    return true; // Continue iteration.
}

static boolean rendBlockLinedef(linedef_t* line, void* data)
{
    vec2_t              start, end;
    arvec2_t            bbox = data;

    V2_Set(start,
           line->L_v1pos[VX] - bbox[0][VX], line->L_v1pos[VY] - bbox[0][VY]);

    V2_Set(end,
           line->L_v2pos[VX] - bbox[0][VX], line->L_v2pos[VY] - bbox[0][VY]);

    glVertex2fv(start);
    glVertex2fv(end);
    return true; // Continue iteration.
}

static boolean rendBlockSubsector(subsector_t* ssec, void* data)
{
    vec2_t              start, end;
    arvec2_t            bbox = data;
    seg_t**             segs = ssec->segs;

    while(*segs)
    {
        seg_t*              seg = *segs;

        V2_Set(start,
               seg->SG_v1pos[VX] - bbox[0][VX], seg->SG_v1pos[VY] - bbox[0][VY]);

        V2_Set(end,
               seg->SG_v2pos[VX] - bbox[0][VX], seg->SG_v2pos[VY] - bbox[0][VY]);

        glBegin(GL_LINES);
            glVertex2fv(start);
            glVertex2fv(end);
        glEnd();

        {
        float               length, dx, dy;
        float               normal[2], unit[2];
        float               scale = MAX_OF(bmapDebugSize, 1);
        float               width = (theWindow->width / 16) / scale;

        dx = end[VX] - start[VX];
        dy = end[VY] - start[VY];
        length = sqrt(dx * dx + dy * dy);
        if(length > 0)
        {
            unit[VX] = dx / length;
            unit[VY] = dy / length;
            normal[VX] = -unit[VY];
            normal[VY] = unit[VX];

            glBindTexture(GL_TEXTURE_2D, GL_PrepareLSTexture(LST_DYNAMIC));

            glEnable(GL_TEXTURE_2D);
            GL_BlendOp(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            glBegin(GL_QUADS);
                glTexCoord2f(0.75f, 0.5f);
                glVertex2fv(start);
                glTexCoord2f(0.75f, 0.5f);
                glVertex2fv(end);
                glTexCoord2f(0.75f, 1);
                glVertex2f(end[VX] - normal[VX] * width,
                           end[VY] - normal[VY] * width);
                glTexCoord2f(0.75f, 1);
                glVertex2f(start[VX] - normal[VX] * width,
                           start[VY] - normal[VY] * width);
            glEnd();

            glDisable(GL_TEXTURE_2D);
            GL_BlendMode(BM_NORMAL);
            }
        }

        // Draw the bounding box.
        V2_Set(start, ssec->bBox[0].pos[VX] - bbox[0][VX],
                      ssec->bBox[0].pos[VY] - bbox[0][VY]);
        V2_Set(end, ssec->bBox[1].pos[VX] - bbox[0][VX],
                    ssec->bBox[1].pos[VY] - bbox[0][VY]);

        glBegin(GL_LINES);
            glVertex2f(start[VX], start[VY]);
            glVertex2f(end[VX], start[VY]);
            glVertex2f(end[VX], start[VY]);
            glVertex2f(end[VX], end[VY]);
            glVertex2f(end[VX], end[VY]);
            glVertex2f(start[VX], end[VY]);
            glVertex2f(start[VX], end[VY]);
            glVertex2f(start[VX], start[VY]);
        glEnd();
        segs++;
    }
    return true; // Continue iteration.
}

void rendBlockLinedefs(void* blockPtr, void* param, float r, float g, float b, float a)
{
    bmapblock_t* block = blockPtr;

    // Lines?
    if(block->lineDefs)
    {
        bmapiterparams_t args;

        args.localValidCount = validCount;
        args.func = rendBlockLinedef;
        args.param = param;

        glColor4f(r, g, b, a);
        glBegin(GL_LINES);
            bmapBlockLinesIterator(block, &args);
        glEnd();
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

        glColor4f(r, g, b, a);
        glBegin(GL_LINES);
            bmapBlockPolyobjsIterator(block, &args);
        glEnd();
    }
}

void rendBlockMobjs(void* blockPtr, void* data, float r, float g, float b, float a)
{
    bmapblock_t* block = blockPtr;

    // Mobjs?
    if(block->mobjLinks)
    {
        bmapmoiterparams_t  args;

        args.localValidCount = validCount;
        args.func = rendBlockMobj;
        args.param = data;

        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
            bmapBlockMobjsIterator(block, (void*) &args);
        glEnd();
    }
}

void rendBlockSubsectors(void* blockPtr, void* param,
                         float r, float g, float b, float a)
{
    ssecmapblock_t*     block = blockPtr;

    if(block->ssecs)
    {
        sseciterparams_t    args;

        args.box = NULL;
        args.localValidCount = validCount;
        args.sector = NULL;
        args.func = rendBlockSubsector;
        args.param = param;

        glColor4f(r, g, b, a);
        ssecBlockIterator(block, &args);
    }
}

static void drawInfoBox(int x, int y, long blockIdx, uint blockX,
                        uint blockY, int lineCount, int moCount, int poCount)
{
    int                 w, h;
    char                buf[160];

    sprintf(buf, "Block: %li [%u, %u] Lines: #%i Mobjs: #%i Polyobjs: #%i",
            blockIdx, blockX, blockY, lineCount, moCount, poCount);
    FR_SetFont(glFontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    w = FR_TextFragmentWidth(buf) + 16;
    h = FR_TextFragmentHeight(buf) + 16;
    x -= w / 2;
    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM), UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);
    UI_SetColor(UI_Color(UIC_TEXT));
    UI_TextOutEx2(buf, x + 8, y + h / 2, UI_Color(UIC_TITLE), 1, DTF_ALIGN_LEFT|DTF_ONLY_SHADOW);
}

static void drawInfoBox2(float minX, float minY, float maxX, float maxY,
                         float blockWidth, float blockHeight,
                         uint width, uint height)
{
    int th, w, h, x, y;
    char buf[80];

    FR_SetFont(glFontFixed);
    FR_LoadDefaultAttrib();
    FR_SetShadowOffset(UI_SHADOW_OFFSET, UI_SHADOW_OFFSET);
    w = 16 + FR_TextFragmentWidth("(+000.0,+000.0)(+000.0,+000.0)");
    th = FR_TextFragmentHeight("a");
    h = th * 4 + 16;

    x = theWindow->width  - 10 - w;
    y = theWindow->height - 10 - h;

    UI_GradientEx(x, y, w, h, 6, UI_Color(UIC_BG_MEDIUM),
                  UI_Color(UIC_BG_LIGHT), .5f, .5f);
    UI_DrawRectEx(x, y, w, h, 6, false, UI_Color(UIC_BRD_HI), NULL, .5f, -1);

    x += 8;
    y += 8 + th/2;

    UI_TextOutEx2("Blockmap", x, y, UI_Color(UIC_TITLE), 1, DTF_ALIGN_LEFT|DTF_ONLY_SHADOW);
    y += th;

    sprintf(buf, "Dimensions:[%u,%u] #%li", width, height,
            width * (long) height);
    UI_TextOutEx2(buf, x, y, UI_Color(UIC_TEXT), 1, DTF_ALIGN_LEFT|DTF_ONLY_SHADOW);
    y += th;

    sprintf(buf, "Blksize:[%.2f,%.2f]", blockWidth, blockHeight);
    UI_TextOutEx2(buf, x, y, UI_Color(UIC_TEXT), 1, DTF_ALIGN_LEFT|DTF_ONLY_SHADOW);
    y += th;

    sprintf(buf, "(%+06.0f,%+06.0f)(%+06.0f,%+06.0f)",
            minX, minY, maxX, maxY);
    UI_TextOutEx2(buf, x, y, UI_Color(UIC_TEXT), 1, DTF_ALIGN_LEFT|DTF_ONLY_SHADOW);
    y += th;
}

static void drawBlockInfoBox(uint vBlock[2])
{
    int                 lineCount = -1, moCount = -1, poCount = -1;
    long                blockIdx = -1;
    bmap_t*             bmap = (bmap_t*) BlockMap;
    bmapblock_t*        block;

    block = M_GridmapGetBlock(bmap->gridmap, vBlock[VX], vBlock[VY], false);
    if(block)
    {
        blockIdx = vBlock[VY] * bmap->dimensions[VY] + vBlock[VX];

        // Count the number of lines linked to this block.
        lineCount = 0;
        if(block->lineDefs)
        {
            linedef_t** iter = block->lineDefs;
            while(*iter)
            {
                lineCount++;
                iter++;
            }
        }

        // Count the number of mobjs linked to this block.
        moCount = 0;
        if(block->mobjLinks)
        {
            linkmobj_t* link = block->mobjLinks;
            while(link)
            {
                if(link->mobj)
                    moCount++;
                link = link->next;
            }
        }

        // Count the number of polyobjs linked to this block.
        poCount = 0;
        if(block->polyLinks)
        {
            linkpolyobj_t* link = block->polyLinks;
            while(link)
            {
                if(link->polyobj)
                    poCount++;
                link = link->next;
            }
        }
    }

    drawInfoBox(theWindow->width / 2, 30,
                blockIdx, vBlock[VX], vBlock[VY], lineCount, moCount, poCount);
}

/**
 * Draw the blockmap in 2D HUD mode.
 */
static void blockmapDebug(blockmap_t* blockmap, mobj_t* followMobj,
                          void (*func) (void* blockPtr, void* param,
                                        float r, float g, float b, float a))
{
    bmap_t*             bmap = (bmap_t*) blockmap;
    void*               block;
    uint                x, y, vBlock[2], vBlockBox[4];
    float               scale, radius;
    vec2_t              start, end, box[2];

    scale = bmapDebugSize / MAX_OF(theWindow->height / 100, 1);

    if(followMobj)
    {   // Determine the mobj's block.
        if(!P_ToBlockmapBlockIdx(blockmap, vBlock, followMobj->pos))
            followMobj = NULL; // The target is outside the blockmap.
    }

    if(followMobj)
    {
        // Determine the mobj's collision blockbox.
        radius = followMobj->radius + DDMOBJ_RADIUS_MAX * 2;
        V2_Set(start, followMobj->pos[VX] - radius, followMobj->pos[VY] - radius);
        V2_Set(end,   followMobj->pos[VX] + radius, followMobj->pos[VY] + radius);
        V2_InitBox(box, start);
        V2_AddToBox(box, end);
        P_BoxToBlockmapBlocks(blockmap, vBlockBox, box);
    }

    // Go into screen projection mode.
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    glTranslatef((theWindow->width / 2), (theWindow->height / 2), 0);
    glScalef(scale, -scale, 1);

    if(followMobj)
    {   // Offset relatively to center on the location of the mobj.
        V2_Set(start,
               (vBlock[VX] * bmap->blockSize[VX]),
               (vBlock[VY] * bmap->blockSize[VY]));
        glTranslatef(-start[VX], -start[VY], 0);
    }
    else
    {   // Offset to center the blockmap on the screen.
        glTranslatef(-(bmap->blockSize[VX] * bmap->dimensions[VX] / 2),
                     -(bmap->blockSize[VY] * bmap->dimensions[VY] / 2), 0);
    }

    // Draw a background.
    V2_Set(start, 0, 0);
    V2_Set(end, bmap->blockSize[VX] * bmap->dimensions[VX],
                bmap->blockSize[VY] * bmap->dimensions[VY]);

    glColor4f(.25f, .25f, .25f, .66f);
    glBegin(GL_QUADS);
        glVertex2f(start[VX], start[VY]);
        glVertex2f(end[VX], start[VY]);
        glVertex2f(end[VX], end[VY]);
        glVertex2f(start[VX], end[VY]);
    glEnd();

    /**
     * Draw the blocks.
     */

    for(y = 0; y < bmap->dimensions[VY]; ++y)
        for(x = 0; x < bmap->dimensions[VX]; ++x)
        {
            boolean             draw = false;
            bmapblock_t*        block =
                M_GridmapGetBlock(bmap->gridmap, x, y, false);

            if(followMobj)
            {
                if(x == vBlock[VX] && y == vBlock[VY])
                {   // The block the viewPlayer is in.
                    glColor4f(.66f, .66f, 1, .66f);
                    draw = true;
                }
                else if(x >= vBlockBox[BOXLEFT]   && x <= vBlockBox[BOXRIGHT] &&
                        y >= vBlockBox[BOXBOTTOM] && y <= vBlockBox[BOXTOP])
                {   // In the viewPlayer's extended collision range.
                    glColor4f(.33f, .33f, .66f, .33f);
                    draw = true;
                }
            }

            if(!draw && !block)
            {   // NULL block.
                glColor4f(0, 0, 0, .95f);
                draw = true;
            }

            if(draw)
            {
                V2_Set(start, x * bmap->blockSize[VX],
                              y * bmap->blockSize[VY]);
                V2_Set(end, bmap->blockSize[VX],
                            bmap->blockSize[VY]);
                V2_Sum(end, end, start);

                glBegin(GL_QUADS);
                    glVertex2f(start[VX], start[VY]);
                    glVertex2f(end[VX], start[VY]);
                    glVertex2f(end[VX], end[VY]);
                    glVertex2f(start[VX], end[VY]);
                glEnd();
            }
        }

    /**
     * Draw the grid lines
     */

    glColor4f(.5f, .5f, .5f, .125f);

    // Vertical lines:
    glBegin(GL_LINES);
    for(x = 1; x < bmap->dimensions[VX]; ++x)
    {
        glVertex2f(x * bmap->blockSize[VX],  0);
        glVertex2f(x * bmap->blockSize[VX],
                   bmap->blockSize[VY] * bmap->dimensions[VY]);
    }
    glEnd();

    // Horizontal lines
    glBegin(GL_LINES);
    for(y = 1; y < bmap->dimensions[VY]; ++y)
    {
        glVertex2f(0, y * bmap->blockSize[VY]);
        glVertex2f(bmap->blockSize[VX] * bmap->dimensions[VX],
                   y * bmap->blockSize[VY]);
    }
    glEnd();

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
                    func(block, bmap->bBox, .33f, 0, 0, .75f);
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
                    func(block, bmap->bBox, 1, .5f, 0, 1);
                }
            }

        validCount++;

        // Lastly, the block the viewPlayer is in.
        block = M_GridmapGetBlock(bmap->gridmap, vBlock[VX], vBlock[VY], false);
        if(block)
        {
            func(block, bmap->bBox, 1, 1, 0, 1);
        }

        /**
         * Draw the followMobj.
         */

        radius = followMobj->radius;
        V2_Set(start,
               followMobj->pos[VX] - bmap->bBox[0][VX] - radius,
               followMobj->pos[VY] - bmap->bBox[0][VY] - radius);
        V2_Set(end,
               followMobj->pos[VX] - bmap->bBox[0][VX] + radius,
               followMobj->pos[VY] - bmap->bBox[0][VY] + radius);

        glColor4f(0, 1, 0, 1);

        glBegin(GL_QUADS);
            glVertex2f(start[VX], start[VY]);
            glVertex2f(end[VX], start[VY]);
            glVertex2f(end[VX], end[VY]);
            glVertex2f(start[VX], end[VY]);
        glEnd();
    }
    else
    {   // Just draw the lot.
        for(y = 0; y < bmap->dimensions[VY]; ++y)
            for(x = 0; x < bmap->dimensions[VX]; ++x)
            {
                block = M_GridmapGetBlock(bmap->gridmap, x, y, false);
                if(block)
                {
                    func(block, bmap->bBox, .33f, 0, 0, .75f);
                }
            }
    }

    /**
     * Draw the blockmap bounds.
     */

    V2_Set(start, -1, -1);
    V2_Set(end, 1 + bmap->blockSize[VX] * bmap->dimensions[VX],
           1 + bmap->blockSize[VY] * bmap->dimensions[VY]);

    glColor4f(1, .5f, .5f, 1);

    glBegin(GL_LINES);
        glVertex2f(start[VX], start[VY]);
        glVertex2f(end[VX], start[VY]);

        glVertex2f(end[VX], start[VY]);
        glVertex2f(end[VX], end[VY]);

        glVertex2f(end[VX], end[VY]);
        glVertex2f(start[VX], end[VY]);

        glVertex2f(start[VX], end[VY]);
        glVertex2f(start[VX], start[VY]);
    glEnd();

    glPopMatrix();
}

void P_BlockmapDebug(void)
{
    blockmap_t*         blockmap;
    bmap_t*             bmap;
    mobj_t*             followMobj = NULL;
    void              (*func) (void*, void*, float r, float g, float b, float a);

    if(!bmapShowDebug || bmapShowDebug > 3)
        return;

    switch(bmapShowDebug)
    {
    default: // Mobjs.
        if(!BlockMap)
            return;

        blockmap = BlockMap;
        func = rendBlockMobjs;
        break;

    case 2: // Linedefs.
        if(!BlockMap)
            return;

        blockmap = BlockMap;
        func = rendBlockLinedefs;
        break;

    case 3: // Subsectors.
        if(!SSecBlockMap)
            return;

        blockmap = SSecBlockMap;
        func = rendBlockSubsectors;
        break;
    }

    // If possible, we'll tailor what we draw relative to the viewPlayer.
    if(viewPlayer && viewPlayer->shared.mo)
        followMobj = viewPlayer->shared.mo;

    blockmapDebug(blockmap, followMobj, func);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    glEnable(GL_TEXTURE_2D);

    // Draw info about the block mobj is in?
    { uint vBlock[2];
    if(followMobj && bmapShowDebug != 3 && P_ToBlockmapBlockIdx(blockmap, vBlock, followMobj->pos))
        drawBlockInfoBox(vBlock); }

    // Draw info about the blockmap.
    bmap = (bmap_t*) blockmap;
    drawInfoBox2(bmap->bBox[0][VX], bmap->bBox[0][VY],
                 bmap->bBox[1][VX], bmap->bBox[1][VY],
                 bmap->blockSize[VX], bmap->blockSize[VY],
                 bmap->dimensions[VX], bmap->dimensions[VY]);

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
