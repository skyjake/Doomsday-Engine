/** @file blockmap.cpp  Map element/object blockmap.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 by id Software, Inc.
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

#include "doomsday/world/blockmap.h"

#include <de/vector.h>
#include <de/legacy/memoryzone.h>
#include <de/legacy/vector1.h>
#include <cmath>
#include <list>

using namespace de;

namespace world {

struct RingNode {
    void *    elem;
    RingNode *prev;
    RingNode *next;
};

struct CellData {
    RingNode *ringNodes;
    dint      elemCount; ///< Total number of linked elements.
    
    bool unlink(void *elem)
    {
        if (RingNode *node = findNode(elem))
        {
            clearElement(*node);
            return true;
        }
        return false;
    }
    
    void unlinkAll()
    {
        for (RingNode *node = ringNodes; node; node = node->next)
        {
            clearElement(*node);
        }
    }
    
    bool link(void *elem)
    {
        addElement(newNode(), elem);
        return true;
    }
    
private:
    RingNode &newNode()
    {
        RingNode *node = nullptr;
        
        if (!ringNodes)
        {
            // Create a new root node.
            node       = reinterpret_cast<RingNode *>(Z_Malloc(sizeof(*node), PU_MAP, nullptr));
            node->next = nullptr;
            node->prev = nullptr;
            node->elem = nullptr;
            
            ringNodes = node;
            
            return *node;
        }
        
        // Is there an available node in the ring we can reuse?
        for (node = ringNodes; node->next && node->elem; node = node->next)
        {
        }
        
        if (!node->elem)
        {
            // This will do nicely.
            return *node;
        }
        
        // Add a new node to the ring.
        node->next       = reinterpret_cast<RingNode *>(Z_Malloc(sizeof(*node), PU_MAP, nullptr));
        node->next->next = nullptr;
        node->next->prev = node;
        node->next->elem = nullptr;
        
        return *node->next;
    }
    
    RingNode *findNode(void *elem)
    {
        if (elem)
        {
            for (RingNode *found = ringNodes; found; found = found->next)
            {
                if (found->elem == elem) return found;
            }
        }
        return nullptr;
    }
    
    void clearElement(RingNode &node)
    {
        if (!node.elem) return;
        
        node.elem = nullptr;
        elemCount--;
    }
    
    void addElement(RingNode &node, void *elem)
    {
        DE_ASSERT(node.elem == nullptr);
        node.elem = elem;
        elemCount++;
    }
};

DE_PIMPL(Blockmap)
{
    /**
     * Quadtree node.
     */
    struct Node
    {
        /// Logical child quadrant identifiers.
        enum Quadrant
        {
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight
        };

        Cell cell;   ///< Cell coordinates for this node.
        duint size;  ///< Size of the cell at this node (width=height).
        union {
            Node *children[4];   /* One per quadrant (if any, not owned). */
            CellData *leafData;  /* Data associated with the leaf cell.
                                    Allocated from the zone; not freed by BlockMap. */
        };

        /**
         * Construct a new node.
         *
         * @param cell  Cell coordinates for the node.
         * @param size  Size of the cell.
         */
        Node(const Cell &cell, duint size)
            : cell(cell)
            , size(size)
        {
            zap(children);
        }

        /**
         * Returns @c true iff the cell is a leaf (i.e., equal to a unit in the
         * gridmap coordinate space).
         */
        inline bool isLeaf() const { return size == 1; }

        /**
         * In which child quadrant is the given @a point?
         */
        Quadrant quadrant(const Cell &point) const
        {
            const duint subSize = size >> 1;
            if(point.x < cell.x + subSize)
            {
                return (point.y < cell.y + subSize)? TopLeft  : BottomLeft;
            }
            else
            {
                return (point.y < cell.y + subSize)? TopRight : BottomRight;
            }
        }
    };
    typedef std::list<Node> Nodes;

    AABoxd bounds;    ///< Map space units.
    duint cellSize;   ///< Map space units.
    Cell dimensions;  ///< Dimensions of the indexed space, in cells.

    Nodes nodes;      ///< Quadtree nodes. The first being the root.

    Impl(Public *i, const AABoxd &bounds, duint cellSize)
        : Base(i)
        , bounds    (bounds)
        , cellSize  (cellSize)
        , dimensions(Vec2ui(de::ceil((bounds.maxX - bounds.minX) / cellSize),
                            de::ceil((bounds.maxY - bounds.minY) / cellSize)))
    {
        // Quadtree must subdivide the space equally into 1x1 unit cells.
        newNode(Cell(0, 0), ceilPow2(de::max(dimensions.x, dimensions.y)));
    }

    inline dint toCellIndex(duint cellX, duint cellY)
    {
        return dint(cellY * dimensions.x + cellX);
    }

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
    duint toCellX(ddouble x, bool &didClip)
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
        return duint((x - bounds.minX) / cellSize);
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
    duint toCellY(ddouble y, bool &didClip)
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
        return uint((y - bounds.minY) / cellSize);
    }

    /// @return @c true, if clipped; otherwise @c false.
    bool clipCell(Cell &cell) const
    {
        bool didClip = false;
        if(cell.x > dimensions.x)
        {
            cell.x = dimensions.x;
            didClip = true;
        }
        if(cell.y > dimensions.y)
        {
            cell.y = dimensions.y;
            didClip = true;
        }
        return didClip;
    }

    /**
     * Clip the cell coordinates in @a block vs the dimensions of this gridmap
     * so that they are inside the boundary this defines.
     *
     * @param block  Block coordinates to be clipped.
     *
     * @return  @c true iff the block coordinates were changed.
     */
    bool clipBlock(CellBlock &block)
    {
        const bool didClipMin = clipCell(block.min);
        const bool didClipMax = clipCell(block.max);
        return didClipMin | didClipMax;
    }

    Node *newNode(const Cell &at, duint size)
    {
        nodes.emplace_back(at, size);
        return &nodes.back();
    }

    Node *findLeaf(Node *node, const Cell &at, bool canSubdivide)
    {
        if(node->isLeaf()) return node;

        // Into which quadrant do we need to descend?
        Node::Quadrant q = node->quadrant(at);

        // Has this quadrant been initialized yet?
        Node **childAdr = &node->children[q];
        if(!*childAdr)
        {
            if(!canSubdivide) return nullptr;

            // Subdivide the space.
            const duint subSize = node->size >> 1;
            switch(q)
            {
            case Node::TopLeft:
                *childAdr = newNode(node->cell, subSize);
                break;

            case Node::TopRight:
                *childAdr = newNode(Cell(node->cell.x + subSize, node->cell.y), subSize);
                break;

            case Node::BottomLeft:
                *childAdr = newNode(Cell(node->cell.x, node->cell.y + subSize), subSize);
                break;
            case Node::BottomRight:
                *childAdr = newNode(Cell(node->cell.x + subSize, node->cell.y + subSize), subSize);
                break;
            }
        }

        return findLeaf(*childAdr, at, canSubdivide);
    }

    inline Node *findLeaf(const Cell &at, bool canCreate = false)
    {
        return findLeaf(&nodes.front(), at, canCreate);
    }

    /**
     * Retrieve the user data associated with the identified cell.
     *
     * @param cell       Cell coordinates to retrieve user data for.
     * @param canCreate  @c true= allocate new data if not already present
     *                   for the referenced cell.
     *
     * @return  User data for the identified cell else @c 0if an invalid
     *          reference or no there is no data present (and not allocating).
     */
    CellData *cellData(const Cell &cell, bool canCreate = false)
    {
        // Outside our boundary?
        if(cell.x >= dimensions.x || cell.y >= dimensions.y)
        {
            return nullptr;
        }

        // Try to locate this leaf (may fail if not present and we are
        // not allocating user data (there will be no corresponding cell)).
        if(Node *node = findLeaf(cell, canCreate))
        {
            // Exisiting user data for this cell?
            if(!node->leafData)
            {
                // Can we allocate new user data?
                if(canCreate)
                {
                    node->leafData = (CellData *)Z_Calloc(sizeof(CellData), PU_MAPSTATIC, nullptr);
                }
            }
            return node->leafData;
        }
        return nullptr;
    }
};

