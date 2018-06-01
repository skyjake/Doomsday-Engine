/** @file bindings_world.cpp  World related Doomsday Script bindings.
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

#include "world/bindings_world.h"
#include "world/clientserverworld.h"
#include "world/map.h"
#include "world/thinkers.h"
#include "audio/audiosystem.h"
#include "dd_main.h"

#include <doomsday/defs/ded.h>
#include <doomsday/world/mobj.h>
#include <de/Context>

using namespace de;

namespace world {

static Value *Function_Thing_Health(Context &ctx, const Function::ArgumentValues &)
{
    return new NumberValue(ClientServerWorld::contextMobj(ctx).health);
}

static Value *Function_Thing_StartSound(Context &ctx, const Function::ArgumentValues &args)
{
    const mobj_t &mo     = ClientServerWorld::contextMobj(ctx);
    const int     sound  = DED_Definitions()->getSoundNum(args.at(0)->asText());
    const float   volume = float(args.at(1)->asNumber());
    if (sound >= 0)
    {
        S_StartSoundAtVolume(sound, &mo, volume);
    }
    else
    {
        throw Error("Function_Thing_StartSound", "Undefined sound: " + args.at(0)->asText());
    }
    return nullptr;
}

void initBindings(Binder &binder, Record &worldModule)
{
    // Thing
    {
        Record &thing = worldModule.addSubrecord("Thing");

        Function::Defaults startSoundArgs;
        startSoundArgs["volume"] = new NumberValue(1.0);

        binder.init(thing)
                << DE_FUNC_NOARG(Thing_Health, "health");
                << DE_FUNC_DEFS (Thing_StartSound, "startSound", "id" << "volume", startSoundArgs);

    }
}

}  // namespace world
