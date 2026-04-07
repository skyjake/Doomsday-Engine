/** @file partitioner.cpp  World map, binary space partitioner.
 *
 * @authors Copyright © 2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
 * @authors Copyright © 1997-1998 Raphael.Quinet <raphael.quinet@eed.ericsson.se>
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

#include "doomsday/world/bsp/partitioner.h"

#include "doomsday/world/bspleaf.h"
#include "doomsday/world/bspnode.h"
#include "doomsday/world/line.h"
#include "doomsday/world/sector.h"
#include "doomsday/world/vertex.h"
#include "doomsday/world/bsp/convexsubspaceproxy.h"
#include "doomsday/world/bsp/edgetip.h"
#include "doomsday/world/bsp/hplane.h"
#include "doomsday/world/bsp/linesegment.h"
#include "doomsday/world/bsp/partitionevaluator.h"
#include "doomsday/world/bsp/superblockmap.h"

#include <de/legacy/vector1.h>

#include <de/hash.h>
#include <de/logbuffer.h>
#include <algorithm>

using namespace de;

namespace world {

using namespace bsp;

using Lines            = List<Line *>;
using LineSegments     = List<LineSegment *>;
using LineSegmentSides = List<LineSegmentSide *>;
using SubspaceProxys   = std::list<ConvexSubspaceProxy>;
using EdgeTipSetMap    = Hash<Vertex *, EdgeTips>;

DE_PIMPL(Partitioner)
{
    int splitCostFactor = 7; ///< Cost of splitting a line segment.
    
    Lines lines;          ///< Set of map lines to build from (in index order, not owned).
    mesh::Mesh *mesh = nullptr; ///< Provider of map geometries (cf. Factory).
    
    int segmentCount = 0; ///< Running total of segments built.
    int vertexCount  = 0; ///< Running total of vertexes built.
    
    LineSegments   lineSegments; ///< Line segments in the plane.
    SubspaceProxys subspaces;    ///< Proxy subspaces in the plane.
    EdgeTipSetMap  edgeTipSets;  ///< One set for each vertex.
    
    BspTree *bspRoot = nullptr; ///< The BSP tree under construction.
    HPlane   hplane;            ///< Current space half-plane (partitioner state).

    struct LineSegmentBlockTree
    {
        LineSegmentBlockTreeNode *rootNode;

        /// @param bounds  Map space bounding box for the root block.
        LineSegmentBlockTree(const AABox &bounds)
            : rootNode(new LineSegmentBlockTreeNode(new LineSegmentBlock(bounds)))
        {}

        ~LineSegmentBlockTree()
        {
            clear();
        }

        /// Implicit conversion from LineSegmentBlockTree to root tree node.
        inline operator LineSegmentBlockTreeNode /*const*/ &()
        {
            return *rootNode;
        }

    private:
        void clear()
        {
            delete rootNode;
            rootNode = nullptr;
        }
    };

    Impl(Public *i) : Base(i) {}
    ~Impl() { clear(); }

    static int clearBspElementWorker(BspTree &subtree, void *)
    {
        delete subtree.userData();
        subtree.setUserData(nullptr);
        return 0; // Continue iteration.
    }

    void clearBspTree()
    {
        if(!bspRoot) return;
        bspRoot->traversePostOrder(clearBspElementWorker);
        delete bspRoot; bspRoot = nullptr;
    }

    void clear()
    {
        //clearBspTree();

        lines.clear();
        mesh = nullptr;
        deleteAll(lineSegments);
        lineSegments.clear();
        subspaces.clear();
        edgeTipSets.clear();
        hplane.clearIntercepts();

        segmentCount = vertexCount = 0;
    }

    /**
     * Returns a newly allocated Vertex at the given map space @a origin from the
     * map geometry mesh (ownership is @em not given to the caller).
     */
    Vertex *makeVertex(const Vec2d &origin)
    {
        Vertex *vtx = mesh->newVertex(origin);
        vertexCount += 1; // We built another one.
        return vtx;
    }

    /**
     * @return The new line segment (front is from @a start to @a end).
     */
    LineSegment *makeLineSegment(Vertex &start, Vertex &end, Sector *frontSec,
        Sector *backSec, LineSide *frontSide, Line *partitionLine = nullptr)
    {
        LineSegment *newSeg = new LineSegment(start, end);
        lineSegments << newSeg;

        LineSegmentSide &front = newSeg->front();
        front.setMapSide(frontSide);
        front.setPartitionMapLine(partitionLine);
        front.setSector(frontSec);

        LineSegmentSide &back = newSeg->back();
        back.setMapSide(frontSide? &frontSide->back() : nullptr);
        back.setPartitionMapLine(partitionLine);
        back.setSector(backSec);

        return newSeg;
    }

    /**
     * Link @a seg into the line segment block tree.
     *
     * Performs k-d tree subdivision of the 2D coordinate space, splitting the node
     * tree as necessary, however new nodes are created only when they need to be
     * populated (i.e., a split does not generate two nodes at the same time).
     */
    void linkLineSegmentInBlockTree(LineSegmentBlockTreeNode &node_, LineSegmentSide &seg)
    {
        // Traverse the node tree beginning at "this" node.
        for(LineSegmentBlockTreeNode *node = &node_; ;)
        {
            LineSegmentBlock &block = *node->userData();
            const AABox &bounds     = block.bounds();

            // The segment "touches" this node; increment the ref counters.
            block.addRef(seg);

            // Determine whether further subdivision is necessary/possible.
            Vec2i dimensions(Vec2i(bounds.max) - Vec2i(bounds.min));
            if(dimensions.x <= 256 && dimensions.y <= 256)
            {
                // Thats as small as we go; link it in and return.
                block.link(seg);
                seg.setBlockTreeNode(node);
                return;
            }

            // Determine whether the node should be split and on which axis.
            const int splitAxis = (dimensions.x < dimensions.y); // x=0, y=1
            const int midOnAxis = (bounds.min[splitAxis] + bounds.max[splitAxis]) / 2;
            LineSegmentBlockTreeNode::ChildId fromSide = LineSegmentBlockTreeNode::ChildId(seg.from().origin()[splitAxis] >= midOnAxis);
            LineSegmentBlockTreeNode::ChildId toSide   = LineSegmentBlockTreeNode::ChildId(seg.to  ().origin()[splitAxis] >= midOnAxis);

            // Does the segment lie entirely within one half of this node?
            if(fromSide != toSide)
            {
                // No, the segment crosses @var midOnAxis; link it in and return.
                block.link(seg);
                seg.setBlockTreeNode(node);
                return;
            }

            // Do we need to create the child node?
            if(!node->hasChild(fromSide))
            {
                const bool toLeft = (fromSide == LineSegmentBlockTreeNode::Left);

                AABox childBounds;
                if(splitAxis)
                {
                    // Vertical split.
                    int division = bounds.minY + 0.5 + (bounds.maxY - bounds.minY) / 2;

                    childBounds.minX = bounds.minX;
                    childBounds.minY = (toLeft? division : bounds.minY);

                    childBounds.maxX = bounds.maxX;
                    childBounds.maxY = (toLeft? bounds.maxY : division);
                }
                else
                {
                    // Horizontal split.
                    int division = bounds.minX + 0.5 + (bounds.maxX - bounds.minX) / 2;

                    childBounds.minX = (toLeft? division : bounds.minX);
                    childBounds.minY = bounds.minY;

                    childBounds.maxX = (toLeft? bounds.maxX : division);
                    childBounds.maxY = bounds.maxY;
                }

                // Add a new child node and link it to its parent.
                LineSegmentBlock *childBlock = new LineSegmentBlock(childBounds);
                node->setChild(fromSide, new LineSegmentBlockTreeNode(childBlock, node));
            }

            // Descend to the child node.
            node = node->childPtr(fromSide);
        }
    }

    /**
     * Returns the EdgeTips set associated with @a vertex.
     */
    EdgeTips &edgeTipSet(const Vertex &vertex)
    {
        EdgeTipSetMap::iterator found = edgeTipSets.find(const_cast<Vertex *>(&vertex));
        if(found == edgeTipSets.end())
        {
            // Time to construct a new set.
            found = edgeTipSets.insert(const_cast<Vertex *>(&vertex), EdgeTips());
        }
        return found->second;
    }

    /**
     * Create all initial line segments. We can be certain there are no zero-length
     * lines as these are screened earlier.
     */
    void createInitialLineSegments(LineSegmentBlockTreeNode &rootNode)
    {
        for(Line *line : lines)
        {
            Sector *frontSec = line->front().sectorPtr();
            Sector *backSec  = line->back().sectorPtr();

            // Handle the "one-way window" effect.
            if(!backSec && line->_bspWindowSector)
            {
                backSec = line->_bspWindowSector;
            }

            LineSegment *seg = makeLineSegment(line->from(), line->to(),
                                               frontSec, backSec, &line->front());

            if(seg->front().hasSector())
            {
                linkLineSegmentInBlockTree(rootNode, seg->front());
            }
            if(seg->back().hasSector())
            {
                linkLineSegmentInBlockTree(rootNode, seg->back());
            }

            edgeTipSet(line->from()) << EdgeTip(seg->front());
            edgeTipSet(line->to())   << EdgeTip(seg->back());
        }
    }

    /**
     * Splits the given line segment at the point (x,y). The new line segment
     * is returned. The old line segment is shortened (the original start vertex
     * is unchanged), the new line segment becomes the cut-off tail (keeping
     * the original end vertex).
     *
     * @note If the line segment has a twin it is also split.
     */
    LineSegmentSide &splitLineSegment(LineSegmentSide &frontLeft,
        const Vec2d &point, bool updateEdgeTips = true)
    {
        DE_ASSERT(point != frontLeft.from().origin() &&
                     point != frontLeft.to().origin());

        //LOG_DEBUG("Splitting line segment %p at %s")
        //        << &frontLeft << point.asText();

        Vertex *newVert = makeVertex(point);

        LineSegment &oldSeg = frontLeft.line();
        LineSegment &newSeg = *makeLineSegment(oldSeg.from(), oldSeg.to(),
                                               oldSeg.front().sectorPtr(),
                                               oldSeg.back().sectorPtr(),
                                               oldSeg.front().mapSidePtr(),
                                               oldSeg.front().partitionMapLine());

        // Perform the split, updating vertex and relative segment links.
        LineSegmentSide &frontRight = newSeg.side(frontLeft.lineSideId());

        oldSeg.replaceVertex(frontLeft.lineSideId() ^ LineSegment::To, *newVert);
        newSeg.replaceVertex(frontLeft.lineSideId(),                   *newVert);

        LineSegmentSide &backRight = frontLeft.back();
        LineSegmentSide &backLeft  = frontRight.back();

        if(ConvexSubspaceProxy *convexSet = frontLeft.convexSubspace())
        {
            *convexSet << frontRight;
            frontRight.setConvexSubspace(convexSet);
        }

        frontLeft.setRight(&frontRight);
        frontRight.setLeft(&frontLeft);

        // Handle the twin.
        if(ConvexSubspaceProxy *convexSet = backRight.convexSubspace())
        {
            *convexSet << backLeft;
            backLeft.setConvexSubspace(convexSet);
        }

        backLeft.setRight(&backRight);
        backRight.setLeft(&backLeft);

        if(updateEdgeTips)
        {
            /// @todo Optimize: Avoid clearing tips by implementing update logic.
            edgeTipSet(oldSeg.from()).clearByLineSegment(oldSeg);
            edgeTipSet(oldSeg.to()  ).clearByLineSegment(oldSeg);

            edgeTipSet(newSeg.from()).clearByLineSegment(newSeg);
            edgeTipSet(newSeg.to()  ).clearByLineSegment(newSeg);

            edgeTipSet(oldSeg.from()) << EdgeTip(oldSeg.front());
            edgeTipSet(oldSeg.to())   << EdgeTip(oldSeg.back());
            edgeTipSet(newSeg.from()) << EdgeTip(newSeg.front());
            edgeTipSet(newSeg.to())   << EdgeTip(newSeg.back());
        }

        return frontRight;
    }

    /**
     * Find the intersection point between a line segment and the current
     * partition plane. Takes advantage of some common situations like
     * horizontal and vertical lines to choose a 'nicer' intersection point.
     */
    Vec2d intersectPartition(const LineSegmentSide &seg, coord_t fromDist,
                                coord_t toDist) const
    {
        // Horizontal partition vs vertical line segment.
        if(hplane.slopeType() == ST_HORIZONTAL && seg.slopeType() == ST_VERTICAL)
        {
            return Vec2d(seg.from().origin().x, hplane.partition().origin.y);
        }

        // Vertical partition vs horizontal line segment.
        if(hplane.slopeType() == ST_VERTICAL && seg.slopeType() == ST_HORIZONTAL)
        {
            return Vec2d(hplane.partition().origin.x, seg.from().origin().y);
        }

        // 0 = start, 1 = end.
        coord_t ds = fromDist / (fromDist - toDist);

        Vec2d point = seg.from().origin();
        if(seg.slopeType() != ST_VERTICAL)
            point.x += seg.direction().x * ds;

        if(seg.slopeType() != ST_HORIZONTAL)
            point.y += seg.direction().y * ds;

        return point;
    }

    /// @todo refactor away
    inline void interceptPartition(LineSegmentSide &seg, int edge)
    {
        hplane.intercept(seg, edge, edgeTipSet(seg.vertex(edge)));
    }

    /**
     * Take the given line segment @a lineSeg, compare it with the partition
     * plane and determine into which of the two sets it should be. If the
     * line segment is found to intersect the partition, the intercept point
     * is determined and the line segment then split in two at this point.
     * Each piece of the line segment is then added to the relevant set.
     *
     * If the line segment is collinear with, or intersects the partition then
     * a new intercept is added to the partitioning half-plane.
     *
     * @note Any existing @em twin of @a lineSeg is so too handled uniformly.
     *
     * @param seg     Line segment to be "partitioned".
     * @param rights  Set of line segments on the right side of the partition.
     * @param lefts   Set of line segments on the left side of the partition.
     */
    void divideOneSegment(LineSegmentSide &seg, LineSegmentBlockTreeNode &rights,
                          LineSegmentBlockTreeNode &lefts)
    {
        coord_t fromDist, toDist;
        LineRelationship rel = hplane.relationship(seg, &fromDist, &toDist);
        switch(rel)
        {
        case Collinear: {
            interceptPartition(seg, LineSegment::From);
            interceptPartition(seg, LineSegment::To);

            // Direction (vs that of the partition plane) determines in which
            // subset this line segment belongs.
            if(seg.direction().dot(hplane.partition().direction) < 0)
            {
                linkLineSegmentInBlockTree(lefts, seg);
            }
            else
            {
                linkLineSegmentInBlockTree(rights, seg);
            }
            break; }

        case Right:
        case RightIntercept:
            if(rel == RightIntercept)
            {
                // Direction determines which edge of the line segment interfaces
                // with the new half-plane intercept.
                interceptPartition(seg, (fromDist < DIST_EPSILON? LineSegment::From : LineSegment::To));
            }
            linkLineSegmentInBlockTree(rights, seg);
            break;

        case Left:
        case LeftIntercept:
            if(rel == LeftIntercept)
            {
                interceptPartition(seg, (fromDist > -DIST_EPSILON? LineSegment::From : LineSegment::To));
            }
            linkLineSegmentInBlockTree(lefts, seg);
            break;

        case Intersects: {
            // Calculate the intersection point and split this line segment.
            Vec2d point = intersectPartition(seg, fromDist, toDist);
            LineSegmentSide &newFrontRight = splitLineSegment(seg, point);

            // Ensure the new back left segment is inserted into the same block as
            // the old back right segment.
            if (LineSegmentBlockTreeNode *backLeftBlock =
                    reinterpret_cast<LineSegmentBlockTreeNode *>(seg.back().blockTreeNodePtr()))
            {
                linkLineSegmentInBlockTree(*backLeftBlock, newFrontRight.back());
            }

            interceptPartition(seg, LineSegment::To);

            // Direction determines which subset the line segments are added to.
            if (fromDist < 0)
            {
                linkLineSegmentInBlockTree(rights, newFrontRight);
                linkLineSegmentInBlockTree(lefts, seg);
            }
            else
            {
                linkLineSegmentInBlockTree(rights, seg);
                linkLineSegmentInBlockTree(lefts, newFrontRight);
            }
            break; }
        }
    }

    /**
     * Remove all the line segments from the list, partitioning them into the
     * left or right sets according to their position relative to partition line.
     * Adds any intersections onto the intersection list as it goes.
     *
     * @param node    Block tree node containing the line segments to be partitioned.
     * @param rights  Set of line segments on the right side of the partition.
     * @param lefts   Set of line segments on the left side of the partition.
     */
    void divideSegments(LineSegmentBlockTreeNode &node, LineSegmentBlockTreeNode &rights,
                        LineSegmentBlockTreeNode &lefts)
    {
        /**
         * @todo Revise this algorithm so that @var segments is not modified
         * during the partitioning process.
         */
        const int totalSegs = node.userData()->totalCount();
        DE_ASSERT(totalSegs != 0);
        DE_UNUSED(totalSegs);

        // Iterative pre-order traversal of SuperBlock.
        LineSegmentBlockTreeNode *cur  = &node;
        LineSegmentBlockTreeNode *prev = nullptr;
        while(cur)
        {
            while(cur)
            {
                LineSegmentBlock &segs = *cur->userData();

                LineSegmentSide *seg;
                while((seg = segs.pop()))
                {
                    // Disassociate the line segment from the block tree.
                    seg->setBlockTreeNode(nullptr);

                    divideOneSegment(*seg, rights, lefts);
                }

                if(prev == cur->parentPtr())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->rightPtr();
                    else                cur = cur->leftPtr();
                }
                else if(prev == cur->rightPtr())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->leftPtr();
                }
                else if(prev == cur->leftPtr())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parentPtr();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parentPtr();
            }
        }

        // Sanity checks...
        DE_ASSERT(rights.userData()->totalCount());
        DE_ASSERT(lefts.userData ()->totalCount());
        DE_ASSERT((  rights.userData()->totalCount()
                      + lefts.userData ()->totalCount()) >= totalSegs);
    }

    /**
     * Analyze the half-plane intercepts, building new line segments to cap
     * any gaps (new segments are added onto the end of the appropriate list
     * (rights to @a rightList and lefts to @a leftList)).
     *
     * @param rights  Set of line segments on the right of the partition.
     * @param lefts   Set of line segments on the left of the partition.
     */
    void addPartitionLineSegments(LineSegmentBlockTreeNode &rights,
                                  LineSegmentBlockTreeNode &lefts)
    {
        LOG_TRACE("Building line segments along partition %s",
                  hplane.partition().asText());

        // First, fix any near-distance issues with the intercepts.
        hplane.sortAndMergeIntercepts();

        //hplane.printIntercepts();

        // We must not create new line segments on top of the source partition
        // line segment (as this will result in duplicate edges finding their
        // way into the BSP leaf geometries).
        LineSegmentSide *partSeg = hplane.lineSegment();
        double nearDist = 0, farDist = 0;

        if(partSeg)
        {
            nearDist = hplane.intersect(*partSeg, LineSegment::From);
            farDist  = hplane.intersect(*partSeg, LineSegment::To);
        }

        // Create new line segments.
        for(int i = 0; i < hplane.interceptCount() - 1; ++i)
        {
            const HPlaneIntercept &cur  = hplane.intercepts()[i];
            const HPlaneIntercept &next = hplane.intercepts()[i+1];

            // Does this range overlap the partition line segment?
            if(partSeg && cur.distance() >= nearDist && next.distance() <= farDist)
                continue;

            // Void space or an existing segment between the two intercepts?
            if(!cur.after() && !next.before())
                continue;

            // Check for some nasty open/closed or close/open cases.
            if(cur.after() && !next.before())
            {
                if(!cur.lineSegmentIsSelfReferencing())
                {
                    Vec2d nearPoint = (cur.vertex().origin() + next.vertex().origin()) / 2;
                    notifyUnclosedSectorFound(*cur.after(), nearPoint);
                }
                continue;
            }

            if(!cur.after() && next.before())
            {
                if(!next.lineSegmentIsSelfReferencing())
                {
                    Vec2d nearPoint = (cur.vertex().origin() + next.vertex().origin()) / 2;
                    notifyUnclosedSectorFound(*next.before(), nearPoint);
                }
                continue;
            }

            /*
             * This is definitely open space.
             */
            Vertex &fromVertex = cur.vertex();
            Vertex &toVertex   = next.vertex();

            Sector *sector = cur.after();
            if(!cur.before() && next.before() == next.after())
            {
                sector = next.before();
            }
            // Choose the non-self-referencing sector when we can.
            else if(cur.after() != next.before())
            {
                if(!cur.lineSegmentIsSelfReferencing() &&
                   !next.lineSegmentIsSelfReferencing())
                {
                    LOG_DEBUG("Sector mismatch #%d %s != #%d %s")
                            << cur.after()->indexInMap()
                            << cur.vertex().origin().asText()
                            << next.before()->indexInMap()
                            << next.vertex().origin().asText();
                }

                LineSegmentSide *afterSeg = cur.afterLineSegment();
                if(afterSeg->hasMapLine() && afterSeg->mapLine().isSelfReferencing())
                {
                    LineSegmentSide *beforeSeg = next.beforeLineSegment();
                    if(beforeSeg->hasMapLine() && !beforeSeg->mapLine().isSelfReferencing())
                    {
                        sector = next.before();
                    }
                }
            }

            DE_ASSERT(sector);

            LineSegment &newSeg = *makeLineSegment(fromVertex, toVertex,
                                                   sector, sector, nullptr /*no map line*/,
                                                   partSeg? &partSeg->mapLine() : nullptr);

            edgeTipSet(newSeg.from()) << EdgeTip(newSeg.front());
            edgeTipSet(newSeg.to())   << EdgeTip(newSeg.back());

            // Add each new line segment to the appropriate set.
            linkLineSegmentInBlockTree(rights, newSeg.front());
            linkLineSegmentInBlockTree(lefts,  newSeg.back());

            /*
            LOG_DEBUG("Built line segment from %s to %s (sector #%i)")
                    << fromVertex.origin().asText()
                    << toVertex.origin().asText()
                    << sector->indexInArchive();
            */
        }
    }

    /**
     * Collate (unlink) all line segments at or beneath @a node to a new list.
     */
    static LineSegmentSides collectAllSegments(LineSegmentBlockTreeNode &node)
    {
        LineSegmentSides allSegs;

#ifdef DE_QT_4_7_OR_NEWER
        allSegs.reserve(node.userData()->totalCount());
#endif

        // Iterative pre-order traversal.
        LineSegmentBlockTreeNode *cur  = &node;
        LineSegmentBlockTreeNode *prev = nullptr;
        while(cur)
        {
            while(cur)
            {
                LineSegmentBlock &segs = *cur->userData();

                LineSegmentSide *seg;
                while((seg = segs.pop()))
                {
                    allSegs << seg;
                }

                if(prev == cur->parentPtr())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->rightPtr();
                    else                cur = cur->leftPtr();
                }
                else if(prev == cur->rightPtr())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->leftPtr();
                }
                else if(prev == cur->leftPtr())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parentPtr();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parentPtr();
            }
        }

        return allSegs;
    }

    /**
     * Determine the axis-aligned bounding box containing the vertex coordinates
     * from all segments.
     */
    static AABoxd segmentBounds(const LineSegmentSides &allSegments)
    {
        bool initialized = false;
        AABoxd bounds;

        for(LineSegmentSide *seg : allSegments)
        {
            if(initialized)
            {
                V2d_UniteBox(bounds.arvec2, seg->bounds().arvec2);
            }
            else
            {
                V2d_CopyBox(bounds.arvec2, seg->bounds().arvec2);
                initialized = true;
            }
        }

        return bounds;
    }

    /**
     * Determine the axis-aligned bounding box containing the vertices of all
     * line segments at or beneath @a node in the block tree.
     *
     * @return  Determined AABox (might be empty (i.e., min > max) if no segments).
     */
    static AABoxd segmentBounds(const LineSegmentBlockTreeNode &node)
    {
        bool initialized = false;
        AABoxd bounds;

        // Iterative pre-order traversal.
        const LineSegmentBlockTreeNode *cur  = &node;
        const LineSegmentBlockTreeNode *prev = nullptr;
        while(cur)
        {
            while(cur)
            {
                const LineSegmentBlock &block = *cur->userData();

                if(block.totalCount())
                {
                    AABoxd boundsAtNode = segmentBounds(block.all());
                    if(initialized)
                    {
                        V2d_AddToBox(bounds.arvec2, boundsAtNode.min);
                    }
                    else
                    {
                        V2d_InitBox(bounds.arvec2, boundsAtNode.min);
                        initialized = true;
                    }
                    V2d_AddToBox(bounds.arvec2, boundsAtNode.max);
                }

                if(prev == cur->parentPtr())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->hasRight()) cur = cur->rightPtr();
                    else                cur = cur->leftPtr();
                }
                else if(prev == cur->rightPtr())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->leftPtr();
                }
                else if(prev == cur->leftPtr())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parentPtr();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parentPtr();
            }
        }

        return bounds;
    }

    LineSegmentSide *choosePartition(LineSegmentBlockTreeNode &candidateSet)
    {
        return PartitionEvaluator(splitCostFactor).choose(candidateSet);
    }

    /**
     * Takes the line segment list and determines if it is convex, possibly
     * converting it into a BSP leaf. Otherwise, the list is divided into two
     * halves and recursion will continue on the new sub list.
     *
     * This is done by scanning all of the line segments and finding the one
     * that does the least splitting and has the least difference in numbers
     * of line segments on either side (why is this valued? -ds).
     *
     * If the line segments on the left side are convex create another leaf
     * else put the line segments into the left list.
     *
     * If the line segments on the right side are convex create another leaf
     * else put the line segments into the right list.
     *
     * @param node  Tree node for the block containing the line segments to
     *              be partitioned.
     *
     * @return  Newly created BSP subtree; otherwise @c nullptr (degenerate).
     */
    BspTree *partitionSpace(LineSegmentBlockTreeNode &node)
    {
        LOG_AS("Partitioner::partitionSpace");

        BspElement *bspElement = nullptr; ///< Built BSP map element at this node.
        BspTree *rightBspTree  = nullptr;
        BspTree *leftBspTree   = nullptr;

        // Pick a line segment to use as the next partition plane.
        if(LineSegmentSide *partSeg = choosePartition(node))
        {
            // Reconfigure the half-plane for the next round of partitioning.
            hplane.configure(*partSeg);

            /*
            LOG_TRACE("%s, segment side %p %i (segment #%i) %s %s")
                    << hplane.partition().asText()
                    << partSeg
                    << partSeg->lineSideId()
                    << lineSegments.indexOf(&partSeg->line())
                    << partSeg->from().origin().asText()
                    << partSeg->to().origin().asText();
            */

            // Take a copy of the current partition - we'll need this for any
            // BspNode we produce later.
            Partition partition(hplane.partition());

            // Create left and right block trees.
            /// @todo There should be no need to use additional independent
            ///       structures to contain these subsets.
            LineSegmentBlockTree rightTree(node.userData()->bounds());
            LineSegmentBlockTree leftTree(node.userData()->bounds());

            // Partition the line segements into two subsets according to their
            // spacial relationship with the half-plane (splitting any which
            // intersect).
            divideSegments(node, rightTree, leftTree);
            node.clear();

            addPartitionLineSegments(rightTree, leftTree);

            // Take a copy of the geometry bounds for each child/sub space
            // - we'll need this for any BspNode we produce later.
            //AABoxd rightBounds = segmentBounds(rightTree);
            //AABoxd leftBounds  = segmentBounds(leftTree);

            // Recurse on each suspace, first the right space then left.
            rightBspTree = partitionSpace(rightTree);
            leftBspTree  = partitionSpace(leftTree);

            // Collapse degenerates upward.
            if(!rightBspTree || !leftBspTree)
                return rightBspTree? rightBspTree : leftBspTree;

            // Make a new BSP node.
            bspElement = new BspNode(partition);
        }
        else
        {
            // No partition required/possible -- already convex (or degenerate).
            LineSegmentSides segments = collectAllSegments(node);
            node.clear();

            subspaces.emplace_back();
            ConvexSubspaceProxy &convexSet = subspaces.back();

            convexSet.addSegments(segments);

            for(LineSegmentSide *seg : segments)
            {
                // Attribute the segment to the convex subspace.
                seg->setConvexSubspace(&convexSet);

                // Disassociate the segment from the block tree.
                seg->setBlockTreeNode(nullptr);
            }

            // Make a new BSP leaf.
            /// @todo Defer until necessary.
            auto *leaf = new BspLeaf;

            // Attribute the leaf to the convex subspace.
            convexSet.setBspLeaf(leaf);

            bspElement = leaf;
        }

        // Make a new BSP subtree and link up the children.
        auto *subtree = new BspTree(bspElement, nullptr/*no parent*/, rightBspTree, leftBspTree);
        if(rightBspTree) rightBspTree->setParent(subtree);
        if(leftBspTree)  leftBspTree->setParent(subtree);

        return subtree;
    }

    /**
     * Split any overlapping line segments in the convex subspaces, creating new
     * line segments (and vertices) as required. A subspace may well include such
     * overlapping segments as if they do not break the convexity rule they won't
     * have been split during the partitioning process.
     *
     * @todo Perform the split in divideSpace()
     */
    void splitOverlappingSegments()
    {
        for (const auto &subspace : subspaces)
        {
            /*
             * The subspace provides a specially ordered list of the segments to
             * simplify this task. The primary clockwise ordering (decreasing angle
             * relative to the center of the subspace) places overlapping segments
             * adjacently. The secondary CounterClockwise ordering sorts the overlapping
             * segments enabling the use of single pass algorithm here.
             */
            OrderedSegments convexSet = subspace.segments();
            const int numSegments = convexSet.count();
            for(int i = 0; i < numSegments - 1; ++i)
            {
                // Determine the indice range of the partially overlapping segments.
                int k = i;
                while(de::fequal(convexSet[k + 1].fromAngle, convexSet[i].fromAngle) &&
                      ++k < numSegments - 1)
                {}

                // Split each overlapping segment at the point defined by the end
                // vertex of each of the other overlapping segments.
                for(int l = i; l < k; ++l)
                {
                    OrderedSegment &a = convexSet[l];
                    for(int m = l + 1; m <= k; ++m)
                    {
                        OrderedSegment &b = convexSet[m];

                        // Segments of the same length will not be split.
                        if(de::fequal(b.segment->length(), a.segment->length()))
                            continue;

                        // Do not attempt to split at an existing vertex.
                        /// @todo fixme: For this to happen we *must* be dealing with
                        /// an invalid mapping construct such as a two-sided line in
                        /// the void. These cannot be dealt with here as they require
                        /// a detection algorithm ran prior to splitting overlaps (so
                        /// that we can skip them here). Presently it is sufficient to
                        /// simply not split if the would-be split point is equal to
                        /// either of the segment's existing vertexes.
                        const Vec2d &point = b.segment->to().origin();
                        if(point == a.segment->from().origin() ||
                           point == a.segment->to().origin())
                            continue;

                        splitLineSegment(*a.segment, point, false /*don't update edge tips*/);
                    }
                }

                i = k;
            }
        }
    }

    void buildSubspaceGeometries()
    {
        for (const auto &subspace : subspaces)
        {
            /// @todo Move BSP leaf construction here?
            BspLeaf &bspLeaf = *subspace.bspLeaf();

            subspace.buildGeometry(bspLeaf, *mesh);

            // Account the new segments.
            /// @todo Refactor away.
            for(const OrderedSegment &oseg : subspace.segments())
            {
                if(oseg.segment->hasHEdge())
                {
                    // There is now one more line segment.
                    segmentCount += 1;
                }
            }
        }

        /*
         * Finalize the built geometry by adding a twin half-edge for any
         * which don't yet have one.
         */
        for (const auto &convexSet : subspaces)
        {
            for (const OrderedSegment &oseg : convexSet.segments())
            {
                LineSegmentSide *seg = oseg.segment;

                if(seg->hasHEdge() && !seg->back().hasHEdge())
                {
                    auto *hedge = &seg->hedge();
                    DE_ASSERT(!hedge->hasTwin());

                    // Allocate the twin from the same mesh.
                    hedge->setTwin(hedge->mesh().newHEdge(seg->back().from()));
                    hedge->twin().setTwin(hedge);
                }
            }
        }
    }

    /**
     * Notify interested parties of an unclosed sector in the map.
     *
     * @param sector    The problem sector.
     * @param nearPoint  Coordinates near to where the problem was found.
     */
    void notifyUnclosedSectorFound(Sector &sector, const Vec2d &nearPoint)
    {
        DE_NOTIFY_PUBLIC_VAR(UnclosedSectorFound, i)
        {
            i->unclosedSectorFound(sector, nearPoint);
        }
    }

