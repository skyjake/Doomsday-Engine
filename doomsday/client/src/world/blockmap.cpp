/** @file blockmap.cpp World map element blockmap.
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

#include <de/memoryzone.h>

#include <de/Vector>

#include "de_platform.h"
#include "de_console.h"
#include "dd_main.h"
#include "gridmap.h"

#include "world/blockmap.h"

struct RingNode
{
    void *elem;
    RingNode *prev;
    RingNode *next;
};

struct CellData
{
    RingNode *ringNodes;

    /// Running total of the number of elements linked in this cell.
    int elemCount;

    bool link(void *elem)
    {
        RingNode *node = allocNode();
        node->elem = elem;
        elemCount++;
        return true;
    }

    bool unlink(void *elem)
    {
        if(RingNode *node = findNode(elem))
        {
            node->elem = 0;
            elemCount--;
            return true;
        }
        return false;
    }

    int iterate(int (*callback) (void *elem, void *context), void *context)
    {
        RingNode *node = ringNodes;
        while(node)
        {
            RingNode *next = node->next;

            if(node->elem)
            {
                int result = callback(node->elem, context);
                if(result) return result; // Stop iteration.
            }

            node = next;
        }
        return false; // Continue iteration.
    }

private:
    RingNode *findNode(void *elem)
    {
        if(!elem) return 0;
        for(RingNode *found = ringNodes; found; found = found->next)
        {
            if(found->elem == elem) return found;
        }
        return 0;
    }

    RingNode *allocNode()
    {
        RingNode *node = 0;

        if(!ringNodes)
        {
            // Create a new root node.
            node = (RingNode *) Z_Malloc(sizeof(*node), PU_MAP, 0);
            node->next = 0;
            node->prev = 0;
            ringNodes = node;
            return node;
        }

        // Is there an available node in the ring we can reuse?
        for(node = ringNodes; node->next && node->elem; node = node->next)
        {}

        if(!node->elem)
        {
            // This will do nicely.
            return node;
        }

        // Add a new node to the ring.
        node->next = (RingNode *) Z_Malloc(sizeof(*node), PU_MAP, 0);
        node->next->next = 0;
        node->next->prev = node;
        return node->next;
    }
};

namespace de {

DENG2_PIMPL(Blockmap), public Gridmap
{
    /// Minimal and Maximal points in map space coordinates.
    AABoxd bounds;

    /// Cell dimensions in map space coordinates.
    Vector2d cellDimensions;

    Instance(Public *i, AABoxd const &bounds, Vector2ui const &cellDimensions_)
        : Base(i),
          Gridmap(Vector2ui(de::ceil((bounds.maxX - bounds.minX) / cellDimensions_.x),
                            de::ceil((bounds.maxY - bounds.minY) / cellDimensions_.y)),
                  sizeof(CellData), PU_MAPSTATIC),
          bounds(bounds),
          cellDimensions(Vector2d(cellDimensions_.x, cellDimensions_.y))
    {}

    /**
     * Given map space X coordinate @a x, return the corresponding cell coordinate.
     * If @a x is outside the blockmap it will be clamped to the nearest edge on
     * the X axis.
     *
     * @param x        Map space X coordinate to be translated.
     * @param didClip  Set to @c true iff clamping was necessary.
     *
     * @return  Translated blockmap cell X coordinate.
     */
    uint toCellX(coord_t x, bool &didClip) const
    {
        didClip = false;
        if(x < bounds.minX)
        {
            x = bounds.minX;
            didClip = true;
        }
        else if(x >= bounds.maxX)
        {
            x = bounds.maxX - 1;
            didClip = true;
        }
        return uint((x - bounds.minX) / cellDimensions.x);
    }

    /**
     * Given map space Y coordinate @a y, return the corresponding cell coordinate.
     * If @a y is outside the blockmap it will be clamped to the nearest edge on
     * the Y axis.
     *
     * @param y        Map space Y coordinate to be translated.
     * @param didClip  Set to @c true iff clamping was necessary.
     *
     * @return  Translated blockmap cell Y coordinate.
     */
    uint toCellY(coord_t y, bool &didClip) const
    {
        didClip = false;
        if(y < bounds.minY)
        {
            y = bounds.minY;
            didClip = true;
        }
        else if(y >= bounds.maxY)
        {
            y = bounds.maxY - 1;
            didClip = true;
        }
        return uint((y - bounds.minY) / cellDimensions.y);
    }
};

Blockmap::Blockmap(AABoxd const &bounds, Vector2ui const &cellDimensions)
    : d(new Instance(this, bounds, cellDimensions))
{}

