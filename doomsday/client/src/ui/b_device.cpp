/** @file b_device.cpp  Input system, control => impulse binding.
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
#include "ui/bindcontext.h"
#include "ui/inputdevice.h"
#include "ui/inputdeviceaxiscontrol.h"
#include "ui/inputdevicebuttoncontrol.h"
#include "ui/inputdevicehatcontrol.h"

using namespace de;

#define EVTYPE_TO_IBDTYPE(evt)  ((evt) == E_AXIS? IBD_AXIS : (evt) == E_TOGGLE? IBD_TOGGLE : IBD_ANGLE)
#define IBDTYPE_TO_EVTYPE(cbt)  ((cbt) == IBD_AXIS? E_AXIS : (cbt) == IBD_TOGGLE? E_TOGGLE : E_ANGLE)

byte zeroControlUponConflict = true;

static float stageThreshold = 6.f/35;
static float stageFactor    = .5f;

static inline InputSystem &inputSys()
{
    return ClientApp::inputSystem();
}

static ImpulseBinding *B_AllocImpulseBinding()
{
    ImpulseBinding *cb = (ImpulseBinding *) M_Calloc(sizeof(*cb));
    cb->id = B_NewIdentifier();
    return cb;
}

/**
 * Allocates a control state condition within an impulse binding.
 *
 * @return  Pointer to the new condition, which should be filled with the condition parameters.
 */
static statecondition_t *B_AllocControlBindingCondition(ImpulseBinding *ib)
{
    ib->conds = (statecondition_t *) M_Realloc(ib->conds, ++ib->numConds * sizeof(*ib->conds));
    de::zap(ib->conds[ib->numConds - 1]);
    return &ib->conds[ib->numConds - 1];
}

void B_InitImpulseBindingList(ImpulseBinding *listRoot)
{
    DENG2_ASSERT(listRoot);
    de::zapPtr(listRoot);
    listRoot->next = listRoot->prev = listRoot;
}

void B_DestroyImpulseBindingList(ImpulseBinding *listRoot)
{
    if(!listRoot) return;
    while(listRoot->next != listRoot)
    {
        B_DestroyImpulseBinding(listRoot->next);
    }
}

static dd_bool parseControl(ImpulseBinding *ib, char const *desc)
{
    DENG2_ASSERT(ib && desc);

    AutoStr *str = AutoStr_NewStd();
    ddeventtype_t type;

    // First, the device name.
    desc = Str_CopyDelim(str, desc, '-');
    if(!Str_CompareIgnoreCase(str, "key"))
    {
        ib->deviceId = IDEV_KEYBOARD;
        ib->type   = IBD_TOGGLE;

        // Parse the key.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseKeyId(Str_Text(str), &ib->controlId))
        {
            return false;
        }
    }
    else if(!Str_CompareIgnoreCase(str, "mouse"))
    {
        ib->deviceId = IDEV_MOUSE;

        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseMouseTypeAndId(Str_Text(str), &type, &ib->controlId))
        {
            return false;
        }
        ib->type = EVTYPE_TO_IBDTYPE(type);
    }
    else if(!Str_CompareIgnoreCase(str, "joy") ||
            !Str_CompareIgnoreCase(str, "head"))
    {
        ib->deviceId = (!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER);

        // Next part defined button, axis, or hat.
        desc = Str_CopyDelim(str, desc, '-');
        if(!B_ParseJoystickTypeAndId(inputSys().device(ib->deviceId), Str_Text(str), &type, &ib->controlId))
        {
            return false;
        }
        ib->type = EVTYPE_TO_IBDTYPE(type);

        // Hats include the angle.
        if(type == E_ANGLE)
        {
            desc = Str_CopyDelim(str, desc, '-');
            if(!B_ParseAnglePosition(Str_Text(str), &ib->angle))
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
            ib->flags |= IBDF_INVERSE;
        }
        else if(!Str_CompareIgnoreCase(str, "staged"))
        {
            ib->flags |= IBDF_TIME_STAGED;
        }
        else
        {
            LOG_INPUT_WARNING("Unrecognized \"%s\"") << desc;
            return false;
        }
    }

    return true;
}

dd_bool B_ParseControlDescriptor(ImpulseBinding *ib, char const *desc)
{
    DENG2_ASSERT(ib && desc);
    LOG_AS("B_ParseControl");

    AutoStr *str = AutoStr_NewStd();

    // The first part specifies the device state.
    desc = Str_CopyDelim(str, desc, '+');

    if(!parseControl(ib, Str_Text(str)))
    {
        // Failure in parsing the device.
        return false;
    }

    // Any conditions?
    while(desc)
    {
        // A new condition.
        desc = Str_CopyDelim(str, desc, '+');

        statecondition_t *cond = B_AllocControlBindingCondition(ib);
        if(!B_ParseStateCondition(cond, Str_Text(str)))
        {
            // Failure parsing the condition.
            return false;
        }
    }

    // Success.
    return true;
}

