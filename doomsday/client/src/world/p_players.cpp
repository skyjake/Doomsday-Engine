/** @file p_players.cpp  World player entities.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define DENG_NO_API_MACROS_PLAYER

#include "world/p_players.h"

#include <QList>
#include <QMap>
#include <QtAlgorithms>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#ifdef __CLIENT__
#  include "clientapp.h"
#endif

#include "world/impulseaccumulator.h"
#include "world/map.h"
#include "world/p_object.h"
#include "SectorCluster"
#include "Surface"

#ifdef __CLIENT__
#  include "BindContext"
#  include "ui/b_util.h"
#  include "ui/inputdevice.h"
#endif

using namespace de;

player_t *viewPlayer;
player_t ddPlayers[DDMAXPLAYERS];
int consolePlayer;
int displayPlayer;

typedef QMap<int, PlayerImpulse *> Impulses; // ID lookup.
static Impulses impulses;

typedef QMap<String, PlayerImpulse *> ImpulseNameMap; // Name lookup
static ImpulseNameMap impulsesByName;

typedef QMap<int, ImpulseAccumulator *> ImpulseAccumulators; // ImpulseId lookup.
static ImpulseAccumulators accumulators[DDMAXPLAYERS];

static PlayerImpulse *addImpulse(int id, impulsetype_t type, String name, String bindContextName)
{
    auto *imp = new PlayerImpulse;

    imp->id   = id;
    imp->type = type;
    imp->name = name;
    imp->bindContextName = bindContextName;

    impulses.insert(imp->id, imp);
    impulsesByName.insert(imp->name.toLower(), imp);

    // Generate impulse accumulators for each player.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        ImpulseAccumulator::AccumulatorType accumType = (type == IT_BINARY? ImpulseAccumulator::Binary : ImpulseAccumulator::Analog);
        auto *accum = new ImpulseAccumulator(imp->id, accumType, type != IT_ANALOG);
        accum->setPlayerNum(i);
        accumulators[i].insert(accum->impulseId(), accum);
    }

    return imp;
}

static ImpulseAccumulator *accumulator(int impulseId, int playerNum)
{
    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return nullptr;

    if(!accumulators[playerNum].contains(impulseId))
        return nullptr;

    return accumulators[playerNum][impulseId];
}

int P_LocalToConsole(int localPlayer)
{
    int count = 0;
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        int console      = (i + consolePlayer) % DDMAXPLAYERS;
        player_t *plr    = &ddPlayers[console];
        ddplayer_t *ddpl = &plr->shared;

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
    player_t *plr = &ddPlayers[playerNum];

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
    {
        // Invalid.
        return -1;
    }
    if(playerNum == consolePlayer)
    {
        return 0;
    }

    if(!(plr->shared.flags & DDPF_LOCAL))
        return -1; // Not local at all.

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        int console = (i + consolePlayer) % DDMAXPLAYERS;
        player_t *plr = &ddPlayers[console];

        if(console == playerNum)
        {
            return count;
        }

        if(plr->shared.flags & DDPF_LOCAL)
            count++;
    }
    return -1;
}

int P_GetDDPlayerIdx(ddplayer_t *ddpl)
{
    if(ddpl)
    for(uint i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(&ddPlayers[i].shared == ddpl)
            return i;
    }

    return -1;
}

bool P_IsInVoid(player_t *player)
{
    if(!player) return false;
    ddplayer_t *ddpl = &player->shared;

    // Cameras are allowed to move completely freely (so check z height
    // above/below ceiling/floor).
    if(ddpl->flags & DDPF_CAMERA)
    {
        if(ddpl->inVoid || !ddpl->mo)
            return true;
        mobj_t *mo = ddpl->mo;

        if(!Mobj_HasSubspace(*mo))
            return true;
        SectorCluster &cluster = Mobj_Cluster(*mo);

#ifdef __CLIENT__
        if(cluster.visCeiling().surface().hasSkyMaskedMaterial())
        {
            coord_t const skyCeil = cluster.sector().map().skyFixCeiling();
            if(skyCeil < DDMAXFLOAT && mo->origin[VZ] > skyCeil - 4)
                return true;
        }
        else if(mo->origin[VZ] > cluster.visCeiling().heightSmoothed() - 4)
#else
        if(mo->origin[VZ] > cluster.ceiling().height() - 4)
#endif
        {
            return true;
        }

#ifdef __CLIENT__
        if(cluster.visFloor().surface().hasSkyMaskedMaterial())
        {
            coord_t const skyFloor = cluster.sector().map().skyFixFloor();
            if(skyFloor > DDMINFLOAT && mo->origin[VZ] < skyFloor + 4)
                return true;
        }
        else if(mo->origin[VZ] < cluster.visFloor().heightSmoothed() + 4)
#else
        if(mo->origin[VZ] < cluster.floor().height() + 4)
#endif
        {
            return true;
        }
    }

    return false;
}

short P_LookDirToShort(float lookDir)
{
    int dir = int( lookDir/110.f * DDMAXSHORT );

    if(dir < DDMINSHORT) return DDMINSHORT;
    if(dir > DDMAXSHORT) return DDMAXSHORT;
    return (short) dir;
}

float P_ShortToLookDir(short s)
{
    return s / float( DDMAXSHORT ) * 110.f;
}

void P_ClearPlayerImpulses()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        qDeleteAll(accumulators[i]);
        accumulators[i].clear();
    }
    qDeleteAll(impulses);
    impulses.clear();
    impulsesByName.clear();
}

PlayerImpulse *P_PlayerImpulsePtr(int id)
{
    auto found = impulses.find(id);
    if(found != impulses.end()) return *found;
    return nullptr;
}

PlayerImpulse *P_PlayerImpulseByName(String const &name)
{
    if(!name.isEmpty())
    {
        auto found = impulsesByName.find(name.toLower());
        if(found != impulsesByName.end()) return *found;
    }
    return nullptr;
}

D_CMD(ListImpulses)
{
    DENG2_UNUSED3(argv, argc, src);

    // Group the defined impulses by binding context.
    typedef QList<PlayerImpulse *> ImpulseList;
    QMap<String, ImpulseList> contextGroups;
    for(PlayerImpulse *imp : impulsesByName)
    {
        if(!contextGroups.contains(imp->bindContextName))
        {
            contextGroups.insert(imp->bindContextName, ImpulseList());
        }
        contextGroups[imp->bindContextName].append(imp);
    }

    LOG_MSG(_E(b) "Player impulses");
    LOG_MSG("There are " _E(b) "%i" _E(.) " impulses, in " _E(b) "%i" _E(.) " contexts")
            << impulses.count() << contextGroups.count();

    for(auto const &group : contextGroups)
    {
        if(group.isEmpty()) continue;

        LOG_MSG(_E(D)_E(b) "%s" _E(.) " context: " _E(l) "(%i)")
                << group.first()->bindContextName
                << group.count();

        for(PlayerImpulse const *imp : group)
        {
            LOG_MSG("  [%4i] " _E(>) _E(b) "%s " _E(.) _E(2) "%s%s")
                    << imp->id << imp->name
                    << (imp->type == IT_BINARY? "binary" : "analog")
                    << (IMPULSETYPE_IS_TRIGGERABLE(imp->type)? ", triggerable" : "");
        }
    }
    return true;
}

D_CMD(Impulse)
{
    DENG2_UNUSED(src);

    if(argc < 2 || argc > 3)
    {
        LOG_SCR_NOTE("Usage:\n  %s (impulse-name)\n  %s (impulse-name) (player-ordinal)")
                << argv[0] << argv[0];
        return true;
    }

    if(PlayerImpulse *imp = P_PlayerImpulseByName(argv[1]))
    {
        int const playerNum = (argc == 3? P_LocalToConsole(String(argv[2]).toInt()) : consolePlayer);
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
    DENG2_UNUSED3(argv, argc, src);

    ClientApp::inputSystem().forAllDevices([] (InputDevice &device)
    {
        device.reset();
        return LoopContinue;
    });

    // For all players.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(ImpulseAccumulator *accum : accumulators[i])
    {
        accum->clearAll();
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

DENG_EXTERN_C ddplayer_t *DD_GetPlayer(int number)
{
    return &ddPlayers[number].shared;
}

// net_main.c
DENG_EXTERN_C char const *Net_GetPlayerName(int player);
DENG_EXTERN_C ident_t Net_GetPlayerID(int player);
DENG_EXTERN_C Smoother *Net_PlayerSmoother(int player);

DENG_EXTERN_C void P_NewPlayerControl(int id, impulsetype_t type, char const *name,
    char const *bindContextName)
{
    DENG2_ASSERT(name && bindContextName);
    LOG_AS("P_NewPlayerControl");

    // Ensure the given id is unique.
    if(PlayerImpulse const *existing = P_PlayerImpulsePtr(id))
    {
        LOG_INPUT_WARNING("Id: %i is already in use by impulse '%s' - Won't replace")
                << id << existing->name;
        return;
    }
    // Ensure the given name is unique.
    if(PlayerImpulse const *existing = P_PlayerImpulseByName(name))
    {
        LOG_INPUT_WARNING("Name: '%s' is already in use by impulse Id: %i - Won't replace")
                << name << existing->id;
        return;
    }

    addImpulse(id, type, name, bindContextName);
}

DENG_EXTERN_C int P_IsControlBound(int playerNum, int impulseId)
{
#ifdef __CLIENT__
    LOG_AS("P_IsControlBound");

    // Impulse bindings are associated with local player numbers rather than
    // the player console number - translate.
    int const localPlayer = P_ConsoleToLocal(playerNum);
    if(localPlayer < 0) return false;

    if(PlayerImpulse const *imp = P_PlayerImpulsePtr(impulseId))
    {
        InputSystem &isys = ClientApp::inputSystem();

        BindContext *bindContext = isys.contextPtr(imp->bindContextName);
        if(!bindContext)
        {
            LOGDEV_INPUT_WARNING("Unknown binding context '%s'") << imp->bindContextName;
            return false;
        }

        int found = bindContext->forAllImpulseBindings(localPlayer, [&isys, &impulseId] (Record &rec)
        {
            ImpulseBinding bind(rec);
            // Wrong impulse?
            if(bind.geti("impulseId") != impulseId) return LoopContinue;

            if(InputDevice const *device = isys.devicePtr(bind.geti("deviceId")))
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
    DENG2_UNUSED2(playerNum, impulseId);
    return 0;
#endif
}

DENG_EXTERN_C void P_GetControlState(int playerNum, int impulseId, float *pos,
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
    DENG2_UNUSED4(playerNum, impulseId, pos, relativeOffset);
#endif
}

DENG_EXTERN_C int P_GetImpulseControlState(int playerNum, int impulseId)
{
    LOG_AS("P_GetImpulseControlState");

    ImpulseAccumulator *accum = accumulator(impulseId, playerNum);
    if(!accum) return 0;

    // Ensure this is really a binary accumulator.
    if(accum->type() != ImpulseAccumulator::Binary)
    {
        LOG_INPUT_WARNING("ImpulseAccumulator '%s' is not binary") << impulses[impulseId]->name;
        return 0;
    }

    return accum->takeBinary();
}

DENG_EXTERN_C void P_Impulse(int playerNum, int impulseId)
{
    LOG_AS("P_Impulse");

    ImpulseAccumulator *accum = accumulator(impulseId, playerNum);
    if(!accum) return;

    // Ensure this is really a binary accumulator.
    if(accum->type() != ImpulseAccumulator::Binary)
    {
        LOG_INPUT_WARNING("ImpulseAccumulator '%s' is not binary") << impulses[impulseId]->name;
        return;
    }

    accum->receiveBinary();
}

DENG_DECLARE_API(Player) =
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
