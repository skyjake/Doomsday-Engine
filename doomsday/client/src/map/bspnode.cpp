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

BspNode::BspNode(const_pvec2d_t partitionOrigin, const_pvec2d_t partitionDirection)
    : MapElement(DMU_BSPNODE), _partition(partitionOrigin, partitionDirection)
{
    std::memset(_aaBox,      0, sizeof(_aaBox));
    std::memset(_children,   0, sizeof(_children));
    _index = 0;

    _children[RIGHT] = 0;
    _children[LEFT]  = 0;

    setRightAABox(0);
    setLeftAABox(0);
}

BspNode::~BspNode()
{}

Partition const &BspNode::partition() const
{
    return _partition;
}

bool BspNode::hasChild(int left) const
{
    return !!_children[left? LEFT : RIGHT];
}

MapElement &BspNode::child(int left) const
{
    if(_children[left? LEFT:RIGHT])
    {
        return *_children[left? LEFT:RIGHT];
    }
    /// @throw MissingChildError The specified child element is missing.
    throw MissingChildError("BspNode::child", QString("No %1 child is configured").arg(left? "left" : "right"));
}

void BspNode::setChild(int left, MapElement *newChild)
{
    DENG2_ASSERT(newChild != this);
    _children[left? LEFT:RIGHT] = newChild;
}

AABoxd const &BspNode::childAABox(int left) const
{
    return _aaBox[left? LEFT:RIGHT];
}

void BspNode::setChildAABox(int left, AABoxd *newAABox)
{
    if(newAABox)
    {
        AABoxd &dst = _aaBox[left? LEFT:RIGHT];
        V2d_CopyBox(dst.arvec2, newAABox->arvec2);
    }
    else
    {
        // Clear.
        AABoxd &dst = _aaBox[left? LEFT:RIGHT];
        V2d_Set(dst.min, DDMAXFLOAT, DDMAXFLOAT);
        V2d_Set(dst.max, DDMINFLOAT, DDMINFLOAT);
    }
}

uint BspNode::origIndex() const
{
    return _index;
}

// Partition -------------------------------------------------------------------
/// @todo Move to another file.

Partition::Partition(coord_t xOrigin, coord_t yOrigin, coord_t xDirection, coord_t yDirection)
{
    V2d_Set(_origin,    xOrigin, yOrigin);
    V2d_Set(_direction, xDirection, yDirection);
}

Partition::Partition(const_pvec2d_t origin, const_pvec2d_t direction)
{
    V2d_Copy(_origin, origin);
    V2d_Copy(_direction, direction);
}

vec2d_t const &Partition::origin() const
{
    return _origin;
}

vec2d_t const &Partition::direction() const
{
    return _direction;
}

/// @todo This geometric math logic should be defined in <de/math.h>
int Partition::pointOnSide(const_pvec2d_t point) const
{
    if(!_direction[VX])
    {
        if(point[VX] <= _origin[VX])
            return (_direction[VY] > 0? 1:0);
        else
            return (_direction[VY] < 0? 1:0);
    }
    if(!_direction[VY])
    {
        if(point[VY] <= _origin[VY])
            return (_direction[VX] < 0? 1:0);
        else
            return (_direction[VX] > 0? 1:0);
    }

    coord_t delta[2] = { (point[VX] - _origin[VX]),
                         (point[VY] - _origin[VY]) };

    // Try to quickly decide by looking at the signs.
    if(_direction[VX] < 0)
    {
        if(_direction[VY] < 0)
        {
            if(delta[VX] < 0)
            {
                if(delta[VY] >= 0) return 0;
            }
            else if(delta[VY] < 0)
            {
                return 1;
            }
        }
        else
        {
            if(delta[VX] < 0)
            {
                if(delta[VY] < 0) return 1;
            }
            else if(delta[VY] >= 0)
            {
                return 0;
            }
        }
    }
    else
    {
        if(_direction[VY] < 0)
        {
            if(delta[VX] < 0)
            {
                if(delta[VY] < 0) return 0;
            }
            else if(delta[VY] >= 0)
            {
                return 1;
            }
        }
        else
        {
            if(delta[VX] < 0)
            {
                if(delta[VY] >= 0) return 1;
            }
            else if(delta[VY] < 0)
            {
                return 0;
            }
        }
    }

    coord_t left  = _direction[VY] * delta[VX];
    coord_t right = delta[VY] * _direction[VX];

    if(right < left)
        return 0; // front side
    return 1; // back side
}
