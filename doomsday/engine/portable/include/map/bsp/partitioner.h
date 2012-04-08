/**
 * @file partitioner.h
 * BSP space partitioner. @ingroup bsp
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

#ifndef LIBDENG_BSP_PARTITIONER
#define LIBDENG_BSP_PARTITIONER

#include "p_mapdata.h"

#include "map/bsp/bsptreenode.h"
#include "map/bsp/bsphedgeinfo.h"
#include "map/bsp/linedefinfo.h"
#include "map/bsp/vertexinfo.h"

#include <vector>
#include <list>

#define ET_prev             link[0]
#define ET_next             link[1]
#define ET_edge             hedges

// An edge tip is where an edge meets a vertex.
struct HEdgeTip
{
    // Link in list. List is kept in ANTI-clockwise order.
    HEdgeTip* link[2]; // {prev, next};

    /// Angle that line makes at vertex (degrees; 0 is E, 90 is N).
    double angle;

    // Half-edge on each side of the edge. Left is the side of increasing
    // angles, right is the side of decreasing angles. Either can be NULL
    // for one sided edges.
    struct hedge_s* hedges[2];

    HEdgeTip() : angle()
    {
        link[0] = 0;
        link[1] = 0;
        hedges[0] = 0;
        hedges[1] = 0;
    }
};

namespace de {
namespace bsp {

const double IFFY_LEN = 4.0;

/// Smallest distance between two points before being considered equal.
const double DIST_EPSILON = (1.0 / 128.0);

/// Smallest difference between two angles before being considered equal (in degrees).
const double ANG_EPSILON = (1.0 / 1024.0);

struct HEdgeIntercept;
class HPlane;
class HPlaneIntercept;
class SuperBlock;
class SuperBlockmap;

struct PartitionCost
{
    int total;
    int splits;
    int iffy;
    int nearMiss;
    int realRight;
    int realLeft;
    int miniRight;
    int miniLeft;

    PartitionCost::PartitionCost() :
        total(0), splits(0), iffy(0), nearMiss(0), realRight(0),
        realLeft(0), miniRight(0), miniLeft(0)
    {}

    PartitionCost& operator += (const PartitionCost& other)
    {
        total     += other.total;
        splits    += other.splits;
        iffy      += other.iffy;
        nearMiss  += other.nearMiss;
        realLeft  += other.realLeft;
        realRight += other.realRight;
        miniLeft  += other.miniLeft;
        miniRight += other.miniRight;
        return *this;
    }

    PartitionCost& operator = (const PartitionCost& other)
    {
        total     = other.total;
        splits    = other.splits;
        iffy      = other.iffy;
        nearMiss  = other.nearMiss;
        realLeft  = other.realLeft;
        realRight = other.realRight;
        miniLeft  = other.miniLeft;
        miniRight = other.miniRight;
        return *this;
    }

    bool operator < (const PartitionCost& rhs) const
    {
        return total < rhs.total;
    }
};

/**
 * @algorithm High-level description (courtesy of Raphael Quinet)
 *   1 - Create one Seg for each SideDef: pick each LineDef in turn.  If it
 *       has a "first" SideDef, then create a normal Seg.  If it has a
 *       "second" SideDef, then create a flipped Seg.
 *   2 - Call CreateNodes with the current list of Segs.  The list of Segs is
 *       the only argument to CreateNodes.
 *   3 - Save the Nodes, Segs and BspLeafs to disk.  Start with the leaves of
 *       the Nodes tree and continue up to the root (last Node).
 *
 * CreateNodes does the following:
 *   1 - Pick a nodeline amongst the Segs (minimize the number of splits and
 *       keep the tree as balanced as possible).
 *   2 - Move all Segs on the right of the nodeline in a list (segs1) and do
 *       the same for all Segs on the left of the nodeline (in segs2).
 *   3 - If the first list (segs1) contains references to more than one
 *       Sector or if the angle between two adjacent Segs is greater than
 *       180 degrees, then call CreateNodes with this (smaller) list.
 *       Else, create a BspLeaf with all these Segs.
 *   4 - Do the same for the second list (segs2).
 *   5 - Return the new node (its two children are already OK).
 *
 * Each time CreateBspLeaf is called, the Segs are put in a global list.
 * When there is no more Seg in CreateNodes' list, then they are all in the
 * global list and ready to be saved to disk.
 */
class Partitioner
{
    // Used when sorting BSP leaf half-edges by angle around midpoint.
    typedef std::vector<HEdge*>HEdgeSortBuffer;

public:
    Partitioner(GameMap* _map, uint* numEditableVertexes, Vertex*** editableVertexes, int _splitCostFactor=7);
    ~Partitioner();

