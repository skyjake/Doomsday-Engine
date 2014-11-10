/** @file b_util.cpp  Input system, binding utilities.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/timer.h>
#include "clientapp.h"

#include "BindContext"
#include "ui/inputdevice.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"
#include "network/net_main.h" // netGame

using namespace de;

byte zeroControlUponConflict = true;

static float STAGE_THRESHOLD = 6.f/35;
static float STAGE_FACTOR    = .5f;

static inline InputSystem &inputSys()
{
    return ClientApp::inputSystem();
}

bool B_ParseToggleState(char const *toggleName, ebstate_t *state)
{
    DENG2_ASSERT(toggleName && state);

    if(!qstrlen(toggleName) || !qstricmp(toggleName, "down"))
    {
        *state = EBTOG_DOWN; // this is the default, if omitted
        return true;
    }
    if(!qstricmp(toggleName, "undefined"))
    {
        *state = EBTOG_UNDEFINED;
        return true;
    }
    if(!qstricmp(toggleName, "repeat"))
    {
        *state = EBTOG_REPEAT;
        return true;
    }
    if(!qstricmp(toggleName, "press"))
    {
        *state = EBTOG_PRESS;
        return true;
    }
    if(!qstricmp(toggleName, "up"))
    {
        *state = EBTOG_UP;
        return true;
    }

    LOG_INPUT_WARNING("\"%s\" is not a toggle state") << toggleName;
    return false; // Not recognized.
}

bool B_ParseAxisPosition(char const *desc, ebstate_t *state, float *pos)
{
    DENG2_ASSERT(desc && state && pos);

    if(!qstrnicmp(desc, "within", 6) && qstrlen(desc) > 6)
    {
        *state = EBAXIS_WITHIN;
        *pos   = String((desc + 6)).toFloat();
        return true;
    }
    if(!qstrnicmp(desc, "beyond", 6) && qstrlen(desc) > 6)
    {
        *state = EBAXIS_BEYOND;
        *pos   = String((desc + 6)).toFloat();
        return true;
    }
    if(!qstrnicmp(desc, "pos", 3) && qstrlen(desc) > 3)
    {
        *state = EBAXIS_BEYOND_POSITIVE;
        *pos   = String((desc + 3)).toFloat();
        return true;
    }
    if(!qstrnicmp(desc, "neg", 3) && qstrlen(desc) > 3)
    {
        *state = EBAXIS_BEYOND_NEGATIVE;
        *pos   = -String((desc + 3)).toFloat();
        return true;
    }

    LOG_INPUT_WARNING("Axis position \"%s\" is invalid") << desc;
    return false;
}

dd_bool B_ParseModifierId(char const *desc, int *id)
{
    DENG2_ASSERT(desc && id);
    *id = String(desc).toInt() - 1 + CTL_MODIFIER_1;
    return (*id >= CTL_MODIFIER_1 && *id <= CTL_MODIFIER_4);
}

bool B_ParseKeyId(char const *desc, int *id)
{
    DENG2_ASSERT(desc && id);
    LOG_AS("B_ParseKeyId");

    // The possibilies: symbolic key name, or "codeNNN".
    if(!qstrnicmp(desc, "code", 4) && qstrlen(desc) == 7)
    {
        // Hexadecimal?
        if(desc[4] == 'x' || desc[4] == 'X')
        {
            *id = String((desc + 5)).toInt(nullptr, 16);
            return true;
        }

        // Decimal.
        *id = String((desc + 4)).toInt();
        if(*id > 0 && *id <= 255) return true;

        LOGDEV_INPUT_WARNING("Key code %i out of range") << *id;
        return false;
    }

    // Symbolic key name.
    *id = B_KeyForShortName(desc);
    if(*id) return true;

    LOG_INPUT_WARNING("Unknown key \"%s\"") << desc;
    return false;
}

bool B_ParseMouseTypeAndId(char const *desc, ddeventtype_t *type, int *id)
{
    DENG2_ASSERT(desc && type && id);
    InputDevice const &mouse = inputSys().device(IDEV_MOUSE);

    // Maybe it's one of the named buttons?
    *id = mouse.toButtonId(desc);
    if(*id >= 0)
    {
        *type = E_TOGGLE;
        return true;
    }

    // Perhaps a generic button?
    if(!qstrnicmp(desc, "button", 6) && qstrlen(desc) > 6)
    {
        *type = E_TOGGLE;
        *id   = String((desc + 6)).toInt() - 1;
        if(mouse.hasButton(*id))
            return true;

        LOG_INPUT_WARNING("\"%s\" button %i does not exist") << mouse.title() << *id;
        return false;
    }

    // Must be an axis, then.
    *type = E_AXIS;
    *id   = mouse.toAxisId(desc);
    if(*id >= 0) return true;

    LOG_INPUT_WARNING("\"%s\" axis \"%s\" does not exist") << mouse.title() << desc;
    return false;
}

dd_bool B_ParseDeviceAxisTypeAndId(InputDevice const &device, char const *desc, ddeventtype_t *type, int *id)
{
    DENG2_ASSERT(desc && type && id);

    *type = E_AXIS;
    *id   = device.toAxisId(desc);
    if(*id >= 0) return true;

    LOG_INPUT_WARNING("Axis \"%s\" is not defined in device '%s'") << desc << device.name();
    return false;
}

bool B_ParseJoystickTypeAndId(InputDevice const &device, char const *desc, ddeventtype_t *type, int *id)
{
    if(!qstrnicmp(desc, "button", 6) && qstrlen(desc) > 6)
    {
        *type = E_TOGGLE;
        *id   = String((desc + 6)).toInt() - 1;
        if(device.hasButton(*id))
            return true;

        LOG_INPUT_WARNING("\"%s\" button %i does not exist") << device.title() << *id;
        return false;
    }
    if(!qstrnicmp(desc, "hat", 3) && qstrlen(desc) > 3)
    {
        *type = E_ANGLE;
        *id   = String((desc + 3)).toInt() - 1;
        if(device.hasHat(*id))
            return true;

        LOG_INPUT_WARNING("\"%s\" hat %i does not exist") << device.title() << *id;
        return false;
    }
    if(!qstricmp(desc, "hat"))
    {
        *type = E_ANGLE;
        *id   = 0;
        return true;
    }

    // Try to find the axis.
    return B_ParseDeviceAxisTypeAndId(device, desc, type, id);
}

bool B_ParseAnglePosition(char const *desc, float *pos)
{
    DENG2_ASSERT(desc && pos);
    if(!qstricmp(desc, "center"))
    {
        *pos = -1;
        return true;
    }
    if(!qstrnicmp(desc, "angle", 5) && qstrlen(desc) > 5)
    {
        *pos = String((desc + 5)).toFloat();
        return true;
    }
    LOG_INPUT_WARNING("Angle position \"%s\" is invalid") << desc;
    return false;
}

bool B_ParseStateCondition(statecondition_t *cond, char const *desc)
{
    DENG2_ASSERT(cond && desc);

    // First, we expect to encounter a device name.
    AutoStr *str = AutoStr_NewStd();
    desc = Str_CopyDelim(str, desc, '-');

    if(!Str_CompareIgnoreCase(str, "multiplayer"))
    {
        // This is only intended for multiplayer games.
        cond->type = SCT_STATE;
        cond->flags.multiplayer = true;
    }
    else if(!Str_CompareIgnoreCase(str, "modifier"))
    {
        cond->device = 0; // not used
        cond->type = SCT_MODIFIER_STATE;

        // Parse the modifier number.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseModifierId(Str_Text(str), &cond->id))
        {
            return false;
        }

        // The final part of a modifier is the state.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseToggleState(Str_Text(str), &cond->state))
        {
            return false;
        }
    }
    else if(!Str_CompareIgnoreCase(str, "key"))
    {
        cond->device = IDEV_KEYBOARD;
        cond->type = SCT_TOGGLE_STATE;

        // Parse the key.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseKeyId(Str_Text(str), &cond->id))
        {
            return false;
        }

        // The final part of a key event is the state of the key toggle.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseToggleState(Str_Text(str), &cond->state))
        {
            return false;
        }
    }
    else if(!Str_CompareIgnoreCase(str, "mouse"))
    {
        cond->device = IDEV_MOUSE;

        // What is being targeted?
        desc = Str_CopyDelim(str, desc, '-');
        ddeventtype_t type;
        if(!B_ParseMouseTypeAndId(Str_Text(str), &type, &cond->id))
        {
            return false;
        }

        desc = Str_CopyDelim(str, desc, '-');
        if(type == E_TOGGLE)
        {
            cond->type = SCT_TOGGLE_STATE;
            if(!B_ParseToggleState(Str_Text(str), &cond->state))
            {
                return false;
            }
        }
        else if(type == E_AXIS)
        {
            cond->type = SCT_AXIS_BEYOND;
            if(!B_ParseAxisPosition(Str_Text(str), &cond->state, &cond->pos))
            {
                return false;
            }
        }
    }
    else if(!Str_CompareIgnoreCase(str, "joy") || !Str_CompareIgnoreCase(str, "head"))
    {
        cond->device = (!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER);

        // What is being targeted?
        desc = Str_CopyDelim(str, desc, '-');
        ddeventtype_t type;
        if(!B_ParseJoystickTypeAndId(inputSys().device(cond->device), Str_Text(str), &type, &cond->id))
        {
            return false;
        }

        desc = Str_CopyDelim(str, desc, '-');
        if(type == E_TOGGLE)
        {
            cond->type = SCT_TOGGLE_STATE;
            if(!B_ParseToggleState(Str_Text(str), &cond->state))
            {
                return false;
            }
        }
        else if(type == E_AXIS)
        {
            cond->type = SCT_AXIS_BEYOND;
            if(!B_ParseAxisPosition(Str_Text(str), &cond->state, &cond->pos))
            {
                return false;
            }
        }
        else // Angle.
        {
            cond->type = SCT_ANGLE_AT;
            if(!B_ParseAnglePosition(Str_Text(str), &cond->pos))
            {
                return false;
            }
        }
    }
    else
    {
        LOG_INPUT_WARNING("Unknown input device \"%s\"") << Str_Text(str);
        return false;
    }

    // Check for valid toggle states.
    if(cond->type == SCT_TOGGLE_STATE)
    {
        if(cond->state != EBTOG_UP && cond->state != EBTOG_DOWN)
        {
            LOG_INPUT_WARNING("\"%s\": Toggle condition can only be 'up' or 'down'") << desc;
            return false;
        }
    }

    // Finally, there may be the negation at the end.
    desc = Str_CopyDelim(str, desc, '-');
    if(!Str_CompareIgnoreCase(str, "not"))
    {
        cond->flags.negate = true;
    }

    // Anything left that wasn't used?
    if(!desc) return true; // No errors detected.

    LOG_INPUT_WARNING("Unrecognized condition \"%s\"") << desc;
    return false;
}

bool B_CheckAxisPos(ebstate_t test, float testPos, float pos)
{
    switch(test)
    {
    case EBAXIS_WITHIN:
        return !((pos > 0 && pos > testPos) || (pos < 0 && pos < -testPos));

    case EBAXIS_BEYOND:
        return ((pos > 0 && pos >= testPos) || (pos < 0 && pos <= -testPos));

    case EBAXIS_BEYOND_POSITIVE:
        return !(pos < testPos);

    case EBAXIS_BEYOND_NEGATIVE:
        return !(pos > -testPos);

    default: break;
    }

    return false;
}

bool B_CheckCondition(statecondition_t const *cond, int localNum, BindContext *context)
{
    DENG2_ASSERT(cond);
    bool const fulfilled      = !cond->flags.negate;
    InputDevice const &device = inputSys().device(cond->device);

    switch(cond->type)
    {
    case SCT_STATE:
        if(cond->flags.multiplayer && netGame)
            return fulfilled;
        break;

    case SCT_MODIFIER_STATE:
        if(context)
        {
            // Evaluate the current state of the modifier (in this context).
            float pos = 0, relative = 0;
            B_EvaluateImpulseBindings(context, localNum, cond->id, &pos, &relative, false /*no triggered*/);
            if((cond->state == EBTOG_DOWN && fabs(pos) > .5) ||
               (cond->state == EBTOG_UP && fabs(pos) < .5))
            {
                return fulfilled;
            }
        }
        break;

    case SCT_TOGGLE_STATE: {
        bool isDown = device.button(cond->id).isDown();
        if(( isDown && cond->state == EBTOG_DOWN) ||
           (!isDown && cond->state == EBTOG_UP))
        {
            return fulfilled;
        }
        break; }

    case SCT_AXIS_BEYOND:
        if(B_CheckAxisPos(cond->state, cond->pos, device.axis(cond->id).position()))
            return fulfilled;
        break;

    case SCT_ANGLE_AT:
        if(device.hat(cond->id).position() == cond->pos)
            return fulfilled;
        break;
    }

    return !fulfilled;
}

