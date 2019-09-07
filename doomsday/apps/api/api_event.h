/** @file api_event.h  Public API for input events and bindings.
 * @ingroup input
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_EVENT_H
#define DOOMSDAY_API_EVENT_H

#include "api_base.h"

/// Event types. @ingroup input
typedef enum {
    EV_KEY,
    EV_MOUSE_AXIS,
    EV_MOUSE_BUTTON,
    EV_JOY_AXIS,        ///< Joystick main axes (xyz + Rxyz).
    EV_JOY_SLIDER,      ///< Joystick sliders.
    EV_JOY_BUTTON,
    EV_POV,
    EV_SYMBOLIC,        ///< Symbol text pointed to by data_u64 (data1+data2).
    EV_FOCUS,           ///< Change in game window focus (data1=gained, data2=windowID).
    NUM_EVENT_TYPES
} evtype_t;

/// Event states. @ingroup input
typedef enum {
    EVS_DOWN,
    EVS_UP,
    EVS_REPEAT,
    NUM_EVENT_STATES
} evstate_t;

/// Input event. @ingroup input
typedef struct event_s {
    evtype_t        type;
    evstate_t       state; ///< Only used with digital controls.
    union {
        struct {
            int     data1; ///< Keys/mouse/joystick buttons.
            int     data2; ///< Mouse/joystick x move.
        };
        uint64_t    data_u64;
    };
    int             data3; ///< Mouse/joystick y move.
    int             data4;
    int             data5;
    int             data6;
} event_t;

/// The mouse wheel is considered two extra mouse buttons.
#define DD_MWHEEL_UP        3
#define DD_MWHEEL_DOWN      4
#define DD_MICKEY_ACCURACY  1000

/// @addtogroup bindings
///@{

DE_API_TYPEDEF(B)
{
    de_api_t api;

    void (*SetContextFallback)(const char *name, int (*responderFunc)(event_t *));

    /**
     * Looks through the bindings to find the ones that are bound to the
     * specified command. The result is a space-separated list of bindings
     * such as (idnum is the binding ID number):
     *
     * <tt>idnum@@game:key-space-down idnum@@game:key-e-down</tt>
     *
     * @param cmd      Command to look for.
     * @param buf      Output buffer for the result.
     * @param bufSize  Size of output buffer.
     *
     * @return  Number of bindings found for the command.
     */
    int  (*BindingsForCommand)(const char *cmd, char *buf, size_t bufSize);

    /**
     * Looks through the bindings to find the ones that are bound to the
     * specified control. The result is a space-separated list of bindings.
     *
     * @param localPlayer  Number of the local player (first one always 0).
     * @param controlName  Name of the player control.
     * @param inverse      One of BFCI_*.
     * @param buf          Output buffer for the result.
     * @param bufSize      Size of output buffer.
     *
     * @return  Number of bindings found for the command.
     */
    int  (*BindingsForControl)(int localPlayer, const char *controlName, int inverse, char *buf, size_t bufSize);

    /**
     * Return the key code that corresponds the given key identifier name.
     */
    int  (*GetKeyCode)(const char* name);
}
DE_API_T(B);

#ifndef DE_NO_API_MACROS_BINDING
#define B_SetContextFallback    _api_B.SetContextFallback
#define B_BindingsForCommand    _api_B.BindingsForCommand
#define B_BindingsForControl    _api_B.BindingsForControl
#define DD_GetKeyCode           _api_B.GetKeyCode
#endif

#ifdef __DOOMSDAY__
DE_USING_API(B);
#endif

///@}

#endif // DOOMSDAY_API_EVENT_H
