/** @file partitionevaluator.cpp  Evaluator for a would-be BSP.
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

#include "world/bsp/partitionevaluator.h"
#include <QList>
#include <de/Log>
#include <de/Task>
#include <de/TaskPool>
#include "world/bsp/partitioner.h"
#include "world/worldsystem.h" // validCount

namespace de {
namespace bsp {

namespace internal
{
    struct PartitionCost
    {
        int total;
        int splits;
        int iffy;
        int nearMiss;
        int mapRight;
        int mapLeft;
        int partRight;
        int partLeft;

        PartitionCost() :
            total(0), splits(0), iffy(0), nearMiss(0), mapRight(0),
            mapLeft(0), partRight(0), partLeft(0)
        {}

        inline PartitionCost &addSegmentRight(LineSegmentSide const &seg)
        {
            if(seg.hasMapSide()) mapRight  += 1;
            else                 partRight += 1;
            return *this;
        }

        inline PartitionCost &addSegmentLeft(LineSegmentSide const &seg)
        {
            if(seg.hasMapSide()) mapLeft  += 1;
            else                 partLeft += 1;
            return *this;
        }

        PartitionCost &operator += (PartitionCost const &other)
        {
            total     += other.total;
            splits    += other.splits;
            iffy      += other.iffy;
            nearMiss  += other.nearMiss;
            mapLeft   += other.mapLeft;
            mapRight  += other.mapRight;
            partLeft  += other.partLeft;
            partRight += other.partRight;
            return *this;
        }

        PartitionCost &operator = (PartitionCost const &other)
        {
            total     = other.total;
            splits    = other.splits;
            iffy      = other.iffy;
            nearMiss  = other.nearMiss;
            mapLeft   = other.mapLeft;
            mapRight  = other.mapRight;
            partLeft  = other.partLeft;
            partRight = other.partRight;
            return *this;
        }

        bool operator < (PartitionCost const &rhs) const
        {
            return total < rhs.total;
        }

        String asText() const
        {
            return String("PartitionCost(Total= %1.%2; splits:%3, iffy:%4, near:%5, left:%6+%7, right:%8+%9)")
                    .arg(total / 100).arg(total % 100, 2, QChar('0'))
                    .arg(splits).arg(iffy).arg(nearMiss)
                    .arg(mapLeft).arg(partLeft)
                    .arg(mapRight).arg(partRight);
        }
    };

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
}

using namespace internal;

DENG2_PIMPL_NOREF(PartitionEvaluator)
{
    int splitCostFactor;

    LineSegmentBlockTreeNode *rootNode; ///< Current block tree root node.

    Instance() : splitCostFactor(7), rootNode(0)
    {}

    struct PartitionCandidate
    {
        LineSegmentSide *line;  ///< Candidate partition line.
        PartitionCost cost;     ///< Running cost metric total.

        PartitionCandidate(LineSegmentSide &partition) : line(&partition)
        {}
    };
    typedef QList<PartitionCandidate *> Candidates;
    Candidates candidates;

    PartitionCandidate *nextCandidate()
    {
        DENG2_ASSERT(costTaskPool.isDone());
        if(candidates.isEmpty()) return 0;
        return candidates.takeFirst();
    }

    class CostTask : public Task
    {
    public:
        Instance &evaluator;
        PartitionCandidate &candidate;

        CostTask(Instance &evaluator, PartitionCandidate &candidate)
            : evaluator(evaluator), candidate(candidate)
        {}

        /**
         * Evaluate the cost of the partition candidate.
         *
         * If the candidate is not suitable (or a better choice has already been
         * determined) then @var partition is zeroed. Otherwise the candidate is
         * suitable and @var cost contains valid costing metrics.
         */
        void runTask()
        {
            LineSegmentSide **partition = &candidate.line;
            PartitionCost &cost         = candidate.cost;

            if(evaluator.rootNode->traversePreOrder(costForBlockWorker, this))
            {
                // Unsuitable or we already have a better choice.
                *partition = 0;
                return;
            }

            // Make sure there is at least one map line segment on each side.
            if(!cost.mapLeft || !cost.mapRight)
            {
                //LOG_DEBUG("evaluate: No map line segments on %s%sside")
                //    << (cost.mapLeft? "" : "left ")
                //    << (cost.mapRight? "" : "right ");
                *partition = 0;
                return;
            }

            // This is suitable for use as a partition.

            // Increase cost by the difference between left and right.
            cost.total += 100 * abs(cost.mapLeft - cost.mapRight);

            // Allow partition segment counts to affect the outcome.
            cost.total += 50 * abs(cost.partLeft - cost.partRight);

            // Another little twist, here we show a slight preference for partition
            // lines that lie either purely horizontally or purely vertically.
            if((*partition)->slopeType() != ST_HORIZONTAL &&
               (*partition)->slopeType() != ST_VERTICAL)
            {
                cost.total += 25;
            }
        }

    private:
        void costForSegment(LineSegmentSide const &seg)
        {
            LineSegmentSide **partition = &candidate.line;
            PartitionCost &cost         = candidate.cost;
            int const splitCostFactor   = evaluator.splitCostFactor;

            /// Determine the relationship between @a seg and the partition plane.
            coord_t fromDist, toDist;
            LineRelationship rel = seg.relationship(**partition, &fromDist, &toDist);
            switch(rel)
            {
            case Collinear: {
                // This line segment runs along the same line as the partition.
                // Check whether it goes in the same direction or the opposite.
                if(seg.direction().dot((*partition)->direction()) < 0)
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
                    cost.total += int( 100 * splitCostFactor * (nearDist * nearDist - 1.0) );
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
                    cost.total += int( 70 * splitCostFactor * (nearDist * nearDist - 1.0) );
                }
                break; }

            case Intersects: {
                cost.splits += 1;
                cost.total  += 100 * splitCostFactor;

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
                    cost.total += int( 140 * splitCostFactor * (nearDist * nearDist - 1.0) );
                }
                break; }
            }
        }

        /**
         * Test the whole block against the partition line to quickly handle all the
         * line segments within it at once. Only when the partition line intercepts
         * the block do we need to go deeper into it.
         */
        void costForBlock(LineSegmentBlock const &block)
        {
            LineSegmentSide **partition = &candidate.line;
            PartitionCost &cost         = candidate.cost;

            /// @todo Why are we extending the bounding box for this test? Also,
            /// there is no need to convert from integer to floating-point each
            /// time this is tested. (If we intend to do this with floating-point
            /// then we should return that representation in SuperBlock::bounds() ).
            AABoxd bounds(coord_t( block.bounds().minX ) - SHORT_HEDGE_EPSILON * 1.5,
                          coord_t( block.bounds().minY ) - SHORT_HEDGE_EPSILON * 1.5,
                          coord_t( block.bounds().maxX ) + SHORT_HEDGE_EPSILON * 1.5,
                          coord_t( block.bounds().maxY ) + SHORT_HEDGE_EPSILON * 1.5);

            int side = (*partition)->boxOnSide(bounds);
            if(side > 0)
            {
                // Right.
                cost.mapRight  += block.mapCount();
                cost.partRight += block.partCount();
                return;
            }
            if(side < 0)
            {
                // Left.
                cost.mapLeft  += block.mapCount();
                cost.partLeft += block.partCount();
                return;
            }

            foreach(LineSegmentSide *otherSeg, block.all())
            {
                costForSegment(*otherSeg);
            }
        }

        /// @return  @c false iff the partition is still suitable.
        static int costForBlockWorker(LineSegmentBlockTreeNode &node, void *context)
        {
            CostTask &task = *static_cast<CostTask *>(context);

            task.costForBlock(*node.userData());

            return task.candidate.line == 0;
        }
    };
    TaskPool costTaskPool;

    /**
     * To be able to divide the nodes down, evaluate must decide which is the
     * best line segment to use as a partition. It does this by selecting the
     * line with least splits and has least difference of line segments on either
     * side of it.
     *
     * @param line  Partition line to evaluate.
     */
    void beginPartitionCosting(LineSegmentSide *line)
    {
        DENG2_ASSERT(line != 0 && line->hasMapSide());
        // Run a new partition cost task.
        PartitionCandidate *newCandidate = new PartitionCandidate(*line);
        candidates << newCandidate;
        costTaskPool.start(new CostTask(*this, *newCandidate));
    }
};

