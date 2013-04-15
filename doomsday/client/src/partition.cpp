/** @file partition.cpp Infinite line of the form point + direction vector.
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

#include "partition.h"

using namespace de;

Partition::Partition(Vector2d const &origin, Vector2d const &direction)
    : origin(origin), direction(direction)
{}

Partition::Partition(Partition const &other)
    : origin(other.origin), direction(other.direction)
{}

/// @todo This geometric math logic should be defined in <de/math.h>
int Partition::pointOnSide(Vector2d const &point) const
{
    if(!direction.x)
    {
        if(point.x <= origin.x)
            return (direction.y > 0? 1:0);
        else
            return (direction.y < 0? 1:0);
    }
    if(!direction.y)
    {
        if(point.y <= origin.y)
            return (direction.x < 0? 1:0);
        else
            return (direction.x > 0? 1:0);
    }

    Vector2d delta = point - origin;

    // Try to quickly decide by looking at the signs.
    if(direction.x < 0)
    {
        if(direction.y < 0)
        {
            if(delta.x < 0)
            {
                if(delta.y >= 0) return 0;
            }
            else if(delta.y < 0)
            {
                return 1;
            }
        }
        else
        {
            if(delta.x < 0)
            {
                if(delta.y < 0) return 1;
            }
            else if(delta.y >= 0)
            {
                return 0;
            }
        }
    }
    else
    {
        if(direction.y < 0)
        {
            if(delta.x < 0)
            {
                if(delta.y < 0) return 0;
            }
            else if(delta.y >= 0)
            {
                return 1;
            }
        }
        else
        {
            if(delta.x < 0)
            {
                if(delta.y >= 0) return 1;
            }
            else if(delta.y < 0)
            {
                return 0;
            }
        }
    }

    ddouble left  = direction.y * delta.x;
    ddouble right = delta.y * direction.x;

    return right < left? 0:1;
}
