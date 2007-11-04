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

/*
 * p_bmap.c: Blockmaps
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_misc.h"

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

typedef struct ssecblockmap_s {
    vec2_t      origin;
    vec2_t      blockSize;
    int         width, height; // In blocks.
    gridmap_t  *gridmap;
} ssecblockmap_t;

typedef struct bmapblock_s {
    line_t    **lines;
} bmapblock_t;

typedef struct bmap_s {
    fixed_t     origin[2];              // origin of block map
    uint        width, height;          // in mapblocks
    gridmap_t  *gridmap;
} bmap_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static ssecblockmap_t *SSecBlockMap;

// CODE --------------------------------------------------------------------

static __inline int xToSSecBlockX(ssecblockmap_t *bmap, float x)
{
    if(x >= bmap->origin[VX] && x < bmap->width * bmap->blockSize[VX])
        return (x - bmap->origin[VX]) / bmap->blockSize[VX];

    return -1;
}

static __inline int yToSSecBlockY(ssecblockmap_t *bmap, float y)
{
    if(y >= bmap->origin[VY] && y < bmap->height * bmap->blockSize[VY])
        return (y - bmap->origin[VY]) / bmap->blockSize[VY];

    return -1;
}

static ssecblockmap_t *allocSSecBlockMap(void)
{
    return Z_Malloc(sizeof(ssecblockmap_t), PU_LEVELSTATIC, 0);
}

int setSSecMapBlock(void* p, void* ctx)
{
    ssecmapblock_t *block = (ssecmapblock_t*) p;

    block->ssecs = ctx;
    return true;
}

ssecblockmap_t *P_CreateSSecBlockMap(float originX, float originY,
                                     float blockWidth, float blockHeight,
                                     int width, int height)
{
    ssecblockmap_t *bmap = allocSSecBlockMap();

    V2_Set(bmap->origin, originX, originY);
    V2_Set(bmap->blockSize, blockWidth, blockHeight);
    bmap->width = width;
    bmap->height = height;

    bmap->gridmap =
        M_GridmapCreate(width, height,
                        sizeof(ssecmapblock_t), PU_LEVELSTATIC,
                        setSSecMapBlock);

    VERBOSE(Con_Message
            ("P_CreateSSecBlockMap: bs=%.0f/%.0f w=%i h=%i\n", blockWidth,
             blockHeight, width, height));

    return bmap;
}

void P_BuildSubsectorBlockMap(gamemap_t *map)
{
#define BLOCK_WIDTH         128
#define BLOCK_HEIGHT        128

    uint        startTime = Sys_GetRealTime();

    int         xl, xh, yl, yh, x, y;
    int         subMapWidth, subMapHeight;
    uint        i;
    ssecnode_t *iter, *next;
    ssecmap_t  *bmap, *block;
    vec2_t      bounds[2], point, dims;

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

    // Select a good size for the blocks.
    V2_Subtract(dims, bounds[1], bounds[0]);

    subMapWidth  = ceil(dims[VX] / BLOCK_WIDTH) + 1;
    subMapHeight = ceil(dims[VY] / BLOCK_HEIGHT) + 1;

    SSecBlockMap =
        P_CreateSSecBlockMap(bounds[0][VX], bounds[0][VY],
                             BLOCK_WIDTH, BLOCK_HEIGHT,
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
        xl = xToSSecBlockX(SSecBlockMap, ssec->bbox[0].pos[VX]);
        xh = xToSSecBlockX(SSecBlockMap, ssec->bbox[1].pos[VX]);
        yl = yToSSecBlockY(SSecBlockMap, ssec->bbox[0].pos[VY]);
        yh = yToSSecBlockY(SSecBlockMap, ssec->bbox[1].pos[VY]);

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
                M_GridmapSetBlock(SSecBlockMap->gridmap, x, y, ssecs);
            }
        }

    // Free the temporary link map.
    M_Free(bmap);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("P_BuildSubsectorBlockMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

#undef BLOCK_WIDTH
#undef BLOCK_HEIGHT
}

typedef struct sseciterparams_s {
    arvec2_t    box;
    sector_t   *sector;
    int         localValidCount;
    boolean   (*func) (subsector_t *, void *);
    void       *param;
} sseciterparams_t;

boolean ssecBoxIterator(ssecmapblock_t *block, void *context)
{
    subsector_t **iter;
    sseciterparams_t *args = (sseciterparams_t*) context;

    iter = block->ssecs;
    while(*iter)
    {
        subsector_t *ssec = *iter;

        if(ssec->validCount != args->localValidCount)
        {
            ssec->validCount = args->localValidCount;

            // Check the sector restriction.
            if(!(args->sector && ssec->sector != args->sector) &&
                // Check the bounds.
               !(ssec->bbox[1].pos[VX] < args->box[0][VX] ||
                 ssec->bbox[0].pos[VX] > args->box[1][VX] ||
                 ssec->bbox[1].pos[VY] < args->box[0][VY] ||
                 ssec->bbox[0].pos[VY] > args->box[1][VY]))
            {
                if(!args->func(ssec, args->param))
                    return false;
            }
        }

        *iter++;
    }

    return true;
}

/**
 * Same as the fixed-point version of this routine, but the bounding box
 * is specified using an vec2_t array (see m_vector.c).
 */
