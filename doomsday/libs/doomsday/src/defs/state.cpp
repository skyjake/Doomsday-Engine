/** @file state.cpp  Mobj state definition.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/defs/state.h"
#include "doomsday/defs/ded.h"

#include <de/record.h>
#include <de/recordvalue.h>

using namespace de;

namespace defn {
    
void State::resetToDefaults()
{
    Definition::resetToDefaults();

    // Add all expected fields with their default values.
    def().addText  (VAR_ID, "");
    def().addText  ("sprite", "");
    def().addNumber("flags", 0);
    def().addNumber("frame", 0);
    def().addNumber("tics", 0);
    def().addText  ("action", "");
    def().addText  ("nextState", "");
    def().addText  ("execute", "");
    def().addArray ("misc").array().addMany(NUM_STATE_MISC, 0);
}

dint32 State::misc(dint index) const
{
    return def().geta("misc")[index].asInt();
}

void State::setMisc(dint index, dint32 value)
{
    def()["misc"].array().setElement(index, value);
}

} // namespace defn
