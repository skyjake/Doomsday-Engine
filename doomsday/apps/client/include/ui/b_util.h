/** @file b_util.h  Input system, binding utilities.
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

#ifndef CLIENT_INPUTSYSTEM_BINDING_UTILITIES_H
#define CLIENT_INPUTSYSTEM_BINDING_UTILITIES_H

#include <de/record.h>
#include "dd_types.h"
#include "ddevent.h"
#include "ui/binding.h"

class BindContext;

bool B_ParseAxisPosition(Binding::ControlTest &test, float &pos, const char *desc);

bool B_ParseButtonState(Binding::ControlTest &test, const char *desc);

bool B_ParseHatAngle(float &angle, const char *desc);

bool B_ParseBindingCondition(de::Record &cond, const char *desc);

// ---

de::String B_AxisPositionToString(Binding::ControlTest test, float pos);

de::String B_ButtonStateToString(Binding::ControlTest test);

de::String B_HatAngleToString(float angle);

de::String B_ConditionToString(const de::Record &cond);

de::String B_EventToString(const ddevent_t &ev);

// ---

bool B_CheckAxisPosition(Binding::ControlTest test, float testPos, float pos);

/**
 * @param cond      State condition to check.
 * @param localNum  Local player number.
 * @param context   Relevant binding context, if any (may be @c nullptr).
 */
bool B_CheckCondition(const Binding::CompiledConditionRecord *cond, int localNum, const BindContext *context);

// ---------------------------------------------------------------------------------

extern byte zeroControlUponConflict;

bool B_ParseKeyId(int &id, const char *desc);

bool B_ParseMouseTypeAndId(ddeventtype_t &type, int &id, const char *desc);

bool B_ParseJoystickTypeAndId(ddeventtype_t &type, int &id, int deviceId, const char *desc);

de::String B_ControlDescToString(int deviceId, ddeventtype_t type, int id);

void B_EvaluateImpulseBindings(const BindContext *context, int localNum, int impulseId,
    float *pos, float *relativeOffset, bool allowTriggered);

const char *B_ShortNameForKey(int ddKey, bool forceLowercase = true);

int B_KeyForShortName(const char *key);

#endif // CLIENT_INPUTSYSTEM_BINDING_UTILITIES_H

