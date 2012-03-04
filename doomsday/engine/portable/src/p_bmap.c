/**\file p_bmap.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct blockmap_ringnode_s {
    void* object;
    struct blockmap_ringnode_s* prev;
    struct blockmap_ringnode_s* next;
} BlockmapRingNode;

typedef struct {
    BlockmapRingNode* ringNodes;
} BlockmapCell;

struct blockmap_s
{
    /// Minimal and Maximal points in map space coordinates.
    AABoxf bounds;

    /// Cell dimensions in map space coordinates.
    vec2_t cellSize;

    /// Gridmap which implements the blockmap itself.
    Gridmap* gridmap;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

uint Blockmap_CellX(Blockmap* bm, float x)
{
    uint result;
    assert(bm);
    Blockmap_ClipCellX(bm, &result, x);
    return result;
}

uint Blockmap_CellY(Blockmap* bm, float y)
{
    uint result;
    assert(bm);
    Blockmap_ClipCellY(bm, &result, y);
    return result;
}

boolean Blockmap_ClipCellX(Blockmap* bm, uint* outX, float x)
{
    boolean adjusted = false;
    assert(bm);
    if(x < bm->bounds.minX)
    {
        x = bm->bounds.minX;
        adjusted = true;
    }
    else if(x >= bm->bounds.maxX)
    {
        x = bm->bounds.maxX - 1;
        adjusted = true;
    }
    if(outX)
    {
        *outX = (uint)((x - bm->bounds.minX) / bm->cellSize[VX]);
    }
    return adjusted;
}

boolean Blockmap_ClipCellY(Blockmap* bm, uint* outY, float y)
{
    boolean adjusted = false;
    assert(bm);
    if(y < bm->bounds.minY)
    {
        y = bm->bounds.minY;
        adjusted = true;
    }
    else if(y >= bm->bounds.maxY)
    {
        y = bm->bounds.maxY - 1;
        adjusted = true;
    }
    if(outY)
    {
        *outY = (uint)((y - bm->bounds.minY) / bm->cellSize[VY]);
    }
    return adjusted;
}

boolean Blockmap_CellCoords(Blockmap* bm, uint coords[2], float const pos[2])
{
    assert(bm);
    if(coords && pos)
    {
        // Deliberate bitwise OR - we need to clip both X and Y.
        return Blockmap_ClipCellX(bm, &coords[0], pos[VX]) |
               Blockmap_ClipCellY(bm, &coords[1], pos[VY]);
    }
    return false;
}

boolean Blockmap_CellBlockCoords(Blockmap* bm, GridmapBlock* blockCoords, const AABoxf* box)
{
    assert(bm);
    if(blockCoords && box)
    {
        // Deliberate bitwise OR - we need to clip both Min and Max.
        return Blockmap_CellCoords(bm, blockCoords->min, box->min) |
               Blockmap_CellCoords(bm, blockCoords->max, box->max);
    }
    return false;
}

Blockmap* Blockmap_New(const pvec2_t min, const pvec2_t max, uint cellWidth, uint cellHeight)
{
    Blockmap* bm = Z_Calloc(sizeof *bm, PU_MAPSTATIC, 0);
    uint width, height;
    if(!bm) Con_Error("Blockmap::New: Failed on allocation of %lu bytes for new Blockmap.", (unsigned long) sizeof *bm);

    V2_Copy(bm->bounds.min, min);
    V2_Copy(bm->bounds.max, max);
    bm->cellSize[VX] = cellWidth;
    bm->cellSize[VY] = cellHeight;

    width  = (uint)ceil((max[0] - min[0]) / (float)cellWidth);
    height = (uint)ceil((max[1] - min[1]) / (float)cellHeight);
    bm->gridmap = Gridmap_New(width, height, sizeof(BlockmapCell), PU_MAPSTATIC);

    VERBOSE( Con_Message("Blockmap::New: Width:%u Height:%u\n", width, height) )
    return bm;
}

const pvec2_t Blockmap_Origin(Blockmap* bm)
{
    assert(bm);
    return bm->bounds.min;
}

const AABoxf* Blockmap_Bounds(Blockmap* bm)
{
    assert(bm);
    return &bm->bounds;
}

uint Blockmap_Width(Blockmap* bm)
{
    assert(bm);
    return Gridmap_Width(bm->gridmap);
}

uint Blockmap_Height(Blockmap* bm)
{
    assert(bm);
    return Gridmap_Height(bm->gridmap);
}

void Blockmap_Size(Blockmap* bm, uint v[2])
{
    assert(bm);
    Gridmap_Size(bm->gridmap, v);
}

float Blockmap_CellWidth(Blockmap* bm)
{
    assert(bm);
    return bm->cellSize[VX];
}

float Blockmap_CellHeight(Blockmap* bm)
{
    assert(bm);
    return bm->cellSize[VY];
}

const pvec2_t Blockmap_CellSize(Blockmap* bm)
{
    assert(bm);
    return bm->cellSize;
}

static void linkObjectToRing(void* object, BlockmapRingNode** link)
{
    BlockmapRingNode* tempLink;
    assert(object && link);

    if(!(*link))
    {
        // Create a new link at the current cell.
        *link = Z_Malloc(sizeof(BlockmapRingNode), PU_MAP, 0);
        (*link)->next = NULL;
        (*link)->prev = NULL;
        (*link)->object = object;
        return;
    }

    tempLink = *link;
    while(tempLink->next && tempLink->object)
    {
        tempLink = tempLink->next;
    }

    if(!tempLink->object)
    {
        tempLink->object = object;
        return;
    }

    tempLink->next = Z_Malloc(sizeof(BlockmapRingNode), PU_MAP, 0);
    tempLink->next->next = NULL;
    tempLink->next->prev = tempLink;
    tempLink->next->object = object;
}

/**
 * Unlink the given object from the specified cell ring (if indeed linked).
 *
 * @param object  Object to be unlinked.
 * @return  @c true iff the object was linked to the ring and was unlinked.
 */
