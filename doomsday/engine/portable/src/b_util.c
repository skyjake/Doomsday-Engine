/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * b_util.c: Bindings-related Utility Functions
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_misc.h"

#include "b_main.h"
#include "b_util.h"

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
        *pos = strtod(desc + 3, NULL);
    }
    else
    {
        Con_Message("B_ParseAxisPosition: Axis position \"%s\" is invalid.\n", desc);
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
    if(!strcasecmp(desc, "left"))
    {
        *type = E_TOGGLE;
        *id = 0;
    }
    else if(!strcasecmp(desc, "middle"))
    {
        *type = E_TOGGLE;
        *id = 1;
    }
    else if(!strcasecmp(desc, "right"))
    {
        *type = E_TOGGLE;
        *id = 2;
    }
    else if(!strcasecmp(desc, "wheelup"))
    {
        *type = E_TOGGLE;
        *id = 3;
    }
    else if(!strcasecmp(desc, "wheeldown"))
    {
        *type = E_TOGGLE;
        *id = 4;
    }
    else if(!strncasecmp(desc, "button", 6) && strlen(desc) > 6) // generic button
    {
        *type = E_TOGGLE;
        *id = strtoul(desc + 6, NULL, 10) - 1;
        if(*id < 0 || *id >= I_GetDevice(IDEV_MOUSE, false)->numKeys)
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
        if(*id < 0 || *id >= I_GetDevice(device, false)->numKeys)
        {
            Con_Message("B_ParseJoystickTypeAndId: Button %i does not exist in joystick.\n", *id);
            return false;
        }
    }
    else if(!strncasecmp(desc, "hat", 3) && strlen(desc) > 3) 
    {
        *type = E_ANGLE;
        *id = strtoul(desc + 3, NULL, 10) - 1;
        if(*id < 0 || *id >= I_GetDevice(device, false)->numHats)
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
    boolean successful = false;
    ddstring_t* str = Str_New();
    ddeventtype_t type;        
    
    // First, we expect to encounter a device name.
    desc = Str_CopyDelim(str, desc, '-');    
    
    if(!Str_CompareIgnoreCase(str, "key"))
    {
        cond->device = IDEV_KEYBOARD;
        cond->type = SCT_TOGGLE_STATE;
        
        // Parse the key.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseKeyId(Str_Text(str), &cond->id))
        {
            goto parseEnded;
        }
        
        // The final part of a key event is the state of the key toggle.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseToggleState(Str_Text(str), &cond->state))
        {
            goto parseEnded;
        }
    }
    else if(!Str_CompareIgnoreCase(str, "mouse"))
    {
        cond->device = IDEV_MOUSE;
        
        // What is being targeted?
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseMouseTypeAndId(Str_Text(str), &type, &cond->id))
        {
            goto parseEnded;
        }
        
        desc = Str_CopyDelim(str, desc, '-');
        if(type == E_TOGGLE)
        {
            cond->type = SCT_TOGGLE_STATE;
            if(!B_ParseToggleState(Str_Text(str), &cond->state))
            {
                goto parseEnded;
            }
        }
        else if(type == E_AXIS)
        {
            cond->type = SCT_AXIS_BEYOND;
            if(!B_ParseAxisPosition(Str_Text(str), &cond->state, &cond->pos))
            {
                goto parseEnded;
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
            goto parseEnded;
        }
        
        desc = Str_CopyDelim(str, desc, '-');
        if(type == E_TOGGLE)
        {
            cond->type = SCT_TOGGLE_STATE;
            if(!B_ParseToggleState(Str_Text(str), &cond->state))
            {
                goto parseEnded;
            }
        }
        else if(type == E_AXIS)
        {
            cond->type = SCT_AXIS_BEYOND;
            if(!B_ParseAxisPosition(Str_Text(str), &cond->state, &cond->pos))
            {
                goto parseEnded;
            }
        }
        else // Angle.
        {
            cond->type = SCT_ANGLE_AT;
            if(!B_ParseAnglePosition(Str_Text(str), &cond->pos))
            {
                goto parseEnded;
            }
        }
    }
    else
    {
        Con_Message("B_ParseEvent: Device \"%s\" unknown.\n", Str_Text(str));
        goto parseEnded;
    }        
    
    // Check for valid toggle states.
    if(cond->type == SCT_TOGGLE_STATE && 
       cond->state != EBTOG_UP && cond->state != EBTOG_DOWN)
    {
        Con_Message("B_ParseStateCondition: \"%s\": Toggle condition can only be 'up' or 'down'.\n",
                    desc);
        goto parseEnded;
    }
    
    // Finally, there may be the negation at the end.
    desc = Str_CopyDelim(str, desc, '-');
    if(!Str_CompareIgnoreCase(str, "not"))
    {
        cond->negate = true;
    }
    
    // Anything left that wasn't used?
    if(desc)
    {
        Con_Message("B_ParseStateCondition: Unrecognized \"%s\".\n", desc);
        goto parseEnded;
    }
    
    // No errors detected.
    successful = true;
    
parseEnded:    
    Str_Free(str);
    return successful;
}

boolean B_CheckAxisPos(ebstate_t test, float testPos, float pos)
{
    switch(test)
    {
        case EBAXIS_WITHIN:
            if(pos > 0 && pos > testPos || pos < 0 && pos < -testPos)
                return false;
            break;
            
        case EBAXIS_BEYOND:
            if(!(pos > 0 && pos >= testPos || pos < 0 && pos <= -testPos))
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

boolean B_CheckCondition(statecondition_t* cond)
{
    boolean fulfilled = !cond->negate;
    inputdev_t* dev = I_GetDevice(cond->device, false);
    
    if(cond->type == SCT_TOGGLE_STATE)
    {
        int isDown = (dev->keys[cond->id].isDown != 0);
        if(isDown && cond->state == EBTOG_DOWN ||
           !isDown && cond->state == EBTOG_UP)
            return fulfilled;
    }
    if(cond->type == SCT_AXIS_BEYOND)
    {
        if(B_CheckAxisPos(cond->state, cond->pos, dev->axes[cond->id].position))
            return fulfilled;
    }
    if(cond->type == SCT_ANGLE_AT)
    {
        if(dev->hats[cond->id].pos == cond->pos)
            return fulfilled;
    }
    return !fulfilled;
}
