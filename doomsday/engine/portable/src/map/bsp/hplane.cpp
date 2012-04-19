/**
 * @file hplane.cpp
 * BSP Builder half-plane and plane intersection list. @ingroup bsp
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"

#include "map/bsp/hedgeintercept.h"
#include "map/bsp/hplane.h"

using namespace de::bsp;

void HPlane::clear()
{
    intercepts.clear();
}

HPlane* HPlane::setOrigin(coord_t const newOrigin[2])
{
    if(newOrigin)
    {
        partition.origin[0] = newOrigin[0];
        partition.origin[1] = newOrigin[1];
        clear();
    }
    return this;
}

HPlane* HPlane::setXY(coord_t newX, coord_t newY)
{
    coord_t newOrigin[2] = { newX, newY };
    return setOrigin(newOrigin);
}

HPlane* HPlane::setX(coord_t newX)
{
    partition.origin[VX] = newX;
    clear();
    return this;
}

HPlane* HPlane::setY(coord_t newY)
{
    partition.origin[VY] = newY;
    clear();
    return this;
}

HPlane* HPlane::setDirection(coord_t const newDirection[2])
{
    if(newDirection)
    {
        V2d_Copy(partition.direction, newDirection);
        clear();
    }
    return this;
}

HPlane* HPlane::setDXY(coord_t newDX, coord_t newDY)
{
    coord_t newDirection[2] = { newDX, newDY };
    return setDirection(newDirection);
}

HPlane* HPlane::setDX(coord_t newDX)
{
    partition.direction[VX] = newDX;
    clear();
    return this;
}

HPlane* HPlane::setDY(coord_t newDY)
{
    partition.direction[VY] = newDY;
    clear();
    return this;
}

HPlaneIntercept* HPlane::newIntercept(coord_t distance, void* userData)
{
    Intercepts::reverse_iterator after;
    HPlaneIntercept* inter;

    for(after = intercepts.rbegin();
        after != intercepts.rend() && distance < (*after).distance(); after++)
    {}

    inter = &*intercepts.insert(after.base(), HPlaneIntercept(distance, userData));
    return inter;
}

HPlane::Intercepts::const_iterator HPlane::deleteIntercept(Intercepts::iterator at)
{
    //if(at < intercepts.begin() || at >= intercepts.end()) return at;
    return intercepts.erase(at);
}

#if _DEBUG
void HPlane::DebugPrint(const HPlane& inst)
{
    uint n = 0;
    for(HPlane::Intercepts::const_iterator it = inst.begin(); it != inst.end(); it++, n++)
    {
        const HPlaneIntercept* inter = &*it;
        Con_Printf(" %u: >%1.2f ", n, inter->distance());
        HEdgeIntercept::DebugPrint(*static_cast<HEdgeIntercept*>(inter->userData()));
    }
}
#endif
