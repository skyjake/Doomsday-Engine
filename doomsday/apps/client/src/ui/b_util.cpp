/** @file b_util.cpp  Input system, binding utilities.
 *
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#include "ui/b_util.h"
#include <de/legacy/timer.h>
#include <de/ddkey.h>
#include <de/recordvalue.h>
#include "clientapp.h"

#include "ui/bindcontext.h"
#include "ui/commandbinding.h"
#include "ui/impulsebinding.h"
#include "ui/inputsystem.h"
#include "ui/inputdevice.h"
#include "ui/axisinputcontrol.h"
#include "ui/buttoninputcontrol.h"
#include "ui/hatinputcontrol.h"

#include <doomsday/network/net.h>

using namespace de;

byte zeroControlUponConflict = true;

static float STAGE_THRESHOLD = 6.f/35;
static float STAGE_FACTOR    = .5f;

bool B_ParseButtonState(Binding::ControlTest &test, const char *toggleName)
{
    DE_ASSERT(toggleName);

    if (!strlen(toggleName) || !iCmpStrCase(toggleName, "down"))
    {
        test = Binding::ButtonStateDown; // this is the default, if omitted
        return true;
    }
    if (!iCmpStrCase(toggleName, "undefined"))
    {
        test = Binding::ButtonStateAny;
        return true;
    }
    if (!iCmpStrCase(toggleName, "repeat"))
    {
        test = Binding::ButtonStateRepeat;
        return true;
    }
    if (!iCmpStrCase(toggleName, "press"))
    {
        test = Binding::ButtonStateDownOrRepeat;
        return true;
    }
    if (!iCmpStrCase(toggleName, "up"))
    {
        test = Binding::ButtonStateUp;
        return true;
    }

    LOG_INPUT_WARNING("\"%s\" is not a toggle state") << toggleName;
    return false; // Not recognized.
}

bool B_ParseAxisPosition(Binding::ControlTest &test, float &pos, const char *desc)
{
    DE_ASSERT(desc);

    if (!iCmpStrNCase(desc, "within", 6) && strlen(desc) > 6)
    {
        test = Binding::AxisPositionWithin;
        pos  = String((desc + 6)).toFloat();
        return true;
    }
    if (!iCmpStrNCase(desc, "beyond", 6) && strlen(desc) > 6)
    {
        test = Binding::AxisPositionBeyond;
        pos  = String((desc + 6)).toFloat();
        return true;
    }
    if (!iCmpStrNCase(desc, "pos", 3) && strlen(desc) > 3)
    {
        test = Binding::AxisPositionBeyondPositive;
        pos  = String((desc + 3)).toFloat();
        return true;
    }
    if (!iCmpStrNCase(desc, "neg", 3) && strlen(desc) > 3)
    {
        test = Binding::AxisPositionBeyondNegative;
        pos  = -String((desc + 3)).toFloat();
        return true;
    }

    LOG_INPUT_WARNING("Axis position \"%s\" is invalid") << desc;
    return false;
}

bool B_ParseModifierId(int &id, const char *desc)
{
    DE_ASSERT(desc);
    id = String(desc).toInt() - 1 + CTL_MODIFIER_1;
    return (id >= CTL_MODIFIER_1 && id <= CTL_MODIFIER_4);
}

bool B_ParseKeyId(int &id, const char *desc)
{
    DE_ASSERT(desc);
    LOG_AS("B_ParseKeyId");

    // The possibilies: symbolic key name, or "codeNNN".
    if (!iCmpStrNCase(desc, "code", 4) && strlen(desc) == 7)
    {
        // Hexadecimal?
        if (desc[4] == 'x' || desc[4] == 'X')
        {
            id = String((desc + 5)).toInt(nullptr, 16);
            return true;
        }

        // Decimal.
        id = String((desc + 4)).toInt();
        if (id > 0 && id <= 255) return true;

        LOGDEV_INPUT_WARNING("Key code %i out of range") << id;
        return false;
    }

    // Symbolic key name.
    id = B_KeyForShortName(desc);
    if (id) return true;

    LOG_INPUT_WARNING("Unknown key \"%s\"") << desc;
    return false;
}

bool B_ParseMouseTypeAndId(ddeventtype_t &type, int &id, const char *desc)
{
    DE_ASSERT(desc);
    const InputDevice &mouse = InputSystem::get().device(IDEV_MOUSE);

    // Maybe it's one of the named buttons?
    id = mouse.toButtonId(desc);
    if (id >= 0)
    {
        type = E_TOGGLE;
        return true;
    }

    // Perhaps a generic button?
    if (!iCmpStrNCase(desc, "button", 6) && strlen(desc) > 6)
    {
        type = E_TOGGLE;
        id   = String((desc + 6)).toInt() - 1;
        if (mouse.hasButton(id))
            return true;

        LOG_INPUT_WARNING("\"%s\" button %i does not exist") << mouse.title() << id;
        return false;
    }

    // Must be an axis, then.
    type = E_AXIS;
    id   = mouse.toAxisId(desc);
    if (id >= 0) return true;

    LOG_INPUT_WARNING("\"%s\" axis \"%s\" does not exist") << mouse.title() << desc;
    return false;
}

bool B_ParseDeviceAxisTypeAndId(ddeventtype_t &type, int &id, const InputDevice &device, const char *desc)
{
    DE_ASSERT(desc);

    type = E_AXIS;
    id   = device.toAxisId(desc);
    if (id >= 0) return true;

    LOG_INPUT_WARNING("Axis \"%s\" is not defined in device '%s'") << desc << device.name();
    return false;
}

bool B_ParseJoystickTypeAndId(ddeventtype_t &type, int &id, int deviceId, const char *desc)
{
    InputDevice &device = InputSystem::get().device(deviceId);

    if (!iCmpStrNCase(desc, "button", 6) && strlen(desc) > 6)
    {
        type = E_TOGGLE;
        id   = String((desc + 6)).toInt() - 1;
        if (device.hasButton(id))
            return true;

        LOG_INPUT_WARNING("\"%s\" button %i does not exist") << device.title() << id;
        return false;
    }
    if (!iCmpStrNCase(desc, "hat", 3) && strlen(desc) > 3)
    {
        type = E_ANGLE;
        id   = String((desc + 3)).toInt() - 1;
        if (device.hasHat(id))
            return true;

        LOG_INPUT_WARNING("\"%s\" hat %i does not exist") << device.title() << id;
        return false;
    }
    if (!iCmpStrCase(desc, "hat"))
    {
        type = E_ANGLE;
        id   = 0;
        return true;
    }

    // Try to find the axis.
    return B_ParseDeviceAxisTypeAndId(type, id, device, desc);
}

bool B_ParseHatAngle(float &pos, const char *desc)
{
    DE_ASSERT(desc);
    if (!iCmpStrCase(desc, "center"))
    {
        pos = -1;
        return true;
    }
    if (!iCmpStrNCase(desc, "angle", 5) && strlen(desc) > 5)
    {
        pos = String((desc + 5)).toFloat();
        return true;
    }
    LOG_INPUT_WARNING("Angle position \"%s\" is invalid") << desc;
    return false;
}

bool B_ParseBindingCondition(Record &cond, const char *desc)
{
    DE_ASSERT(desc);

    // First, we expect to encounter a device name.
    AutoStr *str = AutoStr_NewStd();
    desc = Str_CopyDelim(str, desc, '-');

    if (!Str_CompareIgnoreCase(str, "multiplayer"))
    {
        // This is only intended for multiplayer games.
        cond.set("type", int(Binding::GlobalState));
        cond.set("multiplayer", true);
    }
    else if (!Str_CompareIgnoreCase(str, "modifier"))
    {
        cond.set("type", int(Binding::ModifierState));
        cond.set("device", -1); // not used

        // Parse the modifier number.
        desc = Str_CopyDelim(str, desc, '-');
        int id = 0;
        bool ok = B_ParseModifierId(id, Str_Text(str));
        if (!ok) return false;
        cond.set("id", id);

        // The final part of a modifier is the state.
        desc = Str_CopyDelim(str, desc, '-');
        Binding::ControlTest test = Binding::None;
        ok = B_ParseButtonState(test, Str_Text(str));
        if (!ok) return false;
        cond.set("test", int(test));
    }
    else if (!Str_CompareIgnoreCase(str, "key"))
    {
        cond.set("type", int(Binding::ButtonState));
        cond.set("device", int(IDEV_KEYBOARD));

        // Parse the key.
        desc = Str_CopyDelim(str, desc, '-');
        int id = 0;
        bool ok = B_ParseKeyId(id, Str_Text(str));
        if (!ok) return false;
        cond.set("id", id);

        // The final part of a key event is the state of the key toggle.
        desc = Str_CopyDelim(str, desc, '-');
        Binding::ControlTest test = Binding::None;
        ok = B_ParseButtonState(test, Str_Text(str));
        if (!ok) return false;
        cond.set("test", int(test));
    }
    else if (!Str_CompareIgnoreCase(str, "mouse"))
    {
        cond.set("device", int(IDEV_MOUSE));

        // What is being targeted?
        desc = Str_CopyDelim(str, desc, '-');
        ddeventtype_t type = E_TOGGLE;
        int id = 0;
        bool ok = B_ParseMouseTypeAndId(type, id, Str_Text(str));
        if (!ok) return false;
        cond.set("type", int(type));
        cond.set("id", id);

        desc = Str_CopyDelim(str, desc, '-');
        if (type == E_TOGGLE)
        {
            cond.set("type", int(Binding::ButtonState));

            Binding::ControlTest test = Binding::None;
            ok = B_ParseButtonState(test, Str_Text(str));
            if (!ok) return false;
            cond.set("test", int(test));
        }
        else if (type == E_AXIS)
        {
            cond.set("type", int(Binding::AxisState));

            Binding::ControlTest test = Binding::None;
            float pos;
            ok = B_ParseAxisPosition(test, pos, Str_Text(str));
            if (!ok) return false;
            cond.set("test", int(test));
            cond.set("pos", pos);
        }
    }
    else if (!Str_CompareIgnoreCase(str, "joy") || !Str_CompareIgnoreCase(str, "head"))
    {
        cond.set("device", int(!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER));

        // What is being targeted?
        desc = Str_CopyDelim(str, desc, '-');
        ddeventtype_t type = E_TOGGLE;
        int id = 0;
        bool ok = B_ParseJoystickTypeAndId(type, id, cond.geti("device"), Str_Text(str));
        if (!ok) return false;
        cond.set("type", int(type));

        desc = Str_CopyDelim(str, desc, '-');
        if (type == E_TOGGLE)
        {
            cond.set("type", int(Binding::ButtonState));

            Binding::ControlTest test = Binding::None;
            ok = B_ParseButtonState(test, Str_Text(str));
            if (!ok) return false;
            cond.set("test", int(test));
        }
        else if (type == E_AXIS)
        {
            cond.set("type", int(Binding::AxisState));

            Binding::ControlTest test = Binding::None;
            float pos = 0;
            ok = B_ParseAxisPosition(test, pos, Str_Text(str));
            if (!ok) return false;
            cond.set("test", int(test));
            cond.set("pos", pos);
        }
        else // Angle.
        {
            cond.set("type", int(Binding::HatState));

            float pos = 0;
            ok = B_ParseHatAngle(pos, Str_Text(str));
            if (!ok) return false;
            cond.set("pos", pos);
        }
    }
    else
    {
        LOG_INPUT_WARNING("Unknown input device \"%s\"") << Str_Text(str);
        return false;
    }

    // Check for valid button state tests.
    if (cond.geti("type") == Binding::ButtonState)
    {
        if (cond.geti("test") != Binding::ButtonStateUp &&
           cond.geti("test") != Binding::ButtonStateDown)
        {
            LOG_INPUT_WARNING("\"%s\": Button condition can only be 'up' or 'down'") << desc;
            return false;
        }
    }

    // Finally, there may be the negation at the end.
    desc = Str_CopyDelim(str, desc, '-');
    if (!Str_CompareIgnoreCase(str, "not"))
    {
        cond.set("negate", true);
    }

    // Anything left that wasn't used?
    if (!desc) return true; // No errors detected.

    LOG_INPUT_WARNING("Unrecognized condition \"%s\"") << desc;
    return false;
}

bool B_CheckAxisPosition(Binding::ControlTest test, float testPos, float pos)
{
    switch (test)
    {
    case Binding::AxisPositionWithin:
        return !((pos > 0 && pos > testPos) || (pos < 0 && pos < -testPos));

    case Binding::AxisPositionBeyond:
        return ((pos > 0 && pos >= testPos) || (pos < 0 && pos <= -testPos));

    case Binding::AxisPositionBeyondPositive:
        return !(pos < testPos);

    case Binding::AxisPositionBeyondNegative:
        return !(pos > -testPos);

    default: break;
    }

    return false;
}

bool B_CheckCondition(const Binding::CompiledConditionRecord *condRec, int localNum,
                      const BindContext *context)
{
    DE_ASSERT(condRec);

    const auto &cond = condRec->compiled();
    const bool fulfilled = !cond.negate;

    switch (cond.type)
    {
    case Binding::GlobalState:
        if (cond.multiplayer && netState.netGame)
            return fulfilled;
        break;

    case Binding::AxisState: {
        const AxisInputControl &axis = InputSystem::get().device(cond.device).axis(cond.id);
        if (B_CheckAxisPosition(cond.test, cond.pos, axis.position()))
        {
            return fulfilled;
        }
        break; }

    case Binding::ButtonState: {
        const ButtonInputControl &button = InputSystem::get().device(cond.device).button(cond.id);
        bool isDown = button.isDown();
        if (( isDown && cond.test == Binding::ButtonStateDown) ||
            (!isDown && cond.test == Binding::ButtonStateUp))
        {
            return fulfilled;
        }
        break; }

    case Binding::HatState: {
        const HatInputControl &hat = InputSystem::get().device(cond.device).hat(cond.id);
        if (hat.position() == cond.pos)
        {
            return fulfilled;
        }
        break; }

    case Binding::ModifierState:
        if (context)
        {
            // Evaluate the current state of the modifier (in this context).
            float pos = 0, relative = 0;
            B_EvaluateImpulseBindings(context, localNum, cond.id, &pos, &relative, false /*no triggered*/);
            if ((cond.test == Binding::ButtonStateDown && fabs(pos) > .5) ||
                (cond.test == Binding::ButtonStateUp   && fabs(pos) < .5))
            {
                return fulfilled;
            }
        }
        break;

    default: DE_ASSERT_FAIL("B_CheckCondition: Unknown cond.type"); break;
    }

    return !fulfilled;
}

