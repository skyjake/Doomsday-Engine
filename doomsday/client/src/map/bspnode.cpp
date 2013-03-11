/** @file bspnode.cpp Map BSP Node.
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

#include "de_base.h"
#include <de/Log>
#include <de/vector1.h>

#include "map/bspnode.h"

using namespace de;

BspNode::BspNode(coord_t const partitionOrigin[], coord_t const partitionDirection[])
    : MapElement(DMU_BSPNODE)
{
    std::memset(&partition, 0, sizeof(partition));
    std::memset(aaBox,      0, sizeof(aaBox));
    std::memset(children,   0, sizeof(children));
    index = 0;

    V2d_Copy(partition.origin, partitionOrigin);
    V2d_Copy(partition.direction, partitionDirection);

    children[RIGHT] = NULL;
    children[LEFT] = NULL;

    setRightBounds(0);
    setLeftBounds(0);
}

BspNode::~BspNode()
{}

void BspNode::setChild(int left, de::MapElement *newChild)
{
    DENG2_ASSERT(newChild != this);
    children[left? LEFT:RIGHT] = newChild;
}

void BspNode::setChildBounds(int left, AABoxd *newBounds)
{
    if(newBounds)
    {
        AABoxd *dst = &aaBox[left? LEFT:RIGHT];
        V2d_CopyBox(dst->arvec2, newBounds->arvec2);
    }
    else
    {
        // Clear.
        AABoxd *dst = &aaBox[left? LEFT:RIGHT];
        V2d_Set(dst->min, DDMAXFLOAT, DDMAXFLOAT);
        V2d_Set(dst->max, DDMINFLOAT, DDMINFLOAT);
    }
}