static boolean unlinkObjectFromRing(void* object, BlockmapRingNode** list)
{
    BlockmapRingNode* iter;
    assert(object && list);

    iter = *list;
    while(iter && iter->object != object)
    {
        iter = iter->next;
    }

    if(iter)
    {
        iter->object = NULL;
        return true; // Object was unlinked.
    }
    return false; // object was not linked.
}

static int unlinkObjectInCell(void* ptr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*) ptr;
    unlinkObjectFromRing(paramaters/*object ptr*/, &cell->ringNodes);
    return false; // Continue iteration.
}

static int linkObjectInCell(void* ptr, void* paramaters)
{
    BlockmapCell* cell = (BlockmapCell*) ptr;
    linkObjectToRing(paramaters/*object ptr*/, &cell->ringNodes);
    return false; // Continue iteration.
}

boolean Blockmap_CreateCellAndLinkObjectXY(Blockmap* blockmap, uint x, uint y, void* object)
{
    BlockmapCell* cell;
    assert(blockmap && object);
    cell = (BlockmapCell*) Gridmap_CellXY(blockmap->gridmap, x, y, true);
    if(!cell) return false; // Outside the blockmap?
    linkObjectInCell((void*)cell, object);
    return true; // Link added.
}

boolean Blockmap_CreateCellAndLinkObject(Blockmap* blockmap, uint coords[2], void* object)
{
    assert(coords);
    return Blockmap_CreateCellAndLinkObjectXY(blockmap, coords[VX], coords[VY], object);
}

boolean Blockmap_UnlinkObjectInCell(Blockmap* blockmap, uint coords[2], void* object)
{
    boolean unlinked = false;
    BlockmapCell* cell;
    assert(blockmap);

    cell = (BlockmapCell*) Gridmap_Cell(blockmap->gridmap, coords, false);
    if(cell)
    {
        unlinked = unlinkObjectInCell((void*)cell, (void*)object);
    }
    return unlinked;
}

boolean Blockmap_UnlinkObjectInCellXY(Blockmap* blockmap, uint x, uint y, void* object)
{
    uint coords[2];
    coords[0] = x;
    coords[1] = y;
    return Blockmap_UnlinkObjectInCell(blockmap, coords, object);
}

void Blockmap_UnlinkObjectInCellBlock(Blockmap* blockmap, GridmapBlock* blockCoords, void* object)
{
    assert(blockmap);
    if(!blockCoords) return;

    Gridmap_BlockIterate2(blockmap->gridmap, blockCoords, unlinkObjectInCell, object);
}

int BlockmapCell_IterateObjects(BlockmapCell* cell,
    int (*callback) (void* object, void* context), void* context)
{
    BlockmapRingNode* next, *link;
    assert(cell);

    link = cell->ringNodes;
    while(link)
    {
        next = link->next;

        if(link->object)
        {
            int result = callback(link->object, context);
            if(result) return result; // Stop iteration.
        }

        link = next;
    }
    return false; // Continue iteration.
}

int Blockmap_IterateCellObjects(Blockmap* blockmap, const uint coords[2],
    int (*callback) (void* object, void* context), void* context)
{
    BlockmapCell* cell;
    assert(blockmap);

    cell = Gridmap_Cell(blockmap->gridmap, coords, false);
    if(cell)
    {
        return BlockmapCell_IterateObjects(cell, callback, context);
    }
    return false; // Continue iteration.
}

typedef struct {
    int (*callback)(void* userData, void* context);
    void* context;
} cellobjectiterator_params_t;

static int cellObjectIterator(void* userData, void* context)
{
    BlockmapCell* cell = (BlockmapCell*)userData;
    cellobjectiterator_params_t* args = (cellobjectiterator_params_t*)context;
    assert(args);

    return BlockmapCell_IterateObjects(cell, args->callback, args->context);
}

int Blockmap_IterateCellBlockObjects(Blockmap* blockmap, const GridmapBlock* blockCoords,
    int (*callback) (void* object, void* context), void* context)
{
    cellobjectiterator_params_t args;
    assert(blockmap);

    args.callback = callback;
    args.context = context;

    return Gridmap_BlockIterate2(blockmap->gridmap, blockCoords, cellObjectIterator, (void*)&args);
}
