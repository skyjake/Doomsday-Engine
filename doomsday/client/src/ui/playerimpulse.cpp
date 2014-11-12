/** @file playerimpulse.cpp  Player interaction impulses.
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

#include "ui/playerimpulse.h"

#include <QMap>
#include <QtAlgorithms>
#include <de/timer.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#ifdef __CLIENT__
#  include "clientapp.h"
#endif

#include "api_player.h"
#include "world/p_players.h"

#ifdef __CLIENT__
#  include "BindContext"
#  include "ui/b_util.h"
#  include "ui/inputdevice.h"
#endif

using namespace de;

#ifdef __CLIENT__
static inline InputSystem &inputSys()
{
    return ClientApp::inputSystem();
}
#endif

#ifdef __CLIENT__
static int pimpDoubleClickThreshold = 300; ///< Milliseconds, cvar
#endif

DENG2_PIMPL_NOREF(ImpulseAccumulator)
{
    int impulseId = 0;
    AccumulatorType type = Analog;
    bool expireBeforeSharpTick = false;

    int playerNum = 0;

    short binaryAccum = 0;

#ifdef __CLIENT__
    /**
     * Double-"clicks" actually mean double activations that occur within the
     * double-click threshold. This is to allow double-clicks also from the
     * analog impulses.
     */
    struct DoubleClick
    {
        enum State
        {
            None,
            Positive,
            Negative
        };

        bool triggered = false;           //< True if double-click has been detected.
        uint previousClickTime = 0;       //< Previous time an activation occurred.
        State lastState = None;           //< State at the previous time the check was made.
        State previousClickState = None;  /** Previous click state. When duplicated, triggers
                                              the double click. */
    } db;

    /**
     * Track the double-click state of the impulse and generate a bindable
     * symbolic event if the trigger conditions are met.
     *
     * @param pos  State of the impulse.
     */
    void maintainDoubleClick(float pos)
    {
        if(pimpDoubleClickThreshold <= 0)
        {
            // Let's not waste time here.
            db.triggered          = false;
            db.previousClickTime  = 0;
            db.previousClickState = DoubleClick::None;
            return;
        }

        DoubleClick::State newState = DoubleClick::None;
        if(pos > .5)
        {
            newState = DoubleClick::Positive;
        }
        else if(pos < -.5)
        {
            newState = DoubleClick::Negative;
        }
        else
        {
            db.lastState = newState; // Release.
            return;
        }

        // But has it actually changed?
        if(newState == db.lastState)
        {
            return;
        }

        // We have an potential activation!
        uint const threshold = uint( de::max(0, pimpDoubleClickThreshold) );
        uint const nowTime   = Timer_RealMilliseconds();

        if(newState == db.previousClickState && nowTime - db.previousClickTime < threshold)
        {
            db.triggered = true;

            PlayerImpulse *impulse = P_ImpulsePtr(impulseId);
            DENG2_ASSERT(impulse);

            // Compose the name of the symbolic event.
            String symbolicName;
            switch(newState)
            {
            case DoubleClick::Positive: symbolicName += "control-doubleclick-positive-"; break;
            case DoubleClick::Negative: symbolicName += "control-doubleclick-negative-"; break;

            default: break;
            }
            symbolicName += impulse->name;

            int const localPlayer = P_ConsoleToLocal(playerNum);
            DENG2_ASSERT(localPlayer >= 0);
            LOG_INPUT_XVERBOSE("Triggered " _E(b) "'%s'" _E(.) " for player%i state: %i threshold: %i\n  %s")
                    << impulse->name << (localPlayer + 1) << newState << (nowTime - db.previousClickTime)
                    << symbolicName;

            Block symbolicNameUtf8 = symbolicName.toUtf8();
            ddevent_t ev; de::zap(ev);
            ev.device = uint(-1);
            ev.type   = E_SYMBOLIC;
            ev.symbolic.id   = playerNum;
            ev.symbolic.name = symbolicNameUtf8.constData();

            inputSys().postEvent(&ev); // makes a copy.
        }

        db.previousClickTime  = nowTime;
        db.previousClickState = newState;
        db.lastState          = newState;
    }

    void clearDoubleClick()
    {
        db.triggered = false;
    }
#endif
};

ImpulseAccumulator::ImpulseAccumulator(int impulseId, AccumulatorType type, bool expireBeforeSharpTick)
    : d(new Instance)
{
    d->impulseId             = impulseId;
    d->type                  = type;
    d->expireBeforeSharpTick = expireBeforeSharpTick;
}

void ImpulseAccumulator::setPlayerNum(int newPlayerNum)
{
    d->playerNum = newPlayerNum;
}

int ImpulseAccumulator::impulseId() const
{
    return d->impulseId;
}

ImpulseAccumulator::AccumulatorType ImpulseAccumulator::type() const
{
    return d->type;
}

bool ImpulseAccumulator::expireBeforeSharpTick() const
{
    return d->expireBeforeSharpTick;
}

void ImpulseAccumulator::receiveBinary()
{
    // Ensure this is really a binary accumulator.
    DENG2_ASSERT(d->type == Binary);
    LOG_AS("ImpulseAccumulator");

    d->binaryAccum++;

#ifdef __CLIENT__
    // Mark for double click.
    d->maintainDoubleClick(1);
    d->maintainDoubleClick(0);
#endif
}

