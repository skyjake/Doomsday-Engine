/** @file bspnode.h  Map BSP node.
 * @ingroup world
 *
 * @authors Copyright © 2015-2016 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_BSPNODE_H
#define LIBDOOMSDAY_WORLD_BSPNODE_H

#include "../libdoomsday.h"
#include <de/BinaryTree>
#include <de/Partition>

namespace world {

/**
 * Base class for BSP tree elements.
 */
class LIBDOOMSDAY_PUBLIC BspElement
{
public:
    virtual ~BspElement() {}
    DENG2_AS_IS_METHODS()
};

/**
 * Represents a node in the map's binary space partition (BSP) tree. Each node defines
 * a partition line which divides the subspace in two, a left child and a right child
 * subspace. Each child may be associated with either another node, or a @ref BspLeaf.
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 *
 * @ingroup world
 */
class LIBDOOMSDAY_PUBLIC BspNode : public BspElement, public de::Partition
{
public:
    BspNode(de::Partition const &partition);
};

/**
 * Tree of binary space partition elements (BSP tree).
 */
typedef de::BinaryTree<BspElement *> BspTree;

}  // namespace world

#endif  // LIBDOOMSDAY_WORLD_BSPNODE_H
