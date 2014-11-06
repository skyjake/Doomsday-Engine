/** @file b_util.h  Input system, binding utilities.
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

#ifndef CLIENT_INPUTSYSTEM_BINDING_UTILITIES_H
#define CLIENT_INPUTSYSTEM_BINDING_UTILITIES_H

#include <QList>
#include "dd_types.h"
#include "dd_input.h"

class BindContext;
class InputDevice;
struct ImpulseBinding;

// Event Binding Toggle State
enum ebstate_t
{
    EBTOG_UNDEFINED,
    EBTOG_DOWN,
    EBTOG_REPEAT,
    EBTOG_PRESS,
    EBTOG_UP,
    EBAXIS_WITHIN,
    EBAXIS_BEYOND,
    EBAXIS_BEYOND_POSITIVE,
    EBAXIS_BEYOND_NEGATIVE
};

enum stateconditiontype_t
{
    SCT_STATE,          ///< Related to the state of the engine.
    SCT_TOGGLE_STATE,   ///< Toggle is in a specific state.
    SCT_MODIFIER_STATE, ///< Modifier is in a specific state.
    SCT_AXIS_BEYOND,    ///< Axis is past a specific position.
    SCT_ANGLE_AT        ///< Angle is pointing to a specific direction.
};

// Device state condition.
struct statecondition_t
{
    uint device;                ///< Which device?
    stateconditiontype_t type;
    int id;                     ///< device-control or impulse ID.
    ebstate_t state;
    float pos;                  ///< Axis position/angle condition.
    struct {
        uint negate:1;          ///< Test the inverse (e.g., not in a specific state).
        uint multiplayer:1;     ///< Only for multiplayer.
    } flags;
};

bool B_ParseToggleState(char const *toggleName, ebstate_t *state);

bool B_ParseAxisPosition(char const *desc, ebstate_t *state, float *pos);

bool B_ParseKeyId(char const *desc, int *id);

bool B_ParseMouseTypeAndId(char const *desc, ddeventtype_t *type, int *id);

bool B_ParseJoystickTypeAndId(InputDevice const &device, char const *desc, ddeventtype_t *type, int *id);

bool B_ParseAnglePosition(char const *desc, float *pos);

bool B_ParseStateCondition(statecondition_t *cond, char const *desc);

// ---------------------------------------------------------------------------------

de::String B_ControlDescToString(InputDevice const &device, ddeventtype_t type, int id);

de::String B_ToggleStateToString(ebstate_t state);

de::String B_AxisPositionToString(ebstate_t state, float pos);

de::String B_AnglePositionToString(float pos);

de::String B_StateConditionToString(statecondition_t const &cond);

de::String B_EventToString(ddevent_t const &ev);

// ---------------------------------------------------------------------------------

extern byte zeroControlUponConflict;

bool B_CheckAxisPos(ebstate_t test, float testPos, float pos);

/**
 * @param cond      State condition to check.
 * @param localNum  Local player number.
 * @param context   Relevant binding context, if any (may be @c nullptr).
 */
bool B_CheckCondition(statecondition_t const *cond, int localNum, BindContext *context);

bool B_EqualConditions(statecondition_t const &a, statecondition_t const &b);

void B_EvaluateImpulseBindings(BindContext *context, int localNum, int impulseId,
    float *pos, float *relativeOffset, bool allowTriggered);

char const *B_ShortNameForKey(int ddKey, bool forceLowercase = true);

int B_KeyForShortName(char const *key);

/**
 * @return  Never returns zero, as that is reserved for list roots.
 */
int B_NewIdentifier();

void B_ResetIdentifiers();

#endif // CLIENT_INPUTSYSTEM_BINDING_UTILITIES_H

