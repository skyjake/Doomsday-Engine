/** @file b_device.h  Input system, control => device binding.
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

#ifndef CLIENT_INPUTSYSTEM_DEVICEBINDING_H
#define CLIENT_INPUTSYSTEM_DEVICEBINDING_H

#include "b_util.h"

struct bcontext_t;

typedef enum cbdevtype_e {
    CBD_TOGGLE = E_TOGGLE,
    CBD_AXIS   = E_AXIS,
    CBD_ANGLE  = E_ANGLE,
    NUM_CBD_TYPES
} cbdevtype_t;

// Flags for control-device bindings.
#define CBDF_INVERSE        0x1
#define CBDF_TIME_STAGED    0x2

typedef struct dbinding_s {
    struct dbinding_s *next;
    struct dbinding_s *prev;

    int bid;
    int device;
    cbdevtype_t type;
    int id;
    float angle;
    uint flags;

    int numConds;
    statecondition_t *conds;  ///< Additional conditions.
} dbinding_t;

extern byte zeroControlUponConflict;

void B_InitDeviceBindingList(dbinding_t *listRoot);

void B_DestroyDeviceBindingList(dbinding_t *listRoot);

dbinding_t *B_NewDeviceBinding(dbinding_t *listRoot, char const *deviceDesc);

void B_DestroyDeviceBinding(dbinding_t *cb);

/**
 * Does the opposite of the B_Parse* methods for a device binding, including the
 * state conditions.
 */
void B_DeviceBindingToString(dbinding_t const *b, ddstring_t *str);

void B_EvaluateDeviceBindingList(int localNum, dbinding_t *listRoot, float *pos,
    float *relativeOffset, bcontext_t *controlClass, dd_bool allowTriggered);

#endif // CLIENT_INPUTSYSTEM_DEVICEBINDING_H

