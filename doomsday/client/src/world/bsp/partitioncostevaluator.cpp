/** @file partitioncostevaluator.cpp  Cost evaluator for a would-be BSP.
 *
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "world/bsp/partitioncostevaluator.h"
#include "world/bsp/partitioner.h"

namespace de {
namespace bsp {

DENG2_PIMPL_NOREF(PartitionCostEvaluator)
{
    SuperBlockmapNode const &rootNode;
    LineSegmentSide const *best;
    PartitionCost const &bestCost;

    PartitionCost runningTotal;

    LineSegmentSide const *partition;   ///< The candidate partition line.
    int splitCostFactor;

    Instance(SuperBlockmapNode const &rootNode,
             LineSegmentSide const *best, PartitionCost const &bestCost)
        : rootNode       (rootNode)
        , best           (best)
        , bestCost       (bestCost)
        , partition      (0)
        , splitCostFactor(7)
    {}

    void resetForNewPartition(LineSegmentSide const *newPartition)
    {
        DENG2_ASSERT(newPartition != 0);
        partition    = newPartition;
        runningTotal = PartitionCost(); // Reset the cost.
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
                {
                    *distance = SHORT_HEDGE_EPSILON / de::max(fromDist, toDist);
                }
                else
                {
                    *distance = SHORT_HEDGE_EPSILON / de::min(fromDist, toDist);
                }
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
                {
                    *distance = SHORT_HEDGE_EPSILON / -de::min(fromDist, toDist);
                }
                else
                {
                    *distance = SHORT_HEDGE_EPSILON / -de::max(fromDist, toDist);
                }
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

    void costForSegment(LineSegmentSide const &seg, PartitionCost &segCost)
    {
        /// Determine the relationship between @a seg and the partition plane.
        coord_t fromDist, toDist;
        LineRelationship rel = seg.relationship(*partition, &fromDist, &toDist);
        switch(rel)
        {
        case Collinear: {
            // This line segment runs along the same line as the partition.
            // Check whether it goes in the same direction or the opposite.
            if(seg.direction().dot(partition->direction()) < 0)
            {
                segCost.addSegmentLeft(seg);
            }
            else
            {
                segCost.addSegmentRight(seg);
            }
            break; }

        case Right:
        case RightIntercept: {
            segCost.addSegmentRight(seg);

            /*
             * Near misses are bad, as they have the potential to result in
             * really short line segments being produced later on.
             *
             * The closer the near miss, the higher the cost.
             */
            coord_t nearDist;
            if(nearMiss(rel, fromDist, toDist, &nearDist))
            {
                segCost.nearMiss += 1;
                segCost.total += int( 100 * splitCostFactor * (nearDist * nearDist - 1.0) );
            }
            break; }

        case Left:
        case LeftIntercept: {
            segCost.addSegmentLeft(seg);

            // Near miss?
            coord_t nearDist;
            if(nearMiss(rel, fromDist, toDist, &nearDist))
            {
                /// @todo Why the cost multiplier imbalance between the left
                /// and right edge near misses?
                segCost.nearMiss += 1;
                segCost.total += int( 70 * splitCostFactor * (nearDist * nearDist - 1.0) );
            }
            break; }

        case Intersects: {
            segCost.splits += 1;
            segCost.total  += 100 * splitCostFactor;

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
                segCost.iffy += 1;
                segCost.total += int( 140 * splitCostFactor * (nearDist * nearDist - 1.0) );
            }
            break; }
        }
    }

    /// @return  @c true iff the partition is still suitable.
    bool costForNode(SuperBlockmapNode const &node)
    {
        SuperBlockmapNodeData const &ndata = *node.userData();
        AABox const &nodeBounds            = ndata.bounds();

        /*
         * Test the whole block against the partition line to quickly handle
         * all the line segments within it at once. Only when the partition line
         * intercepts the box do we need to go deeper into it.
         */
        /// @todo Why are we extending the bounding box for this test? Also,
        /// there is no need to convert from integer to floating-point each
        /// time this is tested. (If we intend to do this with floating-point
        /// then we should return that representation in SuperBlock::bounds() ).
        AABoxd bounds(coord_t( nodeBounds.minX ) - SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( nodeBounds.minY ) - SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( nodeBounds.maxX ) + SHORT_HEDGE_EPSILON * 1.5,
                      coord_t( nodeBounds.maxY ) + SHORT_HEDGE_EPSILON * 1.5);

        int side = partition->boxOnSide(bounds);
        if(side > 0)
        {
            // Right.
            runningTotal.mapRight  += ndata.mapSegmentCount();
            runningTotal.partRight += ndata.partSegmentCount();
            return true;
        }
        if(side < 0)
        {
            // Left.
            runningTotal.mapLeft  += ndata.mapSegmentCount();
            runningTotal.partLeft += ndata.partSegmentCount();
            return true;
        }

        // Check partition against all line segments.
        foreach(LineSegmentSide *otherSeg, ndata.segments())
        {
            // Do we already have a better choice?
            if(best && !(runningTotal < bestCost)) return false;

            // Evaluate the cost delta for this line segment.
            PartitionCost costDelta;
            costForSegment(*otherSeg, costDelta);

            runningTotal += costDelta;
        }

        // Factor costings for child nodes recursively.
        if(node.hasRight())
        {
            if(!costForNode(node.right()))
                return false;
        }

        if(node.hasLeft())
        {
            if(!costForNode(node.left()))
                return false;
        }

        return true;
    }
};

