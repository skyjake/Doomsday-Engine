/** @file bindings_world.cpp  World related Doomsday Script bindings.
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

#include "world/bindings_world.h"
#include "world/clientserverworld.h"
#include "world/map.h"
#include "world/thinkers.h"
#include "dd_main.h"

#include <doomsday/world/mobj.h>
#include <de/Context>

using namespace de;

namespace world {

static mobj_t &instanceMobj(Context const &ctx)
{
    /// @todo Not necessarily always the current map. -jk
    int const id = ctx.selfInstance().geti(QStringLiteral("__id__"), 0);
    mobj_t *mo = App_World().map().thinkers().mobjById(id);
    if(!mo)
    {
        throw ::Map::MissingObjectError("instanceMobj", QString("Mobj %1 does not exist").arg(id));
    }
    return *mo;
}

static Value *Function_Thing_Health(Context &ctx, Function::ArgumentValues const &)
{
    return new NumberValue(instanceMobj(ctx).health);
}

void initBindings(Binder &binder, Record &worldModule)
{
    // Thing
    {
        Record &thing = worldModule.addSubrecord("Thing");
        binder.init(thing)
                << DENG2_FUNC_NOARG(Thing_Health, "health");
    }
}

} // namespace world
