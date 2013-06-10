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

#include "de_base.h"
#include "de_console.h"
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

/**
 * Construct a new node.
 *
 * @param cell  Cell coordinates for the node.
 * @param size  Size of the cell.
 */
Gridmap::Node::Node(Gridmap::Cell const &cell, uint size)
    : cell(cell),
      size(size),
      topLeft(0),
      topRight(0),
      bottomLeft(0),
      bottomRight(0),
      userData(0)
{}

Gridmap::Node::~Node()
{
    if(userData) Z_Free(userData);
}

/**
 * In which quadrant is the @a point?
 */
Gridmap::Quadrant Gridmap::Node::quadrant(Gridmap::Cell const &point) const
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

Gridmap::Node **Gridmap::Node::getChildAdr(Quadrant quadrant)
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
 * @param tree          TreeCell to traverse.
 * @param leafOnly      Caller is only interested in leaves.
 * @param callback      Callback function.
 * @param parameters    Passed to the callback.
 *
 * @return  Zero iff iteration completed wholly, else the value returned by the
 *          last callback made.
 */
int Gridmap::Node::iterate(bool leafOnly, int (*callback) (Node &node, void *parameters),
            void *parameters)
{
    if(!callback) return false;

    if(!isLeaf())
    {
        if(topLeft)
        {
            int result = topLeft->iterate(leafOnly, callback, parameters);
            if(result) return result;
        }
        if(topRight)
        {
            int result = topRight->iterate(leafOnly, callback, parameters);
            if(result) return result;
        }
        if(bottomLeft)
        {
            int result = bottomLeft->iterate(leafOnly, callback, parameters);
            if(result) return result;
        }
        if(bottomRight)
        {
            int result = bottomRight->iterate(leafOnly, callback, parameters);
            if(result) return result;
        }
    }
    if(!leafOnly || isLeaf())
    {
        return callback(*this, parameters);
    }
    return false;
}

DENG2_PIMPL(Gridmap)
{
    typedef QList<Node> Nodes;

    /// Dimensions of the space we are indexing (in cells).
    Cell dimensions;

    /// Size of the memory block to be allocated for each leaf.
    size_t sizeOfCell;

    /// Zone memory tag for the user data.
    int zoneTag;

    /// Quadtree root node.
    Node root;
    Nodes nodes;

    Instance(Public *i, Cell const &dimensions, size_t sizeOfCell, int zoneTag)
        : Base(i),
          dimensions(dimensions),
          sizeOfCell(sizeOfCell),
          zoneTag(zoneTag),
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
        Quadrant q = node->quadrant(at);

        // Has this quadrant been initialized yet?
        Node **childAdr = node->getChildAdr(q);
        if(!*childAdr)
        {
            if(!canCreate) return 0;

            // Subdivide the space.
            uint const subSize = node->size >> 1;
            switch(q)
            {
            case TopLeft:
                *childAdr = allocNode(node->cell, subSize);
                break;

            case TopRight:
                *childAdr = allocNode(Cell(node->cell.x + subSize, node->cell.y), subSize);
                break;

            case BottomLeft:
                *childAdr = allocNode(Cell(node->cell.x, node->cell.y + subSize), subSize);
                break;
            case BottomRight:
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
};

Gridmap::Gridmap(Cell const &dimensions, size_t cellSize, int zoneTag)
    : d(new Instance(this, dimensions, cellSize, zoneTag))
{}

Gridmap::~Gridmap()
{}

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
    if(cell.x >= d->dimensions.x || cell.y >= d->dimensions.y) return 0;

    // Try to locate this leaf (may fail if not present and we are
    // not allocating user data (there will be no corresponding cell)).
    Node *node = d->findLeaf(cell, canCreate);
    if(!node) return 0;

    // Exisiting user data for this cell?
    if(node->userData) return node->userData;

    // Allocate new user data?
    if(!canCreate) return 0;
    return node->userData = Z_Calloc(d->sizeOfCell, d->zoneTag, 0);
}

int Gridmap::iterate(IterateCallback callback, void *parameters)
{
    int result = false;
    foreach(Node const &node, d->nodes)
    {
        // Only leafs with user data.
        if(!node.isLeaf()) continue;
        if(!node.userData) continue;

        result = callback(node.userData, parameters);
        if(result) break;
    }
    return result;
}

int Gridmap::iterate(CellBlock const &inBlock, IterateCallback callback,
                     void *parameters)
{
    // Clip coordinates to our boundary dimensions (the underlying
    // Quadtree is normally larger than this so we cannot use the
    // dimensions of the root cell here).
    CellBlock block = inBlock;
    clipBlock(block);

    // Traverse cells in the block.
    /// @todo Optimize: We could avoid repeatedly descending the tree...
    Cell cell;
    for(cell.y = block.min.y; cell.y <= block.max.y; ++cell.y)
    for(cell.x = block.min.x; cell.x <= block.max.x; ++cell.x)
    {
        Node *node = d->findLeaf(cell);
        if(!node) continue;
        if(!node->userData) continue;

        int result = callback(node->userData, parameters);
        if(result) return result;
    }
    return false; // Continue iteration.
}

// Debug visual --------------------------------------------------------------

#ifdef __CLIENT__

#define UNIT_WIDTH      1
#define UNIT_HEIGHT     1

static int drawCellGeometry(Gridmap::Node &node, void * /*parameters*/)
{
    Vector2f topLeft(UNIT_WIDTH  * node.cell.x,
                     UNIT_HEIGHT * node.cell.y);

    Vector2f bottomRight(UNIT_WIDTH  * (node.cell.x + node.size),
                         UNIT_HEIGHT * (node.cell.y + node.size));

    glBegin(GL_LINE_LOOP);
        glVertex2f(topLeft.x,     topLeft.y);
        glVertex2f(bottomRight.x, topLeft.y);
        glVertex2f(bottomRight.x, bottomRight.y);
        glVertex2f(topLeft.x,     bottomRight.y);
    glEnd();
    return 0; // Continue iteration.
}

void Gridmap::drawDebugVisual() const
{
    // We'll be changing the color, so query the current and restore later.
    GLfloat oldColor[4]; glGetFloatv(GL_CURRENT_COLOR, oldColor);

    /*
     * Draw our Quadtree.
     */
    glColor4f(1.f, 1.f, 1.f, 1.f / d->root.size);
    d->root.iterate(false/*all cells*/, drawCellGeometry);

    /*
     * Draw our bounds.
     */
    Vector2f start;
    Vector2f end(UNIT_WIDTH * d->dimensions.x, UNIT_HEIGHT * d->dimensions.y);

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
