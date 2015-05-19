/** @file lineblockmap.cpp Specialized world map line blockmap.
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

#include "world/lineblockmap.h"

using namespace de;

LineBlockmap::LineBlockmap(AABoxd const &bounds, uint cellSize)
    : Blockmap(bounds, cellSize)
{}

void LineBlockmap::link(Line &line)
{
    // Polyobj lines are excluded (presently...).
    if(line.definesPolyobj()) return;

    // Determine the block of cells we'll be working within.
    BlockmapCellBlock const cellBlock = toCellBlock(line.aaBox());
    BlockmapCell cell;
    for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
    for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
    {
        if(line.slopeType() == ST_VERTICAL || line.slopeType() == ST_HORIZONTAL)
        {
            Blockmap::link(cell, &line);
            continue;
        }

        Vector2d const point(origin() + cellDimensions() * cell);

        // Choose a cell diagonal to test.
        Vector2d from, to;
        if(line.slopeType() == ST_POSITIVE)
        {
            // Line slope / vs \ cell diagonal.
            from = Vector2d(point.x, point.y + cellDimensions().y);
            to   = Vector2d(point.x + cellDimensions().x, point.y);
        }
        else
        {
            // Line slope \ vs / cell diagonal.
            from = Vector2d(point.x + cellDimensions().x, point.y + cellDimensions().y);
            to   = Vector2d(point.x, point.y);
        }

        // Would Line intersect this?
        if((line.pointOnSide(from) < 0) != (line.pointOnSide(to) < 0))
        {
            Blockmap::link(cell, &line);
        }
    }
}

void LineBlockmap::link(QList<Line *> const &lines)
{
    foreach(Line *line, lines)
    {
        link(*line);
    }
}
