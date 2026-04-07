/**
 * @file sys_input.h
 * Keyboard and mouse input pre-processing. @ingroup input
 *
 * @see joystick.h
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_SYSTEM_INPUT_H
#define DE_SYSTEM_INPUT_H

#ifndef __CLIENT__
#  error "Input requires __CLIENT__"
#endif

#include <de/legacy/types.h>
#include "joystick.h"

#ifdef __cplusplus
extern "C" {
#endif

// Key event types.
enum {
    IKE_NONE,
    IKE_DOWN,
    IKE_UP,
    IKE_REPEAT
};

// Mouse buttons.
enum {
    IMB_LEFT,
    IMB_MIDDLE,
    IMB_RIGHT,
    IMB_MWHEELUP,           // virtual button
    IMB_MWHEELDOWN,         // virtual button
    IMB_EXTRA1,
    IMB_EXTRA2,
    // ...other buttons...
    IMB_MWHEELLEFT = 14,    // virtual button
    IMB_MWHEELRIGHT = 15,   // virtual button

    IMB_MAXBUTTONS = 16
};

// Mouse axes.
enum {
    IMA_POINTER,
    IMA_WHEEL,
    IMA_MAXAXES
};

typedef struct keyevent_s {
    byte type;          ///< Type of the event.
    int ddkey;          ///< DDKEY code.
    int native;         ///< Native code (use this to check for physically equivalent keys).
    char text[8];       ///< For characters, latin1-encoded text to insert. /// @todo Unicode
} keyevent_t;

typedef struct mousestate_s {
    struct {
        int x;
        int y;
    } axis[IMA_MAXAXES];                ///< Relative X and Y.
    int buttonDowns[IMB_MAXBUTTONS];    ///< Button down count.
    int buttonUps[IMB_MAXBUTTONS];      ///< Button up count.
} mousestate_t;

typedef struct mouseinterface_s {
    int (*init)(void);      ///< Initialize the mouse.
    void (*shutdown)(void);
    void (*poll)(void);     ///< Polls the current state of the mouse.
    void (*getState)(mousestate_t*);
    void (*trap)(dd_bool);  ///< Enable or disable mouse grabbing.
} mouseinterface_t;

void I_Register(void);

/**
 * Initialize input.
 *
 * @return @c true, if successful.
 */
dd_bool I_InitInterfaces(void);

void I_ShutdownInterfaces(void);

/**
 * Submits a new key event for preprocessing. The event has likely just been
 * received from the windowing system.
 *
 * @param type    Type of the event (IKE_*).
 * @param ddKey   DDKEY code.
 * @param native  Native code. Identifies the physical key.
 * @param text    For characters, latin1-encoded text to insert. Otherwise @c NULL.
 */
void Keyboard_Submit(int type, int ddKey, int native, const char* text);

size_t Keyboard_GetEvents(keyevent_t *evbuf, size_t bufsize);

//dd_bool Mouse_IsPresent(void);

//void Mouse_Trap(dd_bool enabled);

/*
 * Polls the current state of the mouse. This is called at regular intervals.
 */
//void Mouse_Poll(void);

//void Mouse_GetState(mousestate_t *state);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
