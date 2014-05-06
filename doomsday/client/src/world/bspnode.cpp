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

#include <de/vector1.h> /// @todo Remove me

using namespace de;

DENG2_PIMPL_NOREF(BspNode)
{
    /// Space partition (half-plane).
    Partition partition;

    /// Right and left bounding boxes for each half space.
    AABoxd rightAABox;
    AABoxd leftAABox;

    Instance(Partition const &partition) : partition(partition) {}

    inline AABoxd &aaBox(int left) { return left? leftAABox : rightAABox; }
};

BspNode::BspNode(Partition const &partition)
    : BspElement()
    , d(new Instance(partition))
{
    setRightAABox(0);
    setLeftAABox(0);
}

Partition const &BspNode::partition() const
{
    return d->partition;
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
