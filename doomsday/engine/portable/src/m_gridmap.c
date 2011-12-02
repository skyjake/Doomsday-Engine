/**\file m_gridmap.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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

#include <math.h>
#include "de_base.h"

#include "m_gridmap.h"

typedef uint64_t blockindex;

struct gridmap_s {
    uint width, height;
    size_t sizeOfBlock;
    int zoneTag;
    void** blockData;
};

static __inline blockindex toBlockIndex(Gridmap* gm, uint x, uint y)
{
    assert(gm);
    if(x < gm->width && y < gm->height)
    {
        return 1/*1-based*/ + (blockindex)y * gm->width + x;
    }
    return 0;
}

static __inline void** blockAdrByIndex(Gridmap* gm, blockindex idx)
{
    assert(gm && gm->blockData);
    if(idx == 0) return NULL;
    return gm->blockData + (idx-1); // 1-based.
}

static void allocBlockDataMap(Gridmap* gm)
{
    blockindex blockCount;
    assert(gm);
    blockCount = (blockindex)gm->width * gm->height;
    gm->blockData = Z_Calloc(blockCount * sizeof *gm->blockData, gm->zoneTag, 0);
}

Gridmap* Gridmap_New(uint width, uint height, size_t blockSize, int zoneTag)
{
    Gridmap* gm = Z_Calloc(sizeof *gm, zoneTag, 0);
    gm->width = width;
    gm->height = height;
    gm->sizeOfBlock = blockSize;
    gm->zoneTag = zoneTag;
    gm->blockData = NULL;
    return gm;
}

void Gridmap_Delete(Gridmap* gm)
{
    assert(gm);
    if(gm->blockData)
    {
        blockindex i, blockCount = (blockindex)gm->width * gm->height;
        for(i = 0; i < blockCount; ++i)
        {
            void** blockAdr = gm->blockData + i;
            // Exisiting user data?
            if(!*blockAdr) continue;

            Z_Free(*blockAdr);
        }
        Z_Free(gm->blockData);
    }
    Z_Free(gm);
}

uint Gridmap_Width(Gridmap* gm)
{
    assert(gm);
    return gm->width;
}

uint Gridmap_Height(Gridmap* gm)
{
    assert(gm);
    return gm->height;
}

void Gridmap_Size(Gridmap* gm, uint widthHeight[2])
{
    assert(gm);
    if(!widthHeight) return;
    widthHeight[0] = gm->width;
    widthHeight[1] = gm->height;
}

void* Gridmap_Block(Gridmap* gm, uint x, uint y, boolean alloc)
{
    void** blockAdr;
    blockindex idx;
    assert(gm);

    if(!gm->blockData && !alloc) return NULL;

    // A valid block reference?
    idx = toBlockIndex(gm, x, y);
    if(!idx) return NULL;

    // Are we yet to allocate the block data map?
    if(!gm->blockData)
    {
        allocBlockDataMap(gm);
    }

    // Exisiting user data for this block?
    blockAdr = blockAdrByIndex(gm, idx);
    if(*blockAdr) return *blockAdr;

    // Allocate new user data?
    if(alloc)
    {
        return *blockAdr = Z_Calloc(gm->sizeOfBlock, gm->zoneTag, 0);
    }
    return NULL;
}

int Gridmap_Iterate(Gridmap* gm, Gridmap_IterateCallback callback,
    void* paramaters)
{
    assert(gm);
    if(gm->blockData)
    {
        blockindex i, blockCount = (blockindex)gm->width * gm->height;
        void** blockAdr;
        int result;
        for(i = 0; i < blockCount; ++i)
        {
            // Exisiting user data?
            blockAdr = gm->blockData + i;
            if(!*blockAdr) continue;

            result = callback(*blockAdr, paramaters);
            if(result) return result;
        }
    }
    return false;
}

int Gridmap_BoxIterate(Gridmap* gm, uint xl, uint xh, uint yl, uint yh,
    Gridmap_IterateCallback callback, void* paramaters)
{
    assert(gm);
    if(gm->blockData)
    {
        blockindex idx;
        void** blockAdr;
        int result;
        uint x, y;

        /// \kludge We shouldn't need clamping here!
        if(xh >= gm->width)
            xh = gm->width -1;
        if(yh >= gm->height)
            yh = gm->height - 1;

        for(y = yl; y <= yh; ++y)
            for(x = xl; x <= xh; ++x)
            {
                // A valid block reference?
                idx = toBlockIndex(gm, x, y);
                if(!idx) continue;

                // Exisiting user data?
                blockAdr = blockAdrByIndex(gm, idx);
                if(!*blockAdr) continue;

                result = callback(*blockAdr, paramaters);
                if(result) return result;
            }
    }
    return false;
}

int Gridmap_BoxIteratev(Gridmap* gm, const uint box[4],
    Gridmap_IterateCallback callback, void* paramaters)
{
    return Gridmap_BoxIterate(gm, box[BOXLEFT], box[BOXRIGHT],
        box[BOXBOTTOM], box[BOXTOP], callback, paramaters);
}
