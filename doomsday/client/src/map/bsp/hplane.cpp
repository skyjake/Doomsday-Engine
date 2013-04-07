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

HPlane::HPlane() : Partition(), _intercepts(0)
{}

HPlane::HPlane(const_pvec2d_t origin, const_pvec2d_t direction)
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

const_pvec2d_t &HPlane::origin() const
{
    return Partition::origin();
}

void HPlane::setOrigin(const_pvec2d_t newOrigin)
{
    if(newOrigin)
    {
        Partition::setOrigin(newOrigin);
        clear();
    }
}

void HPlane::setXOrigin(coord_t newX)
{
    Partition::setXOrigin(newX);
    clear();
}

void HPlane::setYOrigin(coord_t newY)
{
    Partition::setYOrigin(newY);
    clear();
}

const_pvec2d_t &HPlane::direction() const
{
    return Partition::direction();
}

void HPlane::setDirection(const_pvec2d_t newDirection)
{
    if(newDirection)
    {
        Partition::setDirection(newDirection);
        clear();
    }
}

void HPlane::setXDirection(coord_t newDX)
{
    Partition::setXDirection(newDX);
    clear();
}

void HPlane::setYDirection(coord_t newDY)
{
    Partition::setYDirection(newDY);
    clear();
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
