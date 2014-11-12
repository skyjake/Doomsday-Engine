/** @file impulsebinding.cpp  Impulse binding record accessor.
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

#include "ui/impulsebinding.h"

#include <de/Log>
#include "ui/b_util.h"

using namespace de;

void ImpulseBinding::resetToDefaults()
{
    def().addNumber("id", 0);                  ///< Unique identifier.

    def().addNumber("deviceId", -1);
    def().addNumber("controlId", -1);
    def().addNumber("type", int(IBD_TOGGLE));  ///< Type of event.
    def().addNumber("angle", 0);
    def().addNumber("flags", 0);

    def().addNumber("impulseId", 0);           ///< Identifier of the bound player impulse.
    def().addNumber("localPlayer", -1);        ///< Local player number.
}

String ImpulseBinding::composeDescriptor()
{
    LOG_AS("ImpulseBinding");
    if(!*this) return "";

    String str = B_ControlDescToString(geti("deviceId"), IBDTYPE_TO_EVTYPE(geti("type")), geti("controlId"));
    if(geti("type") == IBD_ANGLE)
    {
        str += B_HatAngleToString(getf("angle"));
    }

    // Additional flags.
    if(geti("flags") & IBDF_TIME_STAGED)
    {
        str += "-staged";
    }
    if(geti("flags") & IBDF_INVERSE)
    {
        str += "-inverse";
    }

    // Append any state conditions.
    for(BindingCondition const &cond : conditions)
    {
        str += " + " + B_ConditionToString(cond);
    }

    return str;
}
