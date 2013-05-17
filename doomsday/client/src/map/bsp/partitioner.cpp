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

typedef QHash<MapElement *, BspTreeNode *> BuiltBspElementMap;

/// Used when sorting half-edges by angle around a map point.
typedef QList<HEdge *> HEdgeSortBuffer;

/// Used when collecting line segments to build a leaf. @todo Refactor away.
typedef QList<LineSegment::Side *> LineSegmentSideList;

typedef QHash<HEdge *, LineSegment::Side *> LineSegmentSideMap; /// @todo Refactor away.

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

    /// A map from HEdge -> LineSegment::Side @todo refactor away.
    LineSegmentSideMap lineSegmentMap;

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

    /**
     * Returns the associated LineSegment::Side for the given @a hedge.
     */
    LineSegment::Side &lineSegment(HEdge const &hedge)
    {
        LineSegmentSideMap::iterator found = lineSegmentMap.find(const_cast<HEdge *>(&hedge));
        if(found != lineSegmentMap.end())
        {
            return *found.value();
        }
        throw Error("Partitioner::lineSegment", QString("Failed locating a LineSegment::Side for 0x%1")
                                                    .arg(de::dintptr(&hedge), 0, 16));
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

    /**
     * @return The new line segment (front is from @a start to @a end).
     */
    LineSegment *buildLineSegmentBetweenVertexes(Vertex &start, Vertex &end,
        Sector *frontSec, Sector *backSec, Line::Side *frontSide,
        Line *partitionLine = 0)
    {
        lineSegments.append(new LineSegment(start, end));
        LineSegment &seg = *lineSegments.back();

        LineSegment::Side &front = seg.front();
        front.setMapSide(frontSide);
        front.setPartitionMapLine(partitionLine);
        front.setSector(frontSec);

        front.setHEdge(new HEdge(start, frontSide));
        // There is now one more HEdge.
        numHEdges += 1;

        lineSegmentMap.insert(front.hedgePtr(), &front);

        if(backSec)
        {
            LineSegment::Side &back = seg.back();

            back.setSector(backSec);
            back.setMapSide(frontSide? &frontSide->back() : 0);
            back.setPartitionMapLine(partitionLine);

            back.setHEdge(new HEdge(end, frontSide? &frontSide->back() : 0));
            // There is now one more HEdge.
            numHEdges += 1;

            lineSegmentMap.insert(back.hedgePtr(), &back);

            // Twin the half-edges together.
            front.hedge()._twin = back.hedgePtr();
            back.hedge()._twin  = front.hedgePtr();
        }

        return &seg;
    }

    inline void linkLineSegmentSideInSuperBlockmap(SuperBlock &block, LineSegment::Side &lineSeg)
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

                seg = buildLineSegmentBetweenVertexes(line->from(), line->to(),
                                                      frontSec, backSec,
                                                      &line->front());

                linkLineSegmentSideInSuperBlockmap(blockmap, seg->front());
                if(seg->back().hasSector())
                {
                    linkLineSegmentSideInSuperBlockmap(blockmap, seg->back());
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

    void evalPartitionCostForLineSegmentSide(LineSegment::Side const &plSeg,
        LineSegment::Side const &lineSeg, PartitionCost &cost)
    {
        int const costFactorMultiplier = splitCostFactor;

        /// Determine the relationship between @var lineSeg and the partition plane.
        coord_t fromDist, toDist;
        LineRelationship rel = lineSeg.relationship(plSeg, &fromDist, &toDist);
        switch(rel)
        {
        case Collinear: {
            // This line segment runs along the same line as the partition.
            // Check whether it goes in the same direction or the opposite.
            if(lineSeg.direction().dot(plSeg.direction()) < 0)
            {
                cost.addLineSegmentSideLeft(lineSeg);
            }
            else
            {
                cost.addLineSegmentSideRight(lineSeg);
            }
            break; }

        case Right:
        case RightIntercept: {
            cost.addLineSegmentSideRight(lineSeg);

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
            cost.addLineSegmentSideLeft(lineSeg);

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
     * @param lineSeg   The candidate line segment to be evaluated.
     * @param cost      PartitionCost analysis to be completed for the candidate.
     *                  Must have been initialized prior to calling.
     *
     * @return  @c true iff @a lineSeg is suitable for use as a partition.
     */
    bool evalPartitionCostForSuperBlock(SuperBlock const &block,
        LineSegment::Side *best, PartitionCost const &bestCost,
        LineSegment::Side const &lineSeg, PartitionCost &cost)
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

        int side = lineSeg.boxOnSide(bounds);
        if(side > 0)
        {
            // Right.
            cost.mapRight  += block.mapLineSegmentSideCount();
            cost.partRight += block.partLineSegmentSideCount();
            return true;
        }
        if(side < 0)
        {
            // Left.
            cost.mapLeft  += block.mapLineSegmentSideCount();
            cost.partLeft += block.partLineSegmentSideCount();
            return true;
        }

        // Check partition against all line segments.
        foreach(LineSegment::Side *otherLineSeg, block.lineSegments())
        {
            // Do we already have a better choice?
            if(best && !(cost < bestCost)) return false;

            // Evaluate the cost delta for this line segment.
            PartitionCost costDelta;
            evalPartitionCostForLineSegmentSide(lineSeg, *otherLineSeg, costDelta);

            // Merge cost result into the cummulative total.
            cost += costDelta;
        }

        // Handle sub-blocks recursively.
        if(block.hasRight())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.rightPtr(), best, bestCost,
                                                              lineSeg, cost);
            if(unsuitable) return false;
        }

        if(block.hasLeft())
        {
            bool unsuitable = !evalPartitionCostForSuperBlock(*block.leftPtr(), best, bestCost,
                                                              lineSeg, cost);
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
        foreach(LineSegment::Side *lineSeg, partList.lineSegments())
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
     * Create a clone of an existing line segment.
     */
    LineSegment &cloneLineSegment(LineSegment const &other)
    {
        lineSegments.append(new LineSegment(other.from(), other.to()));
        LineSegment &lineSeg = *lineSegments.last();

        LineSegment::Side &front = lineSeg.front();

        front.setSector(other.front().sectorPtr());
        if(other.front().hasMapSide())
            front.setMapSide(&other.front().mapSide());
        front.setPartitionMapLine(other.front().partitionMapLine());

        if(other.front().hasHEdge())
        {
            front.setHEdge(new HEdge(other.front().hedge()));
            // There is now one more HEdge.
            numHEdges += 1;

            lineSegmentMap.insert(front.hedgePtr(), &front);
        }

        LineSegment::Side &back  = lineSeg.back();

        back.setSector(other.back().sectorPtr());
        if(other.back().hasMapSide())
            back.setMapSide(&other.back().mapSide());
        back.setPartitionMapLine(other.back().partitionMapLine());

        if(other.back().hasHEdge())
        {
            back.setHEdge(new HEdge(other.back().hedge()));
            // There is now one more HEdge.
            numHEdges += 1;

            lineSegmentMap.insert(back.hedgePtr(), &back);

            if(front.hasHEdge())
            {
                // Twin the new half-edge pair with one another.
                front.hedge()._twin = back.hedgePtr();
                back.hedge()._twin  = front.hedgePtr();
            }
        }

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

        LineSegment &newSegment = cloneLineSegment(frontLeft.line());

        // Now perform the split, updating vertex and relative segment links.
        LineSegment::Side &frontRight = newSegment.side(frontLeft.lineSideId());

        frontLeft.line().replaceVertex(frontLeft.lineSideId() ^ 1, *newVert);
        newSegment.replaceVertex(frontLeft.lineSideId(), *newVert); frontRight.hedge()._vertex = newVert;

        frontLeft.setRight(&frontRight);
        frontRight.setLeft(&frontLeft);

        // Handle the twin.
        if(frontLeft.back().hasSector())
        {
            LineSegment::Side &backRight = frontLeft.back();
            LineSegment::Side &backLeft  = frontRight.back();

            backRight.hedge()._vertex = newVert;

            backLeft.setRight(&backRight);
            backRight.setLeft(&backLeft);

            // Has this already been added to a leaf?
            if(backRight.hedge().hasBspLeaf())
            {
                // Update the in-leaf references.
                backRight.hedge()._next = backLeft.hedgePtr();

                // There is now one more half-edge in this leaf.
                backRight.hedge().bspLeaf().poly()._hedgeCount += 1;
            }
        }

        if(frontRight.back().hasSector())
        {
            // Ensure the new back left line segment side is inserted into the
            // same block as the old back right line segment side.
            if(!(frontLeft.back().hasHEdge() && frontLeft.back().hedge().hasBspLeaf()))
            {
                SuperBlock *bmapBlock = frontLeft.back().bmapBlockPtr();
                DENG2_ASSERT(bmapBlock != 0);
                linkLineSegmentSideInSuperBlockmap(*bmapBlock, frontRight.back());
            }
        }

        /**
         * @todo Optimize: Avoid clearing tips by implementing update logic.
         */
        LineSegment &left  = frontLeft.line();
        LineSegment &right = frontRight.line();

        edgeTips(left.from()).clearByLineSegment(left);
        edgeTips(left.to()  ).clearByLineSegment(left);

        edgeTips(right.from()).clearByLineSegment(right);
        edgeTips(right.to()  ).clearByLineSegment(right);

        edgeTips(left.from() ).add(left.front().angle(),
                                   left.front().hasSector()? &left.front() : 0,
                                   left.back().hasSector()?  &left.back()  : 0);

        edgeTips(left.to()   ).add(left.back().angle(),
                                   left.back().hasSector()?  &left.back()  : 0,
                                   left.front().hasSector()? &left.front() : 0);

        edgeTips(right.from()).add(right.front().angle(),
                                   right.front().hasSector()? &right.front() : 0,
                                   right.back().hasSector()?  &right.back()  : 0);

        edgeTips(right.to()  ).add(right.back().angle(),
                                   right.back().hasSector()?  &right.back()  : 0,
                                   right.front().hasSector()? &right.front() : 0);

        return frontRight;
    }

    /**
     * Find the intersection point between a line segment and the current
     * partition plane. Takes advantage of some common situations like
     * horizontal and vertical lines to choose a 'nicer' intersection
     * point.
     */
    Vector2d intersectPartition(LineSegment::Side const &lineSeg, coord_t fromDist,
                                coord_t toDist) const
    {
        // Horizontal partition vs vertical line segment.
        if(hplane.slopeType() == ST_HORIZONTAL && lineSeg.slopeType() == ST_VERTICAL)
        {
            return Vector2d(lineSeg.from().origin().x, hplane.partition().origin.y);
        }

        // Vertical partition vs horizontal line segment.
        if(hplane.slopeType() == ST_VERTICAL && lineSeg.slopeType() == ST_HORIZONTAL)
        {
            return Vector2d(hplane.partition().origin.x, lineSeg.from().origin().y);
        }

        // 0 = start, 1 = end.
        coord_t ds = fromDist / (fromDist - toDist);

        Vector2d point = lineSeg.from().origin();
        if(lineSeg.slopeType() != ST_VERTICAL)
            point.x += lineSeg.direction().x * ds;

        if(lineSeg.slopeType() != ST_HORIZONTAL)
            point.y += lineSeg.direction().y * ds;

        return point;
    }

    /// @todo refactor away -ds
    inline void interceptPartition(LineSegment::Side &lineSeg, int edge)
    {
        hplane.intercept(lineSeg, edge, edgeTips(lineSeg.vertex(edge)));
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
     * @param lineSeg  Line segment to be "partitioned".
     * @param rights   Set of line segments on the right side of the partition.
     * @param lefts    Set of line segments on the left side of the partition.
     */
    void partitionOneLineSegmentSide(LineSegment::Side &lineSeg, SuperBlock &rights, SuperBlock &lefts)
    {
        coord_t fromDist, toDist;
        LineRelationship rel = hplane.relationship(lineSeg, &fromDist, &toDist);
        switch(rel)
        {
        case Collinear: {
            interceptPartition(lineSeg, LineSegment::From);
            interceptPartition(lineSeg, LineSegment::To);

            // Direction (vs that of the partition plane) determines in which
            // subset this line segment belongs.
            if(lineSeg.direction().dot(hplane.partition().direction) < 0)
            {
                linkLineSegmentSideInSuperBlockmap(lefts, lineSeg);
            }
            else
            {
                linkLineSegmentSideInSuperBlockmap(rights, lineSeg);
            }
            break; }

        case Right:
        case RightIntercept:
            if(rel == RightIntercept)
            {
                // Direction determines which edge of the line segment interfaces
                // with the new half-plane intercept.
                interceptPartition(lineSeg, (fromDist < DIST_EPSILON? LineSegment::From : LineSegment::To));
            }
            linkLineSegmentSideInSuperBlockmap(rights, lineSeg);
            break;

        case Left:
        case LeftIntercept:
            if(rel == LeftIntercept)
            {
                interceptPartition(lineSeg, (fromDist > -DIST_EPSILON? LineSegment::From : LineSegment::To));
            }
            linkLineSegmentSideInSuperBlockmap(lefts, lineSeg);
            break;

        case Intersects: {
            // Calculate the intersection point and split this line segment.
            Vector2d point = intersectPartition(lineSeg, fromDist, toDist);
            LineSegment::Side &newFrontRight = splitLineSegment(lineSeg, point);

            interceptPartition(lineSeg, LineSegment::To);

            // Direction determines which subset the line segments are added to.
            if(fromDist < 0)
            {
                linkLineSegmentSideInSuperBlockmap(rights, newFrontRight);
                linkLineSegmentSideInSuperBlockmap(lefts,  lineSeg);
            }
            else
            {
                linkLineSegmentSideInSuperBlockmap(rights, lineSeg);
                linkLineSegmentSideInSuperBlockmap(lefts,  newFrontRight);
            }
            break; }
        }
    }

    /**
     * Remove all the line segments from the list, partitioning them into the
     * left or right sets according to their position relative to partition line.
     * Adds any intersections onto the intersection list as it goes.
     *
     * @param lineSegs  The line segments to be partitioned.
     * @param rights    Set of line segments on the right side of the partition.
     * @param lefts     Set of line segments on the left side of the partition.
     */
    void partitionLineSegments(SuperBlock &lineSegs, SuperBlock &rights,
                               SuperBlock &lefts)
    {
        // Iterative pre-order traversal of SuperBlock.
        SuperBlock *cur = &lineSegs;
        SuperBlock *prev = 0;
        while(cur)
        {
            while(cur)
            {
                LineSegment::Side *lineSeg;
                while((lineSeg = cur->pop()))
                {
                    // Disassociate the line segment from the blockmap.
                    lineSeg->setBMapBlock(0);

                    partitionOneLineSegmentSide(*lineSeg, rights, lefts);
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
        if(!rights.totalLineSegmentSideCount())
            throw Error("Partitioner::partitionLineSegments", "Right set is empty");

        if(!lefts.totalLineSegmentSideCount())
            throw Error("Partitioner::partitionLineSegments", "Left set is empty");
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

        //hplane.printIntercepts();

        // First, fix any near-distance issues with the intercepts.
        hplane.sortAndMergeIntercepts();

        for(int i = 0; i < hplane.intercepts().count() - 1; ++i)
        {
            HPlane::Intercept const &cur  = hplane.intercepts()[i];
            HPlane::Intercept const &next = hplane.intercepts()[i+1];

            if(!cur.after && !next.before)
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

            LineSegment *newSeg =
                buildLineSegmentBetweenVertexes(cur.vertex(), next.vertex(),
                                                sector, sector, 0 /*no map line*/,
                                                hplane.mapLine());

            edgeTips(newSeg->from() ).add(newSeg->front().angle(), &newSeg->front(), &newSeg->back());
            edgeTips(newSeg->to()   ).add(newSeg->back().angle(),  &newSeg->back(),  &newSeg->front());

            // Add each new line segment to the appropriate set.
            linkLineSegmentSideInSuperBlockmap(rights, newSeg->front());
            linkLineSegmentSideInSuperBlockmap(lefts,  newSeg->back());

            /*
            LineSegment::Side *left = right->twinPtr();
            LOG_DEBUG("Capped partition gap:"
                      "\n %p RIGHT sector #%d %s to %s"
                      "\n %p LEFT  sector #%d %s to %s")
                << de::dintptr(right)
                << (right->sector? right->sector->indexInMap() : -1)
                << right->fromOrigin().asText()
                << right->toOrigin().asText()
                << de::dintptr(left)
                << (left->sector? left->sector->indexInMap() : -1)
                << left->fromOrigin().asText()
                << left->toOrigin().asText()
            */
        }
    }

    /**
     * Attempt to construct a new BspLeaf from the list of line segments.
     *
     * @param leafSegments  List of line segments from which to build the leaf.
     *                      Ownership of the list and it's contents is given to
     *                      this function. Once emptied, ownership of the list is
     *                      then returned to the caller.
     *
     * @return  Newly created BspLeaf; otherwise @c 0 if degenerate.
     */
    BspLeaf *buildBspLeaf(LineSegmentSideList &leafSegments)
    {
        if(!leafSegments.count()) return 0;

        bool const isDegenerate = leafSegments.count() < 3;

        Polygon *poly = 0;
        while(!leafSegments.isEmpty())
        {
            LineSegment::Side *lineSeg = leafSegments.takeFirst();

            if(isDegenerate)
            {
                if(lineSeg->hasLeft())
                {
                    lineSeg->left().setRight(lineSeg->hasRight()? &lineSeg->right() : 0);
                }
                if(lineSeg->hasRight())
                {
                    lineSeg->right().setLeft(lineSeg->hasLeft()? &lineSeg->left() : 0);
                }

                // Take ownership of and destroy the half-edge.
                delete lineSeg->hedgePtr();
                lineSeg->setHEdge(0);

                // There is now one fewer half-edge.
                numHEdges -= 1;
                continue;
            }

            // Time to construct the polygon geometry?
            if(!poly)
            {
                poly = new Polygon;
            }

            HEdge &hedge = lineSeg->hedge();

            // Link the half-edge for this line segment to the head of the list
            // in the new polygon geometry.
            hedge._next = poly->_hedge;
            poly->_hedge = &hedge;

            // There is now one more half-edge in this polygon.
            poly->_hedgeCount += 1;
        }

        BspLeaf *leaf = new BspLeaf;

        // There is now one more BspLeaf;
        numLeafs += 1;

        // Assign any built polygon geometry to the BSP leaf (takes ownership).
        leaf->setPoly(poly);

        if(poly)
        {
            // Link the half-edges with the leaf.
            /// @todo Encapsulate in BspLeaf.
            HEdge *hedgeIt = poly->firstHEdge();
            do
            {
                hedgeIt->_bspLeaf = leaf;
            } while((hedgeIt = hedgeIt->_next));
        }

        return leaf;
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
    BspTreeNode *partitionSpace(SuperBlock &lineSegs)
    {
        LOG_AS("Partitioner::partitionSpace");

        MapElement *bspElement = 0; ///< Built BSP map element at this node.
        BspTreeNode *rightTree = 0, *leftTree = 0;

        // Pick a line segment to use as the next partition plane.
        LineSegment::Side *partLineSeg = chooseNextPartition(lineSegs);
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
            SuperBlockmap rightLineSegs(lineSegs.bounds());
            SuperBlockmap leftLineSegs(lineSegs.bounds());

            // Partition the line segements into two subsets according to their
            // spacial relationship with the half-plane (splitting any which
            // intersect).
            partitionLineSegments(lineSegs, rightLineSegs, leftLineSegs);
            lineSegs.clear();
            addPartitionLineSegments(rightLineSegs, leftLineSegs);
            hplane.clearIntercepts();

            // Take a copy of the geometry bounds for each child/sub space
            // - we'll need this for any BspNode we produce later.
            AABoxd rightBounds = rightLineSegs.findLineSegmentSideBounds();
            AABoxd leftBounds  = leftLineSegs.findLineSegmentSideBounds();

            // Recurse on each suspace, first the right space then left.
            rightTree = partitionSpace(rightLineSegs);
            leftTree  = partitionSpace(leftLineSegs);

            // Collapse degenerates upward.
            if(!rightTree || !leftTree)
                return rightTree? rightTree : leftTree;

            // Construct a new BSP node and link up the child elements.
            MapElement *rightChildBspElement = reinterpret_cast<MapElement *>(rightTree->userData());
            MapElement *leftChildBspElement  = reinterpret_cast<MapElement *>(leftTree->userData());

            bspElement = newBspNode(partition, rightBounds, leftBounds,
                                    rightChildBspElement, leftChildBspElement);
        }
        else
        {
            // No partition required/possible - already convex (or degenerate).
            LineSegmentSideList collected = collectLineSegmentSides(lineSegs);

            // Attempt to construct a new BSP leaf.
            BspLeaf *leaf = buildBspLeaf(collected);
            lineSegs.clear();

            // Not a leaf? (collapse upward).
            if(!leaf) return 0;

            bspElement = leaf;
        }

        return newTreeNode(bspElement, rightTree, leftTree);
    }

    /**
     * Sort half-edges by angle (from the middle point to the start vertex).
     * The desired order (clockwise) means descending angles.
     */
    static void sortHEdgesByAngleAroundPoint(HEdgeSortBuffer &hedges,
        int lastHEdgeIndex, Vector2d const &point)
    {
        /// @par Algorithm
        /// "double bubble"
        for(int pass = 0; pass < lastHEdgeIndex; ++pass)
        {
            bool swappedAny = false;
            for(int i = 0; i < lastHEdgeIndex; ++i)
            {
                HEdge const *hedge1 = hedges.at(i);
                HEdge const *hedge2 = hedges.at(i+1);

                Vector2d v1Dist = hedge1->origin() - point;
                Vector2d v2Dist = hedge2->origin() - point;

                coord_t v1Angle = M_DirectionToAngleXY(v1Dist.x, v1Dist.y);
                coord_t v2Angle = M_DirectionToAngleXY(v2Dist.x, v2Dist.y);

                if(v1Angle + ANG_EPSILON < v2Angle)
                {
                    hedges.swap(i, i + 1);
                    swappedAny = true;
                }
            }
            if(!swappedAny) break;
        }
    }

    /**
     * Sort the half-edges linked within the given geometry face in a clockwise
     * order according to their position/orientation relative to the specified
     * point.
     *
     * @param poly        Map geometry polygon containing the list of half-edges
     *                    to be sorted.
     * @param point       Map space point around which to order.
     * @param sortBuffer  Buffer to use for sorting of the hedges.
     *
     * @attention Do NOT move this into Polygon. Although this clearly envies the
     * access rights of the Polygon class this algorithm belongs here in the BSP
     * partitioner. -ds
     */
    static void clockwiseOrder(Polygon &poly, Vector2d const &point,
                               HEdgeSortBuffer &sortBuffer)
    {
        if(!poly._hedge) return;

        // Insert the half-edges into the sort buffer.
#ifdef DENG2_QT_4_7_OR_NEWER
        sortBuffer.reserve(poly._hedgeCount);
#endif
        int i = 0;
        for(HEdge *hedge = poly._hedge; hedge; hedge = hedge->_next, ++i)
        {
            sortBuffer.insert(i, hedge);
        }

        sortHEdgesByAngleAroundPoint(sortBuffer, poly._hedgeCount -1, point);

        // Re-link the half-edge list in the order of the sort buffer.
        poly._hedge = 0;
        for(int i = 0; i < poly._hedgeCount; ++i)
        {
            int idx = (poly._hedgeCount - 1) - i;
            int j = idx % poly._hedgeCount;

            sortBuffer[j]->_next = poly._hedge;
            poly._hedge = sortBuffer[j];
        }
    }

    /**
     * Determine which sector this BSP leaf belongs to.
     */
    Sector *chooseSectorForBspLeaf(BspLeaf const &leaf)
    {
        if(!leaf.firstHEdge()) return 0;

        Sector *selfRefChoice = 0;
        HEdge *base = leaf.firstHEdge();
        HEdge *hedge = base;
        do
        {
            if(hedge->hasLineSide() && hedge->lineSide().hasSector())
            {
                Line &line = hedge->lineSide().line();
                Sector &sector = hedge->lineSide().sector();

                // The first sector from a non self-referencing line is our best choice.
                if(!line.isSelfReferencing())
                    return &sector;

                // Remember the self-referencing choice in case we've no better option.
                if(!selfRefChoice)
                    selfRefChoice = &sector;
            }
        } while((hedge = &hedge->next()) != base);

        if(selfRefChoice) return selfRefChoice;

        /**
         * Last resort:
         * @todo This is only necessary because of other failure cases in the
         * partitioning algorithm and to avoid producing a potentially
         * dangerous BSP - not assigning a sector to each leaf may result in
         * obscure fatal errors when in-game.
         */
        hedge = base;
        do
        {
            LineSegment::Side &seg = lineSegment(*hedge);
            if(seg.hasSector())
            {
                return seg.sectorPtr();
            }
        } while((hedge = &hedge->next()) != base);

        return 0; // Not reachable.
    }

    static Vector2d findPolyCenter(Polygon const &poly)
    {
        Vector2d center;
        int numPoints = 0;
        HEdge const *hedge = poly.firstHEdge();
        forever
        {
            center += hedge->origin();
            numPoints += 1;

            if(!hedge->hasNext())
                break;
            hedge = &hedge->next();
        }
        if(numPoints)
        {
            center /= numPoints;
        }
        return center;
    }

    void clockwisePoly(Polygon &poly, HEdgeSortBuffer &sortBuffer)
    {
        Vector2d center = findPolyCenter(poly);
        clockwiseOrder(poly, center, sortBuffer);

        // LOG_DEBUG("Sorted Polygon half-edges around %s" << center.asText();
        // poly.print();

        // Construct the polygon's half-edge ring.
        if(poly._hedge)
        {
            HEdge *hedge = poly._hedge;
            forever
            {
                /// @todo kludge: This should not be done here!
                if(hedge->hasLineSide())
                {
                    Line::Side &side = hedge->lineSide();

                    // Already processed?
                    if(!side.leftHEdge())
                    {
                        HEdge *leftHEdge = hedge;
                        // Find the left-most half-hedge.
                        while(lineSegment(*leftHEdge).hasLeft())
                            leftHEdge = lineSegment(*leftHEdge).left().hedgePtr();
                        side.setLeftHEdge(leftHEdge);

                        // Find the right-most half-edge.
                        HEdge *rightHEdge = hedge;
                        while(lineSegment(*rightHEdge).hasRight())
                            rightHEdge = lineSegment(*rightHEdge).right().hedgePtr();
                        side.setRightHEdge(rightHEdge);
                    }
                }
                /// kludge end

                if(hedge->hasNext())
                {
                    // Reverse link.
                    hedge->_next->_prev = hedge;
                    hedge = hedge->_next;
                }
                else
                {
                    // Circular link.
                    hedge->_next = poly._hedge;
                    hedge->_next->_prev = hedge;
                    break;
                }
            }
        }
    }

    Sector *findFirstSectorInBspLeaf(BspLeaf const &leaf)
    {
        HEdge const *base = leaf.firstHEdge();
        HEdge const *hedge = base;
        do
        {
            LineSegment::Side &seg = lineSegment(*hedge);
            if(seg.hasSector())
            {
                return seg.sectorPtr();
            }
        } while((hedge = &hedge->next()) != base);
        return 0; // Nothing??
    }

    /**
     * Sort the half-edges in each BSP leaf geometry into a clockwise order.
     *
     * @note This cannot be done during partitionSpace() as splitting a line
     * segment with a twin will result in another half-edge being inserted into
     * that twin's leaf geometry (usually in the wrong place order-wise).
     */
    void windLeafs()
    {
        HEdgeSortBuffer sortBuffer;
        foreach(BspTreeNode *node, treeNodeMap)
        {
            if(!node->isLeaf()) continue;

            BspLeaf *leaf = node->userData()->castTo<BspLeaf>();

            if(leaf->hasPoly())
            {
                /*
                 * Finalize the built geometry.
                 */
                Polygon &geom = leaf->poly();
                HEdge *base = geom.firstHEdge();

                // Sort the half-edges.
                clockwisePoly(geom, sortBuffer);

                // Add a twin half-edge for any which don't yet have one.
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

                if(!sanityCheckHasRealHEdge(*leaf))
                    throw Error("Partitioner::windLeafs",
                                QString("BSP Leaf 0x%1 has no line-linked half-edge")
                                    .arg(dintptr(leaf), 0, 16));

                // Look for migrant half-edges in the wrong sector.
                if(Sector *sector = findFirstSectorInBspLeaf(*leaf))
                {
                    HEdge *hedgeIt = base;
                    do
                    {
                        HEdge &hedge = *hedgeIt;
                        LineSegment::Side const &lineSeg = lineSegment(hedge);
                        if(lineSeg.hasSector() && lineSeg.sectorPtr() != sector)
                        {
                            notifyMigrantHEdgeBuilt(hedge, *sector);
                        }

                    } while((hedgeIt = &hedgeIt->next()) != base);
                }

                // See if we built a partial leaf...
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
                        notifyPartialBspLeafBuilt(*leaf, gaps);
                    }
                }
            }

            leaf->setSector(chooseSectorForBspLeaf(*leaf));
            if(!leaf->hasSector())
            {
                LOG_WARNING("BspLeaf %p is degenerate/orphan (%d half-edges).")
                    << de::dintptr(leaf) << leaf->hedgeCount();
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

    LineSegmentSideList collectLineSegmentSides(SuperBlock &partList)
    {
        LineSegmentSideList lineSegs;

        // Iterative pre-order traversal of SuperBlock.
        SuperBlock *cur = &partList;
        SuperBlock *prev = 0;
        while(cur)
        {
            while(cur)
            {
                LineSegment::Side *lineSeg;
                while((lineSeg = cur->pop()))
                {
                    // Disassociate the line segment from the blockmap.
                    lineSeg->setBMapBlock(0);

                    lineSegs.append(lineSeg);
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
        return lineSegs;
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

    /**
     * Notify interested parties that a partial BSP leaf was built.
     *
     * @param leaf      The incomplete BSP leaf.
     * @param gapTotal  Number of gaps in the leaf.
     */
    void notifyPartialBspLeafBuilt(BspLeaf &leaf, uint gapTotal)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(PartialBspLeafBuilt, i)
        {
            i->partialBspLeafBuilt(leaf, gapTotal);
        }
    }

    /**
     * Notify interested parties that a migrant half-edge was built.
     *
     * @param hedge         The migrant half-edge.
     * @param facingSector  Sector that the half-edge is facing.
     */
    void notifyMigrantHEdgeBuilt(HEdge &migrant, Sector &facingSector)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(MigrantHEdgeBuilt, i)
        {
            i->migrantHEdgeBuilt(migrant, facingSector);
        }
    }

    bool sanityCheckHasRealHEdge(BspLeaf const &leaf) const
    {
        HEdge const *base = leaf.firstHEdge();
        HEdge const *hedge = base;
        do
        {
            if(hedge->hasLineSide()) return true;
        } while((hedge = &hedge->next()) != base);
        return false;
    }

#ifdef DENG_DEBUG
    void printSuperBlockLineSegmentSides(SuperBlock const &block)
    {
        foreach(LineSegment::Side const *lineSeg, block.lineSegments())
        {
            LOG_DEBUG("Build: %s line segment %p sector: %d %s -> %s")
                << (lineSeg->hasMapSide()? "map" : "part")
                << de::dintptr(lineSeg)
                << (lineSeg->hasSector()? lineSeg->sector().indexInMap() : -1)
                << lineSeg->from().origin().asText() << lineSeg->to().origin().asText();
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
    d->windLeafs();
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
