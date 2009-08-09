/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009 Daniel Swanson <danij@dengine.net>
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
 * m_gridmap.c: Generalized blockmap
 */

// HEADER FILES ------------------------------------------------------------

#include "math.h"

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct gridblock_s {
    void       *data;
} gridblock_t;

typedef struct gmap_s {
    uint        width, height;
    size_t      sizeOfBlock;
    int         memzoneTag;
    gridblock_t *blocks;
} gmap_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static gmap_t *allocGridmap(int tag)
{
    return (gmap_t*) Z_Calloc(sizeof(gmap_t), tag, 0);
}

static void freeGridmap(gmap_t *gmap)
{
    Z_Free(gmap->blocks);
    Z_Free(gmap);
}

static gridblock_t *getBlock(gmap_t *gmap, uint x, uint y)
{
    if(x < gmap->width && y < gmap->height)
    {
        return &gmap->blocks[y * gmap->width + x];
    }

    return NULL;
}

/**
 * Create a new gridmap.
 *
 * @param width         X dimension of the grid.
 * @param height        Y dimension of the grid.
 * @param sizeOfElement Amount of memory to be allocated for each element.
 */
gridmap_t *M_GridmapCreate(uint width, uint height, size_t sizeOfElement,
                           int memzoneTag)
{
    gmap_t     *gmap = allocGridmap(memzoneTag);

    gmap->width = width;
    gmap->height = height;
    gmap->sizeOfBlock = sizeOfElement;
    gmap->memzoneTag = memzoneTag;

    gmap->blocks = (gridblock_t*)
        Z_Calloc(gmap->width * gmap->height * sizeof(gridblock_t),
                 gmap->memzoneTag, 0);

    return (gridmap_t*) gmap;
}

/**
 * Destroy a gridmap.
 *
 * @param gridmap       The gridmap to be destroyed.
 */
void M_GridmapDestroy(gridmap_t *gridmap)
{
    if(gridmap)
    {
        uint        i, numblocks;
        gmap_t     *gmap = (gmap_t*) gridmap;

        numblocks = gmap->width * gmap->height;
        for(i = 0; i < numblocks; ++i)
        {
            gridblock_t *block = &gmap->blocks[i];

            if(block->data)
                Z_Free(block->data);
        }

        freeGridmap(gmap);
    }
}

void *M_GridmapGetBlock(gridmap_t *gridmap, uint x, uint y, boolean alloc)
{
    if(gridmap)
    {
        gmap_t     *gmap = (gmap_t*) gridmap;
        gridblock_t *block = getBlock(gmap, x, y);

        if(block)
        {
            if(alloc)
            {   // Allocator mode.
                // Have we allocated memory for this block yet?
                if(!block->data)
                {
                    block->data =
                        Z_Calloc(gmap->sizeOfBlock, gmap->memzoneTag, 0);
                }

                return block->data;
            }
            else
            {   // Look up mode.
                if(block->data)
                    return block->data;

                return NULL;
            }
        }
    }

    return NULL;
}

/**
 * Iterate all the blocks of the gridmap, calling func for each.
 *
 * @param gridmap       The gridmap being iterated.
 * @param callback      The callback to be made for each block.
 * @param param         Miscellaneous data to be passed in the callback.
 *
 * @return              @c true, iff all callbacks return @c true;
 */
boolean M_GridmapIterator(gridmap_t *gridmap,
                          boolean (*callback) (void* p, void* ctx),
                          void* param)
{
    if(gridmap)
    {
        gmap_t     *gmap = (gmap_t*) gridmap;
        uint        x, y;

        for(x = 0; x <= gmap->width; ++x)
        {
            for(y = 0; y <= gmap->height; ++y)
            {
                gridblock_t *block = getBlock(gmap, x, y);

                if(block && block->data)
                {
                    if(!callback(block->data, param))
                        return false;
                }
            }
        }
        return true;
    }

    return false;
}

/**
 * Iterate a subset of the blocks of the gridmap and calling func for each.
 *
 * @param gridmap       The gridmap being iterated.
 * @param xl            Min X
 * @param xh            Max X
 * @param yl            Min Y
 * @param yh            Max Y
 * @param callback      The callback to be made for each block.
 * @param param         Miscellaneous data to be passed in the callback.
 *
 * @return              @c true, iff all callbacks return @c true;
 */
boolean M_GridmapBoxIterator(gridmap_t *gridmap,
                             uint xl, uint xh, uint yl, uint yh,
                             boolean (*callback) (void* p, void* ctx),
                             void* param)
{
    if(gridmap)
    {
        gmap_t     *gmap = (gmap_t*) gridmap;
        uint        x, y;

        // Kludge: We shouldn't need clamping here!
        if(xh >= gmap->width)
            xh = gmap->width -1;
        if(yh >= gmap->height)
            yh = gmap->height - 1;

        for(x = xl; x <= xh; ++x)
        {
            for(y = yl; y <= yh; ++y)
            {
                gridblock_t *block = getBlock(gmap, x, y);

                if(block && block->data)
                {
                    if(!callback(block->data, param))
                        return false;
                }
            }
        }
        return true;
    }

    return false;
}

boolean M_GridmapBoxIteratorv(gridmap_t *gridmap, const uint box[4],
                              boolean (*callback) (void* p, void* ctx),
                              void* param)
{
    return M_GridmapBoxIterator(gridmap, box[BOXLEFT], box[BOXRIGHT],
                                box[BOXBOTTOM], box[BOXTOP], callback,
                                param);
}
