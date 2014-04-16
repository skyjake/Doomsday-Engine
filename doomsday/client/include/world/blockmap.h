/** @file blockmap.h World map element blockmap.
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

#ifndef DENG_WORLD_BLOCKMAP_H
#define DENG_WORLD_BLOCKMAP_H

#include <de/aabox.h>

#include <de/Vector>

#ifdef min
#   undef min
#endif

#ifdef max
#   undef max
#endif

namespace de {

/**
 * @ingroup world
 */
class Blockmap
{
public:
    typedef Vector2ui Cell;

    /**
     * POD structure for representing an inclusive-exclusive rectangular range
     * of cells.
     *
     * @todo Use Rectangleui instead -ds
     */
    struct CellBlock
    {
        Cell min;
        Cell max;

        CellBlock(Cell const &min = Cell(), Cell const &max = Cell()) : min(min), max(max) {}
        CellBlock(CellBlock const &other) : min(other.min), max(other.max) {}
    };

public:
    /**
     * @param bounds    Map space boundary.
     * @param cellSize  Width and height of a cell in map space units.
     */
    Blockmap(AABoxd const &bounds, uint cellSize = 128);

    virtual ~Blockmap();

    /**
     * Returns the origin of the blockmap in map space.
     */
    Vector2d origin() const;

    /**
     * Returns the bounds of the blockmap in map space.
     */
    AABoxd const &bounds() const;

    /**
     * Returns the dimensions of the blockmap in cells.
     */
    Cell const &dimensions() const;

    /**
     * Returns the width of the blockmap in cells.
     */
    inline uint width() const { return dimensions().x; }

    /**
     * Returns the height of the blockmap in cells.
     */
    inline uint height() const { return dimensions().y; }

    /**
     * Returns @c true iff the blockmap is of zero-area.
     */
    inline bool isNull() const { return (width() * height()) == 0; }

    /**
     * Returns the size of a cell (width and height) in map space units.
     */
    uint cellSize() const;

    /**
     * Utility function which returns the dimensions of a cell in map space units.
     */
    Vector2d cellDimensions() const { return Vector2d(cellSize(), cellSize()); }

    /**
     * Utility function which returns the linear index of the specified cell.
     */
    int toCellIndex(uint cellX, uint cellY) const;

    /**
     * Given map space XY coordinates @a pos, output the blockmap cell[x, y] it
     * resides in. If @a pos is outside the blockmap it will be clamped to the
     * nearest edge on one or more axes as necessary.
     *
     * @param point    Map coordinate space point to be translated.
     * @param didClip  Set to @c true iff clamping was necessary.
     */
    Cell toCell(Vector2d const &point, bool *didClip = 0) const;

    /**
     * Given map space box XY coordinates @a box, output the blockmap cells[x, y]
     * they reside in. If any point defined by @a box lies outside the blockmap
     * it will be clamped to the nearest edge on one or more axes as necessary.
     *
     * @param box      Map space coordinates to translate.
     * @param didClip  Set to @c true iff clamping was necessary.
     */
    CellBlock toCellBlock(AABoxd const &box, bool *didClip = 0) const;

    /**
     * Retrieve the number of elements linked in the specified @a cell.
     *
     * @param cell  Cell to lookup.
     *
     * @return  Number of unique objects linked into the cell, or @c 0 if invalid.
     */
    int cellElementCount(Cell const &cell) const;

    bool link(Cell const &cell, void *elem);

    bool link(AABoxd const &region, void *elem);

    bool unlink(Cell const &cell, void *elem);

    bool unlink(AABoxd const &region, void *elem);

    void unlinkAll();

    /**
     * Iterate over all elements in the specified @a cell.
     */
    int iterate(Cell const &cell, int (*callback) (void *elem, void *context), void *context = 0) const;

    /**
     * Iterate over all elements in cells which intercept the specified map space
     * @a region.
     */
    int iterate(AABoxd const &region, int (*callback) (void *elem, void *context), void *context = 0) const;

    /**
     * Iterate over all elements in cells which intercept the line specified by
     * the two map space points @a from and @a to. Note that if an element is
     * processed/visited it does @em not mean that the line actually intercepts
     * the element. Further testing between the line and the geometry of the map
     * element is necessary if this is a requirement.
     *
     * @param from  Map space point defining the origin of the line.
     * @param to    Map space point defining the destination of the line.
     */
    int iterate(Vector2d const &from, Vector2d const &to, int (*callback) (void *elem, void *context), void *context = 0) const;

    /**
     * Render a visual for this gridmap to assist in debugging (etc...).
     *
     * This visualizer assumes that the caller has already configured the GL
     * render state (projection matrices, scale, etc...) as desired prior to
     * calling. This function guarantees to restore the previous GL state if
     * any changes are made to it.
     *
     * @note Internally this visual uses fixed unit dimensions [1x1] for cells,
     * therefore the caller should scale the appropriate matrix to scale this
     * visual as desired.
     */
    void drawDebugVisual() const;

private:
    DENG2_PRIVATE(d)
};

typedef Blockmap::Cell BlockmapCell;
typedef Blockmap::CellBlock BlockmapCellBlock;

} //namespace de

#endif // DENG_WORLD_BLOCKMAP_H
