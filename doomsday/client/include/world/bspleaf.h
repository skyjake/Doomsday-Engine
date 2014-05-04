/** @file bspleaf.h  World map BSP leaf.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_WORLD_BSPLEAF_H
#define DENG_WORLD_BSPLEAF_H

#include <de/Error>
#include "MapElement"
#include "ConvexSubspace"
#include "Sector"

class SectorCluster;

/**
 * Represents a leaf in the map's binary space partition (BSP) tree. Each leaf
 * defines a half-space of the parent space (a node, or the whole map space).
 *
 * A leaf may be assigned a two dimensioned convex subspace geometry, which, is
 * represented by a face (polygon) in the map's half-edge @ref de::Mesh.
 *
 * Each leaf is attributed to a @ref Sector in the map regardless of whether a
 * closed convex geometry exists at the leaf.
 *
 * On client side a leaf also provides / links to various geometry data assets
 * and properties used to visualize the subspace.
 *
 * @see http://en.wikipedia.org/wiki/Binary_space_partitioning
 *
 * @ingroup world
 */
class BspLeaf : public de::MapElement
{
    DENG2_NO_COPY  (BspLeaf)
    DENG2_NO_ASSIGN(BspLeaf)

public:
    /// Required subspace is missing. @ingroup errors
    DENG2_ERROR(MissingSubspaceError);

public:
    /**
     * Construct a new BSP leaf and optionally attribute it to @a sector.
     * Ownership is unaffected.
     */
    explicit BspLeaf(Sector *sector = 0);

    /**
     * Convenient method of returning the parent sector of the BSP leaf.
     */
    inline Sector &sector() { return parent().as<Sector>(); }

    /// @copydoc sector()
    inline Sector const &sector() const { return parent().as<Sector>(); }

    /**
     * Convenient method returning a pointer to the sector attributed to the
     * BSP leaf. If not attributed then @c 0 is returned.
     *
     * @see sector()
     */
    inline Sector *sectorPtr() { return hasParent()? &sector() : 0; }

    /// @copydoc sectorPtr()
    inline Sector const *sectorPtr() const { return hasParent()? &sector() : 0; }

    /**
     * Determines whether a subspace geometry is attributed to the BSP leaf.
     *
     * @see subspace(), setSubspace()
     */
    bool hasSubspace() const;

    /**
     * Returns the ConvexSubspace attributed to the BSP leaf.
     *
     * @see hasSubspace()
     */
    ConvexSubspace &subspace() const;

    inline ConvexSubspace *subspacePtr() const { return hasSubspace()? &subspace() : 0; }

    /**
     * Change the subspace geometry attributed to the BSP leaf.
     *
     * @param newSubspace New subspace to attribute to the BSP leaf. Ownership
     *                    of the subspace is given to BspLeaf. Use @c 0 to clear
     *                    the attribution (and destroy the old subspace).
     *
     * @see hasSubspace(), subspace()
     */
    void setSubspace(ConvexSubspace *newSubspace);

public: /// Convenience accessors @todo remove ---------------------------------

    inline bool hasPoly() const              { return hasSubspace(); }
    inline de::Face const &poly() const      { return subspace().poly(); }
    inline SectorCluster &cluster() const    { return subspace().cluster(); }

    inline bool hasCluster() const           {
        return hasSubspace() && subspace().hasCluster();
    }

    inline SectorCluster *clusterPtr() const {
        return hasSubspace()? subspace().clusterPtr() : 0;
    }

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_BSPLEAF_H
