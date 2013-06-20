/** @file partitioner.cpp World map binary space partitioner.
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <algorithm>

#include <QList>
#include <QHash>
#include <QtAlgorithms>

#include <de/Log>

#include "world/map.h"
#include "BspLeaf"
#include "BspNode"
#include "Line"
#include "Sector"
#include "Segment"
#include "Vertex"

#include "render/r_main.h" /// validCount @todo Remove me

#include "world/bsp/convexsubspace.h"
#include "world/bsp/edgetip.h"
#include "world/bsp/hplane.h"
#include "world/bsp/linesegment.h"
#include "world/bsp/partitioncost.h"
#include "world/bsp/superblockmap.h"

#include "world/bsp/partitioner.h"

namespace de {

using namespace bsp;

typedef QHash<MapElement *, BspTreeNode *> BspElementMap;
typedef QList<ConvexSubspace>              ConvexSubspaces;
typedef QHash<Vertex *, EdgeTips>          EdgeTipSetMap;
typedef QList<LineSegment *>               LineSegments;

DENG2_PIMPL(Partitioner)
{
    /// Cost factor attributed to splitting a line segment.
    int splitCostFactor;

    /// The set of map lines we are building BSP data for (not owned).
    LineSet lines;

    /// The mesh from which we'll assign (construct) new geometries(not owned).
    Mesh *mesh;

    /// Running totals of constructed BSP map elements.
    int numNodes;
    int numLeafs;
    int numSegments;
    int numVertexes;

    /// Line segments in the plane.
    LineSegments lineSegments;

    /// Convex subspaces in the plane.
    ConvexSubspaces convexSubspaces;

    /// A set of EdgeTips for each unique line segment vertex.
    EdgeTipSetMap edgeTipSets;

    /// Root node of the internal binary tree used to guide the partitioning
    /// process and around which the built BSP map elements are constructed.
    BspTreeNode *rootNode;

    /// Mapping table which relates built BSP map elements to their counterpart
    /// in the internal tree.
    BspElementMap treeNodeMap;

    /// The "current" binary space half-plane.
    HPlane hplane;

    Instance(Public *i, int splitCostFactor)
      : Base(i),
        splitCostFactor(splitCostFactor),
        mesh(0),
        numNodes(0), numLeafs(0), numSegments(0), numVertexes(0),
        rootNode(0)
    {}

    ~Instance() { clear(); }

    void clear()
    {
        if(rootNode)
        {
            // If ownership of the all built BSP map elements has been claimed
            // this should be a no-op.
            clearAllBspElements();

            // Destroy the internal binary tree.
            delete rootNode; rootNode = 0;
        }

        lines.clear();
        mesh = 0;
        qDeleteAll(lineSegments);
        convexSubspaces.clear();
        edgeTipSets.clear();
        treeNodeMap.clear();
        hplane.clearIntercepts();

        numNodes = numLeafs = numSegments = numVertexes = 0;
    }

    /**
     * Returns the associated EdgeTips set for the given @a vertex.
     */
    EdgeTips &edgeTips(Vertex const &vertex)
    {
        EdgeTipSetMap::iterator found = edgeTipSets.find(const_cast<Vertex *>(&vertex));
        if(found == edgeTipSets.end())
        {
            found = edgeTipSets.insert(const_cast<Vertex *>(&vertex), EdgeTips());
        }
        return found.value();
    }

    inline void linkSegmentInSuperBlockmap(SuperBlock &block, LineSegment::Side &lineSeg)
    {
        // Associate this line segment with the subblock.
        lineSeg.setBMapBlock(&block.push(lineSeg));
    }

    /**
     * Create all initial line segments and add them to @a blockmap.
     */
    void createInitialLineSegments(SuperBlock &blockmap)
    {
        foreach(Line *line, lines)
        {
            LineSegment *seg = 0;
            coord_t angle = 0;

            //if(!line.hasZeroLength()) Screened at a higher level.
            {
                Sector *frontSec = line->frontSectorPtr();
                Sector *backSec  = 0;

                if(line->hasBackSector())
                {
                    backSec = line->backSectorPtr();
                }
                // Handle the "One-way window" effect.
                else if(line->_bspWindowSector)
                {
                    backSec = line->_bspWindowSector;
                }

                seg = &buildLineSegmentBetweenVertexes(line->from(), line->to(),
                                                       frontSec, backSec,
                                                       &line->front());

                linkSegmentInSuperBlockmap(blockmap, seg->front());
                if(seg->back().hasSector())
                {
                    linkSegmentInSuperBlockmap(blockmap, seg->back());
                }

                angle = seg->front().angle();
            }

            /// @todo edge tips should be created when line segments are created.
            edgeTips(line->from()).add(angle,
                                       seg? &seg->front() : 0,
                                       seg && seg->back().hasSector()? &seg->back() : 0);

            edgeTips(line->to()  ).add(M_InverseAngle(angle),
                                       seg && seg->back().hasSector()? &seg->back() : 0,
                                       seg? &seg->front() : 0);
        }
    }

    /**
     * "Near miss" predicate.
     */
    static bool nearMiss(LineRelationship rel, coord_t fromDist, coord_t toDist,
                         coord_t *distance)
    {
        if(rel == Right &&
           !((fromDist >= SHORT_HEDGE_EPSILON && toDist >= SHORT_HEDGE_EPSILON) ||
             (fromDist <= DIST_EPSILON        && toDist >= SHORT_HEDGE_EPSILON) ||
             (toDist <= DIST_EPSILON && fromDist >= SHORT_HEDGE_EPSILON)))
        {
            // Need to know how close?
            if(distance)
            {
                if(fromDist <= DIST_EPSILON || toDist <= DIST_EPSILON)
                    *distance = SHORT_HEDGE_EPSILON / de::max(fromDist, toDist);
                else
                    *distance = SHORT_HEDGE_EPSILON / de::min(fromDist, toDist);
            }
            return true;
        }

        if(rel == Left &&
           !((fromDist <= -SHORT_HEDGE_EPSILON && toDist <= -SHORT_HEDGE_EPSILON) ||
             (fromDist >= -DIST_EPSILON        && toDist <= -SHORT_HEDGE_EPSILON) ||
             (toDist >= -DIST_EPSILON && fromDist <= -SHORT_HEDGE_EPSILON)))
        {
            // Need to know how close?
            if(distance)
            {
                if(fromDist >= -DIST_EPSILON || toDist >= -DIST_EPSILON)
                    *distance = SHORT_HEDGE_EPSILON / -de::min(fromDist, toDist);
                else
                    *distance = SHORT_HEDGE_EPSILON / -de::max(fromDist, toDist);
            }
            return true;
        }

        return false;
    }

    /**
     * "Near edge" predicate. Assumes intersecting line segment relationship.
     */
    static bool nearEdge(coord_t fromDist, coord_t toDist, coord_t *distance)
    {
        if(de::abs(fromDist) < SHORT_HEDGE_EPSILON || de::abs(toDist) < SHORT_HEDGE_EPSILON)
        {
            // Need to know how close?
            if(distance)
            {
                *distance = SHORT_HEDGE_EPSILON / de::min(de::abs(fromDist), de::abs(toDist));
            }
            return true;
        }
        return false;
    }

    void evalPartitionCostForSegment(LineSegment::Side const &plSeg,
        LineSegment::Side const &seg, PartitionCost &cost)
    {
        int const costFactorMultiplier = splitCostFactor;

        /// Determine the relationship between @a seg and the partition plane.
        coord_t fromDist, toDist;
        LineRelationship rel = seg.relationship(plSeg, &fromDist, &toDist);
        switch(rel)
        {
        case Collinear: {
            // This line segment runs along the same line as the partition.
            // Check whether it goes in the same direction or the opposite.
            if(seg.direction().dot(plSeg.direction()) < 0)
            {
                cost.addSegmentLeft(seg);
            }
            else
            {
                cost.addSegmentRight(seg);
            }
            break; }

        case Right:
        case RightIntercept: {
            cost.addSegmentRight(seg);

            /*
             * Near misses are bad, as they have the potential to result in
             * really short line segments being produced later on.
             *
             * The closer the near miss, the higher the cost.
             */
            coord_t nearDist;
            if(nearMiss(rel, fromDist, toDist, &nearDist))
            {
                cost.nearMiss += 1;
                cost.total += int( 100 * costFactorMultiplier * (nearDist * nearDist - 1.0) );
            }
            break; }

        case Left:
        case LeftIntercept: {
            cost.addSegmentLeft(seg);

            // Near miss?
            coord_t nearDist;
            if(nearMiss(rel, fromDist, toDist, &nearDist))
            {
                /// @todo Why the cost multiplier imbalance between the left
                /// and right edge near misses?
                cost.nearMiss += 1;
                cost.total += int( 70 * costFactorMultiplier * (nearDist * nearDist - 1.0) );
            }
            break; }

        case Intersects: {
            cost.splits += 1;
            cost.total  += 100 * costFactorMultiplier;

            /*
             * If the split point is very close to one end, which is quite an
             * undesirable situation (producing really short edges), thus a
             * rather hefty surcharge.
             *
             * The closer to the edge, the higher the cost.
             */
            coord_t nearDist;
            if(nearEdge(fromDist, toDist, &nearDist))
            {
                cost.iffy += 1;
                cost.total += int( 140 * costFactorMultiplier * (nearDist * nearDist - 1.0) );
            }
            break; }
        }
    }

    /**
     * @param block
     * @param best      Best line segment found thus far.
     * @param bestCost  Running cost total result for the best line segment yet.
     * @param seg       The candidate line segment to be evaluated.
     * @param cost      PartitionCost analysis to be completed for the candidate.
     *                  Must have been initialized prior to calling.
     *
     * @return  @c true iff @a lineSeg is suitable for use as a partition.
     */
    bool evalPartitionCostForSuperBlock(SuperBlock const &block,
        LineSegment::Side *best, PartitionCost const &bestCost,
        LineSegment::Side const &seg, PartitionCost &cost)
    {
        /*
         * Test the whole block against the partition line to quickly handle
         * all the line segments within it at once. Only when the partition line
         * intercepts the box do we need to go deeper into it.
         */
        /// @todo Why are we extending the bounding box for this test? Also,
        /// there is no need to convert from integer to floating-point each
        /// time this is tested. (If we intend to do this with floating-point
        /// then we should return that representation in SuperBlock::bounds() ).
        AABox const &blockBounds = block.bounds();
        AABoxd bounds(coord_t( blockBounds.minX ) - SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( blockBounds.minY ) - SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( blockBounds.maxX ) + SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( blockBounds.maxY ) + SHORT_HEDGE_EPSILON * 1.5);

        int side = seg.boxOnSide(bounds);
        if(side > 0)
        {
            // Right.
            cost.mapRight  += block.mapSegmentCount();
            cost.partRight += block.partSegmentCount();
            return true;
        }
        if(side < 0)
        {
            // Left.
            cost.mapLeft  += block.mapSegmentCount();
            cost.partLeft += block.partSegmentCount();
            return true;
        }

        // Check partition against all line segments.
        foreach(LineSegment::Side *otherSeg, block.segments())
        {
            // Do we already have a better choice?
            if(best && !(cost < bestCost)) return false;

            // Evaluate the cost delta for this line segment.
            PartitionCost costDelta;
            evalPartitionCostForSegment(seg, *otherSeg, costDelta);

            // Merge cost result into the cummulative total.
            cost += costDelta;
        }

        // Handle sub-blocks recursively.
        if(block.right())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.right(), best, bestCost,
                                                              seg, cost);
            if(unsuitable) return false;
        }

        if(block.left())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.left(), best, bestCost,
                                                              seg, cost);
            if(unsuitable) return false;
        }

        // This is a suitable candidate.
        return true;
    }

    /**
     * Evaluate a partition and determine the cost, taking into account the
     * number of splits and the difference between left and right.
     *
     * To be able to divide the nodes down, evalPartition must decide which
     * is the best line segment to use as a nodeline. It does this by selecting
     * the line with least splits and has least difference of line segments
     * on either side of it.
     *
     * @return  @c true iff @a lineSeg is suitable for use as a partition.
     */
    bool evalPartition(SuperBlock const &block,
                       LineSegment::Side *best, PartitionCost const &bestCost,
                       LineSegment::Side const &lineSeg, PartitionCost &cost)
    {
        // Only map line segments are potential candidates.
        if(!lineSeg.hasMapSide()) return false;

        if(!evalPartitionCostForSuperBlock(block, best, bestCost, lineSeg, cost))
        {
            // Unsuitable or we already have a better choice.
            return false;
        }

        // Make sure there is at least one map line segment on each side.
        if(!cost.mapLeft || !cost.mapRight)
        {
            //LOG_DEBUG("evalPartition: No map line segments on %s%sside")
            //    << (cost.mapLeft? "" : "left ")
            //    << (cost.mapRight? "" : "right ");
            return false;
        }

        // Increase cost by the difference between left and right.
        cost.total += 100 * abs(cost.mapLeft - cost.mapRight);

        // Allow partition segment counts to affect the outcome.
        cost.total += 50 * abs(cost.partLeft - cost.partRight);

        // Another little twist, here we show a slight preference for partition
        // lines that lie either purely horizontally or purely vertically.
        if(lineSeg.slopeType() != ST_HORIZONTAL && lineSeg.slopeType() != ST_VERTICAL)
            cost.total += 25;

        //LOG_DEBUG("evalPartition: %p: splits=%d iffy=%d near=%d"
        //          " left=%d+%d right=%d+%d cost=%d.%02d")
        //    << de::dintptr(&lineSeg) << cost.splits << cost.iffy << cost.nearMiss
        //    << cost.mapLeft << cost.partLeft << cost.mapRight << cost.partRight
        //    << cost.total / 100 << cost.total % 100;

        return true;
    }

    void chooseNextPartitionFromSuperBlock(SuperBlock const &partList,
        SuperBlock const &segs, LineSegment::Side **best, PartitionCost &bestCost)
    {
        DENG2_ASSERT(best != 0);

        //LOG_AS("chooseNextPartitionFromSuperBlock");

        // Test each line segment as a potential partition.
        foreach(LineSegment::Side *seg, partList.segments())
        {
            //LOG_DEBUG("%sline segment %p sector:%d %s -> %s")
            //    << (seg->hasMapLineSide()? "" : "mini-") << de::dintptr(*seg)
            //    << (seg->sector? seg->sector->indexInMap() : -1)
            //    << seg->fromOrigin().asText()
            //    << seg->toOrigin().asText();

            // Optimization: Only the first line segment produced from a given
            // line is tested per round of partition costing (they are all
            // collinear).
            if(seg->hasMapSide())
            {
                // Can we skip this line segment?
                if(seg->mapLine().validCount() == validCount) continue; // Yes.

                seg->mapLine().setValidCount(validCount);
            }

            // Calculate the cost metrics for this line segment.
            PartitionCost cost;
            if(evalPartition(segs, *best, bestCost, *seg, cost))
            {
                // Suitable for use as a partition.
                if(!*best || cost < bestCost)
                {
                    // We have a new better choice.
                    bestCost = cost;

                    // Remember which line segment.
                    *best = seg;
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
    LineSegment::Side *chooseNextPartition(SuperBlock const &candidates)
    {
        LOG_AS("Partitioner::choosePartition");

        PartitionCost bestCost;
        LineSegment::Side *best = 0;

        // Increment valid count so we can avoid testing the line segments
        // produced from a single line more than once per round of partition
        // selection.
        validCount++;

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock const *cur = &candidates;
        SuperBlock const *prev = 0;
        while(cur)
        {
            while(cur)
            {
                chooseNextPartitionFromSuperBlock(*cur, candidates, &best, bestCost);

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->right()) cur = cur->right();
                    else             cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }

        /*if(best)
        {
            LOG_DEBUG("best %p score: %d.%02d.")
                << de::dintptr(best) << bestCost.total / 100 << bestCost.total % 100;
        }*/

        return best;
    }

    /**
     * @return The new line segment (front is from @a start to @a end).
     */
    LineSegment &buildLineSegmentBetweenVertexes(Vertex &start, Vertex &end,
        Sector *frontSec, Sector *backSec, Line::Side *frontSide,
        Line *partitionLine = 0)
    {
        lineSegments.append(new LineSegment(start, end));
        LineSegment &lineSeg = *lineSegments.back();

        LineSegment::Side &front = lineSeg.front();
        front.setMapSide(frontSide);
        front.setPartitionMapLine(partitionLine);
        front.setSector(frontSec);

        LineSegment::Side &back = lineSeg.back();
        back.setMapSide(frontSide? &frontSide->back() : 0);
        back.setPartitionMapLine(partitionLine);
        back.setSector(backSec);

        return lineSeg;
    }

    /**
     * Splits the given line segment at the point (x,y). The new line segment
     * is returned. The old line segment is shortened (the original start vertex
     * is unchanged), the new line segment becomes the cut-off tail (keeping
     * the original end vertex).
     *
     * @note If the line segment has a twin it is also split.
     */
    LineSegment::Side &splitLineSegment(LineSegment::Side &frontLeft,
        Vector2d const &point, bool updateEdgeTips = true)
    {
        DENG_ASSERT(point != frontLeft.from().origin() &&
                    point != frontLeft.to().origin());

        //LOG_DEBUG("Splitting line segment %p at %s.")
        //    << de::dintptr(&frontLeft) << point.asText();

        Vertex *newVert = newVertex(point);

        LineSegment &oldSeg = frontLeft.line();
        LineSegment &newSeg =
            buildLineSegmentBetweenVertexes(oldSeg.from(), oldSeg.to(),
                                            oldSeg.front().sectorPtr(),
                                            oldSeg.back().sectorPtr(),
                                            oldSeg.front().mapSidePtr(),
                                            oldSeg.front().partitionMapLine());

        // Perform the split, updating vertex and relative segment links.
        LineSegment::Side &frontRight = newSeg.side(frontLeft.lineSideId());

        oldSeg.replaceVertex(frontLeft.lineSideId() ^ LineSegment::To, *newVert);
        newSeg.replaceVertex(frontLeft.lineSideId(),                   *newVert);

        LineSegment::Side &backRight = frontLeft.back();
        LineSegment::Side &backLeft  = frontRight.back();

        if(ConvexSubspace *convexSet = frontLeft.convexSubspace())
        {
            *convexSet << frontRight;
            frontRight.setConvexSubspace(convexSet);
        }

        frontLeft.setRight(&frontRight);
        frontRight.setLeft(&frontLeft);

        // Handle the twin.
        if(ConvexSubspace *convexSet = backRight.convexSubspace())
        {
            *convexSet << backLeft;
            backLeft.setConvexSubspace(convexSet);
        }

        backLeft.setRight(&backRight);
        backRight.setLeft(&backLeft);

        if(updateEdgeTips)
        {
            /**
             * @todo Optimize: Avoid clearing tips by implementing update logic.
             */
            edgeTips(oldSeg.from()).clearByLineSegment(oldSeg);
            edgeTips(oldSeg.to()  ).clearByLineSegment(oldSeg);

            edgeTips(newSeg.from()).clearByLineSegment(newSeg);
            edgeTips(newSeg.to()  ).clearByLineSegment(newSeg);

            edgeTips(oldSeg.from()).add(oldSeg.front().angle(),
                                        oldSeg.front().hasSector()? &oldSeg.front() : 0,
                                        oldSeg.back().hasSector()?  &oldSeg.back()  : 0);

            edgeTips(oldSeg.to()  ).add(oldSeg.back().angle(),
                                        oldSeg.back().hasSector()?  &oldSeg.back()  : 0,
                                        oldSeg.front().hasSector()? &oldSeg.front() : 0);

            edgeTips(newSeg.from()).add(newSeg.front().angle(),
                                        newSeg.front().hasSector()? &newSeg.front() : 0,
                                        newSeg.back().hasSector()?  &newSeg.back()  : 0);

            edgeTips(newSeg.to()  ).add(newSeg.back().angle(),
                                        newSeg.back().hasSector()?  &newSeg.back()  : 0,
                                        newSeg.front().hasSector()? &newSeg.front() : 0);
        }

        return frontRight;
    }

    /**
     * Find the intersection point between a line segment and the current
     * partition plane. Takes advantage of some common situations like
     * horizontal and vertical lines to choose a 'nicer' intersection
     * point.
     */
    Vector2d intersectPartition(LineSegment::Side const &seg, coord_t fromDist,
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

    /// @todo refactor away -ds
    inline void interceptPartition(LineSegment::Side &seg, int edge, bool meetAtVertex = false)
    {
        hplane.intercept(seg, edge, meetAtVertex, edgeTips(seg.vertex(edge)));
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
    void divideOneSegment(LineSegment::Side &seg, SuperBlock &rights, SuperBlock &lefts)
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
                interceptPartition(seg, (fromDist < DIST_EPSILON? LineSegment::From : LineSegment::To),
                                   true /*incident with the edge vertex*/);
            }
            linkSegmentInSuperBlockmap(rights, seg);
            break;

        case Left:
        case LeftIntercept:
            if(rel == LeftIntercept)
            {
                interceptPartition(seg, (fromDist > -DIST_EPSILON? LineSegment::From : LineSegment::To),
                                   true /*incident with the edge vertex*/);
            }
            linkSegmentInSuperBlockmap(lefts, seg);
            break;

        case Intersects: {
            // Calculate the intersection point and split this line segment.
            Vector2d point = intersectPartition(seg, fromDist, toDist);
            LineSegment::Side &newFrontRight = splitLineSegment(seg, point);

            // Ensure the new back left segment is inserted into the same block as
            // the old back right segment.
            SuperBlock *backLeftBlock = seg.back().bmapBlockPtr();
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
    void divideSegments(SuperBlock &segments, SuperBlock &rights, SuperBlock &lefts)
    {
        /**
         * @todo Revise this algorithm so that @var segments is not modified
         * during the partitioning process.
         */

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock *cur = &segments;
        SuperBlock *prev = 0;
        while(cur)
        {
            while(cur)
            {
                LineSegment::Side *seg;
                while((seg = cur->pop()))
                {
                    // Disassociate the line segment from the blockmap.
                    seg->setBMapBlock(0);

                    divideOneSegment(*seg, rights, lefts);
                }

                if(prev == cur->parent())
                {
                    // Descending - right first, then left.
                    prev = cur;
                    if(cur->right()) cur = cur->right();
                    else             cur = cur->left();
                }
                else if(prev == cur->right())
                {
                    // Last moved up the right branch - descend the left.
                    prev = cur;
                    cur = cur->left();
                }
                else if(prev == cur->left())
                {
                    // Last moved up the left branch - continue upward.
                    prev = cur;
                    cur = cur->parent();
                }
            }

            if(prev)
            {
                // No left child - back up.
                cur = prev->parent();
            }
        }

        // Sanity checks...
        if(!rights.totalSegmentCount())
            throw Error("Partitioner::divideSegments", "Right set is empty");

        if(!lefts.totalSegmentCount())
            throw Error("Partitioner::divideSegments", "Left set is empty");
    }

    /**
     * Analyze the half-plane intercepts, building new line segments to cap
     * any gaps (new segments are added onto the end of the appropriate list
     * (rights to @a rightList and lefts to @a leftList)).
     *
     * @param rights  Set of line segments on the right of the partition.
     * @param lefts   Set of line segments on the left of the partition.
     */
    void addPartitionLineSegments(SuperBlock &rights, SuperBlock &lefts)
    {
        LOG_TRACE("Building line segments along partition %s")
            << hplane.partition().asText();

        // First, fix any near-distance issues with the intercepts.
        hplane.sortAndMergeIntercepts();

        //hplane.printIntercepts();

        // We must not create new line segments on top of the source partition
        // line segment (as this will result in duplicate edges finding their
        // way into the BSP leaf geometries).
        LineSegment::Side *partSeg = hplane.lineSegment();
        double nearDist = 0, farDist = 0;

        if(partSeg)
        {
            nearDist = hplane.intersect(*partSeg, LineSegment::From);
            farDist  = hplane.intersect(*partSeg, LineSegment::To);
        }

        // Create new line segments.
        Sector *prevSector = 0;
        for(int i = 0; i < hplane.intercepts().count() - 1; ++i)
        {
            HPlane::Intercept const &cur  = hplane.intercepts()[i];
            HPlane::Intercept const &next = hplane.intercepts()[i+1];

            // Does this range overlap the partition line segment?
            if(partSeg && cur.distance() >= nearDist && next.distance() <= farDist)
                continue;

            if(!cur.after && !next.before)
            {
                prevSector = 0;
                continue;
            }

            // Check for some nasty open/closed or close/open cases.
            if(cur.after && !next.before)
            {
                if(!cur.selfRef)
                {
                    Vector2d nearPoint = (cur.vertex().origin() + next.vertex().origin()) / 2;
                    notifyUnclosedSectorFound(*cur.after, nearPoint);
                }
                prevSector = 0;
                continue;
            }

            if(!cur.after && next.before)
            {
                if(!next.selfRef)
                {
                    Vector2d nearPoint = (cur.vertex().origin() + next.vertex().origin()) / 2;
                    notifyUnclosedSectorFound(*next.before, nearPoint);
                }
                prevSector = 0;
                continue;
            }

            /*
             * This is definitely open space.
             */
            Vertex &fromVertex = cur.vertex();
            Vertex &toVertex   = next.vertex();

            Sector *sector = cur.after;
            if(prevSector && cur.meetAtVertex && cur.before == cur.after)
            {
                sector = prevSector;
            }
            else if(prevSector && next.meetAtVertex && next.before == next.after)
            {
                sector = prevSector;
            }
            else if(!cur.before && next.before == next.after)
            {
                sector = next.before;
            }
            else
            {
                // Choose the non-self-referencing sector when we can.
                if(cur.after != next.before)
                {
                    if(!cur.selfRef && !next.selfRef)
                    {
                        LOG_DEBUG("Sector mismatch #%d %s != #%d %s.")
                            << cur.after->indexInMap()
                            << cur.vertex().origin().asText()
                            << next.before->indexInMap()
                            << next.vertex().origin().asText();
                    }

                    if(cur.selfRef && !next.selfRef)
                        sector = next.before;
                }
            }

            DENG_ASSERT(sector != 0);

            LineSegment &newSeg =
                buildLineSegmentBetweenVertexes(fromVertex, toVertex,
                                                sector, sector, 0 /*no map line*/,
                                                partSeg? &partSeg->mapLine() : 0);

            edgeTips(newSeg.from()).add(newSeg.front().angle(), &newSeg.front(), &newSeg.back());
            edgeTips(newSeg.to()  ).add(newSeg.back().angle(),  &newSeg.back(),  &newSeg.front());

            // Add each new line segment to the appropriate set.
            linkSegmentInSuperBlockmap(rights, newSeg.front());
            linkSegmentInSuperBlockmap(lefts,  newSeg.back());

            /*
            LOG_DEBUG("Built line segment from %s to %s (sector #%i)")
                << fromVertex.origin().asText()
                << toVertex.origin().asText()
                << sector->indexInArchive();
            */

            prevSector = sector;
        }
    }

    /**
     * Create a new BspNode element.
     *
     * @param partition    Half-plane partition line. A copy is made.
     * @param rightBounds  Bounding box around all geometry in the @em right subspace.
     * @param leftBounds   Bounding box around all geometry in the @em right subspace.
     * @param rightChild   BSP element for the @em right child subspace. Can be @c 0.
     * @param leftChild    BSP element for the @em left child subspace. Can be @c 0.
     *
     * @return  The newly created BspNode.
     */
    BspNode *newBspNode(Partition const &partition, AABoxd const &rightBounds,
        AABoxd const &leftBounds, MapElement *rightChild = 0,
        MapElement *leftChild = 0)
    {
        BspNode *node = new BspNode(partition);

        if(rightChild)
        {
            node->setRight(rightChild);
        }
        if(leftChild)
        {
            node->setLeft(leftChild);
        }

        node->setRightAABox(&rightBounds);
        node->setLeftAABox(&leftBounds);

        // There is now one more BspNode.
        numNodes += 1;
        return node;
    }

    BspTreeNode *newTreeNode(MapElement *mapBspElement,
        BspTreeNode *rightChild = 0, BspTreeNode *leftChild = 0)
    {
        BspTreeNode *subtree = new BspTreeNode(mapBspElement);

        if(rightChild)
        {
            subtree->setRight(rightChild);
            rightChild->setParent(subtree);
        }
        if(leftChild)
        {
            subtree->setLeft(leftChild);
            leftChild->setParent(subtree);
        }

        treeNodeMap.insert(mapBspElement, subtree);
        return subtree;
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
    BspTreeNode *divideSpace(SuperBlock &bmap)
    {
        LOG_AS("Partitioner::divideSpace");

        MapElement *bspElement = 0; ///< Built BSP map element at this node.
        BspTreeNode *rightTree = 0, *leftTree = 0;

        // Pick a line segment to use as the next partition plane.
        LineSegment::Side *partSeg = chooseNextPartition(bmap);
        if(partSeg)
        {
            // Reconfigure the half-plane for the next round of partitioning.
            hplane.configure(*partSeg);

            //LOG_TRACE("%s, line segment [%p] %s %s.")
            //    << hplane.partition().asText()
            //    << de::dintptr(partSeg)
            //    << partSeg->from().origin().asText()
            //    << partSeg->to().origin().asText();

            // Take a copy of the current partition - we'll need this for any
            // BspNode we produce later.
            Partition partition(hplane.partition());

            // Create left and right blockmaps.
            /// @todo There should be no need to use additional independent
            ///       structures to contain these subsets.
            // Copy the bounding box of the edge list to the superblocks.
            SuperBlockmap rightBMap(bmap.bounds());
            SuperBlockmap leftBMap(bmap.bounds());

            // Partition the line segements into two subsets according to their
            // spacial relationship with the half-plane (splitting any which
            // intersect).
            divideSegments(bmap, rightBMap, leftBMap);
            bmap.clear(); // Should be empty.

            addPartitionLineSegments(rightBMap, leftBMap);

            // Take a copy of the geometry bounds for each child/sub space
            // - we'll need this for any BspNode we produce later.
            AABoxd rightBounds = rightBMap.findSegmentBounds();
            AABoxd leftBounds  = leftBMap.findSegmentBounds();

            // Recurse on each suspace, first the right space then left.
            rightTree = divideSpace(rightBMap);
            leftTree  = divideSpace(leftBMap);

            // Collapse degenerates upward.
            if(!rightTree || !leftTree)
                return rightTree? rightTree : leftTree;

            // Construct a new BSP node and link up the child elements.
            bspElement = newBspNode(partition, rightBounds, leftBounds,
                                    rightTree->userData(), leftTree->userData());
        }
        else
        {
            // No partition required/possible -- already convex (or degenerate).
            SuperBlock::Segments segments = bmap.collateAllSegments();
            bmap.clear(); // Should be empty.

            convexSubspaces.append(ConvexSubspace());
            ConvexSubspace &convexSet = convexSubspaces.last();

            convexSet.addSegments(segments);

            foreach(LineSegment::Side *seg, segments)
            {
                // Attribute the segment to the convex subspace.
                seg->setConvexSubspace(&convexSet);

                // Disassociate the segment from the blockmap.
                seg->setBMapBlock(0);
            }

            // Produce a BSP leaf.
            /// @todo Defer until necessary.
            BspLeaf *leaf = new BspLeaf;

            // There is now one more BspLeaf;
            numLeafs += 1;

            // Attribute the leaf to the convex subspace.
            convexSet.setBspLeaf(leaf);

            bspElement = leaf;
        }

        return newTreeNode(bspElement, rightTree, leftTree);
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
        foreach(ConvexSubspace const &subspace, convexSubspaces)
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
                while(de::fequal(convexSet[i].fromAngle, convexSet[k + 1].fromAngle) &&
                      k < numSegments)
                { k++; }

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

                        splitLineSegment(*a.segment, b.segment->to().origin(),
                                         false /*don't update edge tips*/);
                    }
                }

                i = k;
            }
        }
    }

    void buildLeafGeometries()
    {
        foreach(ConvexSubspace const &subspace, convexSubspaces)
        {
            /// @todo Move BSP leaf construction here.
            BspLeaf &bspLeaf = *subspace.bspLeaf();

            subspace.buildGeometry(bspLeaf, *mesh);

            // Account the new segments.
            /// @todo Refactor away.
            foreach(OrderedSegment const &oseg, subspace.segments())
            {
                if(oseg.segment->hasSegment())
                {
                    // There is now one more Segment.
                    numSegments += 1;
                }
            }
        }

        /*
         * Finalize the built geometry by adding a twin half-edge for any
         * which don't yet have one.
         */
        foreach(ConvexSubspace const &convexSet, convexSubspaces)
        foreach(OrderedSegment const &oseg, convexSet.segments())
        {
            LineSegment::Side *seg = oseg.segment;

            if(seg->hasSegment() && !seg->back().hasSegment())
            {
                HEdge *hedge = &seg->segment().hedge();
                DENG_ASSERT(!hedge->hasTwin());

                // Allocate the twin from the same mesh.
                hedge->setTwin(hedge->mesh().newHEdge(seg->back().from()));
                hedge->twin().setTwin(hedge);
            }
        }
    }

    void clearBspElement(BspTreeNode &tree)
    {
        LOG_AS("Partitioner::clearBspElement");

        MapElement *elm = tree.userData();
        if(!elm) return;

        if(rootNode) // Built Ok.
        {
            LOG_DEBUG("Clearing unclaimed %s %p.")
                << (tree.isLeaf()? "leaf" : "node") << de::dintptr(elm);
        }

        if(tree.isLeaf())
        {
            DENG2_ASSERT(elm->type() == DMU_BSPLEAF);
            // There is now one less BspLeaf.
            numLeafs -= 1;
        }
        else
        {
            DENG2_ASSERT(elm->type() == DMU_BSPNODE);
            // There is now one less BspNode.
            numNodes -= 1;
        }
        delete elm;
        tree.setUserData(0);

        BspElementMap::iterator found = treeNodeMap.find(elm);
        DENG2_ASSERT(found != treeNodeMap.end());
        treeNodeMap.erase(found);
    }

    void clearAllBspElements()
    {
        foreach(BspTreeNode *node, treeNodeMap)
        {
            clearBspElement(*node);
        }
    }

    BspTreeNode *treeNodeForBspElement(MapElement *ob)
    {
        LOG_AS("Partitioner::treeNodeForBspElement");

        int const elemType = ob->type();
        if(elemType == DMU_BSPLEAF || elemType == DMU_BSPNODE)
        {
            BspElementMap::const_iterator found = treeNodeMap.find(ob);
            if(found == treeNodeMap.end()) return 0;
            return found.value();
        }
        LOG_DEBUG("Attempted to locate using an unknown element %p (type: %d).")
            << de::dintptr(ob) << elemType;
        return 0;
    }

    /**
     * Allocate another Vertex.
     *
     * @param origin  Origin of the vertex in the map coordinate space.
     *
     * @return  Newly created Vertex.
     */
    Vertex *newVertex(Vector2d const &origin)
    {
        Vertex *vtx = mesh->newVertex(origin);

        // There is now one more Vertex.
        numVertexes += 1;

        return vtx;
    }

    bool release(MapElement *elm)
    {
        switch(elm->type())
        {
        case DMU_SEGMENT:
            /// @todo fixme: Implement a mechanic for tracking Segment ownership.
            return true;

        case DMU_BSPLEAF:
        case DMU_BSPNODE: {
            BspTreeNode *treeNode = treeNodeForBspElement(elm);
            if(treeNode)
            {
                BspElementMap::iterator found = treeNodeMap.find(elm);
                DENG2_ASSERT(found != treeNodeMap.end());
                treeNodeMap.erase(found);

                treeNode->setUserData(0);
                if(treeNode->isLeaf())
                {
                    // There is now one fewer BspLeaf.
                    numLeafs -= 1;
                }
                else
                {
                    // There is now one fewer BspNode.
                    numNodes -= 1;
                }
                return true;
            }
            break; }

        default: break;
        }

        // This object is not owned by us.
        return false;
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
    void printSuperBlockSegments(SuperBlock const &block)
    {
        foreach(LineSegment::Side const *seg, block.segments())
        {
            LOG_DEBUG("Build: %s line segment %p sector: %d %s -> %s")
                << (seg->hasMapSide()? "map" : "part")
                << de::dintptr(seg)
                << (seg->hasSector()? seg->sector().indexInMap() : -1)
                << seg->from().origin().asText() << seg->to().origin().asText();
        }
    }
#endif
};

Partitioner::Partitioner(int splitCostFactor) : d(new Instance(this, splitCostFactor))
{}

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

/**
 * Algorithm (description courtesy of Raphael Quinet):
 *
 * 1. Create one Seg for each Side: pick each Line in turn.  If it
 *    has a "first" Side, then create a normal Seg.  If it has a
 *    "second" Side, then create a flipped Seg.
 * 2. Call CreateNodes with the current list of Segs.  The list of Segs is
 *    the only argument to CreateNodes.
 * 3. Save the Nodes, Segs and BspLeafs to disk.  Start with the leaves of
 *    the Nodes tree and continue up to the root (last Node).
 */
BspTreeNode *Partitioner::buildBsp(LineSet const &lines, Mesh &mesh)
{
    d->clear();

    d->lines = lines; // make a copy.
    d->mesh  = &mesh;

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

    SuperBlockmap rootBlock(blockmapBounds(bounds));

    d->createInitialLineSegments(rootBlock);

    d->rootNode = d->divideSpace(rootBlock);

    // At this point we know that *something* useful was built.
    d->splitOverlappingLineSegments();
    d->buildLeafGeometries();

    // Find the half-edges at the edge of each map line side.
    /// @todo Optimize: Performing a search for both sides of the same map
    /// line should be unnecessary provided we produced a complete tree with
    /// no degenerate leaf geometries...
    foreach(LineSegment *lineSeg, d->lineSegments)
    for(int i = 0; i < 2; ++i)
    {
        LineSegment::Side &seg = lineSeg->side(i);

        if(!seg.hasMapSide()) continue;
        if(!seg.hasSegment()) continue; // Oh dear...

        // Find the left-most segment.
        LineSegment::Side *left = &seg;
        while(left->hasLeft() && left->left().hasSegment())
        { left = &left->left(); }

        seg.mapSide().setLeftSegment(left->segmentPtr());

        // Find the right-most segment.
        LineSegment::Side *right = &seg;
        while(right->hasRight() && right->right().hasSegment())
        { right = &right->right(); }

        seg.mapSide().setRightSegment(right->segmentPtr());
    }

    return d->rootNode;
}

BspTreeNode *Partitioner::root() const
{
    return d->rootNode;
}

int Partitioner::numNodes()
{
    return d->numNodes;
}

int Partitioner::numLeafs()
{
    return d->numLeafs;
}

int Partitioner::numSegments()
{
    return d->numSegments;
}

int Partitioner::numVertexes()
{
    return d->numVertexes;
}

void Partitioner::take(MapElement *mapElement)
{
    if(!d->release(mapElement))
    {
        LOG_AS("Partitioner::release");
        LOG_DEBUG("Attempted to release an unknown/unowned %s %p.")
            << DMU_Str(mapElement->type()) << de::dintptr(mapElement);
    }
}

} // namespace de