Blockmap::Blockmap(const AABoxd &bounds, duint cellSize)
    : d(new Impl(this, bounds, cellSize))
{}

Blockmap::~Blockmap()
{}

Vec2d Blockmap::origin() const
{
    return Vec2d(d->bounds.min);
}

const AABoxd &Blockmap::bounds() const
{
    return d->bounds;
}

const BlockmapCell &Blockmap::dimensions() const
{
    return d->dimensions;
}

duint Blockmap::cellSize() const
{
    return d->cellSize;
}

dint Blockmap::toCellIndex(duint cellX, duint cellY) const
{
    return d->toCellIndex(cellX, cellY);
}

BlockmapCell Blockmap::toCell(const Vec2d &point, bool *retDidClip) const
{
    bool didClipX, didClipY;
    Cell cell(d->toCellX(point.x, didClipX), d->toCellY(point.y, didClipY));
    if(retDidClip) *retDidClip = didClipX | didClipY;
    return cell;
}

BlockmapCellBlock Blockmap::toCellBlock(const AABoxd &box, bool *retDidClip) const
{
    bool didClipMin, didClipMax;
    CellBlock block(toCell(Vec2d(box.min), &didClipMin), toCell(Vec2d(box.max), &didClipMax));
    block.max += Vec2ui(1, 1); // CellBlock is inclusive-exclusive.
    if(retDidClip) *retDidClip = didClipMin | didClipMax;
    return block;
}

