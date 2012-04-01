/**
 * @file bspbuilder.hh
 * BSP Builder. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_BSPBUILDER_HH
#define LIBDENG_BSPBUILDER_HH

#include "dd_types.h"
#include "p_mapdata.h"

#include "bspbuilder/hedges.hh"
#include "bspbuilder/intersection.hh"
#include "bspbuilder/superblockmap.hh"

struct binarytree_s;

namespace de {

/// Default cost factor attributed to splitting an existing half-edge.
#define BSPBUILDER_PARTITION_COST_HEDGESPLIT   7

class BspBuilder {
public:
    BspBuilder();
    ~BspBuilder();

    BspBuilder* setSplitCostFactor(int factor)
    {
        splitCostFactor = factor;
        return this;
    }

    void initForMap(GameMap* map);

    /**
     * Build the BSP for the given map.
     *
     * @param map           The map to build the BSP for.
     * @param vertexes      Editable vertex (ptr) array.
     * @param numVertexes   Number of vertexes in the array.
     *
     * @return  @c true= iff completed successfully.
     */
    boolean build(GameMap* map);

    /**
     * Retrieve a pointer to the root BinaryTree node for the constructed BSP.
     * Even if construction fails this will return a valid node.
     *
     * The only time upon which @c NULL is returned is if called prior to calling
     * BspBuilder::build()
     */
    struct binarytree_s* root() const;

    /**
     * Destroy the specified intersection.
     *
     * @param inter  Ptr to the intersection to be destroyed.
     */
    void deleteHEdgeIntercept(HEdgeIntercept* intercept);

private:
    BspLeaf* createBSPLeaf(SuperBlock& hedgeList);

    const HPlaneIntercept* makeHPlaneIntersection(HPlane& hplane, HEdge* hedge, int leftSide);

    const HPlaneIntercept* makeIntersection(HPlane& hplane, HEdge* hedge, int leftSide);

    SuperBlockmap* createBlockmap(const AABoxf& mapBounds);

    /**
     * Initially create all half-edges, one for each side of a linedef.
     *
     * @return  The list of created half-edges.
     */
    SuperBlockmap* createInitialHEdges(GameMap* map);

    void mergeIntersections(HPlane& intersections);

    void buildHEdgesAtIntersectionGaps(HPlane& hplane, SuperBlock& rightList, SuperBlock& leftList);

    void addEdgeTip(Vertex* vert, double dx, double dy, HEdge* back, HEdge* front);

    /**
     * Splits the given half-edge at the point (x,y). The new half-edge is returned.
     * The old half-edge is shortened (the original start vertex is unchanged), the
     * new half-edge becomes the cut-off tail (keeping the original end vertex).
     *
     * @note If the half-edge has a twin, it is also split and is inserted into the
     *       same list as the original (and after it), thus all half-edges (except
     *       the one we are currently splitting) must exist on a singly-linked list
     *       somewhere.
     *
     * @note We must update the count values of any SuperBlock that contains the
     *       half-edge (and/or backseg), so that future processing is not messed up
     *       by incorrect counts.
     */
    HEdge* splitHEdge(HEdge* oldHEdge, double x, double y);

public:
    /**
     * Partition the given edge and perform any further necessary action (moving it into
     * either the left list, right list, or splitting it).
     *
     * Take the given half-edge 'cur', compare it with the partition line, and determine
     * it's fate: moving it into either the left or right lists (perhaps both, when
     * splitting it in two). Handles the twin as well. Updates the intersection list if
     * the half-edge lies on or crosses the partition line.
     *
     * @note AJA: I have rewritten this routine based on evalPartition() (which I've also
     *       reworked, heavily). I think it is important that both these routines follow
     *       the exact same logic.
     */
    void divideHEdge(HEdge* hedge, HPlane& hplane, SuperBlock& rightList, SuperBlock& leftList);

