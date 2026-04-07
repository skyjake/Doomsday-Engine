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

#include <de/record.h>
#include <de/recordvalue.h>

using namespace de;

namespace defn {

void MapInfo::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText  (VAR_ID, "");
    def().addText  ("title", "Untitled");
    def().addText  ("titleImage", "");
    def().addText  ("author", "Unknown");
    def().addNumber("flags", 0);
    def().addText  ("music", "");
    def().addNumber("parTime", -1); // unknown
    def().addArray ("fogColor", new ArrayValue(Vec3f(DEFAULT_FOG_COLOR_RED, DEFAULT_FOG_COLOR_GREEN, DEFAULT_FOG_COLOR_BLUE)));
    def().addNumber("fogStart", DEFAULT_FOG_START);
    def().addNumber("fogEnd", DEFAULT_FOG_END);
    def().addNumber("fogDensity", DEFAULT_FOG_DENSITY);
    def().addText  ("fadeTable", "");
    def().addNumber("ambient", 0);
    def().addNumber("gravity", 1);
    def().addText  ("skyId", "");
    def().addText  ("execute", "");
    def().addText  ("onSetup", "");
    def().addText  (DE_STR("intermissionBg"), "");

    std::unique_ptr<Record> sky(new Record);
    Sky(*sky).resetToDefaults();
    def().add      ("sky", sky.release());
}

} // namespace defn
