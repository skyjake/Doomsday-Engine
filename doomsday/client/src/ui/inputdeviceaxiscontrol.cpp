/** @file inputdeviceaxiscontrol.cpp  Axis control for a logical input device.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "ui/inputdeviceaxiscontrol.h"
#include <de/smoother.h>
#include <de/timer.h> // SECONDSPERTIC
#include <de/Block>
#include <doomsday/console/var.h>
#include "dd_loop.h" // DD_LatestRunTicsStartTime()

using namespace de;

DENG2_PIMPL_NOREF(InputDeviceAxisControl)
{
    Type type = Pointer;
    dint flags = 0;

    ddouble position     = 0;      ///< Current translated position (-1..1) including any filtering.
    ddouble realPosition = 0;      ///< The actual latest position (-1..1).

    dfloat scale    = 1;           ///< Scaling factor for real input values.
    dfloat deadZone = 0;           ///< Dead zone in (0..1) range.

    ddouble sharpPosition = 0;     ///< Current sharp (accumulated) position, entered into the Smoother.
    Smoother *smoother = nullptr;  ///< Smoother for the input values (owned).
    ddouble prevSmoothPos = 0;     ///< Previous evaluated smooth position (needed for producing deltas).

    duint time = 0;                ///< Timestamp of the last position update.

    Instance()
    {
        Smoother_SetMaximumPastNowDelta(smoother = Smoother_New(), 2 * SECONDSPERTIC);
    }

    ~Instance()
    {
        Smoother_Delete(smoother);
    }

#if 0
    static float filter(int grade, float *accumulation, float ticLength)
    {
        DENG2_ASSERT(accumulation);
        int dir     = de::sign(*accumulation);
        float avail = fabs(*accumulation);
        // Determine the target velocity.
        float target = avail * (MAX_AXIS_FILTER - de::clamp(1, grade, 39));

        /*
        // test: clamp
        if(target < -.7) target = -.7;
        else if(target > .7) target = .7;
        else target = 0;
        */

        // Determine the amount of mickeys to send. It depends on the
        // current mouse velocity, and how much time has passed.
        float used = target * ticLength;

        // Don't go past the available motion.
        if(used > avail)
        {
            *accumulation = nullptr;
            used = avail;
        }
        else
        {
            if(*accumulation > nullptr)
                *accumulation -= used;
            else
                *accumulation += used;
        }

        // This is the new (filtered) axis position.
        return dir * used;
    }
#endif
};

InputDeviceAxisControl::InputDeviceAxisControl(String const &name, Type type) : d(new Instance)
{
    setName(name);
    d->type = type;
}

InputDeviceAxisControl::~InputDeviceAxisControl()
{}

InputDeviceAxisControl::Type InputDeviceAxisControl::type() const
{
    return d->type;
}

void InputDeviceAxisControl::setRawInput(bool yes)
{
    if(yes) d->flags |= IDA_RAW;
    else    d->flags &= ~IDA_RAW;
}

bool InputDeviceAxisControl::isActive() const
{
    return (d->flags & IDA_DISABLED) == 0;
}

bool InputDeviceAxisControl::isInverted() const
{
    return (d->flags & IDA_INVERT) != 0;
}

void InputDeviceAxisControl::update(timespan_t ticLength)
{
    Smoother_Advance(d->smoother, ticLength);

    if(d->type == Stick)
    {
        if(d->flags & IDA_RAW)
        {
            // The axis is supposed to be unfiltered.
            d->position = d->realPosition;
        }
        else
        {
            // Absolute positions are straightforward to evaluate.
            Smoother_EvaluateComponent(d->smoother, 0, &d->position);
        }
    }
    else if(d->type == Pointer)
    {
        if(d->flags & IDA_RAW)
        {
            // The axis is supposed to be unfiltered.
            d->position    += d->realPosition;
            d->realPosition = 0;
        }
        else
        {
            // Apply smoothing by converting back into a delta.
            coord_t smoothPos = d->prevSmoothPos;
            Smoother_EvaluateComponent(d->smoother, 0, &smoothPos);
            d->position     += smoothPos - d->prevSmoothPos;
            d->prevSmoothPos = smoothPos;
        }
    }

    // We can clear the expiration now that an updated value is available.
    setBindContextAssociation(Expired, UnsetFlags);
}

