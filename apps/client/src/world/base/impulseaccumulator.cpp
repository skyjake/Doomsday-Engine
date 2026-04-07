/** @file impulseaccumulator.cpp  Player impulse accumulation.
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

#include "world/impulseaccumulator.h"

#include <de/legacy/timer.h>
#include <de/logbuffer.h>
#include <doomsday/console/var.h>
#ifdef __CLIENT__
#  include "ui/inputsystem.h"
#  include "clientapp.h"
#endif

#include "world/p_players.h"

#ifdef __CLIENT__
#  include "ui/bindcontext.h"
#  include "ui/b_util.h"
#endif

using namespace de;

#ifdef __CLIENT__
static inline InputSystem &inputSys()
{
    return ClientApp::input();
}

static int pimpDoubleClickThreshold = 300; ///< Milliseconds, cvar
#endif

DE_PIMPL_NOREF(ImpulseAccumulator)
{
    int impulseId = 0;
    int playerNum = 0;
    AccumulatorType type = Analog;
    bool expireBeforeSharpTick = false;

    short binaryAccum = 0;

    inline PlayerImpulse &getImpulse() const
    {
        auto *impulse = P_PlayerImpulsePtr(impulseId);
        DE_ASSERT(impulse);
        return *impulse;
    }

#if defined (__CLIENT__)
    /**
     * Double-"clicks" actually mean double activations that occur within the
     * double-click threshold. This is to allow double-clicks also from the
     * analog impulses.
     */
    struct DoubleClick {
        enum State { None, Positive, Negative };

        bool  triggered          = false; //< True if double-click has been detected.
        uint  previousClickTime  = 0;     //< Previous time an activation occurred.
        State lastState          = None;  //< State at the previous time the check was made.
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
        const uint threshold = uint( de::max(0, pimpDoubleClickThreshold) );
        const uint nowTime   = Timer_RealMilliseconds();

        if(newState == db.previousClickState && nowTime - db.previousClickTime < threshold)
        {
            db.triggered = true;

            const PlayerImpulse &impulse = getImpulse();

            // Compose the name of the symbolic event.
            String symbolicName;
            switch(newState)
            {
            case DoubleClick::Positive: symbolicName += "control-doubleclick-positive-"; break;
            case DoubleClick::Negative: symbolicName += "control-doubleclick-negative-"; break;

            default: break;
            }
            symbolicName += impulse.name;

            const int localPlayer = P_ConsoleToLocal(playerNum);
            DE_ASSERT(localPlayer >= 0);
            LOG_INPUT_XVERBOSE("Triggered " _E(b) "'%s'" _E(.) " for player%i state: %i threshold: %i\n  %s",
                               impulse.name << (localPlayer + 1) << newState
                               << (nowTime - db.previousClickTime) << symbolicName);

            ddevent_t ev{};
            ev.device = uint(-1);
            ev.type   = E_SYMBOLIC;
            ev.symbolic.id   = playerNum;
            ev.symbolic.name = symbolicName;

            inputSys().postEvent(ev); // makes a copy.
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
    : d(new Impl)
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
    DE_ASSERT(d->type == Binary);
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
    DE_ASSERT(d->type == Binary);
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
    DE_ASSERT(d->type == Analog);
    LOG_AS("ImpulseAccumulator");

    if(pos) *pos = 0;
    if(relOffset) *relOffset = 0;

    if(BindContext *bindContext = inputSys().contextPtr(d->getImpulse().bindContextName))
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

    default: DE_ASSERT_FAIL("ImpulseAccumulator::clearAll: Unknown type");
    }

    // Also clear the double click state.
    d->clearDoubleClick();
}

void ImpulseAccumulator::consoleRegister() // static
{
    LOG_AS("ImpulseAccumulator");
    C_VAR_INT("input-doubleclick-threshold", &pimpDoubleClickThreshold, 0, 0, 2000);
}

#endif // __CLIENT__