bool Blockmap::link(const Cell &cell, void *elem)
{
    if(!elem) return false; // Huh?

    if(auto *cellData = d->cellData(cell, true /*can create*/))
    {
        return cellData->link(elem);
    }
    return false; // Outside the blockmap?
}

bool Blockmap::link(const AABoxd &region, void *elem)
{
    if(!elem) return false; // Huh?

    bool didLink = false;

    CellBlock cellBlock = toCellBlock(region);
    d->clipBlock(cellBlock);

    Cell cell;
    for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
    {
        if(auto *cellData = d->cellData(cell, true))
        {
            if(cellData->link(elem))
            {
                didLink = true;
            }
        }
    }

    return didLink;
}

bool Blockmap::unlink(const Cell &cell, void *elem)
{
    if(!elem) return false; // Huh?

    if(auto *cellData = d->cellData(cell))
    {
        return cellData->unlink(elem);
    }
    return false;
}

bool Blockmap::unlink(const AABoxd &region, void *elem)
{
    if(!elem) return false; // Huh?

    bool didUnlink = false;

    CellBlock cellBlock = toCellBlock(region);
    d->clipBlock(cellBlock);

    Cell cell;
    for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
    {
        if(auto *cellData = d->cellData(cell))
        {
            if(cellData->unlink(elem))
            {
                didUnlink = true;
            }
        }
    }

    return didUnlink;
}

void Blockmap::unlinkAll()
{
    for (const auto &node : d->nodes)
    {
        // Only leafs with user data.
        if (!node.isLeaf()) continue;

        if (auto *cellData = node.leafData)
        {
            cellData->unlinkAll();
        }
    }
}

dint Blockmap::cellElementCount(const Cell &cell) const
{
    if(auto *cellData = d->cellData(cell))
    {
        return cellData->elemCount;
    }
    return 0;
}

LoopResult Blockmap::forAllInCell(const Cell &cell, std::function<LoopResult (void *object)> func) const
{
    if(auto *cellData = d->cellData(cell))
    {
        RingNode *node = cellData->ringNodes;
        while(node)
        {
            RingNode *next = node->next;
            if(node->elem)
            {
                if(auto result = func(node->elem)) return result;
            }
            node = next;
        }
    }
    return LoopContinue;
}

LoopResult Blockmap::forAllInBox(const AABoxd &box, std::function<LoopResult (void *object)> func) const
{
    CellBlock cellBlock = toCellBlock(box);
    d->clipBlock(cellBlock);

    Cell cell;
    for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
    {
        if(auto result = forAllInCell(cell, func)) return result;
    }
    return LoopContinue;
}