int ImpulseAccumulator::takeBinary()
{
    // Ensure this is really a binary accumulator.
    DENG2_ASSERT(d->type == Binary);
    LOG_AS("ImpulseAccumulator");
    short *counter = &d->binaryAccum;
    int count = *counter;
    *counter = 0;
    return count;
}

#ifdef __CLIENT__

void ImpulseAccumulator::takeAnalog(float *pos, float *relOffset)
{
    // Ensure this is really an analog accumulator.
    DENG2_ASSERT(d->type == Analog);
    LOG_AS("ImpulseAccumulator");

    if(pos) *pos = 0;
    if(relOffset) *relOffset = 0;

    PlayerImpulse *impulse = P_ImpulsePtr(d->impulseId);
    DENG2_ASSERT(impulse);

    if(BindContext *bindContext = inputSys().contextPtr(impulse->bindContextName))
    {
        // Impulse bindings are associated with local player numbers rather than
        // the player console number - translate.
        float position, relative;
        B_EvaluateImpulseBindings(bindContext, P_ConsoleToLocal(d->playerNum), d->impulseId,
                                  &position, &relative, d->expireBeforeSharpTick);

        // Mark for double-clicks.
        d->maintainDoubleClick(position);

        if(pos) *pos = position;
        if(relOffset) *relOffset = relative;
    }
}

void ImpulseAccumulator::clearAll()
{
    LOG_AS("ImpulseAccumulator");
    switch(d->type)
    {
    case Analog:
        if(!d->expireBeforeSharpTick)
        {
            takeAnalog();
        }
        break;

    case Binary:
        takeBinary();
        break;

    default: DENG2_ASSERT(!"ImpulseAccumulator::clearAll: Unknown type");
    }

    // Also clear the double click state.
    d->clearDoubleClick();
}

void ImpulseAccumulator::consoleRegister() // static
{
    LOG_AS("ImpulseAccumulator");
    C_VAR_INT("input-doubleclick-threshold", &pimpDoubleClickThreshold, 0, 0, 2000);
}

// -------------------------------------------------------------------------------

bool PlayerImpulse::haveBindingsFor(int localPlayer) const
{
    LOG_AS("Impulse");

    if(localPlayer < 0 || localPlayer >= DDMAXPLAYERS)
        return false;

    InputSystem &isys        = ClientApp::inputSystem();
    BindContext *bindContext = isys.contextPtr(bindContextName);
    if(!bindContext)
    {
        LOG_INPUT_WARNING("Unknown binding context '%s'") << bindContextName;
        return false;
    }

    int found = bindContext->forAllImpulseBindings(localPlayer, [this, &isys] (Record &rec)
    {
        ImpulseBinding bind(rec);
        // Wrong impulse?
        if(bind.geti("impulseId") != id) return LoopContinue;

        if(InputDevice const *device = isys.devicePtr(bind.geti("deviceId")))
        {
            if(device->isActive())
            {
                return LoopAbort; // found a binding.
            }
        }
        return LoopContinue;
    });

    return found;
}
#endif // __CLIENT__

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

void P_ImpulseShutdown()
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

PlayerImpulse *P_ImpulsePtr(int id)
{
    auto found = impulses.find(id);
    if(found != impulses.end()) return *found;
    return nullptr;
}

PlayerImpulse *P_ImpulseByName(String const &name)
{
    if(!name.isEmpty())
    {
        auto found = impulsesByName.find(name.toLower());
        if(found != impulsesByName.end()) return *found;
    }
    return nullptr;
}

/// @todo: Group impulses by binding context.
D_CMD(ListImpulses)
{
    DENG2_UNUSED3(argv, argc, src);
    LOG_MSG(_E(b) "%i player impulses defined:") << impulses.count();

    for(PlayerImpulse const *imp : impulsesByName)
    {
        LOG_MSG("  [%4i] " _E(>) _E(b) "%s " _E(.) "(%s) " _E(2) "%s%s")
                << imp->id << imp->name << imp->bindContextName
                << (imp->type == IT_BINARY? "binary" : "analog")
                << (IMPULSETYPE_IS_TRIGGERABLE(imp->type)? ", triggerable" : "");
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

    if(PlayerImpulse *imp = P_ImpulseByName(argv[1]))
    {
        int const playerNum = (argc == 3? String(argv[2]).toInt() : 0);
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

void P_ImpulseConsoleRegister()
{
    C_CMD("listcontrols",   "",         ListImpulses);
    C_CMD("impulse",        nullptr,    Impulse);

#ifdef __CLIENT__
    C_CMD("resetctlaccum", "", ClearImpulseAccumulation);

    ImpulseAccumulator::consoleRegister();
#endif
}

DENG_EXTERN_C void P_NewPlayerControl(int id, impulsetype_t type, char const *name,
    char const *bindContextName)
{
    DENG2_ASSERT(name && bindContextName);
    LOG_AS("P_NewPlayerControl");

    // Ensure the given id is unique.
    if(PlayerImpulse const *existing = P_ImpulsePtr(id))
    {
        LOG_INPUT_WARNING("Id: %i is already in use by impulse '%s' - Won't replace")
                << id << existing->name;
        return;
    }
    // Ensure the given name is unique.
    if(PlayerImpulse const *existing = P_ImpulseByName(name))
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

    if(PlayerImpulse const *imp = P_ImpulsePtr(impulseId))
    {
        return imp->haveBindingsFor(localPlayer);
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
