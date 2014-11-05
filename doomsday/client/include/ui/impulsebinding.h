/** @file impulsebinding.h  Input system, control => impulse binding.
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

#ifndef CLIENT_INPUTSYSTEM_IMPULSEBINDING_H
#define CLIENT_INPUTSYSTEM_IMPULSEBINDING_H

#include "b_util.h"

class BindContext;

enum ibcontroltype_t
{
    IBD_TOGGLE = E_TOGGLE,
    IBD_AXIS   = E_AXIS,
    IBD_ANGLE  = E_ANGLE,
    NUM_CBD_TYPES
};

// Flags for impulse bindings.
#define IBDF_INVERSE        0x1
#define IBDF_TIME_STAGED    0x2

struct ImpulseBinding
{
    ImpulseBinding *next;
    ImpulseBinding *prev;

    int id;
    int deviceId;
    ibcontroltype_t type;
    int controlId;
    float angle;
    uint flags;

    int numConds;
    statecondition_t *conds;  ///< Additional conditions.
};

extern byte zeroControlUponConflict;

void B_DestroyImpulseBinding(ImpulseBinding *db);

void B_InitImpulseBindingList(ImpulseBinding *listRoot);

void B_DestroyImpulseBindingList(ImpulseBinding *listRoot);

ImpulseBinding *B_NewImpulseBinding(ImpulseBinding *listRoot, char const *ctrlDesc);

/**
 * Does the opposite of the B_Parse* methods for a device binding, including the
 * state conditions.
 */
void ImpulseBinding_ToString(ImpulseBinding const *ib, ddstring_t *str);

void B_EvaluateImpulseBindingList(int localNum, ImpulseBinding *listRoot, float *pos,
    float *relativeOffset, BindContext *context, dd_bool allowTriggered);

#endif // CLIENT_INPUTSYSTEM_IMPULSEBINDING_H