/// @todo: Belongs in BindContext? -ds
void B_EvaluateImpulseBindings(const BindContext *context, int localNum, int impulseId,
    float *pos, float *relativeOffset, bool allowTriggered)
{
    DE_ASSERT(context); // Why call without one?
    DE_ASSERT(pos && relativeOffset);

    *pos = 0;
    *relativeOffset = 0;

    if (localNum < 0 || localNum >= DDMAXPLAYERS) return; // No local player specified.

    const uint nowTime = Timer_RealMilliseconds();
    bool conflicted[NUM_IBD_TYPES]; de::zap(conflicted);
    bool appliedState[NUM_IBD_TYPES]; de::zap(appliedState);

    context->forAllImpulseBindings(localNum, [&] (CompiledImpulseBindingRecord &rec)
    {
        // Wrong impulse?
        //ImpulseBinding bind(rec);
        const auto &bind = rec.compiled();
        if (bind.impulseId != impulseId) return LoopContinue;

        // If the binding has conditions, they may prevent using it.
        bool skip = false;
        const ArrayValue &conds = rec.geta(DE_STR("condition"));
        DE_FOR_EACH_CONST(ArrayValue::Elements, i, conds.elements())
        {
            if (!B_CheckCondition(static_cast<Binding::CompiledConditionRecord *>
                                  ((*i)->as<RecordValue>().record()), localNum, context))
            {
                skip = true;
                break;
            }
        }
        if (skip) return LoopContinue;

        // Get the device.
        const InputDevice *device = InputSystem::get().devicePtr(bind.deviceId);
        if (!device || !device->isActive())
            return LoopContinue; // Not available.

        // Get the control.
        InputControl *ctrl = nullptr;
        switch (bind.type)
        {
        case IBD_AXIS:   ctrl = &device->axis  (bind.controlId); break;
        case IBD_TOGGLE: ctrl = &device->button(bind.controlId); break;
        case IBD_ANGLE:  ctrl = &device->hat   (bind.controlId); break;

        default: DE_ASSERT_FAIL("B_EvaluateImpulseBindings: Invalid bind.type"); break;
        }

        float devicePos = 0;
        float deviceOffset = 0;
        uint deviceTime = 0;

        if (auto *axis = maybeAs<AxisInputControl>(ctrl))
        {
            if (context && axis->bindContext() != context)
            {
                if (axis->hasBindContext() && !axis->bindContext()->
                        findImpulseBinding(bind.deviceId, IBD_AXIS, bind.controlId))
                {
                    // The overriding context doesn't bind to the axis, though.
                    if (axis->type() == AxisInputControl::Pointer)
                    {
                        // Reset the relative accumulation.
                        axis->setPosition(0);
                    }
                }
                return LoopContinue; // Shadowed by a more important active class.
            }

            // Expired?
            if (!(axis->bindContextAssociation() & InputControl::Expired))
            {
                if (axis->type() == AxisInputControl::Pointer)
                {
                    deviceOffset = axis->position();
                    axis->setPosition(0);
                }
                else
                {
                    devicePos = axis->position();
                }
                deviceTime = axis->time();
            }
        }
        if (auto *button = maybeAs<ButtonInputControl>(ctrl))
        {
            if (context && button->bindContext() != context)
                return LoopContinue; // Shadowed by a more important active context.

            // Expired?
            if (!(button->bindContextAssociation() & InputControl::Expired))
            {
                devicePos  = (button->isDown() ||
                              (allowTriggered && (button->bindContextAssociation() & InputControl::Triggered))? 1.0f : 0.0f);
                deviceTime = button->time();

                // We've checked it, so clear the flag.
                button->setBindContextAssociation(InputControl::Triggered, UnsetFlags);
            }
        }
        if (auto *hat = maybeAs<HatInputControl>(ctrl))
        {
            if (context && hat->bindContext() != context)
                return LoopContinue; // Shadowed by a more important active class.

            // Expired?
            if (!(hat->bindContextAssociation() & InputControl::Expired))
            {
                devicePos  = (fequal(hat->position(), bind.angle)? 1.0f : 0.0f);
                deviceTime = hat->time();
            }
        }

        const int bflags = bind.flags;

        // Apply further modifications based on flags.
        if (bflags & IBDF_INVERSE)
        {
            devicePos    = -devicePos;
            deviceOffset = -deviceOffset;
        }
        if (bflags & IBDF_TIME_STAGED)
        {
            if (nowTime - deviceTime < STAGE_THRESHOLD * 1000)
            {
                devicePos *= STAGE_FACTOR;
            }
        }

        *pos += devicePos;
        *relativeOffset += deviceOffset;

        // Is this state contributing to the outcome?
        if (!de::fequal(devicePos, 0.f))
        {
            if (appliedState[bind.type])
            {
                // Another binding already influenced this; we have a conflict.
                conflicted[bind.type] = true;
            }

            // We've found one effective binding that influences this control.
            appliedState[bind.type] = true;
        }
        return LoopContinue;
    });

    if (zeroControlUponConflict)
    {
        for (int i = 0; i < NUM_IBD_TYPES; ++i)
        {
            if (conflicted[i])
                *pos = 0;
        }
    }

    // Clamp to the normalized range.
    *pos = de::clamp(-1.0f, *pos, 1.0f);
}

