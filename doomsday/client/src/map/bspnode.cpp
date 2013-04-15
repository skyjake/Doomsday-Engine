/** @file bspnode.cpp World Map BSP Node.
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

#include <de/vector1.h>
#include <de/Log>

#include "de_base.h"
#include "partition.h"

#include "map/bspnode.h"

using namespace de;

DENG2_PIMPL(BspNode)
{
    /// Space partition (half-plane).
    Partition partition;

    /// Bounding box for each child subspace [Right, Left].
    AABoxd aaBox[2];

    /// Child map elements [Right, Left].
    MapElement *children[2];

    /// Unique index. Set when saving the BSP.
    uint index;

    Instance(Public *i, Partition const &partition_)
        : Base(i), partition(partition_)
    {
        std::memset(children,   0, sizeof(children));
    }
};

BspNode::BspNode(Vector2d partitionOrigin, Vector2d partitionDirection)
    : MapElement(DMU_BSPNODE),
      d(new Instance(this, Partition(partitionOrigin, partitionDirection)))
{
    setRightAABox(0);
    setLeftAABox(0);
}

BspNode::BspNode(Partition const &partition_)
    : MapElement(DMU_BSPNODE), d(new Instance(this, partition_))
{
    setRightAABox(0);
    setLeftAABox(0);
}

Partition const &BspNode::partition() const
{
    return d->partition;
}

bool BspNode::hasChild(int left) const
{
    return d->children[left? LEFT : RIGHT] != 0;
}

MapElement &BspNode::child(int left) const
{
    if(d->children[left? LEFT:RIGHT])
    {
        return *d->children[left? LEFT:RIGHT];
    }
    /// @throw MissingChildError The specified child element is missing.
    throw MissingChildError("BspNode::child", QString("No %1 child is configured").arg(left? "left" : "right"));
}

void BspNode::setChild(int left, MapElement *newChild)
{
    DENG2_ASSERT(newChild != this);
    d->children[left? LEFT:RIGHT] = newChild;
}

AABoxd const &BspNode::childAABox(int left) const
{
    return d->aaBox[left? LEFT:RIGHT];
}

void BspNode::setChildAABox(int left, AABoxd const *newAABox)
{
    if(newAABox)
    {
        AABoxd &dst = d->aaBox[left? LEFT:RIGHT];
        V2d_CopyBox(dst.arvec2, newAABox->arvec2);
    }
    else
    {
        // Clear.
        AABoxd &dst = d->aaBox[left? LEFT:RIGHT];
        V2d_Set(dst.min, DDMAXFLOAT, DDMAXFLOAT);
        V2d_Set(dst.max, DDMINFLOAT, DDMINFLOAT);
    }
}

uint BspNode::origIndex() const
{
    return d->index;
}

void BspNode::setOrigIndex(uint newIndex)
{
    d->index = newIndex;
}