#ifdef DE_DEBUG
    void printSegments(const LineSegmentSides &allSegs)
    {
        for(const LineSegmentSide *seg : allSegs)
        {
            LOG_DEBUG("Build: %s line segment %p sector: %d %s -> %s")
                    << (seg->hasMapSide()? "map" : "part")
                    << seg
                    << (seg->hasSector()? seg->sector().indexInMap() : -1)
                    << seg->from().origin().asText() << seg->to().origin().asText();
        }
    }
#endif
};

Partitioner::Partitioner(int splitCostFactor) : d(new Impl(this))
{
    setSplitCostFactor(splitCostFactor);
}

void Partitioner::setSplitCostFactor(int newFactor)
{
    d->splitCostFactor = newFactor;
}

static AABox blockmapBounds(const AABoxd &mapBounds)
{
    AABox mapBoundsi;
    mapBoundsi.minX = int( de::floor(mapBounds.minX) );
    mapBoundsi.minY = int( de::floor(mapBounds.minY) );
    mapBoundsi.maxX = int(  de::ceil(mapBounds.maxX) );
    mapBoundsi.maxY = int(  de::ceil(mapBounds.maxY) );

    AABox blockBounds;
    blockBounds.minX = mapBoundsi.minX - (mapBoundsi.minX & 0x7);
    blockBounds.minY = mapBoundsi.minY - (mapBoundsi.minY & 0x7);
    int bw = ((mapBoundsi.maxX - blockBounds.minX) / 128) + 1;
    int bh = ((mapBoundsi.maxY - blockBounds.minY) / 128) + 1;

    blockBounds.maxX = blockBounds.minX + 128 * M_CeilPow2(bw);
    blockBounds.maxY = blockBounds.minY + 128 * M_CeilPow2(bh);
    return blockBounds;
}

