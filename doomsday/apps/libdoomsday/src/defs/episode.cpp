/** @file episode.cpp  Episode definition accessor.
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
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText("id", "");
    def().addText("startMap", "Maps:"); // URI. Unknown.
    def().addText("title", "Untitled");
    def().addText("menuHelpInfo", "");  // None.
    def().addText("menuImage", "");     // URI. None.
    def().addText("menuShortcut", "");  // Key name. None.
    def().addArray("hub", new ArrayValue);
    def().addArray("map", new ArrayValue);
}

Record &Episode::addHub()
{
    Record *hub = new Record;

    hub->addBoolean("custom", false);

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

Record *Episode::tryFindHubByMapId(String const &mapId)
{
    de::Uri const mapUri(mapId, RC_NULL);
    if(!mapUri.path().isEmpty())
    {
        for(int i = 0; i < hubCount(); ++i)
        {
            Record &hubRec = hub(i);
            foreach(Value *mapIt, hubRec.geta("map").elements())
            {
                Record &mgNodeDef = mapIt->as<RecordValue>().dereference();
                if(mapUri == de::Uri(mgNodeDef.gets("id"), RC_NULL))
                {
                    return &hubRec;
                }
            }
        }
    }
    return 0; // Not found.
}

Record *Episode::tryFindMapGraphNode(String const &mapId)
{
    de::Uri const mapUri(mapId, RC_NULL);
    if(!mapUri.path().isEmpty())
    {
        // First, try the hub maps.
        for(int i = 0; i < hubCount(); ++i)
        {
            Record const &hubRec = hub(i);
            foreach(Value *mapIt, hubRec.geta("map").elements())
            {
                Record &mgNodeDef = mapIt->as<RecordValue>().dereference();
                if(mapUri == de::Uri(mgNodeDef.gets("id"), RC_NULL))
                {
                    return &mgNodeDef;
                }
            }
        }
        // Try the non-hub maps.
        foreach(Value *mapIt, geta("map").elements())
        {
            Record &mgNodeDef = mapIt->as<RecordValue>().dereference();
            if(mapUri == de::Uri(mgNodeDef.gets("id"), RC_NULL))
            {
                return &mgNodeDef;
            }
        }
    }
    return 0; // Not found.
}

de::Record *Episode::tryFindMapGraphNodeByWarpNumber(int warpNumber)
{
    if(warpNumber > 0)
    {
        // First, try the hub maps.
        for(int i = 0; i < hubCount(); ++i)
        {
            Record const &hubRec = hub(i);
            foreach(Value *mapIt, hubRec.geta("map").elements())
            {
                Record &mgNodeDef = mapIt->as<RecordValue>().dereference();
                if(mgNodeDef.geti("warpNumber") == warpNumber)
                {
                    return &mgNodeDef;
                }
            }
        }
        // Try the non-hub maps.
        foreach(Value *mapIt, geta("map").elements())
        {
            Record &mgNodeDef = mapIt->as<RecordValue>().dereference();
            if(mgNodeDef.geti("warpNumber") == warpNumber)
            {
                return &mgNodeDef;
            }
        }
    }
    return 0; // Not found.
}

} // namespace defn
