/**
 * @file string.hh
 * C++ wrapper for ddstring_t. @ingroup base
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

//#include "bsp_main.h"
#include "bspbuilder/hedges.hh"
//#include "bspbuilder/intersection.hh"
#include "bspbuilder/superblockmap.hh"

#include "m_binarytree.h"

struct hplane_s;
struct hplanebuildinfo_s;
struct hplaneintercept_s;
struct bsp_hedge_s;
struct hedgeintercept_s;
struct vertex_s;
struct gamemap_s;

#ifdef __cplusplus
extern "C" {
#endif

namespace de {

class BspBuilder {
private:
    int placeholder;

public:
    BspBuilder()
    {}

    ~BspBuilder()
    {}

    void initForMap(struct gamemap_s* map);

    bspleafdata_t* BspBuilder::createBSPLeaf(SuperBlock* hEdgeList);

    boolean build(GameMap* map, Vertex*** vertexes, uint* numVertexes);

    struct hplaneintercept_s* makeHPlaneIntersection(struct hplane_s* hPlane, bsp_hedge_t* hEdge, int leftSide);

    struct hplaneintercept_s* makeIntersection(struct hplane_s* hPlane, bsp_hedge_t* hEdge, int leftSide);

    /**
     * Initially create all half-edges, one for each side of a linedef.
     *
     * @return  The list of created half-edges.
     */
    SuperBlockmap* createInitialHEdges(struct gamemap_s* map);

    bspleafdata_t* newLeaf(void);

    void deleteLeaf(bspleafdata_t* leaf);

    void initHPlaneInterceptAllocator(void);
    void shutdownHPlaneInterceptAllocator(void);

    void mergeIntersections(struct hplane_s* intersections);

    void buildHEdgesAtIntersectionGaps(struct hplane_s* hPlane,
        struct superblock_s* rightList, struct superblock_s* leftList);

    void addEdgeTip(struct vertex_s* vert, double dx, double dy, bsp_hedge_t* back,
        bsp_hedge_t* front);

    /**
     * Destroy the specified intersection.
     *
     * @param inter  Ptr to the intersection to be destroyed.
     */
    void deleteHEdgeIntercept(struct hedgeintercept_s* intercept);

    /**
     * Build the BSP for the given map.
     *
     * @param map           The map to build the BSP for.
     * @param vertexes      Editable vertex (ptr) array.
     * @param numVertexes   Number of vertexes in the array.
     *
     * @return  @c true= iff completed successfully.
     */
    //boolean build(GameMap* map, Vertex*** vertexes, uint* numVertexes);

    /**
     * Init the half-edge block allocator.
     */
    void initHEdgeAllocator(void/*BspBuilder* builder*/);

    /**
     * Shutdown the half-edge block allocator. All elements will be free'd!
     */
    void shutdownHEdgeAllocator(void);

    /**
     * Destroys the given half-edge.
     *
     * @param hEdge  Ptr to the half-edge to be destroyed.
     */
    void deleteHEdge(bsp_hedge_t* hEdge);

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
     * @note We must update the count values of any superblock that contains the
     *       half-edge (and/or backseg), so that future processing is not messed up
     *       by incorrect counts.
     */
    bsp_hedge_t* splitHEdge(bsp_hedge_t* oldHEdge, double x, double y);

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
    void divideHEdge(struct bsp_hedge_s* hEdge, struct hplane_s* hPlane,
        SuperBlock* rightList, SuperBlock* leftList);

    /**
     * Find the best half-edge in the list to use as a partition.
     *
     * @param hEdgeList     List of half-edges to choose from.
     * @param depth         Current node depth.
     * @param partition     Ptr to partition to be updated with the results.
     *
     * @return  @c true= A suitable partition was found.
     */
    boolean choosePartition(SuperBlock* hEdgeList, size_t depth, struct hplane_s* hPlane);

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
     * @param hPlane        struct hplaneintercept_s list for storing any new intersections.
     * @return  @c true iff successfull.
     */
    boolean buildNodes(SuperBlock* superblock, binarytree_t** parent,
        size_t depth, struct hplane_s* hPlane);

    /**
     * Traverse the BSP tree and put all the half-edges in each BSP leaf into clockwise
     * order, and renumber their indices.
     *
     * @important This cannot be done during BspBuilder::buildNodes() since splitting a half-edge with
     * a twin may insert another half-edge into that twin's list, usually in the wrong
     * place order-wise.
     */
    void windLeafs(binarytree_t* rootNode);

    /**
     * Remove all the half-edges from the list, partitioning them into the left or right
     * lists based on the given partition line. Adds any intersections onto the
     * intersection list as it goes.
     */
    void partitionHEdges(SuperBlock* hEdgeList, SuperBlock* rightList,
        SuperBlock* leftList, struct hplane_s* hPlane);

    void addHEdgesBetweenIntercepts(struct hplane_s* hPlane,
        struct hedgeintercept_s* start, struct hedgeintercept_s* end, struct bsp_hedge_s** right, struct bsp_hedge_s** left);

    /**
     * Analyze the intersection list, and add any needed minihedges to the given half-edge lists
     * (one minihedge on each side).
     *
     * @note All the intersections in the hPlane will be free'd back into the quick-alloc list.
     */
    void addMiniHEdges(struct hplane_s* hPlane, struct superblock_s* rightList,
                       struct superblock_s* leftList);

    /**
     * Search the given list for an intercept, if found; return it.
     *
     * @param list  The list to be searched.
     * @param vert  Ptr to the vertex to look for.
     *
     * @return  Ptr to the found intercept, else @c NULL;
     */
    struct hplaneintercept_s* hplaneInterceptByVertex(struct hplane_s* hPlane, Vertex* vertex);

    /**
     * Create a new intersection.
     */
    struct hedgeintercept_s* newHEdgeIntercept(Vertex* vertex,
        const struct hplanebuildinfo_s* partition, boolean lineDefIsSelfReferencing);

    /**
     * Create a new half-edge.
     */
    bsp_hedge_t* newHEdge(LineDef* line, LineDef* sourceLine, Vertex* start, Vertex* end,
        Sector* sec, boolean back);

    struct hedgeintercept_s* hedgeInterceptByVertex(struct hplane_s* hPlane, Vertex* vertex);

    /**
     * Check whether a line with the given delta coordinates and beginning at this
     * vertex is open. Returns a sector reference if it's open, or NULL if closed
     * (void space or directly along a linedef).
     */
    Sector* openSectorAtPoint(Vertex* vert, double dx, double dy);
};

} // namespace de

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_BSPBUILDER_HH
