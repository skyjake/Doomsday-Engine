/** @file map/bsp/linesegment.cpp BSP Builder Line Segment.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include "map/bsp/linesegment.h"

namespace de {
namespace bsp {

Vertex &LineSegment::vertex(int to)
{
    DENG_ASSERT((to? _to : _from) != 0);
    return to? *_to : *_from;
}

Vertex const &LineSegment::vertex(int to) const
{
    return const_cast<Vertex const &>(const_cast<LineSegment *>(this)->vertex(to));
}

void LineSegment::replaceVertex(int to, Vertex &newVertex)
{
    if(to) _to   = &newVertex;
    else   _from = &newVertex;
}

bool LineSegment::hasTwin() const
{
    return _twin != 0;
}

LineSegment &LineSegment::twin() const
{
    if(_twin)
    {
        return *_twin;
    }
    /// @throw MissingTwinError Attempted with no twin associated.
    throw MissingTwinError("LineSegment::twin", "No twin line segment is associated");
}

bool LineSegment::hasBspLeaf() const
{
    if(!hedge) return false;
    return hedge->hasBspLeaf();
}

BspLeaf &LineSegment::bspLeaf() const
{
    if(hedge)
    {
        return hedge->bspLeaf();
    }
    /// @throw MissingBspLeafError Attempted with no BSP leaf associated.
    throw MissingBspLeafError("LineSegment::bspLeaf", "No BSP leaf is associated");
}

bool LineSegment::hasLineSide() const
{
    return _lineSide != 0;
}

Line::Side &LineSegment::lineSide() const
{
    if(_lineSide)
    {
        return *_lineSide;
    }
    /// @throw MissingLineError Attempted with no line attributed.
    throw MissingLineSideError("LineSegment::lineSide", "No line side is attributed");
}

} // namespace bsp
} // namespace de
