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

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h" // For debug visual.
#include "de_render.h" // For debug visual.

#include "world/blockmap.h"

namespace de {

struct RingNode
{
    void *elem;
    RingNode *prev;
    RingNode *next;
};

struct CellData
{
    RingNode *ringNodes;
    int elemCount; ///< Total number of linked elements.

    bool unlink(void *elem)
    {
        if(RingNode *node = findNode(elem))
        {
            clearElement(*node);
            return true;
        }
        return false;
    }

    void unlinkAll()
    {
        for(RingNode *node = ringNodes; node; node = node->next)
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
        RingNode *node = 0;

        if(!ringNodes)
        {
            // Create a new root node.
            node = (RingNode *) Z_Malloc(sizeof(*node), PU_MAP, 0);
            node->next = 0;
            node->prev = 0;
            node->elem = 0;
            ringNodes = node;
            return *node;
        }

        // Is there an available node in the ring we can reuse?
        for(node = ringNodes; node->next && node->elem; node = node->next)
        {}

        if(!node->elem)
        {
            // This will do nicely.
            return *node;
        }

        // Add a new node to the ring.
        node->next = (RingNode *) Z_Malloc(sizeof(*node), PU_MAP, 0);
        node->next->next = 0;
        node->next->prev = node;
        node->next->elem = 0;
        return *node->next;
    }

    RingNode *findNode(void *elem)
    {
        if(!elem) return 0;

        for(RingNode *found = ringNodes; found; found = found->next)
        {
            if(found->elem == elem) return found;
        }
        return 0;
    }

    void clearElement(RingNode &node)
    {
        if(!node.elem) return;

        node.elem = 0;
        elemCount--;
    }

    void addElement(RingNode &node, void *elem)
    {
        DENG2_ASSERT(node.elem == 0);
        node.elem = elem;
        elemCount++;
    }
};

template <typename Type>
Type ceilPow2(Type unit)
{
    Type cumul;
    for(cumul = 1; unit > cumul; cumul <<= 1) {}
    return cumul;
}

DENG2_PIMPL(Blockmap)
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

        Cell cell; ///< Cell coordinates for this node.
        uint size; ///< Size of the cell at this node (width=height).
        union {
            Node *children[4]; ///< One per quadrant (if any, not owned).
            CellData *leafData; ///< Data associated with the leaf cell.
        };

        /**
         * Construct a new node.
         *
         * @param cell  Cell coordinates for the node.
         * @param size  Size of the cell.
         */
        Node(Cell const &cell, uint size) : cell(cell), size(size)
        {
            zap(children);
        }

        ~Node()
        {
            if(isLeaf())
            {
                Z_Free(leafData);
            }
        }

        /**
         * Returns @c true iff the cell is a leaf (i.e., equal to a unit in the
         * gridmap coordinate space).
         */
        inline bool isLeaf() const { return size == 1; }

