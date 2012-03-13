/**
 * @file sys_input.h
 * Keyboard and mouse input pre-processing. @ingroup input
 *
 * @see joystick.h
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_SYSTEM_INPUT_H
#define LIBDENG_SYSTEM_INPUT_H

#include "joystick.h"

// Key event types.
#define IKE_NONE        0
#define IKE_KEY_DOWN    0x1
#define IKE_KEY_UP      0x2

// Mouse buttons. (1=left, 2=middle, 3=right, ...)
#define IMB_MAXBUTTONS  16

// Mouse wheel axes.
enum {
    IMW_UP,
    IMW_DOWN,
    IMW_LEFT,
    IMW_RIGHT,
    IMW_NUM_AXES
};

typedef struct keyevent_s {
    char            event; // Type of the event.
    byte            ddkey; // The scancode (extended, corresponds DD_KEYs).
} keyevent_t;

typedef struct mousestate_s {
    int             x, y; // Relative X and Y mickeys since last call.
    int             buttonDowns[IMB_MAXBUTTONS]; // Button down count.
    int             buttonUps[IMB_MAXBUTTONS]; // Button up count.
    int             wheel[IMW_NUM_AXES]; // Mouse wheel.
} mousestate_t;

void            I_Register(void);
boolean         I_Init(void);
void            I_Shutdown(void);

size_t          Keyboard_GetEvents(keyevent_t *evbuf, size_t bufsize);

boolean         Mouse_IsPresent(void);
void            Mouse_GetState(mousestate_t *state);

#endif