    Partitioner& setSplitCostFactor(int factor);

    bool build();

    BspTreeNode* root() const;

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
     * Retrieve the number of BspLeafs owned by this Partitioner. When the
     * build completes this number will be the total number of BspLeafs that
     * were produced during that process. Note that as BspLeaf ownership is
     * claimed this number will decrease respectively.
     *
     * @return  Current number of BspLeafs owned by this Partitioner.
     */
    uint numLeafs();

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
     * Retrieve the total number of Vertexes produced during the build process.
     */
    uint numVertexes();

    /**
     * Retrieve the vertex with specified @a index. If the index is not valid
     * this will result in fatal error. The caller should ensure the index is
     * within valid range using Partitioner::numVertexes()
     */
    Vertex const& vertex(uint index);

private:
    /**
     * Retrieve the extended build info for the specified @a lineDef.
     * @return  Extended info for that LineDef.
     */
    LineDefInfo& lineDefInfo(const LineDef& lineDef) {
        return lineDefInfos[lineDef.buildData.index - 1];
    }
    const LineDefInfo& lineDefInfo(const LineDef& lineDef) const {
        return lineDefInfos[lineDef.buildData.index - 1];
    }

    /**
     * Retrieve the extended build info for the specified @a vertex.
     * @return  Extended info for that Vertex.
     */
    VertexInfo& vertexInfo(const Vertex& vertex) {
        return vertexInfos[vertex.buildData.index - 1];
    }
    const VertexInfo& vertexInfo(const Vertex& vertex) const {
        return vertexInfos[vertex.buildData.index - 1];
    }

    void initForMap();

    void findMapBounds(AABoxf* aaBox) const;

    /**
     * Initially create all half-edges and add them to specified SuperBlock.
     */
    void createInitialHEdges(SuperBlock& hedgeList);

    void initHEdgesAndBuildBsp(SuperBlockmap& blockmap);

    /// @c true  Iff @a hedge has been added to a BSP leaf (i.e., it is no longer
    ///          linked in the hedge blockmap).
    bool hedgeIsInLeaf(const HEdge* hedge) const;

    const HPlaneIntercept* makePartitionIntersection(HEdge* hedge, int leftSide);

    /**
     * @return  Same as @a final for caller convenience.
     */
    HEdgeIntercept& mergeHEdgeIntercepts(HEdgeIntercept& final, HEdgeIntercept& other);

    void mergeIntersections();

    void buildHEdgesAtIntersectionGaps(SuperBlock& rightList, SuperBlock& leftList);

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
    HEdge* splitHEdge(HEdge* oldHEdge, const_pvec2d_t point);

    /**
     * Partition the given edge and perform any further necessary action (moving it
     * into either the left list, right list, or splitting it).
     *
     * Take the given half-edge 'cur', compare it with the partition line and determine
     * it's fate: moving it into either the left or right lists (perhaps both, when
     * splitting it in two). Handles the twin as well. Updates the intersection list
     * if the half-edge lies on or crosses the partition line.
     *
     * @note AJA: I have rewritten this routine based on evalPartition() (which I've
     *       also reworked, heavily). I think it is important that both these routines
     *       follow the exact same logic.
     */
    void divideHEdge(HEdge* hedge, SuperBlock& rightList, SuperBlock& leftList);

    /**
     * Remove all the half-edges from the list, partitioning them into the left or
     * right lists based on the given partition line. Adds any intersections onto the
     * intersection list as it goes.
     */
    void partitionHEdges(SuperBlock& hedgeList, SuperBlock& rightList, SuperBlock& leftList);

    void chooseNextPartitionFromSuperBlock(const SuperBlock& partList, const SuperBlock& hedgeList,
        HEdge** best, PartitionCost& bestCost);

    /**
     * Find the best half-edge in the list to use as the next partition.
     *
     * @param hedgeList     List of half-edges to choose from.
     * @return  The chosen half-edge.
     */
    HEdge* chooseNextPartition(const SuperBlock& hedgeList);

    /**
     * Takes the half-edge list and determines if it is convex, possibly converting
     * it into a BSP leaf. Otherwise, the list is divided into two halves and recursion
     * will continue on the new sub list.
     *
     * This is done by scanning all of the half-edges and finding the one that does
     * the least splitting and has the least difference in numbers of half-edges on
     * either side.
     *
     * If the ones on the left side make a BspLeaf, then create another BspLeaf
     * else put the half-edges into the left list.
     *
     * If the ones on the right side make a BspLeaf, then create another BspLeaf
     * else put the half-edges into the right list.
     *
     * @param superblock    The list of half edges at the current node.
     * @param subtree       Ptr to write back the address of any newly created subtree.
     * @return  @c true iff successfull.
     */
    bool buildNodes(SuperBlock& superblock, BspTreeNode** subtree);

