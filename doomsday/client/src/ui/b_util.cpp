/**\file
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Bindings-related Utility Functions.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_platform.h"
#include "de_console.h"
#include "de_misc.h"

#include "ui/b_main.h"
#include "ui/b_util.h"
#include "ui/b_context.h"

#include "network/net_main.h" // netGame

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

boolean B_ParseToggleState(const char* toggleName, ebstate_t* state)
{
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

    Con_Message("B_ParseToggleState: \"%s\" is not a toggle state.\n", toggleName);
    return false; // Not recognized.
}

boolean B_ParseAxisPosition(const char* desc, ebstate_t* state, float* pos)
{
    if(!strncasecmp(desc, "within", 6) && strlen(desc) > 6)
    {
        *state = EBAXIS_WITHIN;
        *pos = strtod(desc + 6, NULL);
    }
    else if(!strncasecmp(desc, "beyond", 6) && strlen(desc) > 6)
    {
        *state = EBAXIS_BEYOND;
        *pos = strtod(desc + 6, NULL);
    }
    else if(!strncasecmp(desc, "pos", 3) && strlen(desc) > 3)
    {
        *state = EBAXIS_BEYOND_POSITIVE;
        *pos = strtod(desc + 3, NULL);
    }
    else if(!strncasecmp(desc, "neg", 3) && strlen(desc) > 3)
    {
        *state = EBAXIS_BEYOND_NEGATIVE;
        *pos = -strtod(desc + 3, NULL);
    }
    else
    {
        Con_Message("B_ParseAxisPosition: Axis position \"%s\" is invalid.\n", desc);
        return false;
    }
    return true;
}

boolean B_ParseModifierId(const char* desc, int* id)
{
    *id = strtoul(desc, NULL, 10) - 1 + CTL_MODIFIER_1;
    if(*id < CTL_MODIFIER_1 || *id > CTL_MODIFIER_4)
    {
        // Out of range.
        return false;
    }
    return true;
}

boolean B_ParseKeyId(const char* desc, int* id)
{
    // The possibilies: symbolic key name, or "codeNNN".
    if(!strncasecmp(desc, "code", 4) && strlen(desc) == 7)
    {
        if(desc[4] == 'x' || desc[4] == 'X')
        {
            // Hexadecimal.
            *id = strtoul(desc + 5, NULL, 16);
        }
        else
        {
            // Decimal.
            *id = strtoul(desc + 4, NULL, 10);
            if(*id <= 0 || *id > 255)
            {
                Con_Message("B_ParseKeyId: Key code %i out of range.\n", *id);
                return false;
            }
        }
    }
    else
    {
        // Symbolic key name.
        *id = B_KeyForShortName(desc);
        if(!*id)
        {
            Con_Message("B_ParseKeyId: Unknown key \"%s\".\n", desc);
            return false;
        }
    }
    return true;
}

boolean B_ParseMouseTypeAndId(const char* desc, ddeventtype_t* type, int* id)
{
    // Maybe it's one of the buttons?
    *id = I_GetKeyByName(I_GetDevice(IDEV_MOUSE, false), desc);
    if(*id >= 0)
    {
        // Got it.
        *type = E_TOGGLE;
        return true;
    }

    if(!strncasecmp(desc, "button", 6) && strlen(desc) > 6) // generic button
    {
        *type = E_TOGGLE;
        *id = strtoul(desc + 6, NULL, 10) - 1;
        if(*id < 0 || (uint)*id >= I_GetDevice(IDEV_MOUSE, false)->numKeys)
        {
            Con_Message("B_ParseMouseTypeAndId: Button %i does not exist.\n", *id);
            return false;
        }
    }
    else
    {
        // Try to find the axis.
        *type = E_AXIS;
        *id = I_GetAxisByName(I_GetDevice(IDEV_MOUSE, false), desc);
        if(*id < 0)
        {
            Con_Message("B_ParseMouseTypeAndId: Axis \"%s\" is not defined.\n", desc);
            return false;
        }
    }
    return true;
}

boolean B_ParseJoystickTypeAndId(uint device, const char* desc, ddeventtype_t* type, int* id)
{
    if(!strncasecmp(desc, "button", 6) && strlen(desc) > 6)
    {
        *type = E_TOGGLE;
        *id = strtoul(desc + 6, NULL, 10) - 1;
        if(*id < 0 || (uint)*id >= I_GetDevice(device, false)->numKeys)
        {
            Con_Message("B_ParseJoystickTypeAndId: Button %i does not exist in joystick.\n", *id);
            return false;
        }
    }
    else if(!strncasecmp(desc, "hat", 3) && strlen(desc) > 3)
    {
        *type = E_ANGLE;
        *id = strtoul(desc + 3, NULL, 10) - 1;
        if(*id < 0 || (uint)*id >= I_GetDevice(device, false)->numHats)
        {
            Con_Message("B_ParseJoystickTypeAndId: Hat %i does not exist in joystick.\n", *id);
            return false;
        }
    }
    else if(!strcasecmp(desc, "hat"))
    {
        *type = E_ANGLE;
        *id = 0;
    }
    else
    {
        // Try to find the axis.
        *type = E_AXIS;
        *id = I_GetAxisByName(I_GetDevice(device, false), desc);
        if(*id < 0)
        {
            Con_Message("B_ParseJoystickTypeAndId: Axis \"%s\" is not defined in joystick.\n", desc);
            return false;
        }
    }
    return true;
}

boolean B_ParseAnglePosition(const char* desc, float* pos)
{
    if(!strcasecmp(desc, "center"))
    {
        *pos = -1;
    }
    else if(!strncasecmp(desc, "angle", 5) && strlen(desc) > 5)
    {
        *pos = strtod(desc + 5, NULL);
    }
    else
    {
        Con_Message("B_ParseAnglePosition: Angle position \"%s\" invalid.\n", desc);
        return false;
    }
    return true;
}

/**
 * Parse a state condition.
 */
