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
#include <de/memory.h>
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

DENG2_PIMPL_NOREF(PlayerImpulse)
{
    int id;
    String name;
    String bindContextName;

    short booleanCounts[DDMAXPLAYERS];

#ifdef __CLIENT__
    /**
     * Double-"clicks" actually mean double activations that occur within the double-click
     * threshold. This is to allow double-clicks also from the numeric impulses.
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
    } doubleClicks[DDMAXPLAYERS];
#endif

    Instance() { de::zap(booleanCounts); }

#ifdef __CLIENT__
    /**
     * Track the double-click state of the impulse and generate a symbolic
     * input event if the trigger conditions are met.
     *
     * @param playerNum  Console/player number.
     * @param pos        State of the impulse.
     */
    void maintainDoubleClicks(int playerNum, float pos)
    {
        DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);

        DoubleClick &db = doubleClicks[playerNum];
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

            // Compose the name of the symbolic event.
            String symbolicName = "sym-";
            switch(newState)
            {
            case DoubleClick::Positive: symbolicName += "control-doubleclick-positive-"; break;
            case DoubleClick::Negative: symbolicName += "control-doubleclick-negative-"; break;

            default: break;
            }
            symbolicName += name;

            int const localPlayer = P_ConsoleToLocal(playerNum);
            DENG2_ASSERT(localPlayer >= 0);
            LOG_INPUT_XVERBOSE("Triggered " _E(b) "'%s'" _E(.) " for player%i state: %i threshold: %i\n  %s")
                    << name << (localPlayer + 1) << newState << (nowTime - db.previousClickTime)
                    << symbolicName;

            ddevent_t ev; de::zap(ev);
            ev.device = uint(-1);
            ev.type   = E_SYMBOLIC;
            ev.symbolic.id   = playerNum;
            ev.symbolic.name = symbolicName.toUtf8().constData();

            inputSys().postEvent(&ev);
        }

        db.previousClickTime  = nowTime;
        db.previousClickState = newState;
        db.lastState          = newState;
    }
#endif
};

PlayerImpulse::PlayerImpulse(int id, impulsetype_t type, String const &name, String bindContextName)
    : type(type), d(new Instance)
{
    d->id   = id;
    d->name = name;
    d->bindContextName = bindContextName;
}

bool PlayerImpulse::isTriggerable() const
{
    return (type == IT_NUMERIC_TRIGGERED || type == IT_BOOLEAN);
}

int PlayerImpulse::id() const
{
    return d->id;
}

String PlayerImpulse::name() const
{
    return d->name;
}

String PlayerImpulse::bindContextName() const
{
    return d->bindContextName;
}

