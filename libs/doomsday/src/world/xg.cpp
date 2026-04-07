/** @file world/xg.cpp
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

#include "doomsday/world/xg.h"
#include "doomsday/defs/dedparser.h"
#include "doomsday/doomsdayapp.h"

using namespace de;

static xgclass_t nullXgClassLinks;  ///< Used when none defined.
static xgclass_t *xgClassLinks;

void XG_GetGameClasses()
{
    ::xgClassLinks = nullptr;

    // XG class links are provided by the game (which defines the class specific parameter names).
    if (auto getVar = DoomsdayApp::plugins().gameExports().GetPointer)
    {
        ::xgClassLinks = (xgclass_t *) getVar(DD_XGFUNC_LINK);
    }
    if (!::xgClassLinks)
    {
        ::xgClassLinks = &::nullXgClassLinks;
    }

    // Let the parser know of the XG classes.
    DED_SetXGClassLinks(::xgClassLinks);
}

xgclass_t *XG_Class(int number)
{
    return &::xgClassLinks[number];
}