PartitionEvaluator::PartitionEvaluator(int splitCostFactor) : d(new Instance)
{
    d->splitCostFactor = splitCostFactor;
}

LineSegmentSide *PartitionEvaluator::choose(LineSegmentBlockTreeNode &node)
{
    LOG_AS("PartitionEvaluator");

    d->rootNode = &node;

    // Increment valid count so we can avoid testing the line segments
    // produced from a single line more than once per round of partition
    // selection.
    validCount++;

    // Iterative pre-order traversal.
    LineSegmentBlockTreeNode const *cur = d->rootNode;
    LineSegmentBlockTreeNode const *prev = 0;
    while(cur)
    {
        while(cur)
        {
            LineSegmentBlock const &segs = *cur->userData();

            // Test each line segment as a potential partition candidate.
            foreach(LineSegmentSide *candidate, segs.all())
            {
                //LOG_DEBUG("%sline segment %p sector:%d %s -> %s")
                //    << (candidate->hasMapLineSide()? "" : "mini-") << candidate
                //    << (candidate->sector? candidate->sector->indexInMap() : -1)
                //    << candidate->fromOrigin().asText()
                //    << candidate->toOrigin().asText();

                // Only map line segments are suitable candidates.
                if(!candidate->hasMapSide())
                    continue;

                // Optimization: Only the first line segment produced from a
                // given line is tested per round of partition costing because
                // they are all collinear.
                if(candidate->mapLine().validCount() == validCount)
                    continue; // Skip this.

                // Don't consider further segments of the candidate.
                candidate->mapLine().setValidCount(validCount);

                // Determine candidate suitability and cost.
                d->beginPartitionCosting(candidate);
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

    LineSegmentSide *best = 0;
    if(!d->candidates.isEmpty())
    {
        d->costTaskPool.waitForDone();
        PartitionCost bestCost;
        while(Instance::PartitionCandidate *candidate = d->nextCandidate())
        {
            //LOG_DEBUG("%p: %s") << candidate->line << candidate->cost.asText();

            if(candidate->line && (!best || candidate->cost < bestCost))
            {
                // We have a new better choice.
                best     = candidate->line;
                bestCost = candidate->cost;
            }

            delete candidate;
        }

        // LOG_DEBUG("best %p score: %d.%02d.")
        //      << best << bestCost.total / 100 << bestCost.total % 100;
    }

    return best;
}

} // namespace bsp
} // namespace de
