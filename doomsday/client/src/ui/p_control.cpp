/** @file p_control.cpp  Player interaction impulses.
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

#include "ui/p_control.h"

#include <QList>
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
#  include "ui/bindcontext.h"
#  include "ui/inputdevice.h"
#endif

using namespace de;

/*
// Number of triggered impulses buffered into each player's control state
// table.  The buffer is emptied when a ticcmd is built.
#define MAX_IMPULSES    8
#define MAX_DESCRIPTOR_LENGTH 20

#define SLOW_TURN_TIME  (6.0f / 35)
*/

/**
 * The control descriptors contain a mapping between symbolic control
 * names and the identifier numbers.
 */
/*
typedef struct controldesc_s {
    char    name[MAX_DESCRIPTOR_LENGTH + 1];
} controldesc_t;

typedef struct controlclass_s {
    uint    count;
    controldesc_t *desc;
} controlclass_t;
*/
/**
 * Each player has his own control state table.
 */
/*typedef struct controlstate_s {
    // The axes are updated whenever their values are needed,
    // i.e. during the call to P_BuildCommand.
    controlaxis_t *axes;

    // The toggles are modified via console commands.
    controltoggle_t *toggles;

    // The triggered impulses are stored into a ring buffer.
    uint    head, tail;
    impulse_t impulses[MAX_IMPULSES];
} controlstate_t;
*/

enum DoubleClickState
{
    DBCS_NONE,
    DBCS_POSITIVE,
    DBCS_NEGATIVE
};

/**
 * Double-"clicks" actually mean double activations that occur within the double-click
 * threshold. This is to allow double-clicks also from the numeric impulses.
 */
struct DoubleClick
{
    bool triggered;                       //< True if double-click has been detected.
    uint previousClickTime;               //< Previous time an activation occurred.
    DoubleClickState lastState;           //< State at the previous time the check was made.
    DoubleClickState previousClickState;  /** Previous click state. When duplicated, triggers
                                              the double click. */
};

struct ImpulseCounter
{
    int control;
    short impulseCounts[DDMAXPLAYERS];
    DoubleClick doubleClicks[DDMAXPLAYERS];
};

/// @todo: Group by bind-context-name; add an impulseID => PlayerImpulse LUT -ds
typedef QList<PlayerImpulse *> Impulses;
static Impulses impulses;

static ImpulseCounter **counters;

static int doubleClickThresholdMilliseconds = 300;

static PlayerImpulse *newImpulse()
{
    PlayerImpulse *imp = new PlayerImpulse;
    impulses.append(imp);

    int const ctrlCount = impulses.count();
    counters = (ImpulseCounter **) M_Realloc(counters, sizeof(*counters) * ctrlCount);
    counters[ctrlCount - 1] = nullptr;

    return imp;
}

void P_ImpulseShutdown()
{
    for(int i = 0; i < impulses.count(); ++i)
    {
        M_Free(counters[i]);
    }
    M_Free(counters); counters = nullptr;

    qDeleteAll(impulses);
    impulses.clear();
}

PlayerImpulse *P_ImpulseById(int id)
{
    for(PlayerImpulse const *imp : impulses)
    {
        if(imp->id == id)
        {
            return const_cast<PlayerImpulse *>(imp);
        }
    }
    return nullptr;
}

PlayerImpulse *P_ImpulseByName(String const &name)
{
    if(!name.isEmpty())
    {
        for(PlayerImpulse const *imp : impulses)
        {
            if(!imp->name.compareWithoutCase(name))
                return const_cast<PlayerImpulse *>(imp);
        }
    }
    return nullptr;
}

#ifdef __CLIENT__
/**
 * Updates the double-click state of an impulse and marks it as double-clicked
 * when the double-click condition is met.
 *
 * @param playerNum  Player/console number.
 * @param impulse    Index of the impulse.
 * @param pos        State of the impulse.
 */