String B_ControlDescToString(int deviceId, ddeventtype_t type, int id)
{
    InputDevice *device = nullptr;
    String str;
    if (type != E_SYMBOLIC)
    {
        device = &InputSystem::get().device(deviceId);
        // Name of the device.
        str += device->name() + "-";
    }

    switch (type)
    {
    case E_TOGGLE: {
        DE_ASSERT(device);
        ButtonInputControl &button = device->button(id);
        if (!button.name().isEmpty())
        {
            str += button.name();
        }
        else if (device == InputSystem::get().devicePtr(IDEV_KEYBOARD))
        {
            const char *name = B_ShortNameForKey(id);
            if (name)
            {
                str += name;
            }
            else
            {
                str += Stringf("code%03i", id);
            }
        }
        else
        {
            str += "button" + String::asText(id + 1);
        }
        break; }

    case E_AXIS:
        DE_ASSERT(device);
        str += device->axis(id).name();
        break;

    case E_ANGLE:    str += "hat" + String::asText(id + 1); break;
    case E_SYMBOLIC: str += "sym";                          break;

    default: DE_ASSERT_FAIL("B_ControlDescToString: Invalid event type"); break;
    }

    return str;
}

String B_ButtonStateToString(Binding::ControlTest test)
{
    switch (test)
    {
    case Binding::ButtonStateAny:          return "-undefined";
    case Binding::ButtonStateDown:         return "-down";
    case Binding::ButtonStateRepeat:       return "-repeat";
    case Binding::ButtonStateDownOrRepeat: return "-press";
    case Binding::ButtonStateUp:           return "-up";

    default:
        DE_ASSERT_FAIL("B_ButtonStateToString: Unknown test");
        return "";
    }
}