    /**
     * Determine the distance (euclidean) from @a vertex to the current partition plane.
     *
     * @param vertex  Vertex to test.
     */
    inline double vertexDistanceFromPartition(const Vertex* vertex) const;

    /**
     * Determine the distance (euclidean) from @a hedge to the current partition plane.
     *
     * @param hedge  Half-edge to test.
     * @param end    @c true= use the point defined by the end (else start) vertex.
     */
    inline double hedgeDistanceFromPartition(const HEdge* hedge, bool end) const;

    /**
     * Calculate the intersection point between a half-edge and the current partition
     * plane. Takes advantage of some common situations like horizontal and vertical
     * lines to choose a 'nicer' intersection point.
     */
    void interceptHEdgePartition(const HEdge* hedge, double perpC, double perpD,
                                 pvec2d_t point) const;

    void clearPartitionIntercepts();

    bool configurePartition(const HEdge* hedge);

    /**
     * Sort half-edges by angle (from the middle point to the start vertex).
     * The desired order (clockwise) means descending angles.
     */
    static void sortHEdgesByAngleAroundPoint(HEdgeSortBuffer::iterator begin,
        HEdgeSortBuffer::iterator end, pvec2d_t point);

    /**
     * Sort the given list of half-edges into clockwise order based on their
     * position/orientation compared to the specified point.
     *
     * @param headPtr       Ptr to the address of the headPtr to the list
     *                      of hedges to be sorted.
     * @param num           Number of half edges in the list.
     * @param point         The point to order around.
     */
    static void clockwiseOrder(HEdgeSortBuffer& sortBuffer, HEdge** headPtr,
        uint num, pvec2d_t point);

    void clockwiseLeaf(BspTreeNode& tree, HEdgeSortBuffer& sortBuffer);

    /**
     * Traverse the BSP tree and put all the half-edges in each BSP leaf into clockwise
     * order, and renumber their indices.
     *
     * @important This cannot be done during BspBuilder::buildNodes() since splitting
     * a half-edge with a twin may insert another half-edge into that twin's list,
     * usually in the wrong place order-wise.
     */
    void windLeafs();

    void addHEdgesBetweenIntercepts(HEdgeIntercept* start, HEdgeIntercept* end,
                                    HEdge** right, HEdge** left);

    /**
     * Analyze the intersection list, and add any needed minihedges to the given
     * half-edge lists (one minihedge on each side).
     */
    void addMiniHEdges(SuperBlock& rightList, SuperBlock& leftList);

    /**
     * Search the given list for an intercept, if found; return it.
     *
     * @param list  The list to be searched.
     * @param vert  Ptr to the vertex to look for.
     *
     * @return  Ptr to the found intercept, else @c NULL;
     */
    const HPlaneIntercept* partitionInterceptByVertex(Vertex* vertex);

    /**
     * Create a new intersection.
     */
    HEdgeIntercept* newHEdgeIntercept(Vertex* vertex, bool lineDefIsSelfReferencing);

    /**
     * Destroy the specified intersection.
     *
     * @param inter  Ptr to the intersection to be destroyed.
     */
    void deleteHEdgeIntercept(HEdgeIntercept& intercept);

    HEdgeIntercept* hedgeInterceptByVertex(Vertex* vertex);

    void clearBspObject(BspTreeNode& tree);

    void clearAllBspObjects();

    void clearAllHEdgeInfo();

    HEdgeTip* newHEdgeTip();

    void deleteHEdgeTip(HEdgeTip* tip);

    void clearHEdgeTipsByVertex(Vertex* vtx);

    /**
     * Allocate another Vertex.
     *
     * @param point  Origin of the vertex in the map coordinate space.
     * @return  Newly created Vertex.
     */
    Vertex* newVertex(const_pvec2d_t point);

    void addHEdgeTip(Vertex* vert, double angle, HEdge* back, HEdge* front);

    /**
     * Create a new half-edge.
     */
    HEdge* newHEdge(LineDef* line, LineDef* sourceLine, Vertex* start, Vertex* end,
                    Sector* sec, bool back);

    /**
     * Create a clone of an existing half-edge.
     */
    HEdge* cloneHEdge(const HEdge& other);

