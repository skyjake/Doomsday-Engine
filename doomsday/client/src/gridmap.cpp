/** @file gridmap.h Gridmap implementation.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
#include <cstring>

#include <de/memoryzone.h>

#include "de_base.h"
#include "de_graphics.h" // For debug display
#include "de_render.h" // For debug display

#include "gridmap.h"

namespace de {

template <typename Type>
Type ceilPow2(Type unit)
{
    Type cumul;
    for(cumul = 1; unit > cumul; cumul <<= 1) {}
    return cumul;
}

DENG2_PIMPL_NOREF(Gridmap)
{
    /**
     * A node in the internal quadtree.
     */
    struct Node
    {
        /// Quadtree child identifiers (quadrants).
        enum Quadrant
        {
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight
        };

        Cell cell; ///< Cell coordinates for this node.

        uint size; ///< Size of the cell at this node (width=height).

        /// Child nodes, one for each quadrant:
        Node *topLeft;
        Node *topRight;
        Node *bottomLeft;
        Node *bottomRight;

        void *userData; ///< User data associated with the cell.

        /**
         * Construct a new node.
         *
         * @param cell  Cell coordinates for the node.
         * @param size  Size of the cell.
         */
        Node(Cell const &cell, uint size)
            : cell(cell),
              size(size),
              topLeft(0),
              topRight(0),
              bottomLeft(0),
              bottomRight(0),
              userData(0)
        {}

        ~Node() { if(userData) Z_Free(userData); }

        /**
         * Returns @c true iff the cell is a leaf (i.e., equal to a unit in the
         * gridmap coordinate space).
         */
        bool isLeaf() const { return size == 1; }

        /**
         * In which quadrant is the @a point?
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

        Node **getChildAdr(Quadrant quadrant)
        {
            switch(quadrant)
            {
            case TopLeft:     return &topLeft;
            case TopRight:    return &topRight;
            case BottomLeft:  return &bottomLeft;
            case BottomRight: return &bottomRight;
            }
            throw Error("Node", QString("Invalid quadrant %1").arg(quadrant));
        }

        /**
         * Depth-first traversal of the children of this tree, making a callback
         * for each cell. Iteration ends when all selected cells have been visited
         * or a callback returns a non-zero value.
         *
         * @param leafOnly  Caller is only interested in leaves.
         * @param callback  Callback function.
         * @param context   Passed to the callback.
         *
         * @return  Zero iff iteration completed wholly, else the value returned by the
         *          last callback made.
         */
        int iterate(bool leafOnly, int (*callback) (Node &node, void *context),
                    void *context = 0)
        {
            if(!callback) return false;

            if(!isLeaf())
            {
                if(topLeft)
                {
                    if(int result = topLeft->iterate(leafOnly, callback, context))
                        return result;
                }
                if(topRight)
                {
                    if(int result = topRight->iterate(leafOnly, callback, context))
                        return result;
                }
                if(bottomLeft)
                {
                    if(int result = bottomLeft->iterate(leafOnly, callback, context))
                        return result;
                }
                if(bottomRight)
                {
                    if(int result = bottomRight->iterate(leafOnly, callback, context))
                        return result;
                }
            }

            if(!leafOnly || isLeaf())
            {
                return callback(*this, context);
            }
            return false;
        }
    };
    typedef QList<Node> Nodes;

    Cell dimensions;     ///< Dimensions of the indexed space, in cells.
    size_t userDataSize; ///< Amount of storage needed per cell for the user data.
    int userDataZoneTag; ///< Zone memory tag for the user data.

    /// Quadtree root node.
    Node root;
    Nodes nodes;

    Instance(Cell const &dimensions, size_t userDataSize, int userDataZoneTag)
        : dimensions(dimensions),
          userDataSize(userDataSize),
          userDataZoneTag(userDataZoneTag),
          // Quadtree must subdivide the space equally into 1x1 unit cells.
          root(Cell(0, 0), ceilPow2(de::max(dimensions.x, dimensions.y)))
    {}

    Node *allocNode(Cell const &at, uint size)
    {
        nodes.append(Node(at, size));
        return &nodes.last();
    }

    Node *findLeaf(Node *node, Cell const &at, bool canCreate)
    {
        if(node->isLeaf())
            return node;

        // Into which quadrant do we need to descend?
        Node::Quadrant q = node->quadrant(at);

        // Has this quadrant been initialized yet?
        Node **childAdr = node->getChildAdr(q);
        if(!*childAdr)
        {
            if(!canCreate) return 0;

            // Subdivide the space.
            uint const subSize = node->size >> 1;
            switch(q)
            {
            case Node::TopLeft:
                *childAdr = allocNode(node->cell, subSize);
                break;

            case Node::TopRight:
                *childAdr = allocNode(Cell(node->cell.x + subSize, node->cell.y), subSize);
                break;

            case Node::BottomLeft:
                *childAdr = allocNode(Cell(node->cell.x, node->cell.y + subSize), subSize);
                break;
            case Node::BottomRight:
                *childAdr = allocNode(Cell(node->cell.x + subSize, node->cell.y + subSize), subSize);
                break;
            }
        }

        return findLeaf(*childAdr, at, canCreate);
    }

    inline Node *findLeaf(Cell const &at, bool canCreate = false)
    {
        return findLeaf(&root, at, canCreate);
    }

    void *userData(Node &node, bool canCreate = false)
    {
        // Exisiting user data for this cell?
        if(!node.userData)
        {
            // Can we allocate new user data?
            if(canCreate)
            {
                node.userData = Z_Calloc(userDataSize, userDataZoneTag, 0);
            }
        }
        return node.userData;
    }
};

