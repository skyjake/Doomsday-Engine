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

#include <cmath>

#include "de_platform.h"
#include "de_console.h"
#include "dd_main.h"
#include "de_misc.h"

#include "ui/b_util.h"

#include "ui/b_main.h"
#include "ui/b_context.h"
#include "network/net_main.h" // netGame

dd_bool B_ParseToggleState(char const *toggleName, ebstate_t *state)
{
    DENG2_ASSERT(toggleName && state);

    if(!strlen(toggleName) || !strcasecmp(toggleName, "down"))
    {
        *state = EBTOG_DOWN; // this is the default, if omitted
        return true;
    }
    if(!strcasecmp(toggleName, "undefined"))
    {
        *state = EBTOG_UNDEFINED;
        return true;
    }
    if(!strcasecmp(toggleName, "repeat"))
    {
        *state = EBTOG_REPEAT;
        return true;
    }
    if(!strcasecmp(toggleName, "press"))
    {
        *state = EBTOG_PRESS;
        return true;
    }
    if(!strcasecmp(toggleName, "up"))
    {
        *state = EBTOG_UP;
        return true;
    }

    LOG_INPUT_WARNING("\"%s\" is not a toggle state") << toggleName;
    return false; // Not recognized.
}

dd_bool B_ParseAxisPosition(char const *desc, ebstate_t *state, float *pos)
{
    DENG2_ASSERT(desc && state && pos);

    if(!strncasecmp(desc, "within", 6) && strlen(desc) > 6)
    {
        *state = EBAXIS_WITHIN;
        *pos = strtod(desc + 6, nullptr);
        return true;
    }
    if(!strncasecmp(desc, "beyond", 6) && strlen(desc) > 6)
    {
        *state = EBAXIS_BEYOND;
        *pos = strtod(desc + 6, nullptr);
        return true;
    }
    if(!strncasecmp(desc, "pos", 3) && strlen(desc) > 3)
    {
        *state = EBAXIS_BEYOND_POSITIVE;
        *pos = strtod(desc + 3, nullptr);
        return true;
    }
    if(!strncasecmp(desc, "neg", 3) && strlen(desc) > 3)
    {
        *state = EBAXIS_BEYOND_NEGATIVE;
        *pos = -strtod(desc + 3, nullptr);
        return true;
    }

    LOG_INPUT_WARNING("Axis position \"%s\" is invalid") << desc;
    return false;
}

dd_bool B_ParseModifierId(char const *desc, int *id)
{
    DENG2_ASSERT(desc && id);
    *id = strtoul(desc, nullptr, 10) - 1 + CTL_MODIFIER_1;
    return (*id >= CTL_MODIFIER_1 && *id <= CTL_MODIFIER_4);
}