boolean B_ParseStateCondition(statecondition_t* cond, const char* desc)
{
    AutoStr* str = AutoStr_NewStd();
    ddeventtype_t type;

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
    else if(!Str_CompareIgnoreCase(str, "joy"))
    {
        cond->device = IDEV_JOY1;

        // What is being targeted?
        desc = Str_CopyDelim(str, desc, '-');
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
        Con_Message("B_ParseEvent: Device \"%s\" unknown.\n", Str_Text(str));
        return false;
    }

    // Check for valid toggle states.
    if(cond->type == SCT_TOGGLE_STATE &&
       cond->state != EBTOG_UP && cond->state != EBTOG_DOWN)
    {
        Con_Message("B_ParseStateCondition: \"%s\": Toggle condition can only be 'up' or 'down'.\n",
                    desc);
        return false;
    }

    // Finally, there may be the negation at the end.
    desc = Str_CopyDelim(str, desc, '-');
    if(!Str_CompareIgnoreCase(str, "not"))
    {
        cond->flags.negate = true;
    }

    // Anything left that wasn't used?
    if(desc)
    {
        Con_Message("B_ParseStateCondition: Unrecognized \"%s\".\n", desc);
        return false;
    }

    // No errors detected.
    return true;
}

boolean B_CheckAxisPos(ebstate_t test, float testPos, float pos)
{
    switch(test)
    {
    case EBAXIS_WITHIN:
        if((pos > 0 && pos > testPos) || (pos < 0 && pos < -testPos))
            return false;
        break;

    case EBAXIS_BEYOND:
        if(!((pos > 0 && pos >= testPos) || (pos < 0 && pos <= -testPos)))
            return false;
        break;

    case EBAXIS_BEYOND_POSITIVE:
        if(pos < testPos)
            return false;
        break;

    case EBAXIS_BEYOND_NEGATIVE:
        if(pos > -testPos)
            return false;
        break;

    default:
        return false;
    }
    return true;
}

boolean B_CheckCondition(statecondition_t* cond, int localNum, bcontext_t* context)
{
    boolean fulfilled = !cond->flags.negate;
    inputdev_t* dev = I_GetDevice(cond->device, false);

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
            dbinding_t* binds = &B_GetControlBinding(context, cond->id)->deviceBinds[localNum];
            B_EvaluateDeviceBindingList(localNum, binds, &pos, &relative, context, false /*no triggered*/);
            if((cond->state == EBTOG_DOWN && fabs(pos) > .5) ||
               (cond->state == EBTOG_UP && fabs(pos) < .5))
                return fulfilled;
        }
        break;

    case SCT_TOGGLE_STATE:
    {
        int isDown = (dev->keys[cond->id].isDown != 0);
        if((isDown && cond->state == EBTOG_DOWN) ||
           (!isDown && cond->state == EBTOG_UP))
            return fulfilled;
        break;
    }

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

