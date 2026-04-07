/** @file mapgraphnode.cpp  MapGraphNode definition accessor.
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

#include <de/record.h>
#include <de/recordvalue.h>

using namespace de;

namespace defn {

void MapGraphNode::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText  (VAR_ID, "");
    def().addNumber("warpNumber", 0);
    def().addArray ("exit", new ArrayValue);
}

Record &MapGraphNode::addExit()
{
    Record *exit = new Record;

    exit->addBoolean("custom", false);

    exit->addText(VAR_ID, "");
    exit->addText("targetMap", "");

    def()["exit"].array().add(new RecordValue(exit, RecordValue::OwnsRecord));

    return *exit;
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
    return *def().geta("exit")[index].as<RecordValue>().record();
}

const Record &MapGraphNode::exit(int index) const
{
    return *geta("exit")[index].as<RecordValue>().record();
}

} // namespace defn
