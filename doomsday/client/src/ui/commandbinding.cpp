/** @file commandbinding.cpp  Command binding record accessor.
 *
 * @authors Copyright © 2009-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include "ui/commandbinding.h"

#include <de/Log>
#include "ui/b_util.h"

using namespace de;

void CommandBinding::resetToDefaults()
{
    def().addNumber("id", 0);  ///< Unique identifier.

    def().addNumber("deviceId", -1);
    def().addNumber("controlId", -1);
    def().addNumber("type", int(E_TOGGLE));  ///< Type of event.
    def().addText  ("symbolicName", "");
    def().addNumber("test", int(Condition::None));
    def().addNumber("pos", 0);

    def().addText  ("command", "");  ///< Command to execute.
}

String CommandBinding::composeDescriptor()
{
    LOG_AS("CommandBinding");
    if(!*this) return "";

    String str = B_ControlDescToString(geti("deviceId"), ddeventtype_t(geti("type")), geti("controlId"));
    switch(geti("type"))
    {
    case E_TOGGLE:      str += B_ButtonStateToString(Condition::ControlTest(geti("test"))); break;
    case E_AXIS:        str += B_AxisPositionToString(Condition::ControlTest(geti("test")), getf("pos")); break;
    case E_ANGLE:       str += B_HatAngleToString(getf("pos")); break;
    case E_SYMBOLIC:    str += "-" + gets("symbolicName"); break;

    default: DENG2_ASSERT(!"CommandBinding::composeDescriptor: Unknown bind.type"); break;
    }

    // Append any state conditions.
    for(BindingCondition const &cond : conditions)
    {
        str += " + " + B_ConditionToString(cond);
    }

    return str;
}
