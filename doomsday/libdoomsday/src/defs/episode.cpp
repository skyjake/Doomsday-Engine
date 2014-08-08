/** @file defs/episode.cpp  Episode definition accessor.
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

#include "doomsday/defs/episode.h"
#include "doomsday/defs/ded.h"

#include <de/Record>
#include <de/RecordValue>

using namespace de;

namespace defn {

void Episode::resetToDefaults()
{
    // Add all expected fields with their default values.
    def().addText("id", "");
    def().addText("startMap", "Maps:"); // URI. Unknown.
    def().addText("title", "Untitled");
    def().addText("menuHelpInfo", "");  // None.
    def().addText("menuImage", "");     // URI. None.
    def().addText("menuShortcut", "");  // Key name. None.
    def().addArray("hub", new ArrayValue);
}

Record &Episode::addHub()
{
    Record *hub = new Record;

    hub->addText ("id", "");
    hub->addArray("map", new ArrayValue);

    def()["hub"].value<ArrayValue>()
            .add(new RecordValue(hub, RecordValue::OwnsRecord));

    return *hub;
}

int Episode::hubCount() const
{
    return int(geta("hub").size());
}

bool Episode::hasHub(int index) const
{
    return index >= 0 && index < hubCount();
}

Record &Episode::hub(int index)
{
    return *def().geta("hub")[index].as<RecordValue>().record();
}

Record const &Episode::hub(int index) const
{
    return *geta("hub")[index].as<RecordValue>().record();
}

Record *Episode::tryFindMapGraphNode(String const &mapId)
{
    de::Uri const mapUri(mapId, RC_NULL);
    if(!mapUri.path().isEmpty())
    {
        for(int i = 0; i < hubCount(); ++i)
        {
            Record const &hubRec = hub(i);
            foreach(Value *mapIt, hubRec.geta("map").elements())
            {
                Record &mapGraphNode = mapIt->as<RecordValue>().dereference();
                if(mapUri == de::Uri(mapGraphNode.gets("id"), RC_NULL))
                {
                    return &mapGraphNode;
                }
            }
        }
    }
    return 0;
}

} // namespace defn
