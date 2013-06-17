/** @file world/bsp/partitioner.h World Binary Space Partitioner.
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_BSP_PARTITIONER_H
#define DENG_WORLD_BSP_PARTITIONER_H

#include <de/Observers>
#include <de/Vector>

#include "world/bsp/bsptreenode.h" /// @todo remove me

class BspLeaf;
class Segment;
class Line;
class Sector;

namespace de {

class Map;
class Mesh;

namespace bsp {

/// Minimum length of a half-edge post partitioning. Used in cost evaluation.
static coord_t const SHORT_HEDGE_EPSILON = 4.0;

/// Smallest distance between two points before being considered equal.
static coord_t const DIST_EPSILON        = 1.0 / 128.0;

/// Smallest difference between two angles before being considered equal (in degrees).
static coord_t const ANG_EPSILON         = 1.0 / 1024.0;

/**
 * BSP space partitioner.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3).
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @ingroup bsp
 */
class Partitioner
{
public:
    /*
     * Observers to be notified when an unclosed sector is first found.
     */
    DENG2_DEFINE_AUDIENCE(UnclosedSectorFound,
        void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint))

    /*
     * Observers to be notified when a one-way window construct is first found.
     */
    DENG2_DEFINE_AUDIENCE(OneWayWindowFound,
        void oneWayWindowFound(Line &line, Sector &backFacingSector))

public:
    Partitioner(Map const &map, Mesh &mesh, int splitCostFactor = 7);

    /**
     * Set the cost factor associated with splitting an existing half-edge.
     *
     * @param newFactor  New split cost factor.
     */
    void setSplitCostFactor(int newFactor);

    /**
     * Build the BSP for the given map.
     *
     * High-level description (courtesy of Raphael Quinet):
     *   1. Create one Seg for each Side: pick each Line in turn.  If it
     *      has a "first" Side, then create a normal Seg.  If it has a
     *      "second" Side, then create a flipped Seg.
     *   2. Call CreateNodes with the current list of Segs.  The list of Segs is
     *      the only argument to CreateNodes.
     *   3. Save the Nodes, Segs and BspLeafs to disk.  Start with the leaves of
     *      the Nodes tree and continue up to the root (last Node).
     */
    void build();

    /**
     * Retrieve a pointer to the root BinaryTree node for the constructed BSP.
     * Even if construction fails this will return a valid node.
     *
     * The only time upon which @c 0 is returned is if called prior to calling
     * build()
     */
    BspTreeNode *root() const;

    /**
     * Retrieve the number of Segments owned by the partitioner. When the build
     * completes this number will be the total number of line segments that were
     * produced during that process. Note that as BspLeaf ownership is claimed
     * this number will decrease respectively.
     *
     * @return  Current number of Segments owned by the partitioner.
     */
    int numSegments();

    /**
     * Retrieve the number of BspLeafs owned by the partitioner. When the
     * build completes this number will be the total number of BspLeafs that
     * were produced during that process. Note that as BspLeaf ownership is
     * claimed this number will decrease respectively.
     *
     * @return  Current number of BspLeafs owned by the Partitioner.
     */
    int numLeafs();

    /**
     * Retrieve the number of BspNodes owned by the partitioner. When the
     * build completes this number will be the total number of BspNodes that
     * were produced during that process. Note that as BspNode ownership is
     * claimed this number will decrease respectively.
     *
     * @return  Current number of BspNodes owned by the partitioner.
     */
    int numNodes();

    /**
     * Retrieve the total number of Vertexes produced during the build process.
     */
    int numVertexes();

    /**
     * Retrieve the vertex with specified @a index. If the index is not valid
     * this will result in fatal error. The caller should ensure the index is
     * within valid range using numVertexes()
     */
    Vertex &vertex(int index);

    /**
     * Relinquish ownership of the specified map data element to the caller.
     *
     * @param mapElement  Map data element to relinquish ownership of.
     */
    void release(MapElement *mapElement);

private:
    DENG2_PRIVATE(d)
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_BSP_PARTITIONER_H