Vector2d Blockmap::origin() const
{
    return Vector2d(d->bounds.min);
}

AABoxd const &Blockmap::bounds() const
{
    return d->bounds;
}

Blockmap::Cell const &Blockmap::dimensions() const
{
    return d->dimensions();
}

Vector2d const &Blockmap::cellDimensions() const
{
    return d->cellDimensions;
}

Blockmap::Cell Blockmap::toCell(Vector2d const &point, bool *retDidClip) const
{
    bool didClipX, didClipY;
    Cell cell(d->toCellX(point.x, didClipX), d->toCellY(point.y, didClipY));
    if(retDidClip) *retDidClip = didClipX | didClipY;
    return cell;
}

Blockmap::CellBlock Blockmap::toCellBlock(AABoxd const &box, bool *retDidClip) const
{
    bool didClipMin, didClipMax;
    CellBlock block(toCell(box.min, &didClipMin), toCell(box.max, &didClipMax));
    if(retDidClip) *retDidClip = didClipMin | didClipMax;
    return block;
}

bool Blockmap::link(Cell const &cell, void *elem)
{
    if(!elem) return false; // Huh?
    if(CellData *cellData = (CellData *)d->cellData(cell, true /*can create*/))
    {
        return cellData->link(elem);
    }
    return false; // Outside the blockmap?
}

struct BlockLinkWorkerParams
{
    void *elem;
    bool didLink;
};

static int blockLinkWorker(void *cdPtr, void *parameters)
{
    CellData *cellData = (CellData *) cdPtr;
    BlockLinkWorkerParams *p = (BlockLinkWorkerParams *) parameters;
    if(cellData->link(p->elem))
    {
        p->didLink = true;
    }
    return false; // Continue iteration.
}

bool Blockmap::link(CellBlock const &cellBlock, void *elem)
{
    if(!elem) return false; // Huh?
    BlockLinkWorkerParams parm;
    parm.elem    = elem;
    parm.didLink = false;
    d->iterate(cellBlock, blockLinkWorker, &parm);
    return parm.didLink;
}

bool Blockmap::unlink(Cell const &cell, void *elem)
{
    if(!elem) return false; // Huh?
    if(CellData *cellData = (CellData *)d->cellData(cell))
    {
        return cellData->unlink(elem);
    }
    return false;
}

struct BlockUnlinkWorkerParams
{
    void *elem;
    bool didUnlink;
};

static int blockUnlinkWorker(void *cdPtr, void *parameters)
{
    CellData *cellData = (CellData *) cdPtr;
    BlockUnlinkWorkerParams *p = (BlockUnlinkWorkerParams *) parameters;
    if(cellData->unlink(p->elem))
    {
        p->didUnlink = true;
    }
    return false; // Continue iteration.
}

bool Blockmap::unlink(CellBlock const &cellBlock, void *elem)
{
    if(!elem) return false; // Huh?
    BlockUnlinkWorkerParams parm;
    parm.elem      = elem;
    parm.didUnlink = false;
    d->iterate(cellBlock, blockUnlinkWorker, &parm);
    return parm.didUnlink;
}

int Blockmap::cellElementCount(Cell const &cell) const
{
    if(CellData *cellData = (CellData *)d->cellData(cell))
    {
        return cellData->elemCount;
    }
    return 0;
}

int Blockmap::iterate(Cell const &cell, int (*callback) (void *elem, void *parameters), void *parameters)
{
    if(!callback) return false; // Huh?
    if(CellData *cellData = (CellData *)d->cellData(cell))
    {
        return cellData->iterate(callback, parameters);
    }
    return false; // Continue iteration.
}

struct CellDataIterateWorkerParams
{
    int (*callback)(void *userData, void *parameters);
    void *parameters;
};

static int cellDataIterateWorker(void *cdPtr, void *parameters)
{
    CellData *cellData = (CellData *) cdPtr;
    CellDataIterateWorkerParams *p = (CellDataIterateWorkerParams *) parameters;
    return cellData->iterate(p->callback, p->parameters);
}

int Blockmap::iterate(CellBlock const &cellBlock, int (*callback) (void *elem, void *parameters), void *parameters)
{
    if(!callback) return false; // Huh?
    CellDataIterateWorkerParams parm;
    parm.callback   = callback;
    parm.parameters = parameters;
    return d->iterate(cellBlock, cellDataIterateWorker, &parm);
}

Gridmap const &Blockmap::gridmap() const
{
    return *d;
}

} // namespace de