ImpulseBinding *B_NewImpulseBinding(ImpulseBinding *listRoot, char const *ctrlDesc)
{
    DENG2_ASSERT(listRoot);
    ImpulseBinding *ib = B_AllocImpulseBinding();

    // Parse the description of the event.
    if(!B_ParseControlDescriptor(ib, ctrlDesc))
    {
        // Error in parsing, failure to create binding.
        B_DestroyImpulseBinding(ib);
        return nullptr;
    }

    // Link it into the list.
    ib->next = listRoot;
    ib->prev = listRoot->prev;
    listRoot->prev->next = ib;
    listRoot->prev = ib;

    return ib;
}

void B_DestroyImpulseBinding(ImpulseBinding *ib)
{
    if(!ib) return;
    DENG2_ASSERT(ib->id != 0);

    // Unlink first, if linked.
    if(ib->prev)
    {
        ib->prev->next = ib->next;
        ib->next->prev = ib->prev;
    }

    M_Free(ib->conds);
    M_Free(ib);
}

void B_EvaluateImpulseBindingList(int localNum, ImpulseBinding *listRoot, float *pos,
    float *relativeOffset, BindContext *context, dd_bool allowTriggered)
{
    DENG2_ASSERT(pos && relativeOffset);

    *pos = 0;
    *relativeOffset = 0;

    if(!listRoot) return;

    uint const nowTime = Timer_RealMilliseconds();
    dd_bool conflicted[NUM_CBD_TYPES]   = { false, false, false };
    dd_bool appliedState[NUM_CBD_TYPES] = { false, false, false };

    for(ImpulseBinding *ib = listRoot->next; ib != listRoot; ib = ib->next)
    {
        // If this binding has conditions, they may prevent using it.
        dd_bool skip = false;
        for(int i = 0; i < ib->numConds; ++i)
        {
            if(!B_CheckCondition(&ib->conds[i], localNum, context))
            {
                skip = true;
                break;
            }
        }
        if(skip) continue;

        // Get the device.
        InputDevice *dev = inputSys().devicePtr(ib->deviceId);
        if(!dev || !dev->isActive()) continue; // Not available.

        float devicePos = 0;
        float deviceOffset = 0;
        uint deviceTime = 0;

        switch(ib->type)
        {
        case IBD_TOGGLE: {
            InputDeviceButtonControl *button = &dev->button(ib->controlId);

            if(context && button->bindContext() != context)
                continue; // Shadowed by a more important active context.

            // Expired?
            if(button->bindContextAssociation() & InputDeviceControl::Expired)
                break;

            devicePos = (button->isDown() ||
                         (allowTriggered && (button->bindContextAssociation() & InputDeviceControl::Triggered))? 1.0f : 0.0f);
            deviceTime = button->time();

            // We've checked it, so clear the flag.
            button->setBindContextAssociation(InputDeviceControl::Triggered, UnsetFlags);
            break; }

        case IBD_AXIS: {
            InputDeviceAxisControl *axis = &dev->axis(ib->controlId);

            if(context && axis->bindContext() != context)
            {
                if(!axis->bindContext()->findImpulseBinding(ib->deviceId, IBD_AXIS, ib->controlId))
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

        case IBD_ANGLE: {
            InputDeviceHatControl *hat = &dev->hat(ib->controlId);

            if(context && hat->bindContext() != context)
                continue; // Shadowed by a more important active class.

            if(hat->bindContextAssociation() & InputDeviceControl::Expired)
                break;

            devicePos  = (hat->position() == ib->angle? 1.0f : 0.0f);
            deviceTime = hat->time();
            break; }

        default:
            DENG2_ASSERT(!"B_EvaluateImpulseBindingList: Invalid ib->type.");
            break;
        }

        // Apply further modifications based on flags.
        if(ib->flags & IBDF_INVERSE)
        {
            devicePos = -devicePos;
            deviceOffset = -deviceOffset;
        }
        if(ib->flags & IBDF_TIME_STAGED)
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
            if(appliedState[ib->type])
            {
                // Another binding already influenced this; we have a conflict.
                conflicted[ib->type] = true;
            }

            // We've found one effective binding that influences this control.
            appliedState[ib->type] = true;
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

void ImpulseBinding_ToString(ImpulseBinding const *ib, ddstring_t *str)
{
    DENG2_ASSERT(ib && str);

    Str_Clear(str);

    // Name of the control-device and the key/axis/hat.
    B_AppendControlDescToString(inputSys().device(ib->deviceId), IBDTYPE_TO_EVTYPE(ib->type), ib->controlId, str);

    if(ib->type == IBD_ANGLE)
    {
        B_AppendAnglePositionToString(ib->angle, str);
    }

    // Additional flags.
    if(ib->flags & IBDF_TIME_STAGED)
    {
        Str_Append(str, "-staged");
    }
    if(ib->flags & IBDF_INVERSE)
    {
        Str_Append(str, "-inverse");
    }

    // Append any state conditions.
    for(int i = 0; i < ib->numConds; ++i)
    {
        Str_Append(str, " + ");
        B_AppendConditionToString(&ib->conds[i], str);
    }
}