String B_AxisPositionToString(Binding::ControlTest test, float pos)
{
    switch (test)
    {
    case Binding::AxisPositionWithin:         return Stringf("-within%f", pos);
    case Binding::AxisPositionBeyond:         return Stringf("-beyond%f", pos);
    case Binding::AxisPositionBeyondPositive: return Stringf("-pos%f",    pos);
    case Binding::AxisPositionBeyondNegative: return Stringf("-neg%f",    -pos);

    default:
        DE_ASSERT_FAIL("B_AxisPositionToString: Unknown test");
        return "";
    }
}

String B_HatAngleToString(float pos)
{
    return (pos < 0? "-center" : String("-angle") + String::asText(pos));
}

String B_ConditionToString(const Record &cond)
{
    String str;

    if (cond.geti("type") == Binding::GlobalState)
    {
        if (cond.getb("multiplayer"))
        {
            str += "multiplayer";
        }
    }
    else if (cond.geti("type") == Binding::ModifierState)
    {
        str += "modifier-" + String::asText(cond.geti("id") - CTL_MODIFIER_1 + 1);
    }
    else
    {
        str += B_ControlDescToString(cond.geti("device"),
                                     (  cond.geti("type") == Binding::ButtonState? E_TOGGLE
                                      : cond.geti("type") == Binding::AxisState? E_AXIS
                                      : E_ANGLE), cond.geti("id"));
    }

    switch (cond.geti("type"))
    {
    case Binding::ButtonState:
    case Binding::ModifierState:
        str += B_ButtonStateToString(Binding::ControlTest(cond.geti("test")));
        break;

    case Binding::AxisState:
        str += B_AxisPositionToString(Binding::ControlTest(cond.geti("test")), cond.getf("pos"));
        break;

    case Binding::HatState:
        str += B_HatAngleToString(cond.getf("pos"));
        break;

    default: break;
    }

    // Flags.
    if (cond.getb("negate"))
    {
        str += "-not";
    }

    return str;
}

