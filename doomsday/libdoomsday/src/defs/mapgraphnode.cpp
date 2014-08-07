/** @file defs/mapgraphnode.cpp  MapGraphNode definition accessor.
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/defs/mapgraphnode.h"
#include "doomsday/defs/ded.h"

#include <de/Record>
#include <de/RecordValue>

using namespace de;

namespace defn {

void MapGraphNode::resetToDefaults()
{
    DENG2_ASSERT(_def);

    // Add all expected fields with their default values.
    _def->addText  ("id", "");
    _def->addNumber("warpNumber", 0);
    _def->addArray ("exit", new ArrayValue);
}

MapGraphNode &MapGraphNode::operator = (Record *d)
{
    setAccessedRecord(*d);
    _def = d;
    return *this;
}

int MapGraphNode::order() const
{
    if(!accessedRecordPtr()) return -1;
    return geti("__order__");
}

MapGraphNode::operator bool() const
{
    return accessedRecordPtr() != 0;
}

Record &MapGraphNode::addExit()
{
    DENG2_ASSERT(_def);

    Record *def = new Record;

    def->addText("id", 0);
    def->addText("targetMap", "");

    (*_def)["exit"].value<ArrayValue>()
            .add(new RecordValue(def, RecordValue::OwnsRecord));

    return *def;
}

int MapGraphNode::exitCount() const
{
    return int(geta("exit").size());
}

bool MapGraphNode::hasExit(int index) const
{
    return index >= 0 && index < exitCount();
}

Record &MapGraphNode::exit(int index)
{
    DENG2_ASSERT(_def);
    return *_def->geta("exit")[index].as<RecordValue>().record();
}

Record const &MapGraphNode::exit(int index) const
{
    return *geta("exit")[index].as<RecordValue>().record();
}

} // namespace defn