dd_bool B_ParseKeyId(char const *desc, int *id)
{
    DENG2_ASSERT(desc && id);
    LOG_AS("B_ParseKeyId");

    // The possibilies: symbolic key name, or "codeNNN".
    if(!strncasecmp(desc, "code", 4) && strlen(desc) == 7)
    {
        // Hexadecimal?
        if(desc[4] == 'x' || desc[4] == 'X')
        {
            *id = strtoul(desc + 5, nullptr, 16);
            return true;
        }

        // Decimal.
        *id = strtoul(desc + 4, nullptr, 10);
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

dd_bool B_ParseMouseTypeAndId(char const *desc, ddeventtype_t *type, int *id)
{
    DENG2_ASSERT(desc && type && id);

    // Maybe it's one of the named buttons?
    *id = I_GetKeyByName(I_GetDevice(IDEV_MOUSE), desc);
    if(*id >= 0)
    {
        *type = E_TOGGLE;
        return true;
    }

    // Perhaps a generic button?
    if(!strncasecmp(desc, "button", 6) && strlen(desc) > 6)
    {
        *type = E_TOGGLE;
        *id   = strtoul(desc + 6, nullptr, 10) - 1;
        if(*id >= 0 && uint(*id) < I_GetDevice(IDEV_MOUSE)->numKeys)
            return true;

        LOG_INPUT_WARNING("Mouse button %i does not exist") << *id;
        return false;
    }

    // Must be an axis, then.
    *type = E_AXIS;
    *id   = I_GetAxisByName(I_GetDevice(IDEV_MOUSE), desc);
    if(*id >= 0) return true;

    LOG_INPUT_WARNING("Mouse axis \"%s\" does not exist") << desc;
    return false;
}

dd_bool B_ParseDeviceAxisTypeAndId(uint device, char const *desc, ddeventtype_t *type, int *id)
{
    DENG2_ASSERT(desc && type && id);
    inputdev_t *dev = I_GetDevice(device);

    *type = E_AXIS;
    *id   = I_GetAxisByName(dev, desc);
    if(*id >= 0) return true;

    LOG_INPUT_WARNING("Axis \"%s\" is not defined in device '%s'") << desc << dev->name;
    return false;
}

dd_bool B_ParseJoystickTypeAndId(uint device, char const *desc, ddeventtype_t *type, int *id)
{
    if(!strncasecmp(desc, "button", 6) && strlen(desc) > 6)
    {
        *type = E_TOGGLE;
        *id   = strtoul(desc + 6, nullptr, 10) - 1;
        if(*id >= 0 && uint(*id) < I_GetDevice(device)->numKeys)
            return true;

        LOG_INPUT_WARNING("Joystick button %i does not exist") << *id;
        return false;
    }
    if(!strncasecmp(desc, "hat", 3) && strlen(desc) > 3)
    {
        *type = E_ANGLE;
        *id   = strtoul(desc + 3, nullptr, 10) - 1;
        if(*id >= 0 && uint(*id) < I_GetDevice(device)->numHats)
            return true;

        LOG_INPUT_WARNING("Joystick hat %i does not exist") << *id;
        return false;
    }
    if(!strcasecmp(desc, "hat"))
    {
        *type = E_ANGLE;
        *id   = 0;
        return true;
    }

    // Try to find the axis.
    return B_ParseDeviceAxisTypeAndId(device, desc, type, id);
}

dd_bool B_ParseAnglePosition(char const *desc, float *pos)
{
    DENG2_ASSERT(desc && pos);
    if(!strcasecmp(desc, "center"))
    {
        *pos = -1;
        return true;
    }
    if(!strncasecmp(desc, "angle", 5) && strlen(desc) > 5)
    {
        *pos = strtod(desc + 5, nullptr);
        return true;
    }
    LOG_INPUT_WARNING("Angle position \"%s\" is invalid") << desc;
    return false;
}

dd_bool B_ParseStateCondition(statecondition_t *cond, char const *desc)
{
    AutoStr *str = AutoStr_NewStd();

    // First, we expect to encounter a device name.
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
        if(!B_ParseJoystickTypeAndId(cond->device, Str_Text(str), &type, &cond->id))
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

dd_bool B_CheckAxisPos(ebstate_t test, float testPos, float pos)
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

dd_bool B_CheckCondition(statecondition_t *cond, int localNum, bcontext_t *context)
{
    DENG2_ASSERT(cond);
    dd_bool const fulfilled = !cond->flags.negate;

    inputdev_t *dev = I_GetDevice(cond->device);
    DENG2_ASSERT(dev);

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
            dbinding_t *binds = &B_GetControlBinding(context, cond->id)->deviceBinds[localNum];
            B_EvaluateDeviceBindingList(localNum, binds, &pos, &relative, context, false /*no triggered*/);
            if((cond->state == EBTOG_DOWN && fabs(pos) > .5) ||
               (cond->state == EBTOG_UP && fabs(pos) < .5))
            {
                return fulfilled;
            }
        }
        break;

    case SCT_TOGGLE_STATE: {
        int isDown = (dev->keys[cond->id].isDown != 0);
        if(( isDown && cond->state == EBTOG_DOWN) ||
           (!isDown && cond->state == EBTOG_UP))
        {
            return fulfilled;
        }
        break; }

    case SCT_AXIS_BEYOND:
        if(B_CheckAxisPos(cond->state, cond->pos, dev->axes[cond->id].position))
            return fulfilled;
        break;

    case SCT_ANGLE_AT:
        if(dev->hats[cond->id].pos == cond->pos)
            return fulfilled;
        break;
    }

    return !fulfilled;
}

dd_bool B_EqualConditions(statecondition_t const *a, statecondition_t const *b)
{
    DENG2_ASSERT(a && b);
    return (a->device == b->device &&
            a->type   == b->type &&
            a->id     == b->id &&
            a->state  == b->state &&
            de::fequal(a->pos, b->pos) &&
            a->flags.negate      == b->flags.negate &&
            a->flags.multiplayer == b->flags.multiplayer);
}

void B_AppendDeviceDescToString(uint device, ddeventtype_t type, int id, ddstring_t *str)
{
    inputdev_t *dev = I_GetDevice(device);
    DENG2_ASSERT(dev);

    if(type != E_SYMBOLIC)
    {
        // Name of the device.
        Str_Append(str, dev->name);
        Str_Append(str, "-");
    }

    switch(type)
    {
    case E_TOGGLE:
        if(dev->keys[id].name)
        {
            Str_Append(str, dev->keys[id].name);
        }
        else if(device == IDEV_KEYBOARD)
        {
            char const *name = B_ShortNameForKey(id);
            if(name)
                Str_Append(str, name);
            else
                Str_Appendf(str, "code%03i", id);
        }
        else
        {
            Str_Appendf(str, "button%i",id + 1);
        }
        break;

    case E_AXIS:
        Str_Append(str, dev->axes[id].name);
        break;

    case E_ANGLE:
        Str_Appendf(str, "hat%i", id + 1);
        break;

    case E_SYMBOLIC:
        Str_Append(str, "sym");
        break;

    default:
        App_Error("B_AppendDeviceDescToString: Invalid value type: %i.", (int) type);
        break;
    }
}

void B_AppendToggleStateToString(ebstate_t state, ddstring_t *str)
{
    switch(state)
    {
    case EBTOG_UNDEFINED: Str_Append(str, "-undefined"); break;
    case EBTOG_DOWN:      Str_Append(str, "-down");      break;
    case EBTOG_REPEAT:    Str_Append(str, "-repeat");    break;
    case EBTOG_PRESS:     Str_Append(str, "-press");     break;
    case EBTOG_UP:        Str_Append(str, "-up");        break;

    default: break;
    }
}

void B_AppendAxisPositionToString(ebstate_t state, float pos, ddstring_t *str)
{
    switch(state)
    {
    case EBAXIS_WITHIN:          Str_Appendf(str, "-within%g", pos); break;
    case EBAXIS_BEYOND:          Str_Appendf(str, "-beyond%g", pos); break;
    case EBAXIS_BEYOND_POSITIVE: Str_Appendf(str, "-pos%g",    pos); break;

    default: Str_Appendf(str, "-neg%g", -pos); break;
    }
}

void B_AppendAnglePositionToString(float pos, ddstring_t *str)
{
    if(pos < 0)
        Str_Append(str, "-center");
    else
        Str_Appendf(str, "-angle%g", pos);
}

void B_AppendConditionToString(statecondition_t const *cond, ddstring_t *str)
{
    DENG2_ASSERT(cond);

    if(cond->type == SCT_STATE)
    {
        if(cond->flags.multiplayer)
        {
            Str_Append(str, "multiplayer");
        }
    }
    else if(cond->type == SCT_MODIFIER_STATE)
    {
        Str_Appendf(str, "modifier-%i", cond->id - CTL_MODIFIER_1 + 1);
    }
    else
    {
        B_AppendDeviceDescToString(cond->device,
                                   (  cond->type == SCT_TOGGLE_STATE? E_TOGGLE
                                    : cond->type == SCT_AXIS_BEYOND ? E_AXIS
                                    : E_ANGLE),
                                   cond->id, str);
    }

    if(cond->type == SCT_TOGGLE_STATE || cond->type == SCT_MODIFIER_STATE)
    {
        B_AppendToggleStateToString(cond->state, str);
    }
    else if(cond->type == SCT_AXIS_BEYOND)
    {
        B_AppendAxisPositionToString(cond->state, cond->pos, str);
    }
    else if(cond->type == SCT_ANGLE_AT)
    {
        B_AppendAnglePositionToString(cond->pos, str);
    }

    // Flags.
    if(cond->flags.negate)
    {
        Str_Append(str, "-not");
    }
}

void B_AppendEventToString(ddevent_t const *ev, ddstring_t *str)
{
    DENG2_ASSERT(ev);

    B_AppendDeviceDescToString(ev->device, ev->type,
                               (  ev->type == E_TOGGLE  ? ev->toggle.id
                                : ev->type == E_AXIS    ? ev->axis.id
                                : ev->type == E_ANGLE   ? ev->angle.id
                                : ev->type == E_SYMBOLIC? ev->symbolic.id
                                : 0), str);

    switch(ev->type)
    {
    case E_TOGGLE:
        B_AppendToggleStateToString((  ev->toggle.state == ETOG_DOWN? EBTOG_DOWN
                                     : ev->toggle.state == ETOG_UP  ? EBTOG_UP
                                     : EBTOG_REPEAT), str);
        break;

    case E_AXIS:
        B_AppendAxisPositionToString((ev->axis.pos >= 0? EBAXIS_BEYOND_POSITIVE : EBAXIS_BEYOND_NEGATIVE),
                                     ev->axis.pos, str);
        break;

    case E_ANGLE:
        B_AppendAnglePositionToString(ev->angle.pos, str);
        break;

    case E_SYMBOLIC:
        Str_Appendf(str, "-%s", ev->symbolic.name);
        break;

    default: break;
    }
}