        /**
         * In which child quadrant is the given @a point?
         */
        Quadrant quadrant(Cell const &point) const
        {
            uint const subSize = size >> 1;
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
    typedef QList<Node> Nodes;

    AABoxd bounds;    ///< Map space units.
    uint cellSize;    ///< Map space units.
    Cell dimensions;  ///< Dimensions of the indexed space, in cells.

    Nodes nodes;      ///< Quadtree nodes. The first being the root.

    Instance(Public *i, AABoxd const &bounds, uint cellSize)
        : Base(i),
          bounds(bounds),
          cellSize(cellSize),
          dimensions(Vector2ui(de::ceil((bounds.maxX - bounds.minX) / cellSize),
                               de::ceil((bounds.maxY - bounds.minY) / cellSize)))
    {
        // Quadtree must subdivide the space equally into 1x1 unit cells.
        newNode(Cell(0, 0), ceilPow2(de::max(dimensions.x, dimensions.y)));
    }

    inline int toCellIndex(uint cellX, uint cellY)
    {
        return int(cellY * dimensions.x + cellX);
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
    uint toCellX(coord_t x, bool &didClip)
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
        return uint((x - bounds.minX) / cellSize);
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
    uint toCellY(coord_t y, bool &didClip)
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

    void clipCell(Cell &cell, bool &didClip)
    {
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
        bool didClipMin, didClipMax;
        clipCell(block.min, didClipMin);
        clipCell(block.max, didClipMax);
        return didClipMin | didClipMax;
    }

    Node *newNode(Cell const &at, uint size)
    {
        nodes.append(Node(at, size));
        return &nodes.last();
    }

    Node *findLeaf(Node *node, Cell const &at, bool canSubdivide)
    {
        if(node->isLeaf()) return node;

        // Into which quadrant do we need to descend?
        Node::Quadrant q = node->quadrant(at);

        // Has this quadrant been initialized yet?
        Node **childAdr = &node->children[q];
        if(!*childAdr)
        {
            if(!canSubdivide) return 0;

            // Subdivide the space.
            uint const subSize = node->size >> 1;
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

    inline Node *findLeaf(Cell const &at, bool canCreate = false)
    {
        return findLeaf(&nodes.first(), at, canCreate);
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
    CellData *cellData(Cell const &cell, bool canCreate = false)
    {
        // Outside our boundary?
        if(cell.x >= dimensions.x || cell.y >= dimensions.y)
        {
            return 0;
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
                    node->leafData = (CellData *)Z_Calloc(sizeof(CellData), PU_MAPSTATIC, 0);
                }
            }
            return node->leafData;
        }
        return 0;
    }
};

Blockmap::Blockmap(AABoxd const &bounds, uint cellSize)
    : d(new Instance(this, bounds, cellSize))
{}

Blockmap::~Blockmap()
{}

Vector2d Blockmap::origin() const
{
    return Vector2d(d->bounds.min);
}

AABoxd const &Blockmap::bounds() const
{
    return d->bounds;
}

BlockmapCell const &Blockmap::dimensions() const
{
    return d->dimensions;
}

uint Blockmap::cellSize() const
{
    return d->cellSize;
}

int Blockmap::toCellIndex(uint cellX, uint cellY) const
{
    return d->toCellIndex(cellX, cellY);
}

BlockmapCell Blockmap::toCell(Vector2d const &point, bool *retDidClip) const
{
    bool didClipX, didClipY;
    Cell cell(d->toCellX(point.x, didClipX), d->toCellY(point.y, didClipY));
    if(retDidClip) *retDidClip = didClipX | didClipY;
    return cell;
}

BlockmapCellBlock Blockmap::toCellBlock(AABoxd const &box, bool *retDidClip) const
{
    bool didClipMin, didClipMax;
    CellBlock block(toCell(box.min, &didClipMin), toCell(box.max, &didClipMax));
    block.max += Vector2ui(1, 1); // CellBlock is inclusive-exclusive.
    if(retDidClip) *retDidClip = didClipMin | didClipMax;
    return block;
}

bool Blockmap::link(Cell const &cell, void *elem)
{
    if(!elem) return false; // Huh?

    if(CellData *cellData = d->cellData(cell, true /*can create*/))
    {
        return cellData->link(elem);
    }
    return false; // Outside the blockmap?
}

bool Blockmap::link(CellBlock const &cellBlock_, void *elem)
{
    if(!elem) return false; // Huh?

    bool didLink = false;

    CellBlock cellBlock = cellBlock_;
    d->clipBlock(cellBlock);

    Cell cell;
    for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
    {
        Instance::Node *node = d->findLeaf(cell);
        if(!node) continue;

        if(CellData *cellData = node->leafData)
        {
            if(cellData->link(elem))
            {
                didLink = true;
            }
        }
    }

    return didLink;
}

bool Blockmap::unlink(Cell const &cell, void *elem)
{
    if(!elem) return false; // Huh?

    if(CellData *cellData = d->cellData(cell))
    {
        return cellData->unlink(elem);
    }
    return false;
}

bool Blockmap::unlink(CellBlock const &cellBlock_, void *elem)
{
    if(!elem) return false; // Huh?

    bool didUnlink = false;

    CellBlock cellBlock = cellBlock_;
    d->clipBlock(cellBlock);

    Cell cell;
    for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
    {
        Instance::Node *node = d->findLeaf(cell);
        if(!node) continue;

        if(CellData *cellData = node->leafData)
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
    foreach(Instance::Node const &node, d->nodes)
    {
        // Only leafs with user data.
        if(!node.isLeaf()) continue;

        if(CellData *cellData = node.leafData)
        {
            cellData->unlinkAll();
        }
    }
}

int Blockmap::cellElementCount(Cell const &cell) const
{
    if(CellData *cellData = d->cellData(cell))
    {
        return cellData->elemCount;
    }
    return 0;
}

int Blockmap::iterate(Cell const &cell, int (*callback) (void *, void *),
                      void *context) const
{
    if(!callback) return false; // Huh?

    if(CellData *cellData = d->cellData(cell))
    {
        RingNode *node = cellData->ringNodes;
        while(node)
        {
            RingNode *next = node->next;

            if(node->elem)
            {
                if(int result = callback(node->elem, context))
                    return result; // Stop iteration.
            }

            node = next;
        }
    }
    return false; // Continue iteration.
}

int Blockmap::iterate(CellBlock const &cellBlock_, int (*callback) (void *, void *),
                      void *context) const
{
    if(!callback) return false; // Huh?

    CellBlock cellBlock = cellBlock_;
    d->clipBlock(cellBlock);

    Cell cell;
    for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
    {
        if(int result = iterate(cell, callback, context))
            return result;
    }

    return false; // Continue iteration.
}

// Debug visual ----------------------------------------------------------------

#ifdef __CLIENT__

void Blockmap::drawDebugVisual() const
{
#define UNIT_SIZE           1

    // We'll be changing the color, so query the current and restore later.
    GLfloat oldColor[4]; glGetFloatv(GL_CURRENT_COLOR, oldColor);

    /*
     * Draw the Quadtree.
     */
    glColor4f(1.f, 1.f, 1.f, 1.f / d->nodes.first().size);
    foreach(Instance::Node const &node, d->nodes)
    {
        // Only leafs with user data.
        if(!node.isLeaf()) continue;
        if(!node.leafData) continue;

        Vector2f const topLeft     = node.cell * UNIT_SIZE;
        Vector2f const bottomRight = topLeft + Vector2f(UNIT_SIZE, UNIT_SIZE);

        glBegin(GL_LINE_LOOP);
            glVertex2f(topLeft.x,     topLeft.y);
            glVertex2f(bottomRight.x, topLeft.y);
            glVertex2f(bottomRight.x, bottomRight.y);
            glVertex2f(topLeft.x,     bottomRight.y);
        glEnd();
    }

    /*
     * Draw the bounds.
     */
    Vector2f start;
    Vector2f end = start + d->dimensions * UNIT_SIZE;

    glColor3f(1, .5f, .5f);
    glBegin(GL_LINES);
        glVertex2f(start.x, start.y);
        glVertex2f(  end.x, start.y);

        glVertex2f(  end.x, start.y);
        glVertex2f(  end.x,   end.y);

        glVertex2f(  end.x,   end.y);
        glVertex2f(start.x,   end.y);

        glVertex2f(start.x,   end.y);
        glVertex2f(start.x, start.y);
    glEnd();

    // Restore GL state.
    glColor4fv(oldColor);

#undef UNIT_SIZE
}
#endif // __CLIENT__

} // namespace de
