/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

/**
 * Control-Device Bindings.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

#include "b_main.h"
#include "b_device.h"
#include "b_context.h"

// MACROS ------------------------------------------------------------------

#define EVTYPE_TO_CBDTYPE(evt)  ((evt) == E_AXIS? CBD_AXIS : (evt) == E_TOGGLE? CBD_TOGGLE : CBD_ANGLE)
#define CBDTYPE_TO_EVTYPE(cbt)  ((cbt) == CBD_AXIS? E_AXIS : (cbt) == CBD_TOGGLE? E_TOGGLE : E_ANGLE)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float       stageThreshold = 6.f/35;
float       stageFactor = .5f;
byte        zeroControlUponConflict = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static dbinding_t* B_AllocDeviceBinding(void)
{
    dbinding_t* cb = M_Calloc(sizeof(dbinding_t));
    cb->bid = B_NewIdentifier();
    return cb;
}

/**
 * Allocates a device state condition within a device binding.
 *
 * @return  Pointer to the new condition, which should be filled with the condition parameters.
 */
static statecondition_t* B_AllocDeviceBindingCondition(dbinding_t* b)
{
    b->conds = M_Realloc(b->conds, ++b->numConds * sizeof(statecondition_t));
    memset(&b->conds[b->numConds - 1], 0, sizeof(statecondition_t));
    return &b->conds[b->numConds - 1];
}

void B_InitDeviceBindingList(dbinding_t* listRoot)
{
    memset(listRoot, 0, sizeof(*listRoot));
    listRoot->next = listRoot->prev = listRoot;
}

void B_DestroyDeviceBindingList(dbinding_t* listRoot)
{
    while(listRoot->next != listRoot)
    {
        B_DestroyDeviceBinding(listRoot->next);
    }
}

boolean B_ParseDevice(dbinding_t* cb, const char* desc)
{
    AutoStr* str = AutoStr_New();
    ddeventtype_t type;

    // First, the device name.
    desc = Str_CopyDelim(str, desc, '-');
    if(!Str_CompareIgnoreCase(str, "key"))
    {
        cb->device = IDEV_KEYBOARD;
        cb->type = CBD_TOGGLE;

        // Parse the key.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseKeyId(Str_Text(str), &cb->id))
        {
            return false;
        }
    }
    else if(!Str_CompareIgnoreCase(str, "mouse"))
    {
        cb->device = IDEV_MOUSE;

        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseMouseTypeAndId(Str_Text(str), &type, &cb->id))
        {
            return false;
        }
        cb->type = EVTYPE_TO_CBDTYPE(type);
    }
    else if(!Str_CompareIgnoreCase(str, "joy"))
    {
        cb->device = IDEV_JOY1;

        // Next part defined button, axis, or hat.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseJoystickTypeAndId(cb->device, Str_Text(str), &type, &cb->id))
        {
            return false;
        }
        cb->type = EVTYPE_TO_CBDTYPE(type);

        // Hats include the angle.
        if(type == E_ANGLE)
        {
            desc = Str_CopyDelim(str, desc, '-');
            if(!B_ParseAnglePosition(Str_Text(str), &cb->angle))
            {
                return false;
            }
        }
    }

    // Finally, there may be some flags at the end.
    while(desc)
    {
        desc = Str_CopyDelim(str, desc, '-');
        if(!Str_CompareIgnoreCase(str, "inverse"))
        {
            cb->flags |= CBDF_INVERSE;
        }
        else if(!Str_CompareIgnoreCase(str, "staged"))
        {
            cb->flags |= CBDF_TIME_STAGED;
        }
        else
        {
            Con_Message("B_ParseEvent: Unrecognized \"%s\".\n", desc);
            return false;
        }
    }

    return true;
}

boolean B_ParseDeviceDescriptor(dbinding_t* cb, const char* desc)
{
    AutoStr* str = AutoStr_New();

    // The first part specifies the device state.
    desc = Str_CopyDelim(str, desc, '+');

    if(!B_ParseDevice(cb, Str_Text(str)))
    {
        // Failure in parsing the device.
        return false;
    }

    // Any conditions?
    while(desc)
    {
        statecondition_t *cond;

        // A new condition.
        desc = Str_CopyDelim(str, desc, '+');

        cond = B_AllocDeviceBindingCondition(cb);
        if(!B_ParseStateCondition(cond, Str_Text(str)))
        {
            // Failure parusing the condition.
            return false;
        }
    }

    // Success.
    return true;
}

dbinding_t* B_NewDeviceBinding(dbinding_t* listRoot, const char* deviceDesc)
{
    dbinding_t* cb = B_AllocDeviceBinding();

    // Parse the description of the event.
    if(!B_ParseDeviceDescriptor(cb, deviceDesc))
    {
        // Error in parsing, failure to create binding.
        B_DestroyDeviceBinding(cb);
        return NULL;
    }

    // Link it into the list.
    cb->next = listRoot;
    cb->prev = listRoot->prev;
    listRoot->prev->next = cb;
    listRoot->prev = cb;

    return cb;
}

dbinding_t* B_FindDeviceBinding(bcontext_t* context, uint device, cbdevtype_t bindType, int id)
{
    controlbinding_t*   cb;
    dbinding_t*         d;
    int                 i;

    if(!context)
        return NULL;

    for(cb = context->controlBinds.next; cb != &context->controlBinds; cb = cb->next)
    {
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            for(d = cb->deviceBinds[i].next; d != &cb->deviceBinds[i]; d = d->next)
            {
                if(d->device == device && d->type == bindType && d->id == id)
                {
                    return d;
                }
            }
        }
    }
    return NULL;
}

