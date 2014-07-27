/** @file defs/mapinfo.cpp  MapInfo definition accessor.
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

#include "doomsday/defs/mapinfo.h"
#include "doomsday/defs/sky.h"
#include "doomsday/defs/ded.h"

#include <de/Record>
#include <de/RecordValue>

using namespace de;

namespace defn {

void MapInfo::resetToDefaults()
{
    DENG2_ASSERT(_def);

    // Add all expected fields with their default values.
    _def->addText  ("id", "");
    _def->addText  ("title", "");
    _def->addText  ("author", "");
    _def->addNumber("flags", 0);
    _def->addText  ("music", "");
    _def->addNumber("parTime", -1); // unknown
    _def->addArray ("fogColor", new ArrayValue(Vector3f(138.0f/255, 138.0f/255, 138.0f/255)));
    _def->addNumber("fogStart", 0);
    _def->addNumber("fogEnd", 2100);
    _def->addNumber("fogDensity", .0001f);
    _def->addNumber("ambient", 0);
    _def->addNumber("gravity", 1);
    _def->addText  ("skyId", "");
    _def->add      ("sky", new Sky());
    _def->addText  ("execute", "");
}

MapInfo &MapInfo::operator = (Record *d)
{
    setAccessedRecord(*d);
    _def = d;
    return *this;
}

int MapInfo::order() const
{
    if(!accessedRecordPtr()) return -1;
    return geti("__order__");
}

MapInfo::operator bool() const
{
    return accessedRecordPtr() != 0;
}

} // namespace defn
