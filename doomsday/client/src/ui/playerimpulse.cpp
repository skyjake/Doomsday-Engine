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
static int pimpDoubleClickThreshold = 300; ///< Milliseconds, cvar

void PlayerImpulse::maintainDoubleClicks(int playerNum, float pos)
{
    LOG_AS("PlayerImpulse");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return;

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

    // We have an activation!
    uint const nowTime = Timer_RealMilliseconds();

    if(newState == db.previousClickState &&
       nowTime - db.previousClickTime < uint( de::clamp(0, pimpDoubleClickThreshold) ))
    {
        db.triggered = true;

        // Compose the name of the symbolic event.
        String symbolicName;
        switch(newState)
        {
        case DoubleClick::Positive: symbolicName += "control-doubleclick-positive-"; break;
        case DoubleClick::Negative: symbolicName += "control-doubleclick-negative-"; break;

        default: break;
        }
        symbolicName += name;

        LOG_INPUT_XVERBOSE("Triggered plr %i, imp %i, state %i - threshold %i (%s)")
                << playerNum << id << newState << (nowTime - db.previousClickTime)
                << symbolicName;

        ddevent_t ev; de::zap(ev);
        ev.device = uint(-1);
        ev.type   = E_SYMBOLIC;
        ev.symbolic.id   = playerNum;
        ev.symbolic.name = symbolicName.toUtf8().constData();

        ClientApp::inputSystem().postEvent(&ev);
    }

    db.previousClickTime  = nowTime;
    db.previousClickState = newState;
    db.lastState          = newState;
}

int PlayerImpulse::takeDoubleClick(int playerNum)
{
    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return 0;

    bool triggered = false;
    DoubleClick &db = doubleClicks[playerNum];
    if(db.triggered)
    {
        triggered = true;
        db.triggered = false;
    }

    return int(triggered);
}

void PlayerImpulse::consoleRegister() // static
{
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
    impulses.insert(imp->id, imp);
    impulsesByName.insert(imp->name.toLower(), imp);
}

void P_ImpulseShutdown()
{
    qDeleteAll(impulses);
    impulses.clear();
    impulsesByName.clear();
}

PlayerImpulse *P_ImpulseById(int id)
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

    int const playerNum = (argc == 3? P_LocalToConsole(String(argv[2]).toInt()) : consolePlayer);

    if(PlayerImpulse *imp = P_ImpulseByName(argv[1]))
    {
        P_Impulse(playerNum, imp->id);
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
    for(int p = 0; p < DDMAXPLAYERS; ++p)
    {
        if(imp->type == IT_NUMERIC)
        {
            P_GetControlState(p, imp->id, nullptr, nullptr);
        }
        else if(imp->type == IT_BOOLEAN)
        {
            P_GetImpulseControlState(p, imp->id);
        }

        // Also clear the double click state.
        imp->takeDoubleClick(p);
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

#undef P_NewPlayerControl
DENG_EXTERN_C void P_NewPlayerControl(int id, impulsetype_t type, char const *name,
    char const *bindContext)
{
    DENG2_ASSERT(name && bindContext);
    LOG_AS("P_NewPlayerControl");

    // Ensure the given id is unique.
    if(PlayerImpulse const *existing = P_ImpulseById(id))
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

    addImpulse(new PlayerImpulse(id, type, name, bindContext));
}

#undef P_GetControlState
DENG_EXTERN_C void P_GetControlState(int playerNum, int impulseId, float *pos,
    float *relativeOffset)
{
#ifdef __CLIENT__
    InputSystem &isys = ClientApp::inputSystem();

    // Ignore NULLs.
    float tmp;
    if(!pos) pos = &tmp;
    if(!relativeOffset) relativeOffset = &tmp;

    *pos = 0;
    *relativeOffset = 0;

    // ImpulseBindings are associated with local player numbers rather than
    // the player console number - translate.
    int localPlayer = P_ConsoleToLocal(playerNum);
    if(localPlayer < 0 || localPlayer >= DDMAXPLAYERS)
        return;

    // Check that this is really a numeric control.
    PlayerImpulse *imp = P_ImpulseById(impulseId);
    DENG2_ASSERT(imp);
    DENG2_ASSERT(imp->type == IT_NUMERIC || imp->type == IT_NUMERIC_TRIGGERED);

    BindContext *context = isys.contextPtr(imp->bindContextName);
    DENG2_ASSERT(context); // must surely exist by now?
    if(!context) return;

    B_EvaluateImpulseBindings(context, localPlayer, impulseId, pos, relativeOffset,
                              imp->isTriggerable());

    // Mark for double-clicks.
    imp->maintainDoubleClicks(playerNum, *pos);

#else
    DENG2_UNUSED4(playerNum, impulseId, pos, relativeOffset);
#endif
}

#undef P_IsControlBound
DENG_EXTERN_C int P_IsControlBound(int playerNum, int impulseId)
{
#ifdef __CLIENT__
    InputSystem &isys = ClientApp::inputSystem();

    // ImpulseBindings are associated with local player numbers rather than
    // the player console number - translate.
    int const localPlayer = P_ConsoleToLocal(playerNum);
    if(localPlayer < 0 || localPlayer >= DDMAXPLAYERS)
        return false;

    // Ensure this is really a numeric impulse.
    PlayerImpulse const *imp = P_ImpulseById(impulseId);
    DENG2_ASSERT(imp);
    DENG2_ASSERT(imp->type == IT_NUMERIC || imp->type == IT_NUMERIC_TRIGGERED);

    // There must be bindings to active input devices.
    BindContext *context = isys.contextPtr(imp->bindContextName);
    DENG2_ASSERT(context); // must surely exist by now?
    if(!context) return false;

    int found = context->forAllImpulseBindings(localPlayer, [&isys, &impulseId] (ImpulseBinding &bind)
    {
        // Wrong impulse?
        if(bind.impulseId != impulseId) return LoopContinue;

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

#else
    DENG2_UNUSED2(playerNum, impulseId);
    return 0;
#endif
}

#undef P_GetImpulseControlState
DENG_EXTERN_C int P_GetImpulseControlState(int playerNum, int impulseId)
{
    LOG_AS("P_GetImpulseControlState");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return 0;

    PlayerImpulse *imp = P_ImpulseById(impulseId);
    if(!imp) return 0;

    // Ensure this is really a boolean impulse.
    if(imp->type != IT_BOOLEAN)
    {
        LOG_INPUT_WARNING("Impulse '%s' is not boolean") << imp->name;
        return 0;
    }

    short *counter = &imp->booleanCounts[playerNum];
    int count = *counter;
    *counter = 0;
    return count;
}

#undef P_Impulse
DENG_EXTERN_C void P_Impulse(int playerNum, int impulseId)
{
    LOG_AS("P_Impulse");

    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return;

    PlayerImpulse *imp = P_ImpulseById(impulseId);
    if(!imp) return;

    // Ensure this is really a boolean impulse.
    if(imp->type != IT_BOOLEAN)
    {
        LOG_INPUT_WARNING("Impulse '%s' is not boolean") << imp->name;
        return;
    }

    imp->booleanCounts[playerNum]++;

#ifdef __CLIENT__
    // Mark for double clicks.
    imp->maintainDoubleClicks(playerNum, 1);
    imp->maintainDoubleClicks(playerNum, 0);
#endif
}
