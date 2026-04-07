/** @file lineblockmap.cpp  Specialized Blockmap, for map Lines.
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

#include "doomsday/world/lineblockmap.h"
#include "doomsday/world/line.h"

using namespace de;

namespace world {

LineBlockmap::LineBlockmap(const AABoxd &bounds, duint cellSize)
    : Blockmap(bounds, cellSize)
{}

void LineBlockmap::link(Line &line)
{
    // Polyobj lines are excluded (presently...).
    if(line.definesPolyobj()) return;

    // Determine the block of cells we'll be working within.
    const BlockmapCellBlock cellBlock = toCellBlock(line.bounds());
    BlockmapCell cell;
    for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
    {
        if(line.slopeType() == ST_VERTICAL || line.slopeType() == ST_HORIZONTAL)
        {
            Blockmap::link(cell, &line);
            continue;
        }

        Vec2d const point(origin() + cellDimensions() * cell);

        // Choose a cell diagonal to test.
        Vec2d from, to;
        if(line.slopeType() == ST_POSITIVE)
        {
            // Line slope / vs \ cell diagonal.
            from = Vec2d(point.x, point.y + cellDimensions().y);
            to   = Vec2d(point.x + cellDimensions().x, point.y);
        }
        else
        {
            // Line slope \ vs / cell diagonal.
            from = Vec2d(point.x + cellDimensions().x, point.y + cellDimensions().y);
            to   = Vec2d(point.x, point.y);
        }

        // Would Line intersect this?
        if((line.pointOnSide(from) < 0) != (line.pointOnSide(to) < 0))
        {
            Blockmap::link(cell, &line);
        }
    }
}

void LineBlockmap::link(const List<Line *> &lines)
{
    for (Line *line : lines) link(*line);
}

}  // namespace world
