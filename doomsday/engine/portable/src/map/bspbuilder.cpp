/**
 * @file bspbuilder.cpp
 * BspBuilder interface class. @ingroup map
 *
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

BspBuilder::BspBuilder(GameMap& map, uint* numEditableVertexes, Vertex*** editableVertexes, int splitCostFactor)
{
    partitioner = new bsp::Partitioner(map, numEditableVertexes, editableVertexes, splitCostFactor);
}

BspBuilder::~BspBuilder()
{
    delete partitioner;
}

BspBuilder& BspBuilder::setSplitCostFactor(int factor)
{
    partitioner->setSplitCostFactor(factor);
    return *this;
}

bool BspBuilder::build()
{
    try
    {
        return partitioner->build();
    }
    catch(de::Error& er)
    {
        LOG_AS("BspBuilder");
        LOG_WARNING("%s.") << er.asText();
    }
    return false;
}

BspTreeNode* BspBuilder::root() const
{
    return partitioner->root();
}

uint BspBuilder::numNodes()
{
    return partitioner->numNodes();
}

uint BspBuilder::numLeafs()
{
    return partitioner->numLeafs();
}

uint BspBuilder::numHEdges()
{
    return partitioner->numHEdges();
}

uint BspBuilder::numVertexes()
{
    return partitioner->numVertexes();
}

Vertex& BspBuilder::vertex(uint idx)
{
    return partitioner->vertex(idx);
}

BspBuilder& BspBuilder::take(runtime_mapdata_header_t* ob)
{
    partitioner->release(ob);
    return *this;
}