bool B_EqualConditions(statecondition_t const &a, statecondition_t const &b)
{
    return (a.device == b.device &&
            a.type   == b.type &&
            a.id     == b.id &&
            a.state  == b.state &&
            de::fequal(a.pos, b.pos) &&
            a.flags.negate      == b.flags.negate &&
            a.flags.multiplayer == b.flags.multiplayer);
}

/// @todo: Belongs in BindContext? -ds
void B_EvaluateImpulseBindings(BindContext *context, int localNum, int impulseId,
    float *pos, float *relativeOffset, bool allowTriggered)
{
    DENG2_ASSERT(context); // Why call without one?
    DENG2_ASSERT(pos && relativeOffset);

    *pos = 0;
    *relativeOffset = 0;

    uint const nowTime = Timer_RealMilliseconds();
    bool conflicted[NUM_IBD_TYPES]; de::zap(conflicted);
    bool appliedState[NUM_IBD_TYPES]; de::zap(appliedState);

    context->forAllImpulseBindings(localNum, [&] (ImpulseBinding &bind)
    {
        // Wrong impulse?
        if(bind.impulseId != impulseId) return LoopContinue;

        // If the binding has conditions, they may prevent using it.
        bool skip = false;
        for(statecondition_t const &cond : bind.conditions)
        {
            if(!B_CheckCondition(&cond, localNum, context))
            {
                skip = true;
                break;
            }
        }
        if(skip) return LoopContinue;

        // Get the device.
        InputDevice const *device = inputSys().devicePtr(bind.deviceId);
        if(!device || !device->isActive())
            return LoopContinue; // Not available.

        // Get the control.
        InputDeviceControl *ctrl = nullptr;
        switch(bind.type)
        {
        case IBD_AXIS:   ctrl = &device->axis(bind.controlId);   break;
        case IBD_TOGGLE: ctrl = &device->button(bind.controlId); break;
        case IBD_ANGLE:  ctrl = &device->hat(bind.controlId);    break;

        default: DENG2_ASSERT(!"B_EvaluateImpulseBindings: Invalid bind.type"); break;
        }

        float devicePos = 0;
        float deviceOffset = 0;
        uint deviceTime = 0;

        if(auto *axis = ctrl->maybeAs<InputDeviceAxisControl>())
        {
            if(context && axis->bindContext() != context)
            {
                if(axis->hasBindContext() && !axis->bindContext()->findImpulseBinding(bind.deviceId, IBD_AXIS, bind.controlId))
                {
                    // The overriding context doesn't bind to the axis, though.
                    if(axis->type() == InputDeviceAxisControl::Pointer)
                    {
                        // Reset the relative accumulation.
                        axis->setPosition(0);
                    }
                }
                return LoopContinue; // Shadowed by a more important active class.
            }

            // Expired?
            if(!(axis->bindContextAssociation() & InputDeviceControl::Expired))
            {
                if(axis->type() == InputDeviceAxisControl::Pointer)
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
        if(auto *button = ctrl->maybeAs<InputDeviceButtonControl>())
        {
            if(context && button->bindContext() != context)
                return LoopContinue; // Shadowed by a more important active context.

            // Expired?
            if(!(button->bindContextAssociation() & InputDeviceControl::Expired))
            {
                devicePos  = (button->isDown() ||
                              (allowTriggered && (button->bindContextAssociation() & InputDeviceControl::Triggered))? 1.0f : 0.0f);
                deviceTime = button->time();

                // We've checked it, so clear the flag.
                button->setBindContextAssociation(InputDeviceControl::Triggered, UnsetFlags);
            }
        }
        if(auto *hat = ctrl->maybeAs<InputDeviceHatControl>())
        {
            if(context && hat->bindContext() != context)
                return LoopContinue; // Shadowed by a more important active class.

            // Expired?
            if(!(hat->bindContextAssociation() & InputDeviceControl::Expired))
            {
                devicePos  = (hat->position() == bind.angle? 1.0f : 0.0f);
                deviceTime = hat->time();
            }
        }

        // Apply further modifications based on flags.
        if(bind.flags & IBDF_INVERSE)
        {
            devicePos    = -devicePos;
            deviceOffset = -deviceOffset;
        }
        if(bind.flags & IBDF_TIME_STAGED)
        {
            if(nowTime - deviceTime < STAGE_THRESHOLD * 1000)
            {
                devicePos *= STAGE_FACTOR;
            }
        }

        *pos += devicePos;
        *relativeOffset += deviceOffset;

        // Is this state contributing to the outcome?
        if(!de::fequal(devicePos, 0.f))
        {
            if(appliedState[bind.type])
            {
                // Another binding already influenced this; we have a conflict.
                conflicted[bind.type] = true;
            }

            // We've found one effective binding that influences this control.
            appliedState[bind.type] = true;
        }
        return LoopContinue;
    });

    if(zeroControlUponConflict)
    {
        for(int i = 0; i < NUM_IBD_TYPES; ++i)
        {
            if(conflicted[i])
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
    if(type != E_SYMBOLIC)
    {
        device = &inputSys().device(deviceId);
        // Name of the device.
        str += device->name() + "-";
    }

    switch(type)
    {
    case E_TOGGLE: {
        DENG2_ASSERT(device);
        InputDeviceButtonControl &button = device->button(id);
        if(!button.name().isEmpty())
        {
            str += button.name();
        }
        else if(device == inputSys().devicePtr(IDEV_KEYBOARD))
        {
            char const *name = B_ShortNameForKey(id);
            if(name)
            {
                str += name;
            }
            else
            {
                str += String("code%1").arg(id, 3, 10, QChar('0'));
            }
        }
        else
        {
            str += "button" + String::number(id + 1);
        }
        break; }

    case E_AXIS:
        DENG2_ASSERT(device);
        str += device->axis(id).name();
        break;

    case E_ANGLE:    str += "hat" + String::number(id + 1); break;
    case E_SYMBOLIC: str += "sym";                          break;

    default: DENG2_ASSERT(!"B_ControlDescToString: Invalid event type"); break;
    }

    return str;
}

String B_ToggleStateToString(ebstate_t state)
{
    switch(state)
    {
    case EBTOG_UNDEFINED: return "-undefined";
    case EBTOG_DOWN:      return "-down";
    case EBTOG_REPEAT:    return "-repeat";
    case EBTOG_PRESS:     return "-press";
    case EBTOG_UP:        return "-up";

    default: return "";
    }
}

String B_AxisPositionToString(ebstate_t state, float pos)
{
    switch(state)
    {
    case EBAXIS_WITHIN:          return String("-within%1").arg(pos);
    case EBAXIS_BEYOND:          return String("-beyond%1").arg(pos);
    case EBAXIS_BEYOND_POSITIVE: return String("-pos%1"   ).arg(pos);

    default: return String("-neg%1").arg(-pos);
    }
}

String B_AnglePositionToString(float pos)
{
    return (pos < 0? "-center" : String("-angle") + String::number(pos));
}

String B_StateConditionToString(statecondition_t const &cond)
{
    String str;

    if(cond.type == SCT_STATE)
    {
        if(cond.flags.multiplayer)
        {
            str += "multiplayer";
        }
    }
    else if(cond.type == SCT_MODIFIER_STATE)
    {
        str += "modifier-" + String::number(cond.id - CTL_MODIFIER_1 + 1);
    }
    else
    {
        str += B_ControlDescToString(cond.device,
                                     (  cond.type == SCT_TOGGLE_STATE? E_TOGGLE
                                      : cond.type == SCT_AXIS_BEYOND ? E_AXIS
                                      : E_ANGLE), cond.id);
    }

    if(cond.type == SCT_TOGGLE_STATE || cond.type == SCT_MODIFIER_STATE)
    {
        str += B_ToggleStateToString(cond.state);
    }
    else if(cond.type == SCT_AXIS_BEYOND)
    {
        str += B_AxisPositionToString(cond.state, cond.pos);
    }
    else if(cond.type == SCT_ANGLE_AT)
    {
        str += B_AnglePositionToString(cond.pos);
    }

    // Flags.
    if(cond.flags.negate)
    {
        str += "-not";
    }

    return str;
}

String B_EventToString(ddevent_t const &ev)
{
    String str = B_ControlDescToString(ev.device, ev.type,
                                       (  ev.type == E_TOGGLE  ? ev.toggle.id
                                        : ev.type == E_AXIS    ? ev.axis.id
                                        : ev.type == E_ANGLE   ? ev.angle.id
                                        : ev.type == E_SYMBOLIC? ev.symbolic.id
                                        : 0));

    switch(ev.type)
    {
    case E_TOGGLE:
        str += B_ToggleStateToString(  ev.toggle.state == ETOG_DOWN? EBTOG_DOWN
                                     : ev.toggle.state == ETOG_UP  ? EBTOG_UP
                                     : EBTOG_REPEAT);
        break;

    case E_AXIS:
        str += B_AxisPositionToString((ev.axis.pos >= 0? EBAXIS_BEYOND_POSITIVE : EBAXIS_BEYOND_NEGATIVE),
                                      ev.axis.pos);
        break;

    case E_ANGLE:    str += B_AnglePositionToString(ev.angle.pos); break;
    case E_SYMBOLIC: str += "-" + String(ev.symbolic.name);        break;

    default: break;
    }

    return str;
}

struct keyname_t
{
    int key;           ///< DDKEY
    char const *name;
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
    { 0, nullptr}
};

char const *B_ShortNameForKey(int ddKey, bool forceLowercase)
{
    static char nameBuffer[40];

    for(uint idx = 0; keyNames[idx].key; ++idx)
    {
        if(ddKey == keyNames[idx].key)
            return keyNames[idx].name;
    }

    if(isalnum(ddKey))
    {
        // Printable character, fabricate a single-character name.
        nameBuffer[0] = forceLowercase? tolower(ddKey) : ddKey;
        nameBuffer[1] = 0;
        return nameBuffer;
    }

    return nullptr;
}

int B_KeyForShortName(char const *key)
{
    DENG2_ASSERT(key);

    for(uint idx = 0; keyNames[idx].key; ++idx)
    {
        if(!qstricmp(key, keyNames[idx].name))
            return keyNames[idx].key;
    }

    if(qstrlen(key) == 1 && isalnum(key[0]))
    {
        // ASCII char.
        return tolower(key[0]);
    }

    return 0;
}
