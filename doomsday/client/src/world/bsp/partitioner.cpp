/** @file partitioner.cpp  World map, binary space partitioner.
 *
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

#include "de_platform.h"
#include "world/bsp/partitioner.h"

#include "BspLeaf"
#include "BspNode"
#include "Line"
#include "Sector"
#include "Vertex"

#include "world/worldsystem.h" /// validCount @todo Remove me

#include "world/bsp/convexsubspaceproxy.h"
#include "world/bsp/edgetip.h"
#include "world/bsp/hplane.h"
#include "world/bsp/linesegment.h"
#include "world/bsp/partitioncostevaluator.h"
#include "world/bsp/superblockmap.h"

#include <de/Log>
#include <de/vector1.h>
#include <QList>
#include <QHash>
#include <QtAlgorithms>
#include <algorithm>

namespace de {

using namespace bsp;

typedef QList<Line *>              Lines;
typedef QList<LineSegment *>       LineSegments;
typedef QList<LineSegmentSide *>   LineSegmentSides;
typedef QList<ConvexSubspaceProxy> SubspaceProxys;
typedef QHash<Vertex *, EdgeTips>  EdgeTipSetMap;

DENG2_PIMPL(Partitioner)
{
    int splitCostFactor;        ///< Cost of splitting a line segment.

    Lines lines;                ///< Set of map lines to build from (in index order, unowned).
    Mesh *mesh;                 ///< Provider of map geometries (cf. Factory).

    int segmentCount;           ///< Running total of segments built.
    int vertexCount;            ///< Running total of vertexes built.

    LineSegments lineSegments;  ///< Line segments in the plane.
    SubspaceProxys subspaces;   ///< Proxy subspaces in the plane.
    EdgeTipSetMap edgeTipSets;  ///< One set for each vertex.

    BspTree *bspRoot;           ///< The BSP tree under construction.
    HPlane hplane;              ///< Current space half-plane (partitioner state).

    struct SuperBlockmap
    {
        SuperBlockmapNode rootNode;

        /// @param bounds  Map space bounding box for the blockmap.
        SuperBlockmap(AABox const &bounds)
        {
            // Attach the root Node.
            SuperBlockmapNodeData *ndata = new SuperBlockmapNodeData(bounds);
            rootNode.setUserData(ndata);
            ndata->_node = &rootNode;
        }

        ~SuperBlockmap() { clear(); }

        /// Automatic translation from SuperBlockmap to the tree root.
        inline operator SuperBlockmapNode /*const*/ &() {
            return rootNode;
        }

    private:
        static int clearUserDataWorker(SuperBlockmapNode &subtree, void *)
        {
            delete subtree.userData();
            return 0;
        }

        void clear()
        {
            rootNode.traversePostOrder(clearUserDataWorker);
            rootNode.clear();
        }
    };

    Instance(Public *i)
        : Base(i)
        , splitCostFactor(7)
        , mesh           (0)
        , segmentCount   (0)
        , vertexCount    (0)
        , bspRoot        (0)
    {}

    ~Instance() { clear(); }

    static int clearBspElementWorker(BspTree &subtree, void *)
    {
        delete subtree.userData();
        subtree.setUserData(0);
        return false; // Continue iteration.
    }

    void clearBspTree()
    {
        if(!bspRoot) return;
        bspRoot->traversePostOrder(clearBspElementWorker);
        delete bspRoot; bspRoot = 0;
    }

    void clear()
    {
        //clearBspTree();

        lines.clear();
        mesh = 0;
        qDeleteAll(lineSegments);
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
    Vertex *makeVertex(Vector2d const &origin)
    {
        Vertex *vtx = mesh->newVertex(origin);
        vertexCount += 1; // We built another one.
        return vtx;
    }

    /**
     * @return The new line segment (front is from @a start to @a end).
     */
    LineSegment &buildLineSegmentBetweenVertexes(Vertex &start, Vertex &end,
        Sector *frontSec, Sector *backSec, LineSide *frontSide,
        Line *partitionLine = 0)
    {
        LineSegment *lineSeg = new LineSegment(start, end);
        lineSegments << lineSeg;

        LineSegmentSide &front = lineSeg->front();
        front.setMapSide(frontSide);
        front.setPartitionMapLine(partitionLine);
        front.setSector(frontSec);

        LineSegmentSide &back = lineSeg->back();
        back.setMapSide(frontSide? &frontSide->back() : 0);
        back.setPartitionMapLine(partitionLine);
        back.setSector(backSec);

        return *lineSeg;
    }

    inline void linkSegmentInSuperBlockmap(SuperBlockmapNode &block, LineSegmentSide &lineSeg)
    {
        // Associate this line segment with the subblock.
        lineSeg.setBMapBlock(&block.userData()->push(lineSeg));
    }

    /**
     * Returns the EdgeTips set associated with @a vertex.
     */
    EdgeTips &edgeTipSet(Vertex const &vertex)
    {
        EdgeTipSetMap::iterator found = edgeTipSets.find(const_cast<Vertex *>(&vertex));
        if(found == edgeTipSets.end())
        {
            // Time to construct a new set.
            found = edgeTipSets.insert(const_cast<Vertex *>(&vertex), EdgeTips());
        }
        return found.value();
    }

    /**
     * Create all initial line segments and add them to @a blockmap. We can be
     * certain there are no zero-length lines as these are screened earlier.
     */
    void createInitialLineSegments(SuperBlockmapNode &blockmap)
    {
        foreach(Line *line, lines)
        {
            Sector *frontSec = line->frontSectorPtr();
            Sector *backSec  = line->backSectorPtr();

            // Handle the "one-way window" effect.
            if(!backSec && line->_bspWindowSector)
            {
                backSec = line->_bspWindowSector;
            }

            LineSegment *seg = &buildLineSegmentBetweenVertexes(
                line->from(), line->to(), frontSec, backSec, &line->front());

            if(seg->front().hasSector())
            {
                linkSegmentInSuperBlockmap(blockmap, seg->front());
            }
            if(seg->back().hasSector())
            {
                linkSegmentInSuperBlockmap(blockmap, seg->back());
            }

            edgeTipSet(line->from()) << EdgeTip(seg->front());
            edgeTipSet(line->to())   << EdgeTip(seg->back());
        }
    }

    void chooseNextPartitionFromSuperBlock(SuperBlockmapNode const &partList,
        SuperBlockmapNode const &segs, LineSegmentSide **best, PartitionCost &bestCost)
    {
        DENG2_ASSERT(best != 0);

        //LOG_AS("chooseNextPartitionFromSuperBlock");

        // Configure a new cost evaluator.
        PartitionCostEvaluator evaluator(segs, *best, bestCost);
        evaluator.setSplitCost(splitCostFactor);

        // Test each line segment as a potential partition.
        foreach(LineSegmentSide *candidate, partList.userData()->segments())
        {
            //LOG_DEBUG("%sline segment %p sector:%d %s -> %s")
            //    << (canidate->hasMapLineSide()? "" : "mini-") << seg
            //    << (canidate->sector? seg->sector->indexInMap() : -1)
            //    << canidate->fromOrigin().asText()
            //    << canidate->toOrigin().asText();

            // Optimization: Only the first line segment produced from a given
            // line is tested per round of partition costing (they are all collinear).
            if(candidate->hasMapSide())
            {
                // Can we skip this line segment?
                if(candidate->mapLine().validCount() == validCount)
                    continue; // Yes.

                candidate->mapLine().setValidCount(validCount);
            }

            // Evaluate the new candidate.
            PartitionCost costForCandidate;
            if(evaluator.costPartition(*candidate, costForCandidate))
            {
                // Suitable for use as a partition.
                if(!*best || costForCandidate < bestCost)
                {
                    // We have a new better choice.
                    bestCost = costForCandidate;

                    // Remember which line segment.
                    *best = candidate;
                }
            }
        }
    }

    /**
     * Find the best line segment to use as the next partition.
     *
     * @param candidates  Candidate line segments to choose from.
     *
     * @return  The chosen line segment.
     */
    LineSegmentSide *chooseNextPartition(SuperBlockmapNode const &candidates)
    {
        LOG_AS("Partitioner::choosePartition");

        PartitionCost bestCost;
        LineSegmentSide *best = 0;

        // Increment valid count so we can avoid testing the line segments
        // produced from a single line more than once per round of partition
        // selection.
        validCount++;

        // Iterative pre-order traversal of SuperBlock.
        SuperBlockmapNode const *cur = &candidates;
        SuperBlockmapNode const *prev = 0;
        while(cur)
        {
            while(cur)
            {
                chooseNextPartitionFromSuperBlock(*cur, candidates, &best, bestCost);

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

        /*if(best)
        {
            LOG_DEBUG("best %p score: %d.%02d.")
                << best << bestCost.total / 100 << bestCost.total % 100;
        }*/

        return best;
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
        Vector2d const &point, bool updateEdgeTips = true)
    {
        DENG2_ASSERT(point != frontLeft.from().origin() &&
                     point != frontLeft.to().origin());

        //LOG_DEBUG("Splitting line segment %p at %s.")
        //    << &frontLeft << point.asText();

        Vertex *newVert = makeVertex(point);

        LineSegment &oldSeg = frontLeft.line();
        LineSegment &newSeg =
            buildLineSegmentBetweenVertexes(oldSeg.from(), oldSeg.to(),
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
    Vector2d intersectPartition(LineSegmentSide const &seg, coord_t fromDist,
                                coord_t toDist) const
    {
        // Horizontal partition vs vertical line segment.
        if(hplane.slopeType() == ST_HORIZONTAL && seg.slopeType() == ST_VERTICAL)
        {
            return Vector2d(seg.from().origin().x, hplane.partition().origin.y);
        }

        // Vertical partition vs horizontal line segment.
        if(hplane.slopeType() == ST_VERTICAL && seg.slopeType() == ST_HORIZONTAL)
        {
            return Vector2d(hplane.partition().origin.x, seg.from().origin().y);
        }

        // 0 = start, 1 = end.
        coord_t ds = fromDist / (fromDist - toDist);

        Vector2d point = seg.from().origin();
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
    void divideOneSegment(LineSegmentSide &seg, SuperBlockmapNode &rights, SuperBlockmapNode &lefts)
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
                linkSegmentInSuperBlockmap(lefts, seg);
            }
            else
            {
                linkSegmentInSuperBlockmap(rights, seg);
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
            linkSegmentInSuperBlockmap(rights, seg);
            break;

        case Left:
        case LeftIntercept:
            if(rel == LeftIntercept)
            {
                interceptPartition(seg, (fromDist > -DIST_EPSILON? LineSegment::From : LineSegment::To));
            }
            linkSegmentInSuperBlockmap(lefts, seg);
            break;

        case Intersects: {
            // Calculate the intersection point and split this line segment.
            Vector2d point = intersectPartition(seg, fromDist, toDist);
            LineSegmentSide &newFrontRight = splitLineSegment(seg, point);

            // Ensure the new back left segment is inserted into the same block as
            // the old back right segment.
            SuperBlockmapNode *backLeftBlock = (SuperBlockmapNode *)seg.back().bmapBlockPtr();
            if(backLeftBlock)
            {
                linkSegmentInSuperBlockmap(*backLeftBlock, newFrontRight.back());
            }

            interceptPartition(seg, LineSegment::To);

            // Direction determines which subset the line segments are added to.
            if(fromDist < 0)
            {
                linkSegmentInSuperBlockmap(rights, newFrontRight);
                linkSegmentInSuperBlockmap(lefts,  seg);
            }
            else
            {
                linkSegmentInSuperBlockmap(rights, seg);
                linkSegmentInSuperBlockmap(lefts,  newFrontRight);
            }
            break; }
        }
    }

    /**
     * Remove all the line segments from the list, partitioning them into the
     * left or right sets according to their position relative to partition line.
     * Adds any intersections onto the intersection list as it goes.
     *
     * @param segments  The line segments to be partitioned.
     * @param rights    Set of line segments on the right side of the partition.
     * @param lefts     Set of line segments on the left side of the partition.
     */
    void divideSegments(SuperBlockmapNode &segments, SuperBlockmapNode &rights, SuperBlockmapNode &lefts)
    {
        /**
         * @todo Revise this algorithm so that @var segments is not modified
         * during the partitioning process.
         */
        int const totalSegs = segments.userData()->totalSegmentCount();
        DENG2_ASSERT(totalSegs != 0);
        DENG2_UNUSED(totalSegs);

        // Iterative pre-order traversal of SuperBlock.
        SuperBlockmapNode *cur = &segments;
        SuperBlockmapNode *prev = 0;
        while(cur)
        {
            while(cur)
            {
                SuperBlockmapNodeData &node = *cur->userData();

                LineSegmentSide *seg;
                while((seg = node.pop()))
                {
                    // Disassociate the line segment from the blockmap.
                    seg->setBMapBlock(0);

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
        DENG2_ASSERT(rights.userData()->totalSegmentCount());
        DENG2_ASSERT(lefts.userData ()->totalSegmentCount());
        DENG2_ASSERT((  rights.userData()->totalSegmentCount()
                      + lefts.userData ()->totalSegmentCount()) >= totalSegs);
    }

    /**
     * Analyze the half-plane intercepts, building new line segments to cap
     * any gaps (new segments are added onto the end of the appropriate list
     * (rights to @a rightList and lefts to @a leftList)).
     *
     * @param rights  Set of line segments on the right of the partition.
     * @param lefts   Set of line segments on the left of the partition.
     */
    void addPartitionLineSegments(SuperBlockmapNode &rights, SuperBlockmapNode &lefts)
    {
        LOG_TRACE("Building line segments along partition %s")
                << hplane.partition().asText();

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
            HPlane::Intercept const &cur  = hplane.intercepts()[i];
            HPlane::Intercept const &next = hplane.intercepts()[i+1];

            // Does this range overlap the partition line segment?
            if(partSeg && cur.distance() >= nearDist && next.distance() <= farDist)
                continue;

            if(!cur.after() && !next.before())
                continue;

            // Check for some nasty open/closed or close/open cases.
            if(cur.after() && !next.before())
            {
                if(!cur.lineSegmentIsSelfReferencing())
                {
                    Vector2d nearPoint = (cur.vertex().origin() + next.vertex().origin()) / 2;
                    notifyUnclosedSectorFound(*cur.after(), nearPoint);
                }
                continue;
            }

            if(!cur.after() && next.before())
            {
                if(!next.lineSegmentIsSelfReferencing())
                {
                    Vector2d nearPoint = (cur.vertex().origin() + next.vertex().origin()) / 2;
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
            else
            {
                // Choose the non-self-referencing sector when we can.
                if(cur.after() != next.before())
                {
                    if(!cur.lineSegmentIsSelfReferencing() &&
                       !next.lineSegmentIsSelfReferencing())
                    {
                        LOG_DEBUG("Sector mismatch #%d %s != #%d %s.")
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
            }

            DENG2_ASSERT(sector != 0);

            LineSegment &newSeg =
                buildLineSegmentBetweenVertexes(fromVertex, toVertex,
                                                sector, sector, 0 /*no map line*/,
                                                partSeg? &partSeg->mapLine() : 0);

            edgeTipSet(newSeg.from()) << EdgeTip(newSeg.front());
            edgeTipSet(newSeg.to())   << EdgeTip(newSeg.back());

            // Add each new line segment to the appropriate set.
            linkSegmentInSuperBlockmap(rights, newSeg.front());
            linkSegmentInSuperBlockmap(lefts,  newSeg.back());

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
    static LineSegmentSides collectAllSegments(SuperBlockmapNode &node)
    {
        LineSegmentSides allSegs;

#ifdef DENG2_QT_4_7_OR_NEWER
        allSegs.reserve(node.userData()->totalSegmentCount());
#endif

        // Iterative pre-order traversal.
        SuperBlockmapNode *cur  = &node;
        SuperBlockmapNode *prev = 0;
        while(cur)
        {
            while(cur)
            {
                LineSegmentSide *seg;
                while((seg = cur->userData()->pop()))
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
     * from @a allSegments.
     */
    static AABoxd segmentBounds(LineSegmentSides const &allSegments)
    {
        AABoxd bounds;
        bool initialized = false;

        foreach(LineSegmentSide *seg, allSegments)
        {
            AABoxd segBounds = seg->aaBox();
            if(initialized)
            {
                V2d_UniteBox(bounds.arvec2, segBounds.arvec2);
            }
            else
            {
                V2d_CopyBox(bounds.arvec2, segBounds.arvec2);
                initialized = true;
            }
        }

        return bounds;
    }

    /**
     * Determine the axis-aligned bounding box containing the vertices of all
     * segments at or beneath @a node in the blockmap.
     *
     * @return  Determined AABox (might be empty (i.e., min > max) if no segments).
     */
    static AABoxd segmentBounds(SuperBlockmapNode const &node)
    {
        bool initialized = false;
        AABoxd bounds;

        // Iterative pre-order traversal.
        SuperBlockmapNode const *cur = &node;
        SuperBlockmapNode const *prev = 0;
        while(cur)
        {
            while(cur)
            {
                SuperBlockmapNodeData const &ndata = *cur->userData();

                if(ndata.totalSegmentCount())
                {
                    AABoxd segBoundsAtNode = segmentBounds(ndata.segments());
                    if(initialized)
                    {
                        V2d_AddToBox(bounds.arvec2, segBoundsAtNode.min);
                    }
                    else
                    {
                        V2d_InitBox(bounds.arvec2, segBoundsAtNode.min);
                        initialized = true;
                    }
                    V2d_AddToBox(bounds.arvec2, segBoundsAtNode.max);
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

        if(!initialized)
        {
            bounds.clear();
        }

        return bounds;
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
     * @param bmap  SuperBlockmap containing the set of line segments in the
     *              plane to be partitioned into convex subspaces.
     *
     * @return  Newly created subtree; otherwise @c 0 (degenerate).
     */
    BspTree *divideSpace(SuperBlockmapNode &sbnode)
    {
        LOG_AS("Partitioner::divideSpace");

        BspElement *bspElement = 0; ///< Built BSP map element at this node.
        BspTree *rightTree = 0, *leftTree = 0;

        // Pick a line segment to use as the next partition plane.
        if(LineSegmentSide *partSeg = chooseNextPartition(sbnode))
        {
            // Reconfigure the half-plane for the next round of partitioning.
            hplane.configure(*partSeg);

            /*
            LOG_TRACE("%s, segment side %p %i (segment #%i) %s %s.")
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

            // Create left and right blockmaps.
            /// @todo There should be no need to use additional independent
            ///       structures to contain these subsets.
            // Copy the bounding box of the edge list to the superblocks.
            SuperBlockmap rightBMap(sbnode.userData()->bounds());
            SuperBlockmap leftBMap(sbnode.userData()->bounds());

            // Partition the line segements into two subsets according to their
            // spacial relationship with the half-plane (splitting any which
            // intersect).
            divideSegments(sbnode, rightBMap, leftBMap);
            sbnode.clear();

            addPartitionLineSegments(rightBMap, leftBMap);

            // Take a copy of the geometry bounds for each child/sub space
            // - we'll need this for any BspNode we produce later.
            AABoxd rightBounds = segmentBounds(rightBMap);
            AABoxd leftBounds  = segmentBounds(leftBMap);

            // Recurse on each suspace, first the right space then left.
            rightTree = divideSpace(rightBMap);
            leftTree  = divideSpace(leftBMap);

            // Collapse degenerates upward.
            if(!rightTree || !leftTree)
                return rightTree? rightTree : leftTree;

            // Make a new BSP node.
            bspElement = new BspNode(partition, rightBounds, leftBounds);
        }
        else
        {
            // No partition required/possible -- already convex (or degenerate).
            SuperBlockmapNodeData::Segments segments = collectAllSegments(sbnode);
            sbnode.clear();

            subspaces.append(ConvexSubspaceProxy());
            ConvexSubspaceProxy &convexSet = subspaces.last();

            convexSet.addSegments(segments);

            foreach(LineSegmentSide *seg, segments)
            {
                // Attribute the segment to the convex subspace.
                seg->setConvexSubspace(&convexSet);

                // Disassociate the segment from the blockmap.
                seg->setBMapBlock(0);
            }

            // Make a new BSP leaf.
            /// @todo Defer until necessary.
            BspLeaf *leaf = new BspLeaf;

            // Attribute the leaf to the convex subspace.
            convexSet.setBspLeaf(leaf);

            bspElement = leaf;
        }

        // Make a new BSP subtree and link up the children.
        BspTree *subtree = new BspTree(bspElement, 0/*no parent*/, rightTree, leftTree);
        if(rightTree) rightTree->setParent(subtree);
        if(leftTree)  leftTree->setParent(subtree);

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
    void splitOverlappingLineSegments()
    {
        foreach(ConvexSubspaceProxy const &subspace, subspaces)
        {
            /*
             * The subspace provides a specially ordered list of the segments to
             * simplify this task. The primary clockwise ordering (decreasing angle
             * relative to the center of the subspace) places overlapping segments
             * adjacently. The secondary anticlockwise ordering sorts the overlapping
             * segments enabling the use of single pass algorithm here.
             */
            OrderedSegments convexSet = subspace.segments();
            int const numSegments = convexSet.count();
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
                        Vector2d const &point = b.segment->to().origin();
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

    void buildLeafGeometries()
    {
        foreach(ConvexSubspaceProxy const &subspace, subspaces)
        {
            /// @todo Move BSP leaf construction here?
            BspLeaf &bspLeaf = *subspace.bspLeaf();

            subspace.buildGeometry(bspLeaf, *mesh);

            // Account the new segments.
            /// @todo Refactor away.
            foreach(OrderedSegment const &oseg, subspace.segments())
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
        foreach(ConvexSubspaceProxy const &convexSet, subspaces)
        foreach(OrderedSegment const &oseg, convexSet.segments())
        {
            LineSegmentSide *seg = oseg.segment;

            if(seg->hasHEdge() && !seg->back().hasHEdge())
            {
                HEdge *hedge = &seg->hedge();
                DENG2_ASSERT(!hedge->hasTwin());

                // Allocate the twin from the same mesh.
                hedge->setTwin(hedge->mesh().newHEdge(seg->back().from()));
                hedge->twin().setTwin(hedge);
            }
        }
    }

    /**
     * Notify interested parties of an unclosed sector in the map.
     *
     * @param sector    The problem sector.
     * @param nearPoint  Coordinates near to where the problem was found.
     */
    void notifyUnclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(UnclosedSectorFound, i)
        {
            i->unclosedSectorFound(sector, nearPoint);
        }
    }

#ifdef DENG_DEBUG
    void printSuperBlockSegments(SuperBlockmapNode const &block)
    {
        SuperBlockmapNodeData::Segments const &segments = block.userData()->segments();
        foreach(LineSegmentSide const *seg, segments)
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

Partitioner::Partitioner(int splitCostFactor)
    : d(new Instance(this))
{
    setSplitCostFactor(splitCostFactor);
}

void Partitioner::setSplitCostFactor(int newFactor)
{
    if(d->splitCostFactor != newFactor)
    {
        d->splitCostFactor = newFactor;
    }
}

static AABox blockmapBounds(AABoxd const &mapBounds)
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

static bool lineIndexLessThan(Line const *a, Line const *b)
{
     return a->indexInMap() < b->indexInMap();
}

Partitioner::BspTree *Partitioner::makeBspTree(LineSet const &lines, Mesh &mesh)
{
    d->clear();

    // Copy the set of lines and sort by index to ensure deterministically
    // predictable output.
    d->lines = lines.toList();
    qSort(d->lines.begin(), d->lines.end(), lineIndexLessThan);

    d->mesh = &mesh;

    // Initialize vertex info for the initial set of vertexes.
    d->edgeTipSets.reserve(d->lines.count() * 2);

    // Determine the bounds of the line geometry.
    AABoxd bounds;
    bool isFirst = true;
    foreach(Line *line, d->lines)
    {
        if(isFirst)
        {
            // The first line's bounds are used as is.
            V2d_CopyBox(bounds.arvec2, line->aaBox().arvec2);
            isFirst = false;
        }
        else
        {
            // Expand the bounding box.
            V2d_UniteBox(bounds.arvec2, line->aaBox().arvec2);
        }
    }

    Instance::SuperBlockmap rootBlock(blockmapBounds(bounds));

    d->createInitialLineSegments(rootBlock);

    d->bspRoot = d->divideSpace(rootBlock);

    // At this point we know that *something* useful was built.
    d->splitOverlappingLineSegments();
    d->buildLeafGeometries();

    return d->bspRoot;
}

Partitioner::BspTree *Partitioner::root() const
{
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

} // namespace de
