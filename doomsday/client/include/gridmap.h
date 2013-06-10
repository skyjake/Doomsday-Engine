/** @file gridmap.h  Gridmap.
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

#ifndef DENG_DATA_GRIDMAP_H
#define DENG_DATA_GRIDMAP_H

#include <de/libdeng2.h>
#include <de/Vector>

#ifdef min
#   undef min
#endif

#ifdef max
#   undef max
#endif

namespace de {

/**
 * Gridmap. An abstract class designed for mapping objects into a two dimensional
 * spatial index.
 *
 * Gridmap's implementation allows that the whole space is indexable however cells
 * within it need not be populated. Therefore Gridmap may be considered a "sparse"
 * structure as it allows the user to construct the space piece-wise or, leave it
 * deliberately incomplete.
 *
 * Designed around that of a Region Quadtree with inherent sparsity and compression
 * potential.
 *
 * @ingroup data
 */
class Gridmap
{
public:
    typedef Vector2ui Cell;

    /**
     * Handy POD structure for representing a rectangular range of cells (a "cell block").
     */
    struct CellBlock
    {
        Cell min;
        Cell max;

        CellBlock(Cell const &min = Cell(), Cell const &max = Cell()) : min(min), max(max) {}
        CellBlock(CellBlock const &other) : min(other.min), max(other.max) {}
    };

    /// Quadtree child identifiers (quadrants).
    enum Quadrant
    {
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    /**
     * A node in the quadtree.
     */
    struct Node
    {
        /// Cell coordinates for this node.
        Cell cell;

        /// Size of the cell at this node (width=height).
        uint size;

        /// Child nodes, one for each quadrant:
        Node *topLeft;
        Node *topRight;
        Node *bottomLeft;
        Node *bottomRight;

        /// User data associated with the cell.
        void *userData;

        /**
         * Construct a new node.
         *
         * @param cell  Cell coordinates for the node.
         * @param size  Size of the cell.
         */
        Node(Cell const &cell, uint size);

        ~Node();

        /**
         * Returns @c true iff the cell is a leaf (i.e., equal to a unit in the
         * Gridmap coordinate space).
         */
        bool isLeaf() const { return size == 1; }

        /**
         * In which quadrant is the @a point?
         */
        Quadrant quadrant(Cell const &point) const;

        Node **getChildAdr(Quadrant quadrant);

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
        int iterate(bool leafOnly, int (*callback) (Node &node, void *parameters),
                    void *parameters = 0);
    };

public:
    /**
     * Construct a new (empty) gridmap.
     *
     * @param dimensions  Dimensions for the gridmap in cells.
     * @param sizeOfCell  Amount of memory to be allocated for the user data associated with each cell.
     * @param zoneTag     Zone memory tag for the allocated user data.
     */
    Gridmap(Cell const &dimensions, size_t sizeOfCell, int zoneTag);

    virtual ~Gridmap();

    /**
     * Returns the dimensions of the gridmap in cells.
     */
    Cell const &dimensions() const;

    /**
     * Returns the width of the gridmap in cells.
     */
    inline uint width() const { return dimensions().x; }

    /**
     * Returns the height of the gridmap in cells.
     */
    inline uint height() const { return dimensions().y; }

    /**
     * Clip the cell coordinates in @a block vs the dimensions of this gridmap so that they
     * are inside the boundary this defines.
     *
     * @param block  Block coordinates to be clipped.
     *
     * @return  @c true iff the block coordinates were changed.
     */
    bool clipBlock(CellBlock &block) const;

    /**
     * Retrieve the user data associated with the identified cell.
     *
     * @param coords     XY coordinates of the cell whose data to retrieve.
     * @param canCreate  @c true= allocate new data if not already present
     *                   for the referenced cell.
     *
     * @return  User data for the identified cell else @c 0if an invalid
     *          reference or no there is no data present (and not allocating).
     */
    void *cellData(Cell const &cell, bool canCreate = false);

    /*
     * Iteration callback function.
     */
    typedef int (*IterateCallback) (void *cellData, void *parameters);

    /**
     * Iterate over all populated cells in the gridmap making a callback for each. Iteration ends
     * when all cells have been visited or @a callback returns non-zero.
     *
     * @param callback       Callback function ptr.
     * @param parameters     Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterate(IterateCallback callback, void *parameters = 0);

    /**
     * Iterate over a block of populated cells in the gridmap making a callback for each.
     * Iteration ends when all selected cells have been visited or @a callback returns non-zero.
     *
     * @param min            Minimal coordinates for the top left cell.
     * @param max            Maximal coordinates for the bottom right cell.
     * @param callback       Callback function ptr.
     * @param parameters     Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int iterate(CellBlock const &block, IterateCallback callback, void *paramaters = 0);

    /**
     * Render a visual for this gridmap to assist in debugging (etc...).
     *
     * This visualizer assumes that the caller has already configured the GL render state
     * (projection matrices, scale, etc...) as desired prior to calling. This function
     * guarantees to restore the previous GL state if any changes are made to it.
     *
     * @note Internally this visual uses fixed unit dimensions [1x1] for cells, therefore
     *       the caller should scale the appropriate matrix to scale this visual as desired.
     */
    void drawDebugVisual() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_DATA_GRIDMAP_H
