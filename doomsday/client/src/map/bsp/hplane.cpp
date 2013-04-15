/** @file hplane.cpp BSP Builder half-plane and plane intersection list.
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

#include <de/Error>
#include <de/Log>
#include "map/bsp/hplane.h"

using namespace de;
using namespace de::bsp;

HPlane::HPlane(Vector2d const &origin, Vector2d const &direction)
    : Partition(origin, direction), _intercepts(0)
{}

HPlane::~HPlane()
{
    clear();
}

void HPlane::clear()
{
    _intercepts.clear();
}

Vector2d const &HPlane::origin() const
{
    return Partition::origin;
}

void HPlane::setOrigin(Vector2d const &newOrigin)
{
    if(Partition::origin != newOrigin)
    {
        Partition::origin = newOrigin;
        clear();
    }
}

void HPlane::setXOrigin(coord_t newX)
{
    if(!de::fequal(Partition::origin.x, newX))
    {
        Partition::origin.x = newX;
        clear();
    }
}

void HPlane::setYOrigin(coord_t newY)
{
    if(!de::fequal(Partition::origin.y, newY))
    {
        Partition::origin.y = newY;
        clear();
    }
}

Vector2d const &HPlane::direction() const
{
    return Partition::direction;
}

void HPlane::setDirection(Vector2d const &newDirection)
{
    if(Partition::direction != newDirection)
    {
        Partition::direction = newDirection;
        clear();
    }
}

void HPlane::setXDirection(coord_t newDX)
{
    if(!de::fequal(Partition::direction.x, newDX))
    {
        Partition::direction.x = newDX;
        clear();
    }
}

void HPlane::setYDirection(coord_t newDY)
{
    if(!de::fequal(Partition::direction.y, newDY))
    {
        Partition::direction.y = newDY;
        clear();
    }
}

HPlaneIntercept &HPlane::newIntercept(coord_t distance, void *userData)
{
    Intercepts::reverse_iterator after;

    for(after = _intercepts.rbegin();
        after != _intercepts.rend() && distance < (*after).distance(); after++)
    {}

    return *_intercepts.insert(after.base(), HPlaneIntercept(distance, userData));
}

void HPlane::mergeIntercepts(mergepredicate_t predicate, void *userData)
{
    Intercepts::iterator node = _intercepts.begin();
    while(node != _intercepts.end())
    {
        Intercepts::iterator np = node; np++;
        if(np == _intercepts.end()) break;

        // Sanity check.
        coord_t distance = *np - *node;
        if(distance < -0.1)
        {
            throw Error("HPlane::mergeIntercepts", QString("Invalid intercept order - %1 > %2")
                                                       .arg(node->distance(), 0, 'f', 3)
                                                       .arg(  np->distance(), 0, 'f', 3));
        }

        // Are we merging this pair?
        if(predicate(*node, *np, userData))
        {
            // Yes - Unlink this intercept.
            _intercepts.erase(np);
        }
        else
        {
            // No.
            node++;
        }
    }
}

HPlane::Intercepts const &HPlane::intercepts() const
{
    return _intercepts;
}

#ifdef DENG_DEBUG
void HPlane::DebugPrint(HPlane const &inst)
{
    int index = 0;
    DENG2_FOR_EACH_CONST(HPlane::Intercepts, i, inst.intercepts())
    {
        LOG_DEBUG(" %i: >%f ") << index << i->distance();
        index++;
    }
}
#endif