void P_MaintainImpulseDoubleClicks(int playerNum, int impulse, float pos)
{
    if(!counters || !counters[impulse])
    {
        return;
    }

    DoubleClick *db = &counters[impulse]->doubleClicks[playerNum];

    if(doubleClickThresholdMilliseconds <= 0)
    {
        // Let's not waste time here.
        db->triggered = false;
        db->previousClickTime = 0;
        db->previousClickState = DBCS_NONE;
        return;
    }

    DoubleClickState newState = DBCS_NONE;
    if(pos > .5)
    {
        newState = DBCS_POSITIVE;
    }
    else if(pos < -.5)
    {
        newState = DBCS_NEGATIVE;
    }
    else
    {
        db->lastState = newState; // Release.
        return;
    }

    // But has it actually changed?
    if(newState == db->lastState)
    {
        return;
    }

    // We have an activation!
    uint nowTime = Timer_RealMilliseconds();

    if(newState == db->previousClickState &&
       nowTime - db->previousClickTime < uint( de::clamp(0, doubleClickThresholdMilliseconds) ))
    {
        db->triggered = true;

        // Compose the name of the symbolic event.
        String symbolicName;
        switch(newState)
        {
        case DBCS_POSITIVE: symbolicName += "control-doubleclick-positive-"; break;
        case DBCS_NEGATIVE: symbolicName += "control-doubleclick-negative-"; break;

        default: break;
        }
        symbolicName += impulses.at(impulse)->name;

        LOG_AS("P_MaintainImpulseDoubleClicks");
        LOG_INPUT_XVERBOSE("Triggered plr %i, imp %i, state %i - threshold %i (%s)")
                << playerNum << impulse << newState << nowTime - db->previousClickTime
                << symbolicName;

        ddevent_t ev; de::zap(ev);
        ev.device = uint(-1);
        ev.type   = E_SYMBOLIC;
        ev.symbolic.id   = playerNum;
        ev.symbolic.name = symbolicName.toUtf8().constData();

        ClientApp::inputSystem().postEvent(&ev);
    }

    db->previousClickTime  = nowTime;
    db->previousClickState = newState;
    db->lastState          = newState;
}

static int P_GetImpulseDoubleClick(int playerNum, int impulseId)
{
    if(playerNum < 0 || playerNum >= DDMAXPLAYERS)
        return 0;

    PlayerImpulse *imp = P_ImpulseById(impulseId);
    if(!imp) return 0;

    bool triggered = false;
    if(counters[impulses.indexOf(imp)])
    {
        DoubleClick *doubleClick = &counters[impulses.indexOf(imp)]->doubleClicks[playerNum];
        if(doubleClick->triggered)
        {
            triggered = true;
            doubleClick->triggered = false;
        }
    }

    return int(triggered);
}

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
        P_GetImpulseDoubleClick(p, imp->id);
    }

    return true;
}
#endif

/// @todo: Sort impulses by binding context.
D_CMD(ListImpulses)
{
    DENG2_UNUSED3(argv, argc, src);
    LOG_MSG("%i player impulses defined") << impulses.count();

    for(PlayerImpulse const *imp : impulses)
    {
        LOG_MSG("ID %i: " _E(>)_E(b) "%s " _E(.) "(%s) " _E(l) "%s%s")
                << imp->id << imp->name << imp->bindContextName
                << (imp->isTriggerable? "triggerable " : "")
                << (imp->type == IT_BOOLEAN? "boolean" : "numeric");
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

void P_ConsoleRegister()
{
    C_CMD("listcontrols",   "",         ListImpulses);
    C_CMD("impulse",        nullptr,    Impulse);
#ifdef __CLIENT__
    C_CMD("resetctlaccum",  "",         ClearImpulseAccumulation);
#endif

    C_VAR_INT("input-doubleclick-threshold", &doubleClickThresholdMilliseconds, 0, 0, 2000);
}

#undef P_NewPlayerControl
DENG_EXTERN_C void P_NewPlayerControl(int id, impulsetype_t type, char const *name,
    char const *bindContext)
{
    DENG2_ASSERT(name && bindContext);
    PlayerImpulse *imp = newImpulse();
    imp->id              = id;
    imp->type            = type;
    imp->name            = name;
    imp->isTriggerable   = (type == IT_NUMERIC_TRIGGERED || type == IT_BOOLEAN);
    imp->bindContextName = bindContext;
    // Also allocate the impulse and double-click counters.
    counters[impulses.indexOf(imp)] = (ImpulseCounter *) M_Calloc(sizeof(ImpulseCounter));
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
                              imp->isTriggerable);

    // Mark for double-clicks.
    P_MaintainImpulseDoubleClicks(playerNum, impulses.indexOf(imp), *pos);

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

    if(!counters[impulses.indexOf(imp)])
        return 0;

    short *counter = &counters[impulses.indexOf(imp)]->impulseCounts[playerNum];
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

    int const index = impulses.indexOf(imp);
    counters[index]->impulseCounts[playerNum]++;

#ifdef __CLIENT__
    // Mark for double clicks.
    P_MaintainImpulseDoubleClicks(playerNum, index, 1);
    P_MaintainImpulseDoubleClicks(playerNum, index, 0);
#endif
}
