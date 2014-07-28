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
    DENG2_ASSERT(_def);

    // Add all expected fields with their default values.
    _def->addText("startMap", "Maps:"); // URI. Unknown.
    _def->addText("title", "Untitled");
    _def->addText("menuHelpInfo", "");  // None.
    _def->addText("menuImage", "");     // URI. None.
    _def->addText("menuShortcut", "");  // Key name. None.
}

Episode &Episode::operator = (Record *d)
{
    setAccessedRecord(*d);
    _def = d;
    return *this;
}

int Episode::order() const
{
    if(!accessedRecordPtr()) return -1;
    return geti("__order__");
}

Episode::operator bool() const
{
    return accessedRecordPtr() != 0;
}

} // namespace defn
