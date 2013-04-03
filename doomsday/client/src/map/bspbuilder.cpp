/** @file bspbuilder.cpp BSP Builder.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include <de/Error>
#include <de/Log>
#include <BspBuilder>

#include "map/bsp/partitioner.h"

using namespace de;
using namespace bsp;

DENG2_PIMPL_NOREF(BspBuilder)
{
    /// The space partitioner.
    Partitioner partitioner;

    Instance(GameMap &map, QList<Vertex *> const &editableVertexes)
        : partitioner(map, editableVertexes)
    {}
};

BspBuilder::BspBuilder(GameMap &map, QList<Vertex *> const &editableVertexes,
                       int splitCostFactor)
    : d(new Instance(map, editableVertexes))
{
    d->partitioner.setSplitCostFactor(splitCostFactor);
}

void BspBuilder::setSplitCostFactor(int newFactor)
{
    d->partitioner.setSplitCostFactor(newFactor);
}

bool BspBuilder::buildBsp()
{
    try
    {
        return d->partitioner.build();
    }
    catch(Error const &er)
    {
        LOG_AS("BspBuilder");
        LOG_WARNING("%s.") << er.asText();
    }
    return false;
}

BspTreeNode *BspBuilder::root() const
{
    return d->partitioner.root();
}

uint BspBuilder::numNodes()
{
    return d->partitioner.numNodes();
}

uint BspBuilder::numLeafs()
{
    return d->partitioner.numLeafs();
}

uint BspBuilder::numHEdges()
{
    return d->partitioner.numHEdges();
}

uint BspBuilder::numVertexes()
{
    return d->partitioner.numVertexes();
}

Vertex &BspBuilder::vertex(uint idx)
{
    DENG2_ASSERT(d->partitioner.vertex(idx).type() == DMU_VERTEX);
    return d->partitioner.vertex(idx);
}

void BspBuilder::take(MapElement *mapElement)
{
    d->partitioner.release(mapElement);
}
