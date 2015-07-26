/** @file world/actions.cpp
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/actions.h"
#include "doomsday/gameapi.h"
#include "doomsday/doomsdayapp.h"

#include <de/String>
#include <QMap>

using namespace de;

typedef QMap<String, acfnptr_t> ActionMap;  ///< name => native function pointer.
static ActionMap actions;

void P_GetGameActions()
{
    ::actions.clear();

    // Action links are provided by the game (which owns the actual action functions).
    if(auto getVar = DoomsdayApp::plugins().gameExports().GetVariable)
    {
        auto const *links = (actionlink_t const *) getVar(DD_ACTION_LINK);
        for(actionlink_t const *link = links; link && link->name; link++)
        {
            ::actions.insert(String(link->name).toLower(), link->func);
        }
    }
}

acfnptr_t P_GetAction(String const &name)
{
    if(!name.isEmpty())
    {
        auto found = actions.find(name.toLower());
        if(found != actions.end()) return found.value();
    }
    return nullptr;  // Not found.
}

acfnptr_t P_GetAction(char const *name)
{
    return P_GetAction(String(name));
}
