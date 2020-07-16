/** @file bspnode.h  Map BSP node.
 * @ingroup world
 *
 * @authors Copyright © 2015-2016 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDOOMSDAY_WORLD_BSPNODE_H
#define LIBDOOMSDAY_WORLD_BSPNODE_H

#include "../libdoomsday.h"
#include <de/binarytree.h>
#include <de/partition.h>

namespace world {

/**
 * Base class for BSP tree elements.
 */
class LIBDOOMSDAY_PUBLIC BspElement
{
public:
    virtual ~BspElement() {}
    DE_CAST_METHODS()
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
    BspNode(const de::Partition &partition);
};

/**
 * Tree of binary space partition elements (BSP tree).
 */
typedef de::BinaryTree<BspElement *> BspTree;

}  // namespace world

#endif  // LIBDOOMSDAY_WORLD_BSPNODE_H
