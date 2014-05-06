/** @file bspnode.cpp  World map BSP node.
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
#include "world/bspnode.h"

#include <de/Log>
#include <de/vector1.h> /// @todo Remove me

using namespace de;

BspElement::BspElement(int t, MapElement *parent)
    : MapElement(t, parent)
{}

DENG2_PIMPL_NOREF(BspNode)
{
    /// Space partition (half-plane).
    Partition partition;

    /// Right and left child elements for each half space.
    BspElement *rightChild;
    BspElement *leftChild;

    /// Right and left bounding boxes for each half space.
    AABoxd rightAABox;
    AABoxd leftAABox;

    Instance(Partition const &partition)
        : partition(partition)
        , rightChild(0)
        , leftChild(0)
    {}

    inline BspElement **childAdr(int left) {
        return left? &leftChild : &rightChild;
    }

    inline AABoxd &aaBox(int left) {
        return left? leftAABox : rightAABox;
    }
};

BspNode::BspNode(Partition const &partition)
    : BspElement(Node)
    , d(new Instance(partition))
{
    setRightAABox(0);
    setLeftAABox(0);
}

Partition const &BspNode::partition() const
{
    return d->partition;
}

size_t BspNode::height() const
{
    DENG2_ASSERT(hasLeft() || hasRight());
    size_t rHeight = 0;
    if(hasRight() && right().type() == Node)
        rHeight = right().as<BspNode>().height();
    size_t lHeight = 0;
    if(hasLeft() && left().type() == Node)
        lHeight = left().as<BspNode>().height();
    return (rHeight> lHeight? rHeight : lHeight) + 1;
}

bool BspNode::hasChild(int left) const
{
    return *d->childAdr(left) != 0;
}

BspElement &BspNode::child(int left)
{
    if(BspElement *childElm = *d->childAdr(left))
    {
        return *childElm;
    }
    /// @throw MissingChildError  The specified child element is missing.
    throw MissingChildError("BspNode::child", QString("No %1 child is configured").arg(left? "left" : "right"));
}

BspElement const &BspNode::child(int left) const
{
    return const_cast<BspElement &>(const_cast<BspNode *>(this)->child(left));
}

void BspNode::setChild(int left, BspElement *newChild)
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
