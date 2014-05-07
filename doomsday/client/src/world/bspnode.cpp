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
    Partition partition;  ///< Half-plane space partition.
    AABoxd rightBounds;   ///< Right half-space bounds.
    AABoxd leftBounds;    ///< Left half-space bounds.

    inline AABoxd &bounds(int left) { return left? leftBounds : rightBounds; }
};

BspNode::BspNode(Partition const &partition, AABoxd const &rightBounds, AABoxd const &leftBounds)
    : BspElement()
    , d(new Instance)
{
    d->partition   = partition;
    d->rightBounds = rightBounds;
    d->leftBounds  = leftBounds;
}

Partition const &BspNode::partition() const
{
    return d->partition;
}

AABoxd const &BspNode::childAABox(int which) const
{
    return d->bounds(which);
}

void BspNode::setChildAABox(int which, AABoxd const *newBounds)
{
    if(newBounds)
    {
        d->bounds(which) = *newBounds;
    }
    else
    {
        d->bounds(which).clear();
    }
}
