/** @file map/bsp/partitioner.cpp Binary Space Partitioner.
 *
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
 * @authors Copyright &copy; 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright &copy; 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright &copy; 1998-2000 Lee Killough <killough@rsn.hp.com>
 * @authors Copyright &copy; 1997-1998 Raphael.Quinet <raphael.quinet@eed.ericsson.se>
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

#include <cmath>
#include <vector>
#include <algorithm>

#include <QList>
#include <QHash>
#include <QtAlgorithms>

#include <de/Error>
#include <de/Log>

#include "map/gamemap.h"
#include "BspLeaf"
#include "BspNode"
#include "HEdge"
#include "Line"
#include "Sector"
#include "Vertex"

#include "render/r_main.h" /// validCount @todo Remove me

#include "map/bsp/convexsubspace.h"
#include "map/bsp/edgetip.h"
#include "map/bsp/hplane.h"
#include "map/bsp/linesegment.h"
#include "map/bsp/partitioncost.h"
#include "map/bsp/superblockmap.h"

#include "map/bsp/partitioner.h"

using namespace de;
using namespace de::bsp;

typedef std::vector<Vertex *> Vertexes;

typedef QList<EdgeTips> EdgeTipSets;
typedef QList<LineSegment *> LineSegments;
typedef QList<ConvexSubspace> ConvexSubspaces;

typedef QHash<MapElement *, BspTreeNode *> BuiltBspElementMap;

/// Used when collecting line segments to build a leaf. @todo Refactor away.
typedef QList<LineSegment::Side *> LineSegmentList;

DENG2_PIMPL(Partitioner)
{
    /// Cost factor attributed to splitting a line segment.
    int splitCostFactor;

    /// The map we are building BSP data for (not owned).
    GameMap const *map;

    /// Running totals of constructed BSP map elements.
    int numNodes;
    int numLeafs;
    int numHEdges;
    int numVertexes;

    /// Line segments in the plane.
    LineSegments lineSegments;

    /// Convex subspaces in the plane.
    ConvexSubspaces convexSubspaces;

    /// A set of EdgeTips for each unique line segment vertex.
    /// @note May be larger than @var numVertexes (deallocation is lazy).
    EdgeTipSets edgeTipSets;

    /// Additional vertexes produced when splitting line segments.
    /// @note May be larger than @var numVertexes (deallocation is lazy).
    Vertexes vertexes;

    /// Root node of the internal binary tree used to guide the partitioning
    /// process and around which the built BSP map elements are constructed.
    BspTreeNode *rootNode;

    /// Mapping table which relates built BSP map elements to their counterpart
    /// in the internal tree.
    BuiltBspElementMap treeNodeMap;

    /// The "current" binary space half-plane.
    HPlane hplane;

    Instance(Public *i, GameMap const &map, int splitCostFactor)
      : Base(i),
        splitCostFactor(splitCostFactor),
        map(&map),
        numNodes(0), numLeafs(0), numHEdges(0), numVertexes(0),
        rootNode(0)
    {}

    ~Instance()
    {
        clearAllEdgeTips();

        if(rootNode)
        {
            // If ownership of the all built BSP map elements has been claimed
            // this should be a no-op.
            clearAllBspElements();

            // Destroy the internal binary tree.
            delete rootNode;
        }

        qDeleteAll(lineSegments);
    }

    /**
     * Returns the associated EdgeTips set for the given @a vertex.
     */
    inline EdgeTips &edgeTips(Vertex const &vertex) {
        int idx = vertex.indexInMap();
        DENG_ASSERT(idx >= 0 && idx < edgeTipSets.count());
        return edgeTipSets[idx];
    }

    inline void clearEdgeTipsByVertex(Vertex const &vertex) {
        edgeTips(vertex).clear();
    }

    void clearAllEdgeTips()
    {
        DENG2_FOR_EACH(EdgeTipSets, setIt, edgeTipSets)
        {
            setIt->clear();
        }
    }

    struct testForWindowEffectParams
    {
        double frontDist, backDist;
        Sector *frontOpen, *backOpen;
        Line *frontLine, *backLine;
        Line *testLine;
        Vector2d testLineCenter;
        bool castHorizontal;

        testForWindowEffectParams()
            : frontDist(0), backDist(0), frontOpen(0), backOpen(0),
              frontLine(0), backLine(0), testLine(0), castHorizontal(false)
        {}
    };

    static void testForWindowEffect2(Line &line, testForWindowEffectParams &p)
    {
        if(&line == p.testLine) return;
        if(line.isSelfReferencing()) return;
        //if(line._buildData.overlap || line.length() <= 0) return;

        double dist = 0;
        Sector *hitSector = 0;
        bool isFront = false;
        if(p.castHorizontal)
        {
            if(de::abs(line.direction().y) < DIST_EPSILON)
                return;

            if((line.aaBox().maxY < p.testLineCenter.y - DIST_EPSILON) ||
               (line.aaBox().minY > p.testLineCenter.y + DIST_EPSILON))
                return;

            dist = (line.fromOrigin().x +
                    (p.testLineCenter.y - line.fromOrigin().y) * line.direction().x / line.direction().y)
                   - p.testLineCenter.x;

            isFront = ((p.testLine->direction().y > 0) != (dist > 0));
            dist = de::abs(dist);

            // Too close? (overlapping lines?)
            if(dist < DIST_EPSILON)
                return;

            bool dir = (p.testLine->direction().y > 0) ^ (line.direction().y > 0);
            hitSector = line.side(dir ^ !isFront).sectorPtr();
        }
        else // Cast vertically.
        {
            if(de::abs(line.direction().x) < DIST_EPSILON)
                return;

            if((line.aaBox().maxX < p.testLineCenter.x - DIST_EPSILON) ||
               (line.aaBox().minX > p.testLineCenter.x + DIST_EPSILON))
                return;

            dist = (line.fromOrigin().y +
                    (p.testLineCenter.x - line.fromOrigin().x) * line.direction().y / line.direction().x)
                   - p.testLineCenter.y;

            isFront = ((p.testLine->direction().x > 0) == (dist > 0));
            dist = de::abs(dist);

            bool dir = (p.testLine->direction().x > 0) ^ (line.direction().x > 0);
            hitSector = line.side(dir ^ !isFront).sectorPtr();
        }

        // Too close? (overlapping lines?)
        if(dist < DIST_EPSILON)
            return;

        if(isFront)
        {
            if(dist < p.frontDist)
            {
                p.frontDist = dist;
                p.frontOpen = hitSector;
                p.frontLine = &line;
            }
        }
        else
        {
            if(dist < p.backDist)
            {
                p.backDist = dist;
                p.backOpen = hitSector;
                p.backLine = &line;
            }
        }
    }

    static int testForWindowEffectWorker(Line *line, void *parms)
    {
        testForWindowEffect2(*line, *reinterpret_cast<testForWindowEffectParams *>(parms));
        return false; // Continue iteration.
    }

    bool lineMightHaveWindowEffect(Line const &line)
    {
        if(line.definesPolyobj()) return false;
        if(line.hasFrontSector() && line.hasBackSector()) return false;
        if(!line.hasFrontSector()) return false;
        //if(line.hasZeroLength() || line._buildData.overlap) return false;

        // Look for window effects by checking for an odd number of one-sided
        // line owners for a single vertex. Idea courtesy of Graham Jackson.
        if((line.from()._onesOwnerCount % 2) == 1 &&
           (line.from()._onesOwnerCount + line.from()._twosOwnerCount) > 1)
            return true;

        if((line.to()._onesOwnerCount % 2) == 1 &&
           (line.to()._onesOwnerCount + line.to()._twosOwnerCount) > 1)
            return true;

        return false;
    }

    void testForWindowEffect(Line &line)
    {
        if(!lineMightHaveWindowEffect(line)) return;

        testForWindowEffectParams p;
        p.frontDist      = p.backDist = DDMAXFLOAT;
        p.testLine       = &line;
        p.testLineCenter = line.center();
        p.castHorizontal = (de::abs(line.direction().x) < de::abs(line.direction().y)? true : false);

        AABoxd scanRegion = map->bounds();
        if(p.castHorizontal)
        {
            scanRegion.minY = line.aaBox().minY - DIST_EPSILON;
            scanRegion.maxY = line.aaBox().maxY + DIST_EPSILON;
        }
        else
        {
            scanRegion.minX = line.aaBox().minX - DIST_EPSILON;
            scanRegion.maxX = line.aaBox().maxX + DIST_EPSILON;
        }
        validCount++;
        map->linesBoxIterator(scanRegion, testForWindowEffectWorker, &p);

        if(p.backOpen && p.frontOpen && line.frontSectorPtr() == p.backOpen)
        {
            notifyOneWayWindowFound(line, *p.frontOpen);

            line._bspWindowSector = p.frontOpen; /// @todo Refactor away.
        }
    }

    void initForMap()
    {
        // Initialize vertex info for the initial set of vertexes.
#ifdef DENG2_QT_4_7_OR_NEWER
        edgeTipSets.reserve(map->vertexCount());
#endif
        foreach(Vertex *vertex, map->vertexes())
        {
            // Count the total number of one and two-sided line owners for each
            // vertex. (Used in the process of locating window effect lines.)
            vertex->countLineOwners();

            // Add a new EdgeTips set for this vertex.
            edgeTipSets.append(EdgeTips());
        }

        // Search for "one-way window" effects.
        foreach(Line *line, map->lines())
        {
            testForWindowEffect(*line);
        }
    }

    inline void linkSegmentInSuperBlockmap(SuperBlock &block, LineSegment::Side &lineSeg)
    {
        // Associate this line segment with the subblock.
        lineSeg.setBMapBlock(&block.push(lineSeg));
    }

    /**
     * Create all initial line segments and add them to specified SuperBlockmap.
     */
    void createInitialLineSegments(SuperBlock &blockmap)
    {
        foreach(Line *line, map->lines())
        {
            // Polyobj lines are completely ignored.
            if(line->definesPolyobj()) continue;

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

        /// Determine the relationship between @var seg and the partition plane.
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
        if(block.hasRight())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.rightPtr(), best, bestCost,
                                                              seg, cost);
            if(unsuitable) return false;
        }

        if(block.hasLeft())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.leftPtr(), best, bestCost,
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
        SuperBlock const &lineSegList, LineSegment::Side **best, PartitionCost &bestCost)
    {
        DENG2_ASSERT(best != 0);

        //LOG_AS("chooseNextPartitionFromSuperBlock");

        // Test each line segment as a potential partition.
        foreach(LineSegment::Side *lineSeg, partList.segments())
        {
            //LOG_DEBUG("%sline segment %p sector:%d %s -> %s")
            //    << (lineSeg->hasMapLineSide()? "" : "mini-") << de::dintptr(*lineSeg)
            //    << (lineSeg->sector? lineSeg->sector->indexInMap() : -1)
            //    << lineSeg->fromOrigin().asText()
            //    << lineSeg->toOrigin().asText();

            // Optimization: Only the first line segment produced from a given
            // line is tested per round of partition costing (they are all
            // collinear).
            if(lineSeg->hasMapSide())
            {
                // Can we skip this line segment?
                if(lineSeg->mapLine().validCount() == validCount) continue; // Yes.

                lineSeg->mapLine().setValidCount(validCount);
            }

            // Calculate the cost metrics for this line segment.
            PartitionCost cost;
            if(evalPartition(lineSegList, *best, bestCost, *lineSeg, cost))
            {
                // Suitable for use as a partition.
                if(!*best || cost < bestCost)
                {
                    // We have a new better choice.
                    bestCost = cost;

                    // Remember which line segment.
                    *best = lineSeg;
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
    LineSegment::Side &splitLineSegment(LineSegment::Side &frontLeft, Vector2d const &point)
    {
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
    inline void interceptPartition(LineSegment::Side &seg, int edge)
    {
        hplane.intercept(seg, edge, edgeTips(seg.vertex(edge)));
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
    void partitionOneSegment(LineSegment::Side &seg, SuperBlock &rights, SuperBlock &lefts)
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
    void partitionSegments(SuperBlock &segments, SuperBlock &rights,
                           SuperBlock &lefts)
    {
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

                    partitionOneSegment(*seg, rights, lefts);
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
        if(!rights.totalSegmentCount())
            throw Error("Partitioner::partitionSegments", "Right set is empty");

        if(!lefts.totalSegmentCount())
            throw Error("Partitioner::partitionSegments", "Left set is empty");
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
        for(int i = 0; i < hplane.intercepts().count() - 1; ++i)
        {
            HPlane::Intercept const &cur  = hplane.intercepts()[i];
            HPlane::Intercept const &next = hplane.intercepts()[i+1];

            if(!cur.after && !next.before)
                continue;

            // Does this range overlap the partition line segment?
            if(partSeg && cur.distance() >= nearDist && next.distance() <= farDist)
                continue;

            // Check for some nasty open/closed or close/open cases.
            if(cur.after && !next.before)
            {
                if(!cur.selfRef)
                {
                    Vector2d nearPoint = (cur.vertex().origin() + next.vertex().origin()) / 2;
                    notifyUnclosedSectorFound(*cur.after, nearPoint);
                }
                continue;
            }

            if(!cur.after && next.before)
            {
                if(!next.selfRef)
                {
                    Vector2d nearPoint = (cur.vertex().origin() + next.vertex().origin()) / 2;
                    notifyUnclosedSectorFound(*next.before, nearPoint);
                }
                continue;
            }

            // This is definitely open space.
            // Choose the non-self-referencing sector when we can.
            Sector *sector = cur.after;
            if(cur.after != next.before)
            {
                if(!cur.selfRef && !next.selfRef)
                {
                    LOG_DEBUG("Sector mismatch (#%d %s != #%d %s.")
                        << cur.after->indexInMap()
                        << cur.vertex().origin().asText()
                        << next.before->indexInMap()
                        << next.vertex().origin().asText();
                }

                if(cur.selfRef && !next.selfRef)
                    sector = next.before;
            }

            DENG_ASSERT(sector != 0);

            LineSegment &newSeg =
                buildLineSegmentBetweenVertexes(cur.vertex(), next.vertex(),
                                                sector, sector, 0 /*no map line*/,
                                                partSeg? &partSeg->mapLine() : 0);

            edgeTips(newSeg.from()).add(newSeg.front().angle(), &newSeg.front(), &newSeg.back());
            edgeTips(newSeg.to()  ).add(newSeg.back().angle(),  &newSeg.back(),  &newSeg.front());

            // Add each new line segment to the appropriate set.
            linkSegmentInSuperBlockmap(rights, newSeg.front());
            linkSegmentInSuperBlockmap(lefts,  newSeg.back());

            /*
            LineSegment::Side *left = right->twinPtr();
            LOG_DEBUG("Built line segment from %s to %s (sector #%i)")
                << cur.vertex().origin().asText()
                << next.vertex().origin().asText()
                << sector->indexInArchive()
            */
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
     * @param lineSegs  Set of line segments in the plane to be carved into
     *                  convex regions.
     *
     * @return  Newly created subtree; otherwise @c 0 (degenerate).
     */
    BspTreeNode *partitionSpace(SuperBlock &segs)
    {
        LOG_AS("Partitioner::partitionSpace");

        MapElement *bspElement = 0; ///< Built BSP map element at this node.
        BspTreeNode *rightTree = 0, *leftTree = 0;

        // Pick a line segment to use as the next partition plane.
        LineSegment::Side *partLineSeg = chooseNextPartition(segs);
        if(partLineSeg)
        {
            // Reconfigure the half-plane for the next round of partitioning.
            hplane.configure(*partLineSeg);

            //LOG_TRACE("%s, line segment %p %s %s.")
            //    << hplane.partition().asText()
            //    << de::dintptr(*partLineSeg)
            //    << partLineSeg->fromOrigin().asText()
            //    << partLineSeg->toOrigin().asText()

            // Take a copy of the current partition - we'll need this for any
            // BspNode we produce later.
            Partition partition(hplane.partition());

            // Create left and right super blockmaps.
            /// @todo There should be no need to use additional independent
            ///       structures to contain these subsets.
            // Copy the bounding box of the edge list to the superblocks.
            SuperBlockmap rightSegs(segs.bounds());
            SuperBlockmap leftSegs(segs.bounds());

            // Partition the line segements into two subsets according to their
            // spacial relationship with the half-plane (splitting any which
            // intersect).
            partitionSegments(segs, rightSegs, leftSegs);

            segs.clear(); // Should be empty.

            addPartitionLineSegments(rightSegs, leftSegs);

            // Take a copy of the geometry bounds for each child/sub space
            // - we'll need this for any BspNode we produce later.
            AABoxd rightBounds = rightSegs.findSegmentBounds();
            AABoxd leftBounds  = leftSegs.findSegmentBounds();

            // Recurse on each suspace, first the right space then left.
            rightTree = partitionSpace(rightSegs);
            leftTree  = partitionSpace(leftSegs);

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
            LineSegmentList segments = collectLineSegments(segs);

            convexSubspaces.append(ConvexSubspace());
            ConvexSubspace &convexSet = convexSubspaces.last();

            convexSet.addSegments(segments);

            // Attribute all line segments to the convex subspace.
            foreach(LineSegment::Side *seg, segments)
            {
                seg->setConvexSubspace(&convexSet);
            }

            // Produce a BSP leaf.
            /// @todo Defer until necessary.
            BspLeaf *leaf = new BspLeaf;

            // There is now one more BspLeaf;
            numLeafs += 1;

            // Attribute the leaf to the convex subspace.
            convexSet.setBspLeaf(leaf);

            // We have finished with the SuperBlock at this node.
            segs.clear();

            bspElement = leaf;
        }

        return newTreeNode(bspElement, rightTree, leftTree);
    }

    static bool hasSegmentLinkedToMap(ConvexSubspace const &convexSet)
    {
        foreach(LineSegment::Side *seg, convexSet.segments())
        {
            if(seg->hasMapSide())
                return true;
        }
        return false;
    }

    void buildLeafGeometries()
    {
        foreach(ConvexSubspace const &convexSet, convexSubspaces)
        {
            bool const isDegenerate = convexSet.segmentCount() < 3;

            /// @todo Move BSP leaf construction here.
            BspLeaf *leaf = convexSet.bspLeaf();

            if(!isDegenerate)
            {
                if(!hasSegmentLinkedToMap(convexSet))
                    throw Error("Partitioner::buildLeafGeometries",
                                QString("ConvexSubspace 0x%1 has no map line linked segment")
                                    .arg(dintptr(&convexSet), 0, 16));

                // Construct a new polygon geometry and assign it to
                // the BSP leaf (takes ownership).
                leaf->setPoly(convexSet.buildLeafGeometry());

                // Link the half-edges with the leaf and account.
                HEdge *base = leaf->poly().firstHEdge();
                HEdge *hedgeIt = base;
                do
                {
                    // Attribute the half edge to the BSP leaf.
                    /// @todo Encapsulate in BspLeaf.
                    hedgeIt->_bspLeaf = leaf;

                    // There is now one more HEdge.
                    numHEdges += 1;

                } while((hedgeIt = &hedgeIt->next()) != base);
            }
            else
            {
                // Degenerate. Unlink the neighbor segments.
                foreach(LineSegment::Side *seg, convexSet.segments())
                {
                    if(seg->hasLeft())
                    {
                        seg->left().setRight(seg->hasRight()? &seg->right() : 0);
                    }
                    if(seg->hasRight())
                    {
                        seg->right().setLeft(seg->hasLeft()? &seg->left() : 0);
                    }
                }
            }

            // Determine which sector to attribute to the BSP leaf.
            leaf->setSector(convexSet.chooseSectorForBspLeaf());
            if(!leaf->hasSector())
            {
                LOG_WARNING("BspLeaf %p is degenerate/orphan (%d half-edges).")
                    << de::dintptr(leaf) << leaf->hedgeCount();
            }
        }

        /*
         * Finalize the built geometry by adding a twin half-edge for any
         * which don't yet have one.
         */
        foreach(ConvexSubspace const &convexSet, convexSubspaces)
        {
            BspLeaf *leaf = convexSet.bspLeaf();
            if(!leaf->hasPoly())
                continue;

            Polygon &geom = leaf->poly();
            HEdge *base = geom.firstHEdge();
            HEdge *hedgeIt = base;
            do
            {
                HEdge &hedge = *hedgeIt;
                if(!hedge.hasTwin())
                {
                    DENG_ASSERT(&hedge.next() != &hedge);
                    DENG_ASSERT(&hedge.next().vertex() != &hedge.vertex());

                    hedge._twin = new HEdge(hedge.next().vertex());
                    hedge._twin->_twin = &hedge;

                    // There is now one more HEdge.
                    numHEdges += 1;
                }

            } while((hedgeIt = &hedgeIt->next()) != base);

#ifdef DENG_DEBUG
            // See if we built a partial geometry...
            {
                uint gaps = 0;
                HEdge *hedgeIt = base;
                do
                {
                    HEdge &hedge = *hedgeIt;

                    if(hedge.next().origin() != hedge.twin().origin())
                    {
                        gaps++;
                    }
                } while((hedgeIt = &hedgeIt->next()) != base);

                if(gaps > 0)
                {
                    LOG_WARNING("Polygon geometry for BSP leaf [%p] (at %s) in sector %i "
                                "is not contiguous %i gaps/overlaps (%i half-edges).")
                        << de::dintptr(leaf)
                        << leaf->poly().center().asText()
                        << leaf->sector().indexInArchive()
                        << gaps << leaf->hedgeCount();
                    geom.print();
                }
            }
#endif
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

        BuiltBspElementMap::iterator found = treeNodeMap.find(elm);
        DENG2_ASSERT(found != treeNodeMap.end());
        treeNodeMap.erase(found);
    }

    void clearAllBspElements()
    {
        DENG2_FOR_EACH(Vertexes, it, vertexes)
        {
            Vertex *vtx = *it;
            // Has ownership of this vertex been claimed?
            if(!vtx) continue;

            clearEdgeTipsByVertex(*vtx);
            delete vtx;
        }

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
            BuiltBspElementMap::const_iterator found = treeNodeMap.find(ob);
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
        Vertex *vtx = new Vertex(origin);

        /// @todo We do not have authorization to specify this index. -ds
        /// (This job should be done post BSP build.)
        vtx->setIndexInMap(map->vertexCount() + uint(vertexes.size()));
        vertexes.push_back(vtx);

        // There is now one more Vertex.
        numVertexes += 1;
        edgeTipSets.append(EdgeTips());

        return vtx;
    }

    LineSegmentList collectLineSegments(SuperBlock &superblock)
    {
        LineSegmentList segments;

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock *cur = &superblock;
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

                    segments << seg;
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

        return segments;
    }

    bool release(MapElement *elm)
    {
        switch(elm->type())
        {
        case DMU_VERTEX: {
            Vertex *vtx = elm->castTo<Vertex>();
            /// @todo optimize: Poor performance O(n).
            for(uint i = 0; i < vertexes.size(); ++i)
            {
                if(vertexes[i] == vtx)
                {
                    vertexes[i] = 0;
                    // There is now one fewer Vertex.
                    numVertexes -= 1;
                    return true;
                }
            }
            break; }

        case DMU_HEDGE:
            /// @todo fixme: Implement a mechanic for tracking HEdge ownership.
            return true;

        case DMU_BSPLEAF:
        case DMU_BSPNODE: {
            BspTreeNode *treeNode = treeNodeForBspElement(elm);
            if(treeNode)
            {
                BuiltBspElementMap::iterator found = treeNodeMap.find(elm);
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
     * Notify interested parties of a "one-way window" in the map.
     *
     * @param line    The window line.
     * @param backFacingSector  Sector that the back of the line is facing.
     */
    void notifyOneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OneWayWindowFound, i)
        {
            i->oneWayWindowFound(line, backFacingSector);
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

Partitioner::Partitioner(GameMap const &map, int splitCostFactor)
    : d(new Instance(this, map, splitCostFactor))
{
    d->initForMap();
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

void Partitioner::build()
{
    d->rootNode = 0;

    SuperBlockmap rootBlock(blockmapBounds(d->map->bounds()));

    d->createInitialLineSegments(rootBlock);

    d->rootNode = d->partitionSpace(rootBlock);

    // At this point we know that *something* useful was built.
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
        if(!seg.hasHEdge()) continue; // Oh dear...

        // Find the left-most segment.
        LineSegment::Side *left = &seg;
        while(left->hasLeft() && left->left().hasHEdge())
        { left = &left->left(); }

        seg.mapSide().setLeftHEdge(left->hedgePtr());

        // Find the right-most segment.
        LineSegment::Side *right = &seg;
        while(right->hasRight() && right->right().hasHEdge())
        { right = &right->right(); }

        seg.mapSide().setRightHEdge(right->hedgePtr());
    }
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

int Partitioner::numHEdges()
{
    return d->numHEdges;
}

int Partitioner::numVertexes()
{
    return d->numVertexes;
}

Vertex &Partitioner::vertex(int idx)
{
    DENG2_ASSERT(idx >= 0 && (unsigned)idx < d->vertexes.size());
    DENG2_ASSERT(d->vertexes[idx]);
    return *d->vertexes[idx];
}

void Partitioner::release(MapElement *mapElement)
{
    if(!d->release(mapElement))
    {
        LOG_AS("Partitioner::release");
        LOG_DEBUG("Attempted to release an unknown/unowned %s %p.")
            << DMU_Str(mapElement->type()) << de::dintptr(mapElement);
    }
}
