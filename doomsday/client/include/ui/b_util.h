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

#include "dd_types.h"
#include "dd_input.h"

// Event Binding Toggle State
typedef enum ebstate_e {
    EBTOG_UNDEFINED,
    EBTOG_DOWN,
    EBTOG_REPEAT,
    EBTOG_PRESS,
    EBTOG_UP,
    EBAXIS_WITHIN,
    EBAXIS_BEYOND,
    EBAXIS_BEYOND_POSITIVE,
    EBAXIS_BEYOND_NEGATIVE
} ebstate_t;

typedef enum stateconditiontype_e {
    SCT_STATE,          ///< Related to the state of the engine.
    SCT_TOGGLE_STATE,   ///< Toggle is in a specific state.
    SCT_MODIFIER_STATE, ///< Modifier is in a specific state.
    SCT_AXIS_BEYOND,    ///< Axis is past a specific position.
    SCT_ANGLE_AT        ///< Angle is pointing to a specific direction.
} stateconditiontype_t;

// Device state condition.
typedef struct statecondition_s {
    uint device;                ///< Which device?
    stateconditiontype_t type;
    int id;                     ///< Toggle/axis/angle identifier in the device.
    ebstate_t state;
    float pos;                  ///< Axis position/angle condition.
    struct {
        uint negate:1;          ///< Test the inverse (e.g., not in a specific state).
        uint multiplayer:1;     ///< Only for multiplayer.
    } flags;
} statecondition_t;

dd_bool B_ParseToggleState(char const *toggleName, ebstate_t *state);

dd_bool B_ParseAxisPosition(char const *desc, ebstate_t *state, float *pos);

dd_bool B_ParseKeyId(char const *desc, int *id);

dd_bool B_ParseMouseTypeAndId(char const *desc, ddeventtype_t *type, int *id);

dd_bool B_ParseJoystickTypeAndId(int device, char const *desc, ddeventtype_t *type, int *id);

dd_bool B_ParseAnglePosition(char const *desc, float *pos);

dd_bool B_ParseStateCondition(statecondition_t *cond, char const *desc);

dd_bool B_CheckAxisPos(ebstate_t test, float testPos, float pos);

/**
 * @param cond      State condition to check.
 * @param localNum  Local player number.
 * @param context   Relevant binding context, if any (may be @c nullptr).
 */
dd_bool B_CheckCondition(statecondition_t *cond, int localNum, struct bcontext_s *context);

dd_bool B_EqualConditions(statecondition_t const *a, statecondition_t const *b);

void B_AppendDeviceDescToString(int device, ddeventtype_t type, int id, ddstring_t *str);

void B_AppendToggleStateToString(ebstate_t state, ddstring_t *str);

void B_AppendAxisPositionToString(ebstate_t state, float pos, ddstring_t *str);

void B_AppendAnglePositionToString(float pos, ddstring_t *str);

/**
 * Converts a state condition to text string format and appends it to a string.
 *
 * @param cond  State condition.
 * @param str   The condition in textual format is appended here.
 */
void B_AppendConditionToString(statecondition_t const *cond, ddstring_t *str);

/**
 * @param ev   Event.
 * @param str  The event in textual format is appended here.
 */
void B_AppendEventToString(ddevent_t const *ev, ddstring_t *str);

#endif // CLIENT_INPUTSYSTEM_BINDING_UTILITIES_H

