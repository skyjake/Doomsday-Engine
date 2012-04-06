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
public:
    Partitioner(GameMap* _map, uint* numEditableVertexes, Vertex*** editableVertexes, int _splitCostFactor=7);
    ~Partitioner();

    void setSplitCostFactor(int factor)
    {
        splitCostFactor = factor;
    }

    bool build();

    BspTreeNode* root() const;

    uint numNodes();

    uint numLeafs();

    uint numVertexes();

    Vertex const& vertex(uint idx);

private:
    void initForMap();

    Vertex* newVertex(const_pvec2d_t point);

    /**
     * Destroy the specified intersection.
     *
     * @param inter  Ptr to the intersection to be destroyed.
     */
    void deleteHEdgeIntercept(HEdgeIntercept& intercept);

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

    void findMapBounds(AABoxf* aaBox) const;

    /**
     * Create a new leaf from a list of half-edges.
     */
    BspLeaf* createBSPLeaf(SuperBlock& hedgeList);

    const HPlaneIntercept* makePartitionIntersection(HEdge* hedge, int leftSide);

    /**
     * Initially create all half-edges, one for each side of a linedef.
     *
     * @return  The list of created half-edges.
     */
    void createInitialHEdges(SuperBlock& hedgeList);

    void initHEdgesAndBuildBsp(SuperBlockmap& blockmap);

    /**
     * @return  Same as @a final for caller convenience.
     */
    HEdgeIntercept& mergeHEdgeIntercepts(HEdgeIntercept& final, HEdgeIntercept& other);

    void mergeIntersections();

    void buildHEdgesAtIntersectionGaps(SuperBlock& rightList, SuperBlock& leftList);

    void addHEdgeTip(Vertex* vert, double angle, HEdge* back, HEdge* front);

    /// @c true  Iff @a hedge has been added to a BSP leaf (i.e., it is no longer
    ///          linked in the hedge blockmap).
    bool hedgeIsInLeaf(const HEdge* hedge) const;

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
public:
    void divideHEdge(HEdge* hedge, SuperBlock& rightList, SuperBlock& leftList);

private:
    void clearPartitionIntercepts();

    bool configurePartition(const HEdge* hedge);

    /**
     * Find the best half-edge in the list to use as the next partition.
     *
     * @param hedgeList     List of half-edges to choose from.
     * @return  @c true= A suitable partition was found.
     */
    bool chooseNextPartition(SuperBlock& hedgeList);

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
     * @param parent        Ptr to write back the address of any newly created subtree.
     * @return  @c true iff successfull.
     */
    bool buildNodes(SuperBlock& superblock, BspTreeNode** parent);

    /**
     * Traverse the BSP tree and put all the half-edges in each BSP leaf into clockwise
     * order, and renumber their indices.
     *
     * @important This cannot be done during BspBuilder::buildNodes() since splitting
     * a half-edge with a twin may insert another half-edge into that twin's list,
     * usually in the wrong place order-wise.
     */
    void windLeafs();

    /**
     * Remove all the half-edges from the list, partitioning them into the left or
     * right lists based on the given partition line. Adds any intersections onto the
     * intersection list as it goes.
     */
    void partitionHEdges(SuperBlock& hedgeList, SuperBlock& rightList, SuperBlock& leftList);

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
     * Create a new half-edge.
     */
    HEdge* newHEdge(LineDef* line, LineDef* sourceLine, Vertex* start, Vertex* end,
                    Sector* sec, bool back);

    HEdgeTip* newHEdgeTip();

    void deleteHEdgeTip(HEdgeTip* tip);

    void deleteHEdgeTips(Vertex* vtx);

    /**
     * Create a clone of an existing half-edge.
     */
    HEdge* cloneHEdge(const HEdge& other);

    HEdgeIntercept* hedgeInterceptByVertex(Vertex* vertex);

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
     * @return @c true iff sector newly registered.
     */
    bool registerUnclosedSector(Sector* sector, double x, double y);

    bool registerMigrantHEdge(Sector* sector, HEdge* migrant);

public:
    void registerMigrantHEdges(const BspLeaf* leaf);

private:
    /// HEdge split cost factor.
    int splitCostFactor;

    /// Current map which we are building BSP data for.
    GameMap* map;

    /// @todo Refactor me away:
    uint* numEditableVertexes;
    Vertex*** editableVertexes;

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