void B_DestroyDeviceBinding(dbinding_t* cb)
{
    if(cb)
    {
        assert(cb->bid != 0);

        // Unlink first, if linked.
        if(cb->prev)
        {
            cb->prev->next = cb->next;
            cb->next->prev = cb->prev;
        }

        if(cb->conds)
            M_Free(cb->conds);
        M_Free(cb);
    }
}

void B_EvaluateDeviceBindingList(int localNum, dbinding_t* listRoot, float* pos, float* relativeOffset,
                                 bcontext_t* controlClass, boolean allowTriggered)
{
    dbinding_t* cb;
    int         i;
    boolean     skip;
    inputdev_t* dev;
    inputdevaxis_t* axis;
    float       devicePos;
    float       deviceOffset;
    uint        deviceTime;
    uint        nowTime = Sys_GetRealTime();
    boolean     conflicted[NUM_CBD_TYPES] = { false, false, false };
    boolean     appliedState[NUM_CBD_TYPES] = { false, false, false };

    *pos = 0;
    *relativeOffset = 0;

    if(!listRoot)
        return;

    for(cb = listRoot->next; cb != listRoot; cb = cb->next)
    {
        // If this binding has conditions, they may prevent using it.
        skip = false;
        for(i = 0; i < cb->numConds; ++i)
        {
            if(!B_CheckCondition(&cb->conds[i], localNum, controlClass))
            {
                skip = true;
                break;
            }
        }
        if(skip)
            continue;

        // Get the device.
        dev = I_GetDevice(cb->device, true);
        if(!dev)
            continue; // Not available.

        devicePos = 0;
        deviceOffset = 0;
        deviceTime = 0;

        switch(cb->type)
        {
        case CBD_TOGGLE:
            if(controlClass && dev->keys[cb->id].assoc.bContext != controlClass)
                continue; // Shadowed by a more important active class.

            // Expired?
            if(dev->keys[cb->id].assoc.flags & IDAF_EXPIRED)
                break;

            devicePos = (dev->keys[cb->id].isDown ||
                         (allowTriggered && (dev->keys[cb->id].assoc.flags & IDAF_TRIGGERED))? 1.0f : 0.0f);
            deviceTime = dev->keys[cb->id].time;

            // We've checked it, so clear the flag.
            dev->keys[cb->id].assoc.flags &= ~IDAF_TRIGGERED;
            break;

        case CBD_AXIS:
            axis = &dev->axes[cb->id];
            if(controlClass && axis->assoc.bContext != controlClass)
            {
                if(!B_FindDeviceBinding(axis->assoc.bContext, cb->device, CBD_AXIS, cb->id))
                {
                    // The overriding context doesn't bind to the axis, though.
                    if(axis->type == IDAT_POINTER)
                    {
                        // Reset the relative accumulation.
                        axis->position = 0;
                    }
                }
                continue; // Shadowed by a more important active class.
            }

            // Expired?
            if(axis->assoc.flags & IDAF_EXPIRED)
                break;

            if(axis->type == IDAT_POINTER)
            {
                deviceOffset = axis->position;
                axis->position = 0;
            }
            else
            {
                devicePos = axis->position;
            }
            deviceTime = axis->time;
            break;

        case CBD_ANGLE:
            if(controlClass && dev->hats[cb->id].assoc.bContext != controlClass)
                continue; // Shadowed by a more important active class.

            if(dev->hats[cb->id].assoc.flags & IDAF_EXPIRED)
                break;

            devicePos = (dev->hats[cb->id].pos == cb->angle? 1.0f : 0.0f);
            deviceTime = dev->hats[cb->id].time;
            break;

        default:
            Con_Error("B_EvaluateDeviceBindingList: Invalid value, cb->type = %i.", cb->type);
            break;
        }

        // Apply further modifications based on flags.
        if(cb->flags & CBDF_INVERSE)
        {
            devicePos = -devicePos;
            deviceOffset = -deviceOffset;
        }
        if(cb->flags & CBDF_TIME_STAGED)
        {
            if(nowTime - deviceTime < stageThreshold * 1000)
            {
                devicePos *= stageFactor;
            }
        }

        *pos += devicePos;
        *relativeOffset += deviceOffset;

        // Is this state contributing to the outcome?
        if(!FEQUAL(devicePos, 0.f))
        {
            if(appliedState[cb->type])
            {
                // Another binding already influenced this; we have a conflict.
                conflicted[cb->type] = true;
            }

            // We've found one effective binding that influences this control.
            appliedState[cb->type] = true;
        }
    }

    if(zeroControlUponConflict)
    {
        for(i = 0; i < NUM_CBD_TYPES; ++i)
            if(conflicted[i])
                *pos = 0;
    }

    // Clamp to the normalized range.
    *pos = MINMAX_OF(-1.0f, *pos, 1.0f);
}

/**
 * Does the opposite of the B_Parse* methods for a device binding, including the
 * state conditions.
 */
void B_DeviceBindingToString(const dbinding_t* b, ddstring_t* str)
{
    int         i;

    Str_Clear(str);

    // Name of the device and the key/axis/hat.
    B_AppendDeviceDescToString(b->device, CBDTYPE_TO_EVTYPE(b->type), b->id, str);

    if(b->type == CBD_ANGLE)
    {
        B_AppendAnglePositionToString(b->angle, str);
    }

    // Additional flags.
    if(b->flags & CBDF_TIME_STAGED)
    {
        Str_Append(str, "-staged");
    }
    if(b->flags & CBDF_INVERSE)
    {
        Str_Append(str, "-inverse");
    }

    // Append any state conditions.
    for(i = 0; i < b->numConds; ++i)
    {
        Str_Append(str, " + ");
        B_AppendConditionToString(&b->conds[i], str);
    }
}
