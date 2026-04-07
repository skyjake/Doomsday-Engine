/** @file bspleaf.h  Map BSP half-space (leaf).
 * @ingroup world
 *
 * @authors Copyright © 2014-2016 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include <de/error.h>
#include <doomsday/world/bspnode.h>

namespace world {

class ConvexSubspace;
class Subsector;
class Sector;

/**
 * Represents a leaf in the map's binary space partition (BSP) tree. Each leaf defines a
 * half-space of the parent space (a node, or the whole map space).
 *
 * A leaf may be attributed to a two-dimensioned ConvexSubspace geometry.
 *
 * Each leaf is attributed to a @ref Sector in the map regardless of whether a convex
 * geometry exists at the leaf.
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 */
class LIBDOOMSDAY_PUBLIC BspLeaf : public BspElement
{
public:
    /// Required subspace is missing. @ingroup errors
    DE_ERROR(MissingSubspaceError);

public:
    /**
     * Construct a new BSP leaf and optionally attribute it to @a sector. Ownership is unaffected.
     */
    explicit BspLeaf(Sector *sector = nullptr);

    /**
     * Returns @c true if subspace geometry is attributed to the BSP leaf.
     *
     * @see subspace(), setSubspace()
     */
    bool hasSubspace() const;

    /**
     * Returns the subspace attributed to the BSP leaf.
     *
     * @see hasSubspace()
     */
    ConvexSubspace &subspace   () const;
    ConvexSubspace *subspacePtr() const;

    /**
     * Change the subspace geometry attributed to the BSP leaf.
     *
     * @param newSubspace   Subspace to attribute to the BSP leaf (use @c nullptr to clear).
     * Ownership is unaffected.
     *
     * @see hasSubspace(), subspace()
     */
    void setSubspace(ConvexSubspace *newSubspace);

    /**
     * Convenient method returning a pointer to the Sector attributed to the BSP leaf.
     *
     * Note that this does @em not necessarily mean there is a subspace at this leaf.
     * Usually one should resolve the sector from the subspace. This method is primarily
     * intended for legacy compatibility logics which don't care if a subspace exists at
     * the leaf, or not.
     */
    inline Sector       *sectorPtr()       { return _sector; }
    inline const Sector *sectorPtr() const { return _sector; }

    /**
     * Attribute this BSP leaf to the given world @a sector.
     */
    void setSector(Sector *newSector);

private:
    Sector *_sector = nullptr;
    ConvexSubspace *_subspace = nullptr;
};

}  // namespace world
