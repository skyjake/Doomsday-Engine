/** @file bspleaf.h  World map BSP leaf half-space.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_BSPLEAF_H
#define DENG_WORLD_BSPLEAF_H

#include <de/Error>
#include "MapElement"

class ConvexSubspace;
class Sector;

/**
 * Represents a leaf in the map's binary space partition (BSP) tree. Each leaf
 * defines a half-space of the parent space (a node, or the whole map space).
 *
 * A leaf may be attributed to a two-dimensioned ConvexSubspace geometry.
 *
 * Each leaf is attributed to a @ref Sector in the map regardless of whether a
 * convex geometry exists at the leaf.
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 *
 * @ingroup world
 */
class BspLeaf : public de::MapElement
{
public:
    /// Required subspace is missing. @ingroup errors
    DENG2_ERROR(MissingSubspaceError);

public:
    /**
     * Construct a new BSP leaf and optionally attribute it to @a sector.
     * Ownership is unaffected.
     */
    BspLeaf(Sector *sector = 0);

    /**
     * Determines whether a subspace geometry is attributed to the BSP leaf half-space.
     *
     * @see subspace(), setSubspace()
     */
    bool hasSubspace() const;

    /**
     * Returns the subspace attributed to the BSP leaf half-space.
     *
     * @see hasSubspace()
     */
    ConvexSubspace &subspace() const;

    /**
     * Convenient method returning a pointer to the ConvexSubspace attributed to
     * the BSP leaf half-space; otherwise @c 0 if no subspace is assigned.
     *
     * @see subspace(), hasSubspace()
     */
    inline ConvexSubspace *subspacePtr() const { return hasSubspace()? &subspace() : 0; }

    /**
     * Change the subspace geometry attributed to the BSP leaf.
     *
     * @param newSubspace New subspace to attribute to the BSP leaf. Ownership
     *                    is unaffected. Use @c 0 to clear the attribution.
     *
     * @see hasSubspace(), subspace()
     */
    void setSubspace(ConvexSubspace *newSubspace);

    /**
     * Convenient method returning a pointer to the Sector attributed to the BSP
     * leaf half-space.
     *
     * Note that this does @em not necessarily mean there is a subspace at this
     * leaf. Usually one should resolve the sector from the subspace. This method
     * is primarily intended for legacy compatibility logics which don't care if
     * subspace exists at the leaf or not.
     */
    Sector *sectorPtr();

    /// @copydoc sectorPtr()
    Sector const *sectorPtr() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_BSPLEAF_H
