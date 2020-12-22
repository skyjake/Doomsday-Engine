/** @file lineblockmap.h  Specialized Blockmap, for map Lines.
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

#include "blockmap.h"
#include <de/list.h>

namespace world {

class Line;

/**
 * Specializes Blockmap for use with map Lines, implementing a linkage algorithm
 * that replicates the quirky behavior of doom.exe
 */
class LineBlockmap : public Blockmap
{
public:
    /**
     * @param bounds    Map space boundary.
     * @param cellSize  Width and height of a cell in map space units.
     */
    LineBlockmap(const AABoxd &bounds, unsigned int cellSize = 128);

    /// @note Assumes @a line is not yet linked!
    void link(Line &line);

    /// @note Assumes none of the specified @a lines are yet linked!
    void link(const de::List<Line *> &lines);
};

}  // namespace world