LoopResult Blockmap::forAllInPath(const Vec2d &from_, const Vec2d &to_,
    std::function<LoopResult (void *object)> func) const
{
    // We may need to clip and/or adjust these points.
    Vec2d from = from_;
    Vec2d to   = to_;

    // Abort if the trace originates from outside the blockmap.
    if(!(from.x >= d->bounds.minX && from.x <= d->bounds.maxX &&
         from.y >= d->bounds.minY && from.y <= d->bounds.maxY))
    {
        return LoopContinue;
    }

    // Check the easy case of a trace line completely outside the blockmap.
    if((from.x < d->bounds.minX && to.x < d->bounds.minX) ||
       (from.x > d->bounds.maxX && to.x > d->bounds.maxX) ||
       (from.y < d->bounds.minY && to.y < d->bounds.minY) ||
       (from.y > d->bounds.maxY && to.y > d->bounds.maxY))
    {
        return LoopContinue;
    }

    /*
     * Trace lines should not be perfectly parallel to a blockmap axis. This is
     * because lines in the line blockmap are only linked in the blocks on one
     * side of the axis parallel line. Note that the same logic is applied to all
     * of the blockmaps and not just the line blockmap.
     */
    const ddouble epsilon = FIX2FLT(FRACUNIT);
    const ddouble offset  = FIX2FLT(FRACUNIT);

    const Vec2d delta = (to - origin()) / d->cellSize;
    if(de::fequal(delta.x, 0, epsilon)) to.x += offset;
    if(de::fequal(delta.y, 0, epsilon)) to.y += offset;

    /*
     * It is possible that one or both points are outside the blockmap. Clip the
     * trace so that is wholly within the AABB of the blockmap (note we would have
     * already abandoned if the origin of the trace is outside).
     */
    if(!(to.x >= d->bounds.minX && to.x <= d->bounds.maxX &&
         to.y >= d->bounds.minY && to.y <= d->bounds.maxY))
    {
        // 'to' is outside the blockmap.
        vec2d_t fromV1, toV1, bmapBounds[4], point;
        ddouble ab;

        V2d_Set(fromV1, from.x, from.y);
        V2d_Set(toV1, to.x, to.y);

        V2d_Set(bmapBounds[0], d->bounds.minX, d->bounds.minY);
        V2d_Set(bmapBounds[1], d->bounds.minX, d->bounds.maxY);
        V2d_Set(bmapBounds[2], d->bounds.maxX, d->bounds.maxY);
        V2d_Set(bmapBounds[3], d->bounds.maxX, d->bounds.minY);

        ab = V2d_Intercept(fromV1, toV1, bmapBounds[0], bmapBounds[1], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(toV1, point);

        ab = V2d_Intercept(fromV1, toV1, bmapBounds[1], bmapBounds[2], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(toV1, point);

        ab = V2d_Intercept(fromV1, toV1, bmapBounds[2], bmapBounds[3], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(toV1, point);

        ab = V2d_Intercept(fromV1, toV1, bmapBounds[3], bmapBounds[0], point);
        if(ab >= 0 && ab <= 1)
            V2d_Copy(toV1, point);

        from = Vec2d(fromV1);
        to   = Vec2d(toV1);
    }

    const BlockmapCell originCell = toCell(from);
    const BlockmapCell destCell   = toCell(to);

    // Determine the starting point in blockmap space (preserving the fractional part).
    Vec2d intercept = (from - origin()) / d->cellSize;

    // Determine the step deltas.
    Vec2i cellStep;
    Vec2d interceptStep;
    Vec2d frac;
    if(destCell.x == originCell.x)
    {
        cellStep.x      = 0;
        interceptStep.y = 256;
        frac.y          = 1;
    }
    else if(destCell.x > originCell.x)
    {
        cellStep.x      = 1;
        interceptStep.y = (to.y - from.y) / de::abs(to.x - from.x);
        frac.y          = 1 - (intercept.x - int( intercept.x ));
    }
    else // toCell.x < fromCell.x
    {
        cellStep.x      = -1;
        interceptStep.y = (to.y - from.y) / de::abs(to.x - from.x);
        frac.y          = intercept.x - int( intercept.x );
    }

    if(destCell.y == originCell.y)
    {
        cellStep.y      = 0;
        interceptStep.x = 256;
        frac.x          = 1;
    }
    else if(destCell.y > originCell.y)
    {
        cellStep.y      = 1;
        interceptStep.x = (to.x - from.x) / de::abs(to.y - from.y);
        frac.x          = 1 - (intercept.y - int( intercept.y ));
    }
    else // toCell.y < fromCell.y
    {
        cellStep.y      = -1;
        interceptStep.x = (to.x - from.x) / de::abs(to.y - from.y);
        frac.x          = intercept.y - int( intercept.y );
    }

    intercept += frac * interceptStep;

    // Walk the cells of the blockmap.
    BlockmapCell cell = originCell;
    for(int pass = 0; pass < 64; ++pass) // Prevent a round off error leading us into
                                         // an infinite loop...
    {
        if(auto result = forAllInCell(cell, func))
            return result;

        if(cell == destCell) break;

        if(cell.y == uint( intercept.y ))
        {
            cell.x      += cellStep.x;
            intercept.y += interceptStep.y;
        }
        else if(cell.x == uint( intercept.x ))
        {
            cell.y      += cellStep.y;
            intercept.x += interceptStep.x;
        }
    }

    return LoopContinue;
}

}  // namespace world
