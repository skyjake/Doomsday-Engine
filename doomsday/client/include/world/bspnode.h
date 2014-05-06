/** @file bspnode.h  World map BSP node.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_BSPNODE_H
#define DENG_WORLD_BSPNODE_H

#include "MapElement"
#include "partition.h"
#include <de/Error>
#include <de/aabox.h>

class BspElement
{
public:
    /// Special identifier used to mark an invalid index.
    enum { NoIndex = -1 };

    BspElement() : _indexInMap(NoIndex) {}
    virtual ~BspElement() {}
    DENG2_AS_IS_METHODS()

    /**
     * Returns the "in-map" index attributed to the BSP element.
     */
    int indexInMap() const
    {
        return _indexInMap;
    }

    /**
     * Change the "in-map" index attributed to the BSP element.
     *
     * @param newIndex  New index to attribute to the BSP element. @c NoIndex
     *                  clears the attribution (not a valid index).
     */
    void setIndexInMap(int newIndex = NoIndex)
    {
        _indexInMap = newIndex;
    }

private:
    int _indexInMap;
};

/**
 * Represents a node in the map's binary space partition (BSP) tree. Each node
 * defines a partition line which divides the subspace in two, a left child and
 * a right child subspace. Each child may be associated with either another node,
 * or a @ref BspLeaf.
 *
 * @todo There is a missing abstraction here. All BSP tree elements should be
 * derived from a common base class, thereby enforcing which objects can be
 * linked into the tree. -ds
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 *
 * @ingroup world
 */
class BspNode : public BspElement
{
    DENG2_NO_COPY  (BspNode)
    DENG2_NO_ASSIGN(BspNode)

public:
    /// Child element identifiers:
    enum { Right, Left };

public:
    /**
     * Construct a new BSP node.
     *
     * @param partition  Partition line in the map coordinate space which divides
     *                   the space into two 'child' subspaces.
     */
    BspNode(de::Partition const &partition = de::Partition());

    /**
     * Returns the space partition line at the node.
     */
    de::Partition const &partition() const;

    /**
     * Returns the axis-aligned bounding box for the specified child, which,
     * encompases all the vertexes which define the geometry of that subspace
     * of the BSP, in map coordinate space units.
     */
    AABoxd const &childAABox(int left) const;

    /**
     * Returns the axis-aligned bounding box for the Right child, which,
     * encompases all the vertexes which define the geometry of that subspace
     * of the BSP, in map coordinate space units.
     */
    inline AABoxd const &rightAABox() const { return childAABox(Right); }

    /**
     * Returns the axis-aligned bounding box for the Left child, which,
     * encompases all the vertexes which define the geometry of that subspace
     * of the BSP, in map coordinate space units.
     */
    inline AABoxd const &leftAABox() const { return childAABox(Left); }

    void setChildAABox(int left, AABoxd const *newAABox);

    inline void setRightAABox(AABoxd const *newAABox) { setChildAABox(Right, newAABox); }

    inline void setLeftAABox(AABoxd const *newAABox) { setChildAABox(Left, newAABox); }

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_BSPNODE_H