String B_EventToString(const ddevent_t &ev)
{
    String str = B_ControlDescToString(ev.device, ev.type,
                                       (  ev.type == E_TOGGLE  ? ev.toggle.id
                                        : ev.type == E_AXIS    ? ev.axis.id
                                        : ev.type == E_ANGLE   ? ev.angle.id
                                        : ev.type == E_SYMBOLIC? ev.symbolic.id
                                        : 0));

    switch (ev.type)
    {
    case E_TOGGLE:
        str += B_ButtonStateToString(  ev.toggle.state == ETOG_DOWN? Binding::ButtonStateDown
                                     : ev.toggle.state == ETOG_UP  ? Binding::ButtonStateUp
                                     : Binding::ButtonStateUp);
        break;

    case E_AXIS:
        str += B_AxisPositionToString((ev.axis.pos >= 0? Binding::AxisPositionBeyondPositive : Binding::AxisPositionBeyondNegative),
                                      ev.axis.pos);
        break;

    case E_ANGLE:    str += B_HatAngleToString(ev.angle.pos); break;
    case E_SYMBOLIC: str += "-" + String(ev.symbolic.name);        break;

    default: break;
    }

    return str;
}

struct keyname_t
{
    int key;           ///< DDKEY
    const char *name;
};
static keyname_t const keyNames[] = {
    { DDKEY_PAUSE,       "pause" },
    { DDKEY_ESCAPE,      "escape" },
    { DDKEY_ESCAPE,      "esc" },
    { DDKEY_RIGHTARROW,  "right" },
    { DDKEY_LEFTARROW,   "left" },
    { DDKEY_UPARROW,     "up" },
    { DDKEY_DOWNARROW,   "down" },
    { DDKEY_RETURN,      "return" },
    { DDKEY_TAB,         "tab" },
    { DDKEY_RSHIFT,      "shift" },
    { DDKEY_RCTRL,       "ctrl" },
    { DDKEY_RCTRL,       "control" },
    { DDKEY_RALT,        "alt" },
    { DDKEY_INS,         "insert" },
    { DDKEY_INS,         "ins" },
    { DDKEY_DEL,         "delete" },
    { DDKEY_DEL,         "del" },
    { DDKEY_PGUP,        "pageup" },
    { DDKEY_PGUP,        "pgup" },
    { DDKEY_PGDN,        "pagedown" },
    { DDKEY_PGDN,        "pgdown" },
    { DDKEY_PGDN,        "pgdn" },
    { DDKEY_HOME,        "home" },
    { DDKEY_END,         "end" },
    { DDKEY_BACKSPACE,   "backspace" },
    { DDKEY_BACKSPACE,   "bkspc" },
    { '/',               "slash" },
    { DDKEY_BACKSLASH,   "backslash" },
    { '[',               "sqbracketleft" },
    { ']',               "sqbracketright" },
    { '+',               "plus" },
    { '-',               "minus" },
    { '+',               "plus" },
    { '=',               "equals" },
    { ' ',               "space" },
    { ';',               "semicolon" },
    { ',',               "comma" },
    { '.',               "period" },
    { '\'',              "apostrophe" },
    { DDKEY_F10,         "f10" },
    { DDKEY_F11,         "f11" },
    { DDKEY_F12,         "f12" },
    { DDKEY_F1,          "f1" },
    { DDKEY_F2,          "f2" },
    { DDKEY_F3,          "f3" },
    { DDKEY_F4,          "f4" },
    { DDKEY_F5,          "f5" },
    { DDKEY_F6,          "f6" },
    { DDKEY_F7,          "f7" },
    { DDKEY_F8,          "f8" },
    { DDKEY_F9,          "f9" },
    { '`',               "tilde" },
    { DDKEY_NUMLOCK,     "numlock" },
    { DDKEY_CAPSLOCK,    "capslock" },
    { DDKEY_SCROLL,      "scrlock" },
    { DDKEY_NUMPAD0,     "pad0" },
    { DDKEY_NUMPAD1,     "pad1" },
    { DDKEY_NUMPAD2,     "pad2" },
    { DDKEY_NUMPAD3,     "pad3" },
    { DDKEY_NUMPAD4,     "pad4" },
    { DDKEY_NUMPAD5,     "pad5" },
    { DDKEY_NUMPAD6,     "pad6" },
    { DDKEY_NUMPAD7,     "pad7" },
    { DDKEY_NUMPAD8,     "pad8" },
    { DDKEY_NUMPAD9,     "pad9" },
    { DDKEY_DECIMAL,     "decimal" },
    { DDKEY_DECIMAL,     "padcomma" },
    { DDKEY_SUBTRACT,    "padminus" },
    { DDKEY_ADD,         "padplus" },
    { DDKEY_PRINT,       "print" },
    { DDKEY_PRINT,       "prtsc" },
    { DDKEY_ENTER,       "enter" },
    { DDKEY_DIVIDE,      "divide" },
    { DDKEY_MULTIPLY,    "multiply" },
    { DDKEY_SECTION,     "section" },
    { DDKEY_WINMENU,     "winmenu" },
    { DDKEY_ISOEXTRAKEY, "extra" },
    { 0, nullptr}
};

const char *B_ShortNameForKey(int ddKey, bool forceLowercase)
{
    static char nameBuffer[40];

    for (uint idx = 0; keyNames[idx].key; ++idx)
    {
        if (ddKey == keyNames[idx].key)
            return keyNames[idx].name;
    }

    if (ddKey < 127 && isprint(ddKey))
    {
        // Printable character, fabricate a single-character name.
        nameBuffer[0] = forceLowercase? tolower(ddKey) : ddKey;
        nameBuffer[1] = 0;
        return nameBuffer;
    }

    return nullptr;
}

int B_KeyForShortName(const char *key)
{
    DE_ASSERT(key);

    for (uint idx = 0; keyNames[idx].key; ++idx)
    {
        if (!iCmpStrCase(key, keyNames[idx].name))
            return keyNames[idx].key;
    }

    if (strlen(key) == 1 && key[0] < 127 && isprint(key[0]))
    {
        // ASCII char.
        return tolower(key[0]);
    }

    return 0;
}