ddouble InputDeviceAxisControl::position() const
{
    return d->position;
}

void InputDeviceAxisControl::setPosition(ddouble newPosition)
{
    d->position = newPosition;
}

void InputDeviceAxisControl::applyRealPosition(dfloat pos)
{
    dfloat const oldRealPos  = d->realPosition;
    dfloat const transformed = translateRealPosition(pos);

    // The unfiltered position.
    d->realPosition = transformed;

    if(oldRealPos != d->realPosition)
    {
        // Mark down the time of the change.
        d->time = DD_LatestRunTicsStartTime();
    }

    if(d->type == Stick)
    {
        d->sharpPosition = d->realPosition;
    }
    else // Cumulative.
    {
        // Convert the delta to an absolute position for smoothing.
        d->sharpPosition += d->realPosition;
    }

    Smoother_AddPosXY(d->smoother, DD_LatestRunTicsStartTime(), d->sharpPosition, 0);
}

dfloat InputDeviceAxisControl::translateRealPosition(dfloat realPos) const
{
    // An inactive axis is always zero.
    if(!isActive()) return 0;

    // Apply scaling, deadzone and clamping.
    float outPos = realPos * d->scale;
    if(d->type == Stick) // Only stick axes are dead-zoned and clamped.
    {
        if(fabs(outPos) <= d->deadZone)
        {
            outPos = 0;
        }
        else
        {
            outPos -= d->deadZone * de::sign(outPos);  // Remove the dead zone.
            outPos *= 1.0f/(1.0f - d->deadZone);       // Normalize.
            outPos = de::clamp(-1.0f, outPos, 1.0f);
        }
    }

    if(isInverted())
    {
        // Invert the axis position.
        outPos = -outPos;
    }

    return outPos;
}

dfloat InputDeviceAxisControl::deadZone() const
{
    return d->deadZone;
}

void InputDeviceAxisControl::setDeadZone(dfloat newDeadZone)
{
    d->deadZone = newDeadZone;
}

dfloat InputDeviceAxisControl::scale() const
{
    return d->scale;
}

void InputDeviceAxisControl::setScale(dfloat newScale)
{
    d->scale = newScale;
}

duint InputDeviceAxisControl::time() const
{
    return d->time;
}

String InputDeviceAxisControl::description() const
{
    QStringList flags;
    if(!isActive()) flags << "disabled";
    if(isInverted()) flags << "inverted";

    String flagsString;
    if(!flags.isEmpty())
    {
        String flagsAsText = flags.join("|");
        flagsString = String(_E(l) " Flags :" _E(.)_E(i) "%1" _E(.)).arg(flagsAsText);
    }

    return String(_E(b) "%1 " _E(.) "(Axis-%2)"
                  //_E(l) " Filter: "    _E(.)_E(i) "%3" _E(.)
                  _E(l) " Dead Zone: " _E(.)_E(i) "%3" _E(.)
                  _E(l) " Scale: "     _E(.)_E(i) "%4" _E(.)
                  "%5")
               .arg(fullName())
               .arg(d->type == Stick? "Stick" : "Pointer")
               //.arg(d->filter)
               .arg(d->deadZone)
               .arg(d->scale)
               .arg(flagsString);
}

bool InputDeviceAxisControl::inDefaultState() const
{
    return d->position == 0; // Centered?
}

void InputDeviceAxisControl::reset()
{
    if(d->type == Pointer)
    {
        // Clear the accumulation.
        d->position      = 0;
        d->sharpPosition = 0;
        d->prevSmoothPos = 0;
    }
    Smoother_Clear(d->smoother);
}

void InputDeviceAxisControl::consoleRegister()
{
    DENG2_ASSERT(hasDevice() && !name().isEmpty());
    String controlName = String("input-%1-%2").arg(device().name()).arg(name());

    Block scale = (controlName + "-scale").toUtf8();
    C_VAR_FLOAT(scale.constData(), &d->scale, CVF_NO_MAX, 0, 0);

    Block flags = (controlName + "-flags").toUtf8();
    C_VAR_INT(flags.constData(), &d->flags, 0, 0, 7);

    if(d->type == Stick)
    {
        Block deadzone = (controlName + "-deadzone").toUtf8();
        C_VAR_FLOAT(deadzone.constData(), &d->deadZone, 0, 0, 1);
    }
}
