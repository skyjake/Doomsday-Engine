/** @file b_device.cpp  Input system, control => device binding.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2014 Daniel Swanson <danij@dengine.net>
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

#include "ui/b_device.h"

#include <de/memory.h>
#include <de/timer.h>
#include "clientapp.h"
#include "dd_main.h"
#include "ui/b_main.h"
#include "ui/b_context.h"
#include "ui/inputdevice.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"

using namespace de;

#define EVTYPE_TO_CBDTYPE(evt)  ((evt) == E_AXIS? CBD_AXIS : (evt) == E_TOGGLE? CBD_TOGGLE : CBD_ANGLE)
#define CBDTYPE_TO_EVTYPE(cbt)  ((cbt) == CBD_AXIS? E_AXIS : (cbt) == CBD_TOGGLE? E_TOGGLE : E_ANGLE)

byte zeroControlUponConflict = true;

static float stageThreshold = 6.f/35;
static float stageFactor    = .5f;

static inline InputSystem &inputSys()
{
    return ClientApp::inputSystem();
}

static dbinding_t *B_AllocDeviceBinding()
{
    dbinding_t *cb = (dbinding_t *) M_Calloc(sizeof(*cb));
    cb->bid = B_NewIdentifier();
    return cb;
}

/**
 * Allocates a device state condition within a device binding.
 *
 * @return  Pointer to the new condition, which should be filled with the condition parameters.
 */
static statecondition_t *B_AllocDeviceBindingCondition(dbinding_t *b)
{
    b->conds = (statecondition_t *) M_Realloc(b->conds, ++b->numConds * sizeof(*b->conds));
    de::zap(b->conds[b->numConds - 1]);
    return &b->conds[b->numConds - 1];
}

void B_InitDeviceBindingList(dbinding_t *listRoot)
{
    DENG2_ASSERT(listRoot);
    de::zapPtr(listRoot);
    listRoot->next = listRoot->prev = listRoot;
}

void B_DestroyDeviceBindingList(dbinding_t *listRoot)
{
    if(!listRoot) return;
    while(listRoot->next != listRoot)
    {
        B_DestroyDeviceBinding(listRoot->next);
    }
}

dd_bool B_ParseDevice(dbinding_t *cb, char const *desc)
{
    DENG2_ASSERT(cb && desc);
    LOG_AS("B_ParseEvent");

    AutoStr *str = AutoStr_NewStd();
    ddeventtype_t type;

    // First, the device name.
    desc = Str_CopyDelim(str, desc, '-');
    if(!Str_CompareIgnoreCase(str, "key"))
    {
        cb->device = IDEV_KEYBOARD;
        cb->type   = CBD_TOGGLE;

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
    else if(!Str_CompareIgnoreCase(str, "joy") ||
            !Str_CompareIgnoreCase(str, "head"))
    {
        cb->device = (!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER);

        // Next part defined button, axis, or hat.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseJoystickTypeAndId(inputSys().device(cb->device), Str_Text(str), &type, &cb->id))
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
            LOG_INPUT_WARNING("Unrecognized \"%s\"") << desc;
            return false;
        }
    }

    return true;
}

dd_bool B_ParseDeviceDescriptor(dbinding_t *cb, char const *desc)
{
    DENG2_ASSERT(cb && desc);
    AutoStr *str = AutoStr_NewStd();

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
        // A new condition.
        desc = Str_CopyDelim(str, desc, '+');

        statecondition_t *cond = B_AllocDeviceBindingCondition(cb);
        if(!B_ParseStateCondition(cond, Str_Text(str)))
        {
            // Failure parusing the condition.
            return false;
        }
    }

    // Success.
    return true;
}

dbinding_t *B_NewDeviceBinding(dbinding_t *listRoot, char const *deviceDesc)
{
    DENG2_ASSERT(listRoot);
    dbinding_t *cb = B_AllocDeviceBinding();

    // Parse the description of the event.
    if(!B_ParseDeviceDescriptor(cb, deviceDesc))
    {
        // Error in parsing, failure to create binding.
        B_DestroyDeviceBinding(cb);
        return nullptr;
    }

    // Link it into the list.
    cb->next = listRoot;
    cb->prev = listRoot->prev;
    listRoot->prev->next = cb;
    listRoot->prev = cb;

    return cb;
}

dbinding_t *B_FindDeviceBinding(bcontext_t *context, uint device, cbdevtype_t bindType, int id)
{
    if(!context) return nullptr;

    for(controlbinding_t *cb = context->controlBinds.next; cb != &context->controlBinds; cb = cb->next)
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    for(dbinding_t *d = cb->deviceBinds[i].next; d != &cb->deviceBinds[i]; d = d->next)
    {
        if(d->device == device && d->type == bindType && d->id == id)
        {
            return d;
        }
    }

    return nullptr;
}

