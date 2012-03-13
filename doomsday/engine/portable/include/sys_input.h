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

#ifdef __cplusplus
extern "C" {
#endif

// Key event types.
enum {
    IKE_NONE,
    IKE_DOWN,
    IKE_UP
};

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
    byte        type;       ///< Type of the event.
    int         ddkey;      ///< DDKEY code.
    char        text[8];    ///< For characters, latin1-encoded text to insert. /// @todo Unicode
} keyevent_t;

typedef struct mousestate_s {
    int         x, y; // Relative X and Y mickeys since last call.
    int         buttonDowns[IMB_MAXBUTTONS]; // Button down count.
    int         buttonUps[IMB_MAXBUTTONS]; // Button up count.
    int         wheel[IMW_NUM_AXES]; // Mouse wheel.
} mousestate_t;

void I_Register(void);
boolean I_Init(void);
void I_Shutdown(void);

/**
 * Submits a new key event for preprocessing. The event has likely just been
 * received from the windowing system.
 *
 * @param type   Type of the event (IKE_*).
 * @param ddKey  DDKEY code.
 * @param text   For characters, latin1-encoded text to insert. Otherwise @c NULL.
 */
void Keyboard_Submit(int type, int ddKey, const char* text);

size_t Keyboard_GetEvents(keyevent_t *evbuf, size_t bufsize);

boolean Mouse_IsPresent(void);
void Mouse_GetState(mousestate_t *state);

#ifdef __cplusplus
}
#endif

#endif
