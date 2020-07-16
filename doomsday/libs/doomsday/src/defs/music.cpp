/** @file defs/music.cpp  Music definition accessor.
 *
 * @authors Copyright © 2014-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/defs/music.h"
#include "doomsday/defs/ded.h"

#include <de/record.h>
#include <de/recordvalue.h>

using namespace de;

namespace defn {

void Music::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText  (VAR_ID, "");
    def().addText  ("lumpName", "");
    def().addText  ("path", "");
    def().addNumber("cdTrack", 0);
}

dint Music::cdTrack() const
{
    if (dint track = geti("cdTrack"))
        return track;

    const String path = gets("path");
    if (!path.compareWithoutCase("cd"))
    {
        bool ok;
        dint track = path.toInt(&ok);
        if (ok) return track;
    }

    return 0;
}

}  // namespace defn
