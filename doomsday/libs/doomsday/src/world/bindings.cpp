/** @file world/bindings.cpp  World related Doomsday Script bindings.
 *
 * @authors Copyright (c) 2015-2021 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../src/world/bindings.h"
#include "doomsday/defs/ded.h"
#include "doomsday/doomsdayapp.h"
#include "doomsday/players.h"
#include "doomsday/world/map.h"
#include "doomsday/world/mobj.h"
#include "doomsday/world/thinkers.h"
#include "doomsday/world/mobjthinkerdata.h"
#include "doomsday/world/world.h"
#include <de/scripting/context.h>
#include <de/recordvalue.h>

using namespace de;

namespace world {

//-------------------------------------------------------------------------------------------------

static Value *Function_World_FindThings(Context &, const Function::ArgumentValues &args)
{
    const int type = args.at(0)->asInt();

    std::unique_ptr<ArrayValue> things(new ArrayValue);
    World::get().map().thinkers().forAll(1 | 2, [&things, type](thinker_t *th) {
        if (Thinker_IsMobj(th))
        {
            const mobj_t *mo = (mobj_t *) th;
            if (mo->type == type)
            {
                things->add(new RecordValue(THINKER_DATA(*th, MobjThinkerData).objectNamespace()));
            }
        }
        return LoopContinue;
    });
    return things.release();
}

//-------------------------------------------------------------------------------------------------

static Value *Function_Thing_Init(Context &ctx, const Function::ArgumentValues &args)
{
    ctx.nativeSelf().as<RecordValue>().dereference().set("__id__", args.at(0)->asInt());
    return nullptr;
}

static Value *Function_Thing_SetState(Context &ctx, const Function::ArgumentValues &args)
{
    auto &mo = World::get().contextMobj(ctx);
    Mobj_SetState(&mo, args.at(0)->asInt());
    return nullptr;
}

static Value *Function_Thing_State(Context &ctx, const Function::ArgumentValues &)
{
    const auto &mo = World::get().contextMobj(ctx);
    return new NumberValue(runtimeDefs.states.indexOf(mo.state));
}

static Value *Function_Thing_Angle(Context &ctx, const Function::ArgumentValues &)
{
    const auto &mo = World::get().contextMobj(ctx);
    return new NumberValue(double(mo.angle) / double(ANGLE_MAX) * 360.0);
}

static Value *Function_Thing_SetAngle(Context &ctx, const Function::ArgumentValues &args)
{
    mobj_t &mo = World::contextMobj(ctx);
    mo.angle = angle_t(args.at(0)->asNumber() / 360.0 * ANGLE_MAX);
    mo.visAngle = mo.angle >> 16;
    return nullptr;
}

static Value *Function_Thing_AddMom(Context &ctx, const Function::ArgumentValues &args)
{
    mobj_t &   mo    = World::contextMobj(ctx);
    const auto delta = vectorFromValue<Vec3d>(*args.at(0));
    mo.mom[VX] += delta.x;
    mo.mom[VY] += delta.y;
    mo.mom[VZ] += delta.z;
    return nullptr;
}

static Value *Function_Thing_ChangeFlags(Context &ctx, const Function::ArgumentValues &args)
{
    const int flagsIndex = args.at(0)->asInt();
    auto &mo = World::contextMobj(ctx);
    int &flags = (flagsIndex == 3 ? mo.flags3 : flagsIndex == 2 ? mo.flags2 : mo.flags);
    const int oldFlags = flags;
    const auto value = args.at(1)->asUInt();
    if (args.at(2)->isTrue())
    {
        flags |= value;
}
    else
    {
        flags &= ~value;
    }
    return new NumberValue(oldFlags);
}

static Value *Function_Thing_Flags(Context &ctx, const Function::ArgumentValues &args)
{
    const int flagsIndex = args.at(0)->asInt();
    const auto &mo = World::contextMobj(ctx);
    return new NumberValue(uint32_t(flagsIndex == 3 ? mo.flags3 : flagsIndex == 2 ? mo.flags2 : mo.flags));
}

static Value *Function_Thing_Id(Context &ctx, const Function::ArgumentValues &)
{
    return new NumberValue(World::contextMobj(ctx).thinker.id);
}

static Value *Function_Thing_MapSpotNum(Context &ctx, const Function::ArgumentValues &)
{
    return new NumberValue(World::contextMobj(ctx).mapSpotNum);
}

static Value *Function_Thing_Health(Context &ctx, const Function::ArgumentValues &)
{
    return new NumberValue(World::contextMobj(ctx).health);
}

static Value *Function_Thing_Height(Context &ctx, const Function::ArgumentValues &)
{
    return new NumberValue(World::contextMobj(ctx).height);
}

static Value *Function_Thing_Mom(Context &ctx, const Function::ArgumentValues &)
{
    return new ArrayValue(Vec3d(World::contextMobj(ctx).mom));
}

static Value *Function_Thing_Player(Context &ctx, const Function::ArgumentValues &)
{
    const mobj_t &mo = World::contextMobj(ctx);
    if (mo.dPlayer)
    {
        auto &plrs = DoomsdayApp::players();
        return new RecordValue(plrs.at(plrs.indexOf(mo.dPlayer)).objectNamespace());
    }
    return nullptr;
}

static Value *Function_Thing_Pos(Context &ctx, const Function::ArgumentValues &)
{
    return new ArrayValue(Vec3d(World::contextMobj(ctx).origin));
}

static Value *Function_Thing_Recoil(Context &ctx, const Function::ArgumentValues &args)
{
    mobj_t &     mo    = World::contextMobj(ctx);
    const double force = args.at(0)->asNumber();

    const angle_t angle = mo.angle + ANG180;
    const float angle_f = float(angle) / float(ANGLE_180) * PIf;

    mo.mom[MX] += force * cos(angle_f);
    mo.mom[MY] += force * sin(angle_f);

    return nullptr;
}

static Value *Function_Thing_Type(Context &ctx, const Function::ArgumentValues &)
{
    return new NumberValue(World::contextMobj(ctx).type);
}

//-------------------------------------------------------------------------------------------------

void initBindings(Binder &binder, Record &worldModule)
{
    // Functions
    {
        binder.init(worldModule)
            << DE_FUNC(World_FindThings, "findThings", "typeIndex"); // TODO: add more params to make generic finder
    }

    // Thing
    {
        Record &thing = worldModule.addSubrecord("Thing");

        Function::Defaults startSoundArgs;
        startSoundArgs["volume"] = new NumberValue(1.0);

        binder.init(thing)
                << DE_FUNC      (Thing_Init,       "__init__", "id")
                << DE_FUNC      (Thing_AddMom,     "addMom", "delta")
                << DE_FUNC      (Thing_ChangeFlags,"changeFlags", "index" << "flags" << "doSet")
                << DE_FUNC      (Thing_Flags,      "flags", "index")
                << DE_FUNC_NOARG(Thing_Id,         "id")
                << DE_FUNC_NOARG(Thing_Health,     "health")
                << DE_FUNC_NOARG(Thing_MapSpotNum, "mapSpotNum")
                << DE_FUNC_NOARG(Thing_Height,     "height")
                << DE_FUNC_NOARG(Thing_Mom,        "mom")
                << DE_FUNC_NOARG(Thing_Player,     "player")
                << DE_FUNC_NOARG(Thing_Pos,        "pos")
                << DE_FUNC      (Thing_SetState,   "setState", "index")
                << DE_FUNC_NOARG(Thing_State,      "state")
                << DE_FUNC      (Thing_SetAngle,   "setAngle", "degrees")
                << DE_FUNC_NOARG(Thing_Angle,      "angle")
                << DE_FUNC      (Thing_Recoil,     "recoil", "force")
                << DE_FUNC_NOARG(Thing_Type,       "type");
    }
}

}  // namespace world