    /**
     * Allocate another BspLeaf and populate it with half-edges from the supplied list.
     *
     * @param hedgeList  SuperBlock containing the list of half-edges with which
     *                   to build the leaf using.
     * @return  Newly created BspLeaf.
     */
    BspLeaf* newBspLeaf(SuperBlock& hedgeList);

    /**
     * Allocate another BspNode.
     *
     * @param origin  Origin of the half-plane in the map coordinate space.
     * @param angle  Angle of the half-plane in the map coordinate space.
     * @param rightBounds  Boundary of the right child map coordinate subspace. Can be @c NULL.
     * @param leftBoubds   Boundary of the left child map coordinate subspace. Can be @a NULL.
     *
     * @return  Newly created BspNode.
     */
    BspNode* newBspNode(double const origin[2], double const angle[2],
                        AABoxf* rightBounds, AABoxf* leftBounds);

    /**
     * Check whether a line with the given delta coordinates and beginning at this
     * vertex is open. Returns a sector reference if it's open, or NULL if closed
     * (void space or directly along a linedef).
     */
    Sector* openSectorAtAngle(Vertex* vert, double angle);

    /**
     * Initialize the extra info about the current partition plane.
     */
    void initPartitionInfo()
    {
        memset(&partitionInfo, 0, sizeof(partitionInfo));
    }

    /**
     * Update the extra info about the current partition plane.
     */
    void setPartitionInfo(const BspHEdgeInfo& info)
    {
        memcpy(&partitionInfo, &info, sizeof(partitionInfo));
    }

    /**
     * Register the specified sector in the list of unclosed sectors.
     *
     * @param sector  Sector to be registered.
     * @param x  X coordinate near the open point.
     * @param y  X coordinate near the open point.
     *
     * @return @c true iff the sector was newly registered.
     */
    bool registerUnclosedSector(Sector* sector, double x, double y);

    /**
     * Register the specified half-edge as as migrant edge of the specified sector.
     *
     * @param migrant  Migrant half-edge to register.
     * @param sector  Sector containing the @a migrant half-edge.
     *
     * @return @c true iff the half-edge was newly registered.
     */
    bool registerMigrantHEdge(HEdge* migrant, Sector* sector);

    /**
     * Look for migrant half-edges in the specified leaf and register them to the
     * list of migrants.
     *
     * @param leaf  BSP leaf to be searched.
     */
    void registerMigrantHEdges(const BspLeaf* leaf);

private:
    /// HEdge split cost factor.
    int splitCostFactor;

    /// Current map which we are building BSP data for.
    GameMap* map;

    /// @todo Refactor me away:
    uint* numEditableVertexes;
    Vertex*** editableVertexes;

    /// Running totals of constructed BSP data objects.
    uint _numNodes;
    uint _numLeafs;

    /// Extended info about LineDefs in the current map.
    typedef std::vector<LineDefInfo> LineDefInfos;
    LineDefInfos lineDefInfos;

    /// Extended info about Vertexes in the current map (including extras).
    typedef std::vector<VertexInfo> VertexInfos;
    VertexInfos vertexInfos;

    /// Extra vertexes allocated for the current map.
    typedef std::vector<Vertex*> Vertexes;
    Vertexes vertexes;

    /// Root node of our internal binary tree around which the final BSP data
    /// objects are constructed.
    BspTreeNode* rootNode;

    /// HPlane used to model the current BSP partition and the list of intercepts.
    HPlane* partition;

    /// Extra info about the partition plane.
    BspHEdgeInfo partitionInfo;

    /// Unclosed sectors are recorded here so we don't print too many warnings.
    struct UnclosedSectorRecord
    {
        Sector* sector;
        vec2d_t nearPoint;

        UnclosedSectorRecord(Sector* _sector, double x, double y)
            : sector(_sector)
        {
            V2d_Set(nearPoint, x, y);
        }
    };
    typedef std::list<UnclosedSectorRecord> UnclosedSectors;
    UnclosedSectors unclosedSectors;

    /// Migrant hedges are recorded here so we don't print too many warnings.
    struct MigrantHEdgeRecord
    {
        HEdge* hedge;
        Sector* facingSector;

        MigrantHEdgeRecord(HEdge* _hedge, Sector* _facingSector)
            : hedge(_hedge), facingSector(_facingSector)
        {}
    };
    typedef std::list<MigrantHEdgeRecord> MigrantHEdges;
    MigrantHEdges migrantHEdges;

    /// @c true = a BSP for the current map has been built successfully.
    bool builtOK;
};

} // namespace bsp
} // namespace de

#endif /// LIBDENG_BSP_PARTITIONER