void B_DestroyDeviceBinding(dbinding_t *cb)
{
    if(!cb) return;
    DENG2_ASSERT(cb->bid != 0);

    // Unlink first, if linked.
    if(cb->prev)
    {
        cb->prev->next = cb->next;
        cb->next->prev = cb->prev;
    }

    M_Free(cb->conds);
    M_Free(cb);
}

void B_EvaluateDeviceBindingList(int localNum, dbinding_t *listRoot, float *pos,
    float *relativeOffset, bcontext_t *controlClass, dd_bool allowTriggered)
{
    DENG2_ASSERT(pos && relativeOffset);

    *pos = 0;
    *relativeOffset = 0;

    if(!listRoot) return;

    uint const nowTime = Timer_RealMilliseconds();
    dd_bool conflicted[NUM_CBD_TYPES]   = { false, false, false };
    dd_bool appliedState[NUM_CBD_TYPES] = { false, false, false };

    for(dbinding_t *cb = listRoot->next; cb != listRoot; cb = cb->next)
    {
        // If this binding has conditions, they may prevent using it.
        dd_bool skip = false;
        for(int i = 0; i < cb->numConds; ++i)
        {
            if(!B_CheckCondition(&cb->conds[i], localNum, controlClass))
            {
                skip = true;
                break;
            }
        }
        if(skip) continue;

        // Get the device.
        InputDevice *dev = inputSys().devicePtr(cb->device);
        if(!dev || !dev->isActive()) continue; // Not available.

        float devicePos = 0;
        float deviceOffset = 0;
        uint deviceTime = 0;

        switch(cb->type)
        {
        case CBD_TOGGLE: {
            InputDeviceButtonControl *button = &dev->button(cb->id);

            if(controlClass && button->bindContext() != controlClass)
                continue; // Shadowed by a more important active class.

            // Expired?
            if(button->bindContextAssociation() & InputDeviceControl::Expired)
                break;

            devicePos = (button->isDown() ||
                         (allowTriggered && (button->bindContextAssociation() & InputDeviceControl::Triggered))? 1.0f : 0.0f);
            deviceTime = button->time();

            // We've checked it, so clear the flag.
            button->setBindContextAssociation(InputDeviceControl::Triggered, UnsetFlags);
            break; }

        case CBD_AXIS: {
            InputDeviceAxisControl *axis = &dev->axis(cb->id);

            if(controlClass && axis->bindContext() != controlClass)
            {
                if(!B_FindDeviceBinding(axis->bindContext(), cb->device, CBD_AXIS, cb->id))
                {
                    // The overriding context doesn't bind to the axis, though.
                    if(axis->type() == InputDeviceAxisControl::Pointer)
                    {
                        // Reset the relative accumulation.
                        axis->setPosition(0);
                    }
                }
                continue; // Shadowed by a more important active class.
            }

            if(axis->bindContextAssociation() & InputDeviceControl::Expired)
                break;

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
            break; }

        case CBD_ANGLE: {
            InputDeviceHatControl *hat = &dev->hat(cb->id);

            if(controlClass && hat->bindContext() != controlClass)
                continue; // Shadowed by a more important active class.

            if(hat->bindContextAssociation() & InputDeviceControl::Expired)
                break;

            devicePos  = (hat->position() == cb->angle? 1.0f : 0.0f);
            deviceTime = hat->time();
            break; }

        default:
            App_Error("B_EvaluateDeviceBindingList: Invalid value cb->type: %i.", cb->type);
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
        if(!de::fequal(devicePos, 0.f))
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
        for(int i = 0; i < NUM_CBD_TYPES; ++i)
        {
            if(conflicted[i])
                *pos = 0;
        }
    }

    // Clamp to the normalized range.
    *pos = de::clamp(-1.0f, *pos, 1.0f);
}

void B_DeviceBindingToString(dbinding_t const *b, ddstring_t *str)
{
    DENG2_ASSERT(b && str);

    Str_Clear(str);

    // Name of the device and the key/axis/hat.
    B_AppendDeviceDescToString(inputSys().device(b->device), CBDTYPE_TO_EVTYPE(b->type), b->id, str);

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
    for(int i = 0; i < b->numConds; ++i)
    {
        Str_Append(str, " + ");
        B_AppendConditionToString(&b->conds[i], str);
    }
}