PartitionCostEvaluator::PartitionCostEvaluator(SuperBlockmapNode const &node,
    LineSegmentSide const *best, PartitionCost const &bestCost)
    : d(new Instance(node, best, bestCost))
{}

void PartitionCostEvaluator::setSplitCost(int newCostFactor)
{
    d->splitCostFactor = newCostFactor;
}

bool PartitionCostEvaluator::costPartition(LineSegmentSide const &line, PartitionCost &cost)
{
    d->resetForNewPartition(&line);

    // Only map line segments are potential candidates.
    if(!d->partition->hasMapSide())
    {
        cost = d->runningTotal;
        return false;
    }

    if(!d->costForNode(d->rootNode))
    {
        // Unsuitable or we already have a better choice.
        cost = d->runningTotal;
        return false;
    }

    // Make sure there is at least one map line segment on each side.
    if(!d->runningTotal.mapLeft || !d->runningTotal.mapRight)
    {
        //LOG_DEBUG("evalPartition: No map line segments on %s%sside")
        //    << (d->runningTotal.mapLeft? "" : "left ")
        //    << (d->runningTotal.mapRight? "" : "right ");
        cost = d->runningTotal;
        return false;
    }

    // Increase cost by the difference between left and right.
    d->runningTotal.total += 100 * abs(d->runningTotal.mapLeft - d->runningTotal.mapRight);

    // Allow partition segment counts to affect the outcome.
    d->runningTotal.total += 50 * abs(d->runningTotal.partLeft - d->runningTotal.partRight);

    // Another little twist, here we show a slight preference for partition
    // lines that lie either purely horizontally or purely vertically.
    if(d->partition->slopeType() != ST_HORIZONTAL && d->partition->slopeType() != ST_VERTICAL)
    {
        d->runningTotal.total += 25;
    }

    //LOG_DEBUG("evalPartition: %p: splits=%d iffy=%d near=%d"
    //          " left=%d+%d right=%d+%d cost=%d.%02d")
    //    << &lineSeg << d->runningTotal.splits << d->runningTotal.iffy << d->runningTotal.nearMiss
    //    << d->runningTotal.mapLeft << d->runningTotal.partLeft << d->runningTotal.mapRight << d->runningTotal.partRight
    //    << d->runningTotal.total / 100 << d->runningTotal.total % 100;

    cost = d->runningTotal;
    return true;
}

} // namespace bsp
} // namespace de
