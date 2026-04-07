/** @file partitioner.h  World map binary space partitioner.
 *
 * @authors Copyright © 2007-2016 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include "../map.h"
#include "../../mesh/mesh.h"

#include <de/set.h>
#include <de/observers.h>
#include <de/vector.h>

namespace world {

class Line;
class Sector;

namespace bsp {

/// Minimum length of a half-edge post partitioning. Used in cost evaluation.
static const double SHORT_HEDGE_EPSILON = 4.0;

/// Smallest distance between two points before being considered equal.
static const double DIST_EPSILON        = 1.0 / 128.0;

/**
 * World map binary space partitioner (BSP).
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3).
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @ingroup bsp
 */
class Partitioner
{
public:
    /// Notified when an unclosed sector is first found.
    DE_DEFINE_AUDIENCE(UnclosedSectorFound, void unclosedSectorFound(Sector &sector, const de::Vec2d &nearPoint))

public:
    /**
     * Construct a new binary space partitioner.
     *
     * @param splitCostFactor  Cost factor attributed to splitting a half-edge.
     */
    Partitioner(int splitCostFactor = 7);

    /**
     * Set the cost factor associated with splitting an existing half-edge.
     *
     * @param newFactor  New split cost factor.
     */
    void setSplitCostFactor(int newFactor);

    /**
     * Build a new BspTree for the given geometry.
     *
     * @param lines  Set of lines to construct a BSP for. A copy of the set is made however
     * the caller must ensure that line data remains accessible until the build process has
     * completed (ownership is unaffected).
     *
     * @param mesh   Mesh from which to assign new geometries. The caller must ensure that
     * the mesh remains accessible until the build process has completed (ownership is
     * unaffected).
     *
     * @return  Root tree node of the resultant BSP; otherwise @c nullptr if no usable tree
     * data was produced.
     */
    BspTree *makeBspTree(const de::Set<Line *> &lines, mesh::Mesh &mesh);

    /**
     * Retrieve the number of Segments owned by the partitioner. When the build completes
     * this number will be the total number of line segments that were produced during that
     * process. Note that as BspLeaf ownership is claimed this number will decrease respectively.
     *
     * @return  Current number of Segments owned by the partitioner.
     */
    int segmentCount();

    /**
     * Retrieve the total number of Vertexes produced during the build process.
     */
    int vertexCount();

private:
    DE_PRIVATE(d)
};

}  // namespace bsp
}  // namespace world
