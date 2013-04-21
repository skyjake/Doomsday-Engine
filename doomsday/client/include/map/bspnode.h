/** @file bspnode.h World Map BSP Node.
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

#ifndef DENG_WORLD_MAP_BSPNODE
#define DENG_WORLD_MAP_BSPNODE

#include <de/aabox.h>
#include <de/vector1.h>

#include <de/Error>

#include "MapElement"
#include "p_dmu.h"
#include "partition.h"

/*
 * Child node identifiers:
 */
/// @addtogroup map
///@{
#define RIGHT                   0
#define LEFT                    1
///@}

/**
 * Node in the BSP tree. Children of a node can be either instances of BspNode
 * or BspLeaf.
 *
 * @todo There is a missing abstraction here. BSP tree elements should be derived
 *       from a common base class, thereby enforcing which objects can be linked
 *       into the tree. -ds
 *
 * @ingroup map
 */
class BspNode : public de::MapElement
{
public:
    /// Required child element is missing. @ingroup errors
    DENG2_ERROR(MissingChildError);

public:
    /**
     * @param origin  2D point in the map coordinate space which describes the
     *                origin of the half-plane.
     * @param angle   2D vector in the map coordinate space which describes the
     *                angle of the half-plane.
     */
    BspNode(de::Vector2d partitionOrigin, de::Vector2d partitionDirection);
    BspNode(de::Partition const &partition);

    /**
     * Returns the Partition for the BSP node.
     */
    de::Partition const &partition() const;

    /**
     * Convenient accessor method for returning the origin of the partition for
     * the BSP node.
     *
     * @see partition()
     */
    inline de::Vector2d const &partitionOrigin() const { return partition().origin; }

    /**
     * Convenient accessor method for returning the direction of the partition
     * for the BSP node.
     *
     * @see partition()
     */
    inline de::Vector2d const &partitionDirection() const { return partition().direction; }

    /**
     * Returns @c true iff the specified child is configured for the BSP node.
     */
    bool hasChild(int left) const;

    /**
     * Returns @c true iff a Right child element is configured for the BSP node.
     */
    inline bool hasRight() const { return hasChild(RIGHT); }

    /**
     * Returns @c true iff a Left child element is configured for the BSP node.
     */
    inline bool hasLeft() const { return hasChild(LEFT); }

    /**
     * Returns the specified child of the BSP node.
     *
     * @param left  If not @c 0 return the Left child; otherwise the Right child.
     *
     * @see hasChild()
     */
    de::MapElement &child(int left) const;

    /**
     * Returns a pointer to the specified child of the BSP node, which may be @c 0
     * if no child is configured.
     *
     * @param left  If not @c 0 return the Left child; otherwise the Right child.
     *
     * @see hasChild()
     */
    inline de::MapElement *childPtr(int left) const
    {
        return hasChild(left)? &child(left) : 0;
    }

    void setChild(int left, de::MapElement *newChild);

    inline void setRight(de::MapElement *newChild) { setChild(RIGHT, newChild); }

    inline void setLeft(de::MapElement *newChild) { setChild(LEFT, newChild); }

    /**
     * Returns the axis-aligned bounding box for the specified child, which, encompases
     * all the vertexes which define the geometry of that subspace of the BSP, in map
     * coordinate space units.
     */
    AABoxd const &childAABox(int left) const;

    /**
     * Returns the axis-aligned bounding box for the Right child, which, encompases all
     * the vertexes which define the geometry of that subspace of the BSP, in map
     * coordinate space units.
     */
    inline AABoxd const &rightAABox() const { return childAABox(RIGHT); }

    /**
     * Returns the axis-aligned bounding box for the Left child, which, encompases all
     * the vertexes which define the geometry of that subspace of the BSP, in map
     * coordinate space units.
     */
    inline AABoxd const &leftAABox() const { return childAABox(LEFT); }

    void setChildAABox(int left, AABoxd const *newAABox);

    inline void setRightAABox(AABoxd const *newAABox) { setChildAABox(RIGHT, newAABox); }

    inline void setLeftAABox(AABoxd const *newAABox) { setChildAABox(LEFT, newAABox); }

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_MAP_BSPNODE
