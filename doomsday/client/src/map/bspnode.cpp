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

#include <de/vector1.h> /// @todo Remove me

#include <de/Log>

#include "map/bspnode.h"

using namespace de;

DENG2_PIMPL(BspNode)
{
    /// Space partition (half-plane).
    Partition partition;

    /// Right and left child elements for each half space.
    MapElement *rightChild;
    MapElement *leftChild;

    /// Right and left bounding boxes for each half space.
    AABoxd rightAABox;
    AABoxd leftAABox;

    Instance(Public *i, Partition const &partition_)
        : Base(i),
          partition(partition_),
          rightChild(0),
          leftChild(0)
    {}

    inline MapElement **childAdr(int left) {
        return left? &leftChild : &rightChild;
    }

    inline AABoxd &aaBox(int left) {
        return left? leftAABox : rightAABox;
    }
};

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
    return *d->childAdr(left) != 0;
}

MapElement &BspNode::child(int left) const
{
    if(MapElement *childElm = *d->childAdr(left))
    {
        return *childElm;
    }
    /// @throw MissingChildError  The specified child element is missing.
    throw MissingChildError("BspNode::child", QString("No %1 child is configured").arg(left? "left" : "right"));
}

void BspNode::setChild(int left, MapElement *newChild)
{
    if(!newChild || newChild != this)
    {
        *d->childAdr(left) = newChild;
        return;
    }
    /// @throw InvalidChildError  Attempted to set a child element to "this" element.
    throw InvalidChildError("BspNode::setChild", QString("Cannot set \"this\" element as a child of itself"));
}

AABoxd const &BspNode::childAABox(int left) const
{
    return d->aaBox(left);
}

void BspNode::setChildAABox(int left, AABoxd const *newAABox)
{
    if(newAABox)
    {
        d->aaBox(left) = *newAABox;
    }
    else
    {
        d->aaBox(left).clear();
    }
}
