/** @file partitioner.h BSP space partitioner.
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

#ifndef LIBDENG_BSP_PARTITIONER
#define LIBDENG_BSP_PARTITIONER

#include "map/p_maptypes.h"
#include "map/bsp/bsptreenode.h"

namespace de {
namespace bsp {

/// Minimum length of a half-edge post partitioning. Used in cost evaluation.
static const coord_t SHORT_HEDGE_EPSILON = 4.0;

/// Smallest distance between two points before being considered equal.
static const coord_t DIST_EPSILON = (1.0 / 128.0);

/// Smallest difference between two angles before being considered equal (in degrees).
static const coord_t ANG_EPSILON = (1.0 / 1024.0);

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
    Partitioner(GameMap &map, QList<Vertex *> const &editableVertexes,
                int splitCostFactor = 7);
    ~Partitioner();

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
     *   1. Create one Seg for each SideDef: pick each LineDef in turn.  If it
     *      has a "first" SideDef, then create a normal Seg.  If it has a
     *      "second" SideDef, then create a flipped Seg.
     *   2. Call CreateNodes with the current list of Segs.  The list of Segs is
     *      the only argument to CreateNodes.
     *   3. Save the Nodes, Segs and BspLeafs to disk.  Start with the leaves of
     *      the Nodes tree and continue up to the root (last Node).
     *
     * @return  @c true= iff completed successfully.
     */
    bool build();

    /**
     * Retrieve a pointer to the root BinaryTree node for the constructed BSP.
     * Even if construction fails this will return a valid node.
     *
     * The only time upon which @c NULL is returned is if called prior to calling
     * BspBuilder::build()
     */
    BspTreeNode *root() const;

    /**
     * Retrieve the number of HEdges owned by this Partitioner. When the build
     * completes this number will be the total number of half-edges that were
     * produced during that process. Note that as BspLeaf ownership is claimed
     * this number will decrease respectively.
     *
     * @return  Current number of HEdges owned by this Partitioner.
     */
    uint numHEdges();

    /**
     * Retrieve the number of BspLeafs owned by this Partitioner. When the
     * build completes this number will be the total number of BspLeafs that
     * were produced during that process. Note that as BspLeaf ownership is
     * claimed this number will decrease respectively.
     *
     * @return  Current number of BspLeafs owned by this Partitioner.
     */
    uint numLeafs();

    /**
     * Retrieve the number of BspNodes owned by this Partitioner. When the
     * build completes this number will be the total number of BspNodes that
     * were produced during that process. Note that as BspNode ownership is
     * claimed this number will decrease respectively.
     *
     * @return  Current number of BspNodes owned by this Partitioner.
     */
    uint numNodes();

    /**
     * Retrieve the total number of Vertexes produced during the build process.
     */
    uint numVertexes();

    /**
     * Retrieve the vertex with specified @a index. If the index is not valid
     * this will result in fatal error. The caller should ensure the index is
     * within valid range using Partitioner::numVertexes()
     */
    Vertex &vertex(uint index);

    /**
     * Relinquish ownership of the specified map data element to the caller.
     *
     * @param mapElement  Map data element to relinquish ownership of.
     */
    void release(MapElement *mapElement);

private:
    struct Instance;
    Instance *d;
};

} // namespace bsp
} // namespace de

#endif // LIBDENG_BSP_PARTITIONER
