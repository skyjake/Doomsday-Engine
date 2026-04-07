/** @file blockmap.h  Map element blockmap.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include "../libdoomsday.h"
#include <de/legacy/aabox.h>
#include <de/vector.h>

#ifdef WIN32
#  undef max
#  undef min
#endif

class BlockmapDebugVisual;

namespace world {

class LIBDOOMSDAY_PUBLIC Blockmap
{
public:
    typedef de::Vec2ui Cell;

    /**
     * POD structure for representing an inclusive-exclusive rectangular range
     * of cells.
     *
     * @todo Use Rectangleui instead -ds
     */
    struct LIBDOOMSDAY_PUBLIC CellBlock
    {
        Cell min;
        Cell max;

        CellBlock(const Cell &min = Cell(), const Cell &max = Cell()) : min(min), max(max) {}
        CellBlock(const CellBlock &other) : min(other.min), max(other.max) {}
    };

public:
    /**
     * @param bounds    Map space boundary.
     * @param cellSize  Width and height of a cell in map space units.
     */
    Blockmap(const AABoxd &bounds, de::duint cellSize = 128);

    virtual ~Blockmap();

    /**
     * Returns the origin of the blockmap in map space.
     */
    de::Vec2d origin() const;

    /**
     * Returns the bounds of the blockmap in map space.
     */
    const AABoxd &bounds() const;

    /**
     * Returns the dimensions of the blockmap in cells.
     */
    const Cell &dimensions() const;

    /**
     * Returns the width of the blockmap in cells.
     */
    inline de::duint width() const { return dimensions().x; }

    /**
     * Returns the height of the blockmap in cells.
     */
    inline de::duint height() const { return dimensions().y; }

    /**
     * Returns @c true iff the blockmap is of zero-area.
     */
    inline bool isNull() const { return (width() * height()) == 0; }

    /**
     * Returns the size of a cell (width and height) in map space units.
     */
    de::duint cellSize() const;

    /**
     * Utility function which returns the dimensions of a cell in map space units.
     */
    de::Vec2d cellDimensions() const { return de::Vec2d(cellSize(), cellSize()); }

    /**
     * Utility function which returns the linear index of the specified cell.
     */
    int toCellIndex(de::duint cellX, de::duint cellY) const;

    /**
     * Given map space XY coordinates @a pos, output the blockmap cell[x, y] it
     * resides in. If @a pos is outside the blockmap it will be clamped to the
     * nearest edge on one or more axes as necessary.
     *
     * @param point    Map coordinate space point to be translated.
     * @param didClip  Set to @c true iff clamping was necessary.
     */
    Cell toCell(const de::Vec2d &point, bool *didClip = 0) const;

    /**
     * Given map space box XY coordinates @a box, output the blockmap cells[x, y]
     * they reside in. If any point defined by @a box lies outside the blockmap
     * it will be clamped to the nearest edge on one or more axes as necessary.
     *
     * @param box      Map space coordinates to translate.
     * @param didClip  Set to @c true iff clamping was necessary.
     */
    CellBlock toCellBlock(const AABoxd &box, bool *didClip = 0) const;

    /**
     * Retrieve the number of elements linked in the specified @a cell.
     *
     * @param cell  Cell to lookup.
     *
     * @return  Number of unique objects linked into the cell, or @c 0 if invalid.
     */
    int cellElementCount(const Cell &cell) const;

    bool link(const Cell &cell, void *elem);

    bool link(const AABoxd &region, void *elem);

    bool unlink(const Cell &cell, void *elem);

    bool unlink(const AABoxd &region, void *elem);

    void unlinkAll();

    /**
     * Iterate through all objects in the given @a cell.
     */
    de::LoopResult forAllInCell(const Cell &cell, std::function<de::LoopResult (void *object)> func) const;

    /**
     * Iterate through all objects in all cells which intercept the given map
     * space, axis-aligned bounding @a box.
     */
    de::LoopResult forAllInBox(const AABoxd &box, std::function<de::LoopResult (void *object)> func) const;

    /**
     * Iterate over all objects in cells which intercept the line specified by
     * the two map space points @a from and @a to. Note that if an object is
     * processed/visited it does @em not mean that the line actually intercepts
     * the objects. Further testing between the line and the geometry of the map
     * object is necessary if this is a requirement.
     *
     * @param from  Map space point defining the origin of the line.
     * @param to    Map space point defining the destination of the line.
     */
    de::LoopResult forAllInPath(const de::Vec2d &from, const de::Vec2d &to,
                                std::function<de::LoopResult (void *object)> func) const;

private:
    DE_PRIVATE(d)
};

typedef Blockmap::Cell BlockmapCell;
typedef Blockmap::CellBlock BlockmapCellBlock;

}  // namespace world