boolean P_SubsectorBoxIteratorv(arvec2_t box, sector_t *sector,
                                boolean (*func) (subsector_t *, void *),
                                void *param)
{
    static int  localValidCount = 0;
    sseciterparams_t args;
    int         xl, xh, yl, yh;

    // This is only used here.
    localValidCount++;

    // Blockcoords to check.
    xl = xToSSecBlockX(SSecBlockMap, box[0][VX]);
    xh = xToSSecBlockX(SSecBlockMap, box[1][VX]);
    yl = yToSSecBlockY(SSecBlockMap, box[0][VY]);
    yh = yToSSecBlockY(SSecBlockMap, box[1][VY]);

    args.box = box;
    args.localValidCount = localValidCount;
    args.sector = sector;
    args.func = func;
    args.param = param;

    return M_GridmapBoxIterator(SSecBlockMap->gridmap, xl, xh, yl, yh,
                                ssecBoxIterator, &args);
}

/**
 * @return          @c false, if the iterator func returns @c false.
 */
boolean P_SubsectorBoxIterator(fixed_t *box, sector_t *sector,
                               boolean (*func) (subsector_t *, void *),
                               void *parm)
{
    vec2_t  bounds[2];

    bounds[0][VX] = FIX2FLT(box[BOXLEFT]);
    bounds[0][VY] = FIX2FLT(box[BOXBOTTOM]);
    bounds[1][VX] = FIX2FLT(box[BOXRIGHT]);
    bounds[1][VY] = FIX2FLT(box[BOXTOP]);

    return P_SubsectorBoxIteratorv(bounds, sector, func, parm);
}

static bmap_t *allocBmap(void)
{
    return Z_Calloc(sizeof(bmap_t), PU_LEVELSTATIC, 0);
}

int setBlockmapBlock(void* p, void* ctx)
{
    bmapblock_t *block = (bmapblock_t*) p;

    block->lines = ctx;
    return true;
}

blockmap_t *P_BlockmapCreate(fixed_t originX, fixed_t originY, uint width,
                             uint height)
{
    bmap_t     *bmap = allocBmap();

    bmap->origin[VX] = originX;
    bmap->origin[VY] = originY;
    bmap->width = width;
    bmap->height = height;

    bmap->gridmap =
        M_GridmapCreate(width, height,
                        sizeof(bmapblock_t), PU_LEVELSTATIC,
                        setBlockmapBlock);

    VERBOSE(Con_Message
            ("P_BlockMapCreate: w=%i h=%i\n", width, height));

    return (blockmap_t *) bmap;
}

void P_BlockmapSetBlock(blockmap_t *blockmap, uint x, uint y, line_t **lines)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;
        M_GridmapSetBlock(bmap->gridmap, x, y, lines);
    }
}

void P_GetBlockmapOrigin(blockmap_t *blockmap, fixed_t *v)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;

        v[VX] = bmap->origin[VX];
        v[VY] = bmap->origin[VY];
    }
}

void P_GetBlockmapSize(blockmap_t *blockmap, uint *v)
{
    if(blockmap)
    {
        bmap_t     *bmap = (bmap_t*) blockmap;

        v[VX] = bmap->width;
        v[VY] = bmap->height;
    }
}

