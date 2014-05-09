/** @file partitioncostevaluator.h  Cost evaluator for a would-be BSP.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_BSP_PARTITIONCOSTEVALUATOR_H
#define DENG_WORLD_BSP_PARTITIONCOSTEVALUATOR_H

#include "world/bsp/partitioncost.h"
#include "world/bsp/superblockmap.h"

namespace de {
namespace bsp {

/**
 * Evaluate a partition and determine the cost, taking into account the
 * number of splits and the difference between left and right.
 */
class PartitionCostEvaluator
{
public:
    /**
     * @param node      Node at which to begin the descent.
     * @param bestline  Best partition line candidate found thus far.
     * @param bestCost  Cost result for @a bestLine.
     */
    PartitionCostEvaluator(SuperBlockmapNode const &node,
                           LineSegmentSide const *best, PartitionCost const &bestCost);

    /**
     * Change the multiplier for a line split to @a newCostFactor.
     */
    void setSplitCost(int newCostFactor);

    /**
     * To be able to divide the nodes down, evaluate must decide which is the
     * best line segment to use as a nodeline. It does this by selecting the line
     * with least splits and has least difference of line segments on either side
     * of it.
     *
     * @param line  Partition line to evaluate.
     * @param cost  Final costing is written here.
     *
     * @return  @c true iff @a line is suitable.
     */
    bool costPartition(LineSegmentSide const &line, PartitionCost &cost);

private:
    DENG2_PRIVATE(d)
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_BSP_PARTITIONCOSTEVALUATOR_H