BspTree *Partitioner::makeBspTree(const Set<Line *> &lines, mesh::Mesh &mesh)
{
    d->clear();

    // Copy the set of lines and sort by index to ensure deterministically
    // predictable output.
    d->lines = compose<List<Line *>>(lines.begin(), lines.end());
    std::sort(d->lines.begin(), d->lines.end(), [](const Line *a, const Line *b) {
        return a->indexInMap() < b->indexInMap();
    });

    d->mesh = &mesh;

    // Initialize vertex info for the initial set of vertexes.
    d->edgeTipSets.reserve(d->lines.count() * 2);

    // Determine the bounds of the line geometry.
    AABoxd bounds;
    bool isFirst = true;
    for(Line *line : d->lines)
    {
        if(isFirst)
        {
            // The first line's bounds are used as is.
            V2d_CopyBox(bounds.arvec2, line->bounds().arvec2);
            isFirst = false;
        }
        else
        {
            // Expand the bounding box.
            V2d_UniteBox(bounds.arvec2, line->bounds().arvec2);
        }
    }

    Impl::LineSegmentBlockTree blockTree(blockmapBounds(bounds));

    d->createInitialLineSegments(blockTree);

    d->bspRoot = d->partitionSpace(blockTree);

    // At this point we know that *something* useful was built.
    d->splitOverlappingSegments();
    d->buildSubspaceGeometries();

    return d->bspRoot;
}

int Partitioner::segmentCount()
{
    return d->segmentCount;
}

int Partitioner::vertexCount()
{
    return d->vertexCount;
}

}  // namespace world
