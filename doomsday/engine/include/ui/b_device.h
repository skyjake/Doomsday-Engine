/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * b_device.h: Control-Device Bindings
 */

#ifndef __DOOMSDAY_BIND_DEVICE_H__
#define __DOOMSDAY_BIND_DEVICE_H__

#include "b_util.h"

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
    struct dbinding_s* next;
    struct dbinding_s* prev;

    int         bid;
    uint        device;
    cbdevtype_t type;
    int         id;
    float       angle;
    uint        flags;

    // Additional conditions.
    int         numConds;
    statecondition_t* conds;
} dbinding_t;

void        B_InitDeviceBindingList(dbinding_t* listRoot);
void        B_DestroyDeviceBindingList(dbinding_t* listRoot);
dbinding_t* B_NewDeviceBinding(dbinding_t* listRoot, const char* deviceDesc);
void        B_DestroyDeviceBinding(dbinding_t* cb);
void        B_DeviceBindingToString(const dbinding_t* b, ddstring_t* str);
void        B_EvaluateDeviceBindingList(int localNum, dbinding_t* listRoot,
                                        float* pos, float* relativeOffset,
                                        struct bcontext_s* controlClass,
                                        boolean allowTriggered);

#endif // __DOOMSDAY_BIND_DEVICE_H__

