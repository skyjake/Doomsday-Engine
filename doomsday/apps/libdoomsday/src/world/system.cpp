/** @file system.cpp  World subsystem module.
 *
 * @authors Copyright © 2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/system.h"

#include <de/App>

using namespace de;

namespace world {

static world::System *theWorldSystem = nullptr;

DENG2_PIMPL(System)
{
    Instance(Public *i) : Base(i)
    {
        theWorldSystem = thisPublic;
    }
    ~Instance()
    {
        theWorldSystem = nullptr;
    }

    DENG2_PIMPL_AUDIENCE(MapChange)
};

DENG2_AUDIENCE_METHOD(System, MapChange)

System::System() : d(new Instance(this))
{}

void System::timeChanged(Clock const &)
{
    // Nothing to do.
}

world::System &System::get()
{
    DENG2_ASSERT(theWorldSystem);
    return *theWorldSystem;
}

void System::notifyMapChange()
{
    DENG2_FOR_AUDIENCE2(MapChange, i) i->worldSystemMapChanged();
}

}  // namespace world
