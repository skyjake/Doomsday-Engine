/** @file p_players.cpp  World player entities.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#define DE_NO_API_MACROS_PLAYER

#include "world/p_players.h"
#include "world/impulseaccumulator.h"
#include "world/p_object.h"

#ifdef __CLIENT__
#  include "ui/bindcontext.h"
#  include "ui/b_util.h"
#  include "ui/inputdevice.h"
#  include "ui/inputsystem.h"
#  include "world/map.h"
#  include "world/subsector.h"
#  include "client/clskyplane.h"
#  include "clientapp.h"
#endif

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <doomsday/world/subsector.h>
#include <doomsday/world/surface.h>
#include <de/list.h>
#include <de/keymap.h>

using namespace de;

#ifdef __CLIENT__
ClientPlayer *viewPlayer;
#endif
int consolePlayer;
int displayPlayer;

typedef KeyMap<int, PlayerImpulse *> Impulses; // ID lookup.
typedef KeyMap<String, PlayerImpulse *, String::InsensitiveLessThan> ImpulseNameMap; // Name lookup
typedef KeyMap<int, ImpulseAccumulator *> ImpulseAccumulators; // ImpulseId lookup.

struct ImpulseGlobals
{
    Impulses            impulses;
    ImpulseNameMap      impulsesByName;
    ImpulseAccumulators accumulators[DDMAXPLAYERS];
};

static ImpulseGlobals *s_impulse;

static ImpulseGlobals &impulseGlobals()
{
    if (!s_impulse)
    {
        s_impulse = new ImpulseGlobals;
    }
    return *s_impulse;
}

static PlayerImpulse *addImpulse(int id, impulsetype_t type, const String &name, const String &bindContextName)
{
    auto *imp = new PlayerImpulse;

    imp->id              = id;
    imp->type            = type;
    imp->name            = name;
    imp->bindContextName = bindContextName;

    impulseGlobals().impulses.insert(imp->id, imp);
    impulseGlobals().impulsesByName.insert(imp->name.lower(), imp);

    // Generate impulse accumulators for each player.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        ImpulseAccumulator::AccumulatorType accumType =
            (type == IT_BINARY ? ImpulseAccumulator::Binary : ImpulseAccumulator::Analog);
        auto *accum = new ImpulseAccumulator(imp->id, accumType, type != IT_ANALOG);
        accum->setPlayerNum(i);
        impulseGlobals().accumulators[i].insert(accum->impulseId(), accum);
    }

    return imp;
}

static ImpulseAccumulator *accumulator(int impulseId, int playerNum)
{
    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return nullptr;

    if(!impulseGlobals().accumulators[playerNum].contains(impulseId))
        return nullptr;

    return impulseGlobals().accumulators[playerNum][impulseId];
}

AppPlayer *DD_Player(int number)
{
    // This is either ServerPlayer or ClientPlayer.
    return &DoomsdayApp::players().at(number).as<AppPlayer>();
}

int P_LocalToConsole(int localPlayer)
{
    int count = 0;
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        int console      = (i + consolePlayer) % DDMAXPLAYERS;
        player_t *plr    = DD_Player(console);
        ddplayer_t *ddpl = &plr->publicData();

        if(ddpl->flags & DDPF_LOCAL)
        {
            if(count++ == localPlayer)
                return console;
        }
    }

    // No match!
    return -1;
}

int P_ConsoleToLocal(int playerNum)
{
    int i, count = 0;
    player_t *plr = DD_Player(playerNum);

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
    {
        // Invalid.
        return -1;
    }
    if(playerNum == consolePlayer)
    {
        return 0;
    }

    if(!(plr->publicData().flags & DDPF_LOCAL))
        return -1; // Not local at all.

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        int console = (i + consolePlayer) % DDMAXPLAYERS;
        player_t *plr = DD_Player(console);

        if(console == playerNum)
        {
            return count;
        }

        if(plr->publicData().flags & DDPF_LOCAL)
            count++;
    }
    return -1;
}

int P_GetDDPlayerIdx(ddplayer_t *ddpl)
{
    return DoomsdayApp::players().indexOf(ddpl);
}

#ifdef __CLIENT__

bool P_IsInVoid(player_t *player)
{
    if (!player) return false;
    const ddplayer_t *ddpl = &player->publicData();

    // Cameras are allowed to move completely freely (so check z height
    // above/below ceiling/floor).
    if (ddpl->flags & DDPF_CAMERA)
    {
        if (player->inVoid || !ddpl->mo)
            return true;

        const mobj_t *mob = ddpl->mo;
        if (!Mobj_HasSubsector(*mob))
            return true;

        const auto &subsec = Mobj_Subsector(*mob).as<Subsector>();
        if (subsec.visCeiling().surface().hasSkyMaskedMaterial())
        {
            const ClSkyPlane &skyCeiling = subsec.sector().map().as<Map>().skyCeiling();
            if (skyCeiling.height() < DDMAXFLOAT && mob->origin[2] > skyCeiling.height() - 4)
                return true;
        }
        else if (mob->origin[2] > subsec.visCeiling().heightSmoothed() - 4)
        {
            return true;
        }
        if (subsec.visFloor().surface().hasSkyMaskedMaterial())
        {
            const ClSkyPlane &skyFloor = subsec.sector().map().as<Map>().skyFloor();
            if (skyFloor.height() > DDMINFLOAT && mob->origin[2] < skyFloor.height() + 4)
                return true;
        }
        else if (mob->origin[2] < subsec.visFloor().heightSmoothed() + 4)
        {
            return true;
        }
    }
    return false;
}

#endif // __CLIENT__

void P_ClearPlayerImpulses()
{
    auto &g = impulseGlobals();
    for (dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        g.accumulators[i].deleteAll();
        g.accumulators[i].clear();
    }
    g.impulses.deleteAll();
    g.impulses.clear();
    g.impulsesByName.clear();
}

PlayerImpulse *P_PlayerImpulsePtr(dint id)
{
    auto found = impulseGlobals().impulses.find(id);
    if (found != impulseGlobals().impulses.end()) return found->second;
    return nullptr;
}

PlayerImpulse *P_PlayerImpulseByName(const String &name)
{
    if (!name.isEmpty())
    {
        auto found = impulseGlobals().impulsesByName.find(name);
        if (found != impulseGlobals().impulsesByName.end()) return found->second;
    }
    return nullptr;
}

D_CMD(ListImpulses)
{
    DE_UNUSED(argv, argc, src);

    // Group the defined impulses by binding context.
    KeyMap<String, List<PlayerImpulse *>> contextGroups;
    for (const auto &imp : impulseGlobals().impulsesByName)
    {
        if (!contextGroups.contains(imp.second->bindContextName))
        {
            contextGroups.insert(imp.second->bindContextName, {});
        }
        contextGroups[imp.second->bindContextName] << imp.second;
    }

    LOG_MSG(_E(b) "Player impulses");
    LOG_MSG("There are " _E(b) "%i" _E(.) " impulses, in " _E(b) "%i" _E(.) " contexts")
        << impulseGlobals().impulses.size() << contextGroups.size();

    for (const auto &group : contextGroups)
    {
        if (group.second.isEmpty()) continue;

        LOG_MSG(_E(D) _E(b) "%s" _E(.) " context: " _E(l) "(%i)")
            << group.second.first()->bindContextName << group.second.size();

        for (const auto *imp : group.second)
        {
            LOG_MSG("  [%4i] " _E(>) _E(b) "%s " _E(.) _E(2) "%s%s")
                << imp->id << imp->name
                << (imp->type == IT_BINARY ? "binary" : "analog")
                << (IMPULSETYPE_IS_TRIGGERABLE(imp->type) ? ", triggerable" : "");
        }
    }
    return true;
}

D_CMD(Impulse)
{
    DE_UNUSED(src);

    if(argc < 2 || argc > 3)
    {
        LOG_SCR_NOTE("Usage:\n  %s (impulse-name)\n  %s (impulse-name) (player-ordinal)")
                << argv[0] << argv[0];
        return true;
    }

    if(PlayerImpulse *imp = P_PlayerImpulseByName(argv[1]))
    {
        const int playerNum = (argc == 3? P_LocalToConsole(String(argv[2]).toInt()) : consolePlayer);
        if(ImpulseAccumulator *accum = accumulator(imp->id, playerNum))
        {
            accum->receiveBinary();
        }
    }

    return true;
}

#ifdef __CLIENT__
D_CMD(ClearImpulseAccumulation)
{
    DE_UNUSED(argv, argc, src);

    ClientApp::input().forAllDevices([] (InputDevice &device)
    {
        device.reset();
        return LoopContinue;
    });

    // For all players.
    for (int i = 0; i < DDMAXPLAYERS; ++i)
    {
        for (auto &accum : impulseGlobals().accumulators[i])
        {
            accum.second->clearAll();
        }
    }
    return true;
}
#endif

void P_ConsoleRegister()
{
    C_CMD("listcontrols",   "",         ListImpulses);
    C_CMD("impulse",        nullptr,    Impulse);

#ifdef __CLIENT__
    C_CMD("resetctlaccum",  "",         ClearImpulseAccumulation);

    ImpulseAccumulator::consoleRegister();
#endif
}

#undef DD_GetPlayer
DE_EXTERN_C ddplayer_t *DD_GetPlayer(int number)
{
    return &DD_Player(number)->publicData();
}

// net_main.c
#undef Net_GetPlayerName
#undef Net_GetPlayerID
#undef Net_PlayerSmoother
DE_EXTERN_C const char *Net_GetPlayerName(int player);
DE_EXTERN_C ident_t Net_GetPlayerID(int player);
DE_EXTERN_C Smoother *Net_PlayerSmoother(int player);

#undef P_NewPlayerControl
DE_EXTERN_C void P_NewPlayerControl(int id, impulsetype_t type, const char *name,
    const char *bindContextName)
{
    DE_ASSERT(name && bindContextName);
    LOG_AS("P_NewPlayerControl");

    // Ensure the given id is unique.
    if(const PlayerImpulse *existing = P_PlayerImpulsePtr(id))
    {
        LOG_INPUT_WARNING("Id: %i is already in use by impulse '%s' - Won't replace")
                << id << existing->name;
        return;
    }
    // Ensure the given name is unique.
    if(const PlayerImpulse *existing = P_PlayerImpulseByName(name))
    {
        LOG_INPUT_WARNING("Name: '%s' is already in use by impulse Id: %i - Won't replace")
                << name << existing->id;
        return;
    }

    addImpulse(id, type, name, bindContextName);
}

#undef P_IsControlBound
DE_EXTERN_C int P_IsControlBound(int playerNum, int impulseId)
{
#ifdef __CLIENT__
    LOG_AS("P_IsControlBound");

    // Impulse bindings are associated with local player numbers rather than
    // the player console number - translate.
    const int localPlayer = P_ConsoleToLocal(playerNum);
    if(localPlayer < 0) return false;

    if(const PlayerImpulse *imp = P_PlayerImpulsePtr(impulseId))
    {
        InputSystem &isys = ClientApp::input();

        BindContext *bindContext = isys.contextPtr(imp->bindContextName);
        if(!bindContext)
        {
            LOGDEV_INPUT_WARNING("Unknown binding context '%s'") << imp->bindContextName;
            return false;
        }

        int found = bindContext->forAllImpulseBindings(localPlayer, [&isys, &impulseId] (CompiledImpulseBindingRecord &rec)
        {
            const auto &bind = rec.compiled();

            // Wrong impulse?
            if(bind.impulseId != impulseId) return LoopContinue;

            if(const InputDevice *device = isys.devicePtr(bind.deviceId))
            {
                if(device->isActive())
                {
                    return LoopAbort; // found a binding.
                }
            }
            return LoopContinue;
        });

        return (found? 1 : 0);
    }
    return false;

#else
    DE_UNUSED(playerNum, impulseId);
    return 0;
#endif
}

#undef P_GetControlState
DE_EXTERN_C void P_GetControlState(int playerNum, int impulseId, float *pos,
    float *relativeOffset)
{
#ifdef __CLIENT__
    // Ignore NULLs.
    float tmp;
    if(!pos) pos = &tmp;
    if(!relativeOffset) relativeOffset = &tmp;

    *pos = 0;
    *relativeOffset = 0;

    if(ImpulseAccumulator *accum = accumulator(impulseId, playerNum))
    {
        accum->takeAnalog(pos, relativeOffset);
    }
#else
    DE_UNUSED(playerNum, impulseId, pos, relativeOffset);
#endif
}

#undef P_GetImpulseControlState
DE_EXTERN_C int P_GetImpulseControlState(int playerNum, int impulseId)
{
    LOG_AS("P_GetImpulseControlState");

    ImpulseAccumulator *accum = accumulator(impulseId, playerNum);
    if(!accum) return 0;

    // Ensure this is really a binary accumulator.
    if(accum->type() != ImpulseAccumulator::Binary)
    {
        LOG_INPUT_WARNING("ImpulseAccumulator '%s' is not binary")
            << impulseGlobals().impulses[impulseId]->name;
        return 0;
    }

    return accum->takeBinary();
}

#undef P_Impulse
DE_EXTERN_C void P_Impulse(int playerNum, int impulseId)
{
    LOG_AS("P_Impulse");

    ImpulseAccumulator *accum = accumulator(impulseId, playerNum);
    if(!accum) return;

    // Ensure this is really a binary accumulator.
    if(accum->type() != ImpulseAccumulator::Binary)
    {
        LOG_INPUT_WARNING("ImpulseAccumulator '%s' is not binary")
            << impulseGlobals().impulses[impulseId]->name;
        return;
    }

    accum->receiveBinary();
}

DE_DECLARE_API(Player) =
{
    { DE_API_PLAYER },
    Net_GetPlayerName,
    Net_GetPlayerID,
    Net_PlayerSmoother,
    DD_GetPlayer,
    P_NewPlayerControl,
    P_IsControlBound,
    P_GetControlState,
    P_GetImpulseControlState,
    P_Impulse
};