Gridmap::Gridmap(Cell const &dimensions, size_t userDataSize, int userDataZoneTag)
    : d(new Instance(dimensions, userDataSize, userDataZoneTag))
{}

Gridmap::~Gridmap()
{}

int Gridmap::toCellIndex(uint cellX, uint cellY) const
{
    return int(cellY * d->dimensions.x + cellX);
}

Gridmap::Cell const &Gridmap::dimensions() const
{
    return d->dimensions;
}

bool Gridmap::clipBlock(CellBlock &block) const
{
    bool didClip = false;
    if(block.min.x >= d->dimensions.x)
    {
        block.min.x = d->dimensions.x - 1;
        didClip = true;
    }
    if(block.min.y >= d->dimensions.y)
    {
        block.min.y = d->dimensions.y - 1;
        didClip = true;
    }
    if(block.max.x >= d->dimensions.x)
    {
        block.max.x = d->dimensions.x - 1;
        didClip = true;
    }
    if(block.max.y >= d->dimensions.y)
    {
        block.max.y = d->dimensions.y - 1;
        didClip = true;
    }
    return didClip;
}

void *Gridmap::cellData(Cell const &cell, bool canCreate)
{
    // Outside our boundary?
    if(cell.x >= d->dimensions.x || cell.y >= d->dimensions.y)
    {
        return 0;
    }

    // Try to locate this leaf (may fail if not present and we are
    // not allocating user data (there will be no corresponding cell)).
    if(Instance::Node *node = d->findLeaf(cell, canCreate))
    {
        return d->userData(*node, canCreate);
    }
    return 0;
}

int Gridmap::iterate(IterateCallback callback, void *context)
{
    foreach(Instance::Node const &node, d->nodes)
    {
        // Only leafs with user data.
        if(!node.isLeaf()) continue;
        if(!node.userData) continue;

        if(int result = callback(node.userData, context))
            return result;
    }
    return false; // Continue iteration.
}

int Gridmap::iterate(CellBlock const &cellBlock_, IterateCallback callback,
                     void *context)
{
    // Clip coordinates to our boundary dimensions (the underlying
    // Quadtree is normally larger than this so we cannot use the
    // dimensions of the root cell here).
    CellBlock cellBlock = cellBlock_;
    clipBlock(cellBlock);

    // Traverse cells in the block.
    /// @todo Optimize: We could avoid repeatedly descending the tree...
    Cell cell;
    for(cell.y = cellBlock.min.y; cell.y <= cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x <= cellBlock.max.x; ++cell.x)
    {
        Instance::Node *node = d->findLeaf(cell);
        if(!node) continue;
        if(!node->userData) continue;

        if(int result = callback(node->userData, context))
            return result;
    }

    return false; // Continue iteration.
}

// Debug visual ----------------------------------------------------------------

#ifdef __CLIENT__

#define UNIT_SIZE           1

void Gridmap::drawDebugVisual() const
{
    // We'll be changing the color, so query the current and restore later.
    GLfloat oldColor[4]; glGetFloatv(GL_CURRENT_COLOR, oldColor);

    /*
     * Draw the Quadtree.
     */
    glColor4f(1.f, 1.f, 1.f, 1.f / d->root.size);
    foreach(Instance::Node const &node, d->nodes)
    {
        // Only leafs with user data.
        if(!node.isLeaf()) continue;
        if(!node.userData) continue;

        Vector2f const topLeft     = node.cell * UNIT_SIZE;
        Vector2f const bottomRight = topLeft + UNIT_SIZE;

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
}

#endif // __CLIENT__

} // namespace de