#ifdef __CLIENT__
bool PlayerImpulse::haveBindingsFor(int playerNum) const
{
    // Ensure this is really a numeric impulse.
    DENG2_ASSERT(type == IT_NUMERIC || type == IT_NUMERIC_TRIGGERED);
    LOG_AS("PlayerImpulse");

    InputSystem &isys = ClientApp::inputSystem();

    // ImpulseBindings are associated with local player numbers rather than
    // the player console number - translate.
    int const localPlayer = P_ConsoleToLocal(playerNum);
    if(localPlayer < 0) return false;

    BindContext *bindContext = isys.contextPtr(d->bindContextName);
    if(!bindContext) return false;

    int found = bindContext->forAllImpulseBindings(localPlayer, [this, &isys] (ImpulseBinding &bind)
    {
        // Wrong impulse?
        if(bind.impulseId != d->id) return LoopContinue;

        if(InputDevice const *device = isys.devicePtr(bind.deviceId))
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

void PlayerImpulse::triggerBoolean(int playerNum)
{
    // Ensure this is really a boolean impulse.
    DENG2_ASSERT(type == IT_BOOLEAN);
    LOG_AS("PlayerImpulse");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return;

    d->booleanCounts[playerNum]++;

#ifdef __CLIENT__
    // Mark for double clicks.
    d->maintainDoubleClicks(playerNum, 1);
    d->maintainDoubleClicks(playerNum, 0);
#endif
}

int PlayerImpulse::takeBoolean(int playerNum)
{
    // Ensure this is really a boolean impulse.
    DENG2_ASSERT(type == IT_BOOLEAN);
    LOG_AS("PlayerImpulse");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return 0;

    short *counter = &d->booleanCounts[playerNum];
    int count = *counter;
    *counter = 0;
    return count;
}

#ifdef __CLIENT__

void PlayerImpulse::takeNumeric(int playerNum, float *pos, float *relOffset)
{
    // Ensure this is really a numeric impulse.
    DENG2_ASSERT(type == IT_NUMERIC || type == IT_NUMERIC_TRIGGERED);
    LOG_AS("PlayerImpulse");

    if(pos) *pos = 0;
    if(relOffset) *relOffset = 0;

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return;

    if(BindContext *bindContext = inputSys().contextPtr(d->bindContextName))
    {
        // ImpulseBindings are associated with local player numbers rather than
        // the player console number - translate.
        float position, relative;
        B_EvaluateImpulseBindings(bindContext, P_ConsoleToLocal(playerNum), d->id,
                                  &position, &relative, isTriggerable());

        // Mark for double-clicks.
        d->maintainDoubleClicks(playerNum, position);

        if(pos) *pos = position;
        if(relOffset) *relOffset = relative;
    }
}

int PlayerImpulse::takeDoubleClick(int playerNum)
{
    LOG_AS("PlayerImpulse");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return 0;

    bool triggered = false;
    Instance::DoubleClick &db = d->doubleClicks[playerNum];
    if(db.triggered)
    {
        triggered = true;
        db.triggered = false;
    }

    return int(triggered);
}

void PlayerImpulse::clearAccumulation(int playerNum)
{
    LOG_AS("PlayerImpulse");

    bool const haveNamedPlayer = (playerNum < 0 || playerNum >= DDMAXPLAYERS);
    if(haveNamedPlayer)
    {
        switch(type)
        {
        case IT_NUMERIC: takeNumeric(playerNum); break;
        case IT_BOOLEAN: takeBoolean(playerNum); break;

        case IT_NUMERIC_TRIGGERED: break;

        default: DENG2_ASSERT(!"PlayerImpulse::clearAccumulation: Unknown type");
        }
        // Also clear the double click state.
        takeDoubleClick(playerNum);
        return;
    }

    // Clear accumulation for all local players.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        switch(type)
        {
        case IT_NUMERIC: takeNumeric(i); break;
        case IT_BOOLEAN: takeBoolean(i); break;

        case IT_NUMERIC_TRIGGERED: break;

        default: DENG2_ASSERT(!"PlayerImpulse::clearAccumulation: Unknown type");
        }
        // Also clear the double click state.
        takeDoubleClick(i);
    }
}

void PlayerImpulse::consoleRegister() // static
{
    LOG_AS("PlayerImpulse");
    C_VAR_INT("input-doubleclick-threshold", &pimpDoubleClickThreshold, 0, 0, 2000);
}
#endif

typedef QMap<int, PlayerImpulse *> Impulses; // ID lookup
static Impulses impulses;
typedef QMap<String, PlayerImpulse *> ImpulsesByName; // Name lookup
static ImpulsesByName impulsesByName;

static void addImpulse(PlayerImpulse *imp)
{
    if(!imp) return;
    impulses.insert(imp->id(), imp);
    impulsesByName.insert(imp->name().toLower(), imp);
}

void P_ImpulseShutdown()
{
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
                << imp->id() << imp->name() << imp->bindContextName()
                << (imp->type == IT_BOOLEAN? "boolean" : "numeric")
                << (imp->isTriggerable()? ", triggerable" : "");
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

    int const playerNum = (argc == 3? String(argv[2]).toInt() : 0);
    if(PlayerImpulse *imp = P_ImpulseByName(argv[1]))
    {
        imp->triggerBoolean(playerNum);
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

    for(PlayerImpulse *imp : impulses)
    {
        imp->clearAccumulation(); // for all local players.
    }

    return true;
}
#endif

void P_ConsoleRegister()
{
    C_CMD("listcontrols",   "",         ListImpulses);
    C_CMD("impulse",        nullptr,    Impulse);

#ifdef __CLIENT__
    C_CMD("resetctlaccum", "", ClearImpulseAccumulation);

    PlayerImpulse::consoleRegister();
#endif
}

DENG_EXTERN_C void P_NewPlayerControl(int id, impulsetype_t type, char const *name,
    char const *bindContext)
{
    DENG2_ASSERT(name && bindContext);
    LOG_AS("P_NewPlayerControl");

    // Ensure the given id is unique.
    if(PlayerImpulse const *existing = P_ImpulsePtr(id))
    {
        LOG_INPUT_WARNING("Id: %i is already in use by impulse '%s' - Won't replace")
                << id << existing->name();
        return;
    }
    // Ensure the given name is unique.
    if(PlayerImpulse const *existing = P_ImpulseByName(name))
    {
        LOG_INPUT_WARNING("Name: '%s' is already in use by impulse Id: %i - Won't replace")
                << name << existing->id();
        return;
    }

    addImpulse(new PlayerImpulse(id, type, name, bindContext));
}

DENG_EXTERN_C int P_IsControlBound(int playerNum, int impulseId)
{
#ifdef __CLIENT__
    LOG_AS("P_IsControlBound");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return false;

    if(PlayerImpulse const *imp = P_ImpulsePtr(impulseId))
    {
        return imp->haveBindingsFor(playerNum);
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

    if(PlayerImpulse *imp = P_ImpulsePtr(impulseId))
    {
        imp->takeNumeric(playerNum, pos, relativeOffset);
        return;
    }

#else
    DENG2_UNUSED4(playerNum, impulseId, pos, relativeOffset);
#endif
}

DENG_EXTERN_C int P_GetImpulseControlState(int playerNum, int impulseId)
{
    LOG_AS("P_GetImpulseControlState");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return 0;

    PlayerImpulse *imp = P_ImpulsePtr(impulseId);
    if(!imp) return 0;

    // Ensure this is really a boolean impulse.
    if(imp->type != IT_BOOLEAN)
    {
        LOG_INPUT_WARNING("Impulse '%s' is not boolean") << imp->name();
        return 0;
    }

    return imp->takeBoolean(playerNum);
}

DENG_EXTERN_C void P_Impulse(int playerNum, int impulseId)
{
    LOG_AS("P_Impulse");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return;

    PlayerImpulse *imp = P_ImpulsePtr(impulseId);
    if(!imp) return;

    // Ensure this is really a boolean impulse.
    if(imp->type != IT_BOOLEAN)
    {
        LOG_INPUT_WARNING("Impulse '%s' is not boolean") << imp->name();
        return;
    }

    imp->triggerBoolean(playerNum);
}