private:
    /**
     * Find the best half-edge in the list to use as a partition.
     *
     * @param hedgeList     List of half-edges to choose from.
     * @param depth         Current node depth.
     * @param partition     Ptr to partition to be updated with the results.
     *
     * @return  @c true= A suitable partition was found.
     */
    boolean choosePartition(SuperBlock& hedgeList, size_t depth, HPlane& hplane);

    /**
     * Takes the half-edge list and determines if it is convex, possibly converting it
     * into a BSP leaf. Otherwise, the list is divided into two halves and recursion will
     * continue on the new sub list.
     *
     * This is done by scanning all of the half-edges and finding the one that does the
     * least splitting and has the least difference in numbers of half-edges on either side.
     *
     * If the ones on the left side make a BspLeaf, then create another BspLeaf
     * else put the half-edges into the left list.
     *
     * If the ones on the right side make a BspLeaf, then create another BspLeaf
     * else put the half-edges into the right list.
     *
     * @param superblock    Ptr to the list of half edges at the current node.
     * @param parent        Ptr to write back the address of any newly created subtree.
     * @param depth         Current tree depth.
     * @param hplane        HPlaneIntercept list for storing any new intersections.
     * @return  @c true iff successfull.
     */
    boolean buildNodes(SuperBlock& superblock, struct binarytree_s** parent,
                       size_t depth, HPlane& hplane);

    /**
     * Traverse the BSP tree and put all the half-edges in each BSP leaf into clockwise
     * order, and renumber their indices.
     *
     * @important This cannot be done during BspBuilder::buildNodes() since splitting a half-edge with
     * a twin may insert another half-edge into that twin's list, usually in the wrong
     * place order-wise.
     */
    void windLeafs(struct binarytree_s* rootNode);

    /**
     * Remove all the half-edges from the list, partitioning them into the left or right
     * lists based on the given partition line. Adds any intersections onto the
     * intersection list as it goes.
     */
    void partitionHEdges(HPlane& hplane, SuperBlock& hedgeList,
                         SuperBlock& rightList, SuperBlock& leftList);

    void addHEdgesBetweenIntercepts(HPlane& hplane, HEdgeIntercept* start, HEdgeIntercept* end,
                                    HEdge** right, HEdge** left);

    /**
     * Analyze the intersection list, and add any needed minihedges to the given half-edge lists
     * (one minihedge on each side).
     *
     * @note All the intersections in the hplane will be free'd back into the quick-alloc list.
     */
    void addMiniHEdges(HPlane& hplane, SuperBlock& rightList, SuperBlock& leftList);

    /**
     * Search the given list for an intercept, if found; return it.
     *
     * @param list  The list to be searched.
     * @param vert  Ptr to the vertex to look for.
     *
     * @return  Ptr to the found intercept, else @c NULL;
     */
    const HPlaneIntercept* hplaneInterceptByVertex(HPlane& hplane, Vertex* vertex);

    /**
     * Create a new intersection.
     */
    HEdgeIntercept* newHEdgeIntercept(Vertex* vertex, const BspHEdgeInfo* partition,
                                      boolean lineDefIsSelfReferencing);

    /**
     * Create a new half-edge.
     */
    HEdge* newHEdge(LineDef* line, LineDef* sourceLine, Vertex* start, Vertex* end,
                    Sector* sec, boolean back);

    /**
     * Create a clone of an existing half-edge.
     */
    HEdge* cloneHEdge(const HEdge& other);

    HEdgeIntercept* hedgeInterceptByVertex(HPlane& hplane, Vertex* vertex);

    /**
     * Check whether a line with the given delta coordinates and beginning at this
     * vertex is open. Returns a sector reference if it's open, or NULL if closed
     * (void space or directly along a linedef).
     */
    Sector* openSectorAtPoint(Vertex* vert, double dx, double dy);

private:
    int splitCostFactor;

    struct binarytree_s* rootNode;
    boolean builtOK;
};

} // namespace de

#endif /// LIBDENG_BSPBUILDER_HH