typedef struct bmapiterparams_s {
    int         localValidCount;
    boolean   (*func) (line_t *, void *);
    void       *param;
} bmapiterparams_t;

boolean bmapBlockLinesIterator(bmapblock_t *block, void *context)
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

    return true;
}

boolean P_BlockmapLinesIterator(blockmap_t *blockmap, int x, int y,
                                boolean (*func) (line_t *, void *),
                                void *param)
{
    bmap_t     *bmap = (bmap_t*) blockmap;
    int         xl, xh, yl, yh;
    bmapiterparams_t args;

    // Blocks to check.
    xl = xh = x;
    yl = yh = y;

    args.localValidCount = validCount;
    args.func = func;
    args.param = param;

    return M_GridmapBoxIterator(bmap->gridmap, xl, xh, yl, yh,
                                bmapBlockLinesIterator, &args);
}

/**
 * The validCount flags are used to avoid checking lines that are marked
 * in multiple mapblocks, so increment validCount before the first call
 * to P_BlockLinesIterator, then make one or more calls to it.
 */
boolean P_BlockLinesIterator(int x, int y, boolean (*func) (line_t *, void *),
                             void *data)
{
    if(!P_PolyBlockLinesIterator(x, y, func, data))
        return false;

    return P_BlockmapLinesIterator(BlockMap, x, y, func, data);
}

/**
 * Allocates and clears the polyobj blockmap.
 * Normal blockmap must already be initialized when this is called.
 */
void P_InitPolyBlockMap(gamemap_t *map)
{
    size_t      pBMapSize;
    uint        bmapSize[2];

    P_GetBlockmapSize(map->blockmap, bmapSize);

    pBMapSize = bmapSize[VX] * bmapSize[VY] * sizeof(polyblock_t *);

    VERBOSE(Con_Message("P_InitPolyBlockMap: w=%i h=%i\n",
                        bmapSize[VX], bmapSize[VY]));

    map->polyBlockMap = Z_Calloc(pBMapSize, PU_LEVELSTATIC, 0);
}


/**
 * The validCount flags are used to avoid checking polys
 * that are marked in multiple mapblocks, so increment validCount
 * before the first call, then make one or more calls to it.
 */
boolean P_BlockPolyobjsIterator(int x, int y,
                                boolean (*func) (polyobj_t *, void *),
                                void *data)
{
    polyblock_t *polyLink, *polyNext;
    gamemap_t  *map = P_GetCurrentMap();
    uint        bmapSize[2];

    P_GetBlockmapSize(map->blockmap, bmapSize);

    if(x < 0 || y < 0 || x >= bmapSize[VX] || y >= bmapSize[VY])
        return true;

    polyLink = polyblockmap[y * bmapSize[VX] + x];
    while(polyLink)
    {
        polyNext = polyLink->next;
        if(polyLink->polyobj)
        {
            if(polyLink->polyobj->validCount != validCount)
            {
                polyLink->polyobj->validCount = validCount;
                if(!func(polyLink->polyobj, data))
                    return false;
            }
        }
        polyLink = polyNext;
    }
    return true;
}

boolean P_PolyBlockLinesIterator(int x, int y, boolean (*func) (line_t *, void *),
                                 void *data)
{
    uint        i;
    int         offset;
    line_t     *ld;
    polyblock_t *polyLink, *polyNext;
    seg_t     **tempSeg;
    gamemap_t  *map = P_GetCurrentMap();
    uint        bmapSize[2];

    P_GetBlockmapSize(map->blockmap, bmapSize);

    if(x < 0 || y < 0 || x >= bmapSize[VX] || y >= bmapSize[VY])
        return true;

    offset = y * bmapSize[VX] + x;

    polyLink = polyblockmap[offset];
    while(polyLink)
    {
        polyNext = polyLink->next;
        if(polyLink->polyobj)
        {
            if(polyLink->polyobj->validCount != validCount)
            {
                polyLink->polyobj->validCount = validCount;
                tempSeg = polyLink->polyobj->segs;
                for(i = 0; i < polyLink->polyobj->numsegs; ++i, tempSeg++)
                {
                    ld = (*tempSeg)->linedef;
                    if(ld->validCount == validCount)
                        continue;

                    ld->validCount = validCount;

                    if(!func(ld, data))
                        return false;
                }
            }
        }
        polyLink = polyNext;
    }

    return true;
}
