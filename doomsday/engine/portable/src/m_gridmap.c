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
    int         width, height;
    size_t      sizeOfBlock;
    int         memzoneTag;
    gridblock_t *blocks;

    int       (*setBlock)(void* p, void* ctx);
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
    return Z_Calloc(sizeof(gmap_t), tag, 0);
}

static void freeGridmap(gmap_t *gmap)
{
    Z_Free(gmap->blocks);
    Z_Free(gmap);
}

static gridblock_t *getBlock(gmap_t *gmap, int x, int y)
{
    if(x >= 0 && y >= 0 && x < gmap->width && y < gmap->height)
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
gridmap_t *M_GridmapCreate(int width, int height, size_t sizeOfElement,
                           int memzoneTag,
                           int (*setBlock)(void* p, void* ctx))
{
    gmap_t     *gmap = allocGridmap(memzoneTag);

    gmap->width = width;
    gmap->height = height;
    gmap->sizeOfBlock = sizeOfElement;
    gmap->memzoneTag = memzoneTag;

    gmap->blocks =
        Z_Calloc(gmap->width * gmap->height * sizeof(gridblock_t),
                 gmap->memzoneTag, 0);

    gmap->setBlock = setBlock;

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

boolean M_GridmapSetBlock(gridmap_t *gridmap, int x, int y, void *context)
{
    if(gridmap)
    {
        gmap_t     *gmap = (gmap_t*) gridmap;
        gridblock_t *block = getBlock(gmap, x, y);

        if(block)
        {
            // Have we allocated memory for this block yet?
            if(!block->data)
            {
                block->data =
                    Z_Calloc(gmap->sizeOfBlock, gmap->memzoneTag, 0);
            }

            return gmap->setBlock(block->data, context);
        }
    }

    return false;
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
        int         x, y;

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
                             int xl, int xh, int yl, int yh,
                             boolean (*callback) (void* p, void* ctx),
                             void* param)
{
    if(gridmap)
    {
        gmap_t     *gmap = (gmap_t*) gridmap;
        int         x, y;

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
