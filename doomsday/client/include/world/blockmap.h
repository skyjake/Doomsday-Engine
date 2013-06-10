/** @file world/blockmap.h World map element blockmap.
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

#include "gridmap.h"

namespace de {

/**
 * @ingroup world
 */
class Blockmap
{
public:
    typedef Gridmap::Cell Cell;
    typedef Gridmap::CellBlock CellBlock;

public:
    /**
     * @param bounds
     * @param cellDimensions  Dimensions of a cell in map coordinate space units.
     */
    Blockmap(AABoxd const &bounds, Vector2ui const &cellDimensions);

    /**
     * Returns the origin of the blockmap in the map coordinate space.
     */
    Vector2d origin() const;

    /**
     * Returns the bounds of the blockmap in the map coordinate space.
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
     * Returns the dimension of a cell in map coordinate space units.
     */
    Vector2d const &cellDimensions() const;

    /**
     * Returns the width of a cell in map coordinate space units.
     */
    inline coord_t cellWidth() const { return cellDimensions().x; }

    /**
     * Returns the height of a cell in map coordinate space units.
     */
    inline coord_t cellHeight() const { return cellDimensions().y; }

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

    bool link(CellBlock const &cellBlock, void *elem);

    bool unlink(Cell const &cell, void *elem);

    bool unlink(CellBlock const &cellBlock, void *elem);

    int iterate(Cell const &cell, int (*callback) (void *elem, void *context), void *context);

    int iterate(CellBlock const &cellBlock, int (*callback) (void *elem, void *context), void *context);

    /**
     * Retrieve an immutable pointer to the underlying Gridmap instance
     * (primarily intended for debug purposes).
     */
    Gridmap const &gridmap() const;

private:
    DENG2_PRIVATE(d)
};

} //namespace de

#endif // DENG_WORLD_BLOCKMAP_H