boolean B_EqualConditions(const statecondition_t* a, const statecondition_t* b)
{
    return (a->device == b->device &&
            a->type == b->type &&
            a->id == b->id &&
            a->state == b->state &&
            FEQUAL(a->pos, b->pos) &&
            a->flags.negate == b->flags.negate &&
            a->flags.multiplayer == b->flags.multiplayer);
}

void B_AppendDeviceDescToString(uint device, ddeventtype_t type, int id, ddstring_t* str)
{
    inputdev_t* dev = I_GetDevice(device, false);
    const char* name;

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
            name = B_ShortNameForKey(id);
            if(name)
                Str_Append(str, name);
            else
                Str_Appendf(str, "code%03i", id);
        }
        else
            Str_Appendf(str, "button%i",id + 1);
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
        Con_Error("B_AppendDeviceDescToString: Invalid value, type = %i.",
                  (int) type);
        break;
    }
}

void B_AppendToggleStateToString(ebstate_t state, ddstring_t* str)
{
    if(state == EBTOG_UNDEFINED)
        Str_Append(str, "-undefined");
    if(state == EBTOG_DOWN)
        Str_Append(str, "-down");
    if(state == EBTOG_REPEAT)
        Str_Append(str, "-repeat");
    if(state == EBTOG_PRESS)
        Str_Append(str, "-press");
    if(state == EBTOG_UP)
        Str_Append(str, "-up");
}

void B_AppendAxisPositionToString(ebstate_t state, float pos, ddstring_t* str)
{
    if(state == EBAXIS_WITHIN)
        Str_Appendf(str, "-within%g", pos);
    else if(state == EBAXIS_BEYOND)
        Str_Appendf(str, "-beyond%g", pos);
    else if(state == EBAXIS_BEYOND_POSITIVE)
        Str_Appendf(str, "-pos%g", pos);
    else
        Str_Appendf(str, "-neg%g", -pos);
}

void B_AppendAnglePositionToString(float pos, ddstring_t* str)
{
    if(pos < 0)
        Str_Append(str, "-center");
    else
        Str_Appendf(str, "-angle%g", pos);
}

void B_AppendConditionToString(const statecondition_t* cond, ddstring_t* str)
{
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
                                   cond->type == SCT_TOGGLE_STATE? E_TOGGLE :
                                   cond->type == SCT_AXIS_BEYOND? E_AXIS : E_ANGLE,
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

/**
 * @param ev  Event.
 * @param str  The event in textual format is appended here.
 */
void B_AppendEventToString(const ddevent_t* ev, ddstring_t* str)
{
    B_AppendDeviceDescToString(ev->device, ev->type,
                               ev->type == E_TOGGLE? ev->toggle.id :
                               ev->type == E_AXIS? ev->axis.id :
                               ev->type == E_ANGLE? ev->angle.id :
                               ev->type == E_SYMBOLIC? ev->symbolic.id :
                               0, str);

    switch(ev->type)
    {
    case E_TOGGLE:
        B_AppendToggleStateToString(ev->toggle.state == ETOG_DOWN? EBTOG_DOWN :
                                    ev->toggle.state == ETOG_UP? EBTOG_UP :
                                    EBTOG_REPEAT, str);
        break;

    case E_AXIS:
        B_AppendAxisPositionToString(ev->axis.pos >= 0? EBAXIS_BEYOND_POSITIVE :
                                     EBAXIS_BEYOND_NEGATIVE, ev->axis.pos, str);
        break;

    case E_ANGLE:
        B_AppendAnglePositionToString(ev->angle.pos, str);
        break;

    case E_SYMBOLIC:
        Str_Appendf(str, "-%s", ev->symbolic.name);
        break;

    case E_FOCUS:
        break;
    }
}
