/** @file dd_input.h  Input Subsystem.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_CORE_INPUT_H
#define CLIENT_CORE_INPUT_H

#include <de/Event>
#include <de/String>
#include "api_event.h"

class InputDevice;

// Input device identifiers:
enum
{
    IDEV_KEYBOARD,
    IDEV_MOUSE,
    IDEV_JOY1,
    IDEV_JOY2,
    IDEV_JOY3,
    IDEV_JOY4,
    IDEV_HEAD_TRACKER,
    NUM_INPUT_DEVICES  ///< Theoretical maximum.
};

enum ddeventtype_t
{
    E_TOGGLE,    ///< Two-state device
    E_AXIS,      ///< Axis position
    E_ANGLE,     ///< Hat angle
    E_SYMBOLIC,  ///< Symbolic event
    E_FOCUS      ///< Window focus
};

enum ddevent_togglestate_t
{
    ETOG_DOWN,
    ETOG_UP,
    ETOG_REPEAT
};

enum ddevent_axistype_t
{
    EAXIS_ABSOLUTE,  ///< Absolute position on the axis
    EAXIS_RELATIVE   ///< Offset relative to the previous position
};

/**
 * Internal input event.
 *
 * These are used internally, a cutdown version containing
 * only need-to-know metadata is sent down the games' responder chain.
 *
 * @todo Replace with a de::Event-derived class.
 */
struct ddevent_t
{
    uint device;         ///< e.g. IDEV_KEYBOARD
    ddeventtype_t type;  ///< E_TOGGLE, E_AXIS, E_ANGLE, or E_SYMBOLIC
    union {
        struct {
            int id;                       ///< Button/key index number
            ddevent_togglestate_t state;  ///< State of the toggle
            char text[8];                 ///< For characters, latin1-encoded text to insert (or empty).
        } toggle;
        struct {
            int id;                       ///< Axis index number
            float pos;                    ///< Position of the axis
            ddevent_axistype_t type;      ///< Type of the axis (absolute or relative)
        } axis;
        struct {
            int id;                       ///< Angle index number
            float pos;                    ///< Angle, or negative if centered
        } angle;
        struct {
            int id;                       ///< Console that originated the event.
            char const *name;             ///< Symbolic name of the event.
        } symbolic;
        struct {
            dd_bool gained;                ///< Gained or lost focus.
            int inWindow;                 ///< Window where the focus change occurred (index).
        } focus;
    };
};

// Convenience macros.
#define IS_TOGGLE_DOWN(evp)            ((evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_DOWN)
#define IS_TOGGLE_DOWN_ID(evp, togid)  ((evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_DOWN && (evp)->toggle.id == togid)
#define IS_TOGGLE_UP(evp)              ((evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_UP)
#define IS_TOGGLE_REPEAT(evp)          ((evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_REPEAT)
#define IS_KEY_TOGGLE(evp)             ((evp)->device == IDEV_KEYBOARD && (evp)->type == E_TOGGLE)
#define IS_KEY_DOWN(evp)               ((evp)->device == IDEV_KEYBOARD && (evp)->type == E_TOGGLE && (evp)->toggle.state == ETOG_DOWN)
#define IS_KEY_PRESS(evp)              ((evp)->device == IDEV_KEYBOARD && (evp)->type == E_TOGGLE && (evp)->toggle.state != ETOG_UP)
#define IS_MOUSE_DOWN(evp)             ((evp)->device == IDEV_MOUSE && IS_TOGGLE_DOWN(evp))
#define IS_MOUSE_UP(evp)               ((evp)->device == IDEV_MOUSE && IS_TOGGLE_UP(evp))
#define IS_MOUSE_MOTION(evp)           ((evp)->device == IDEV_MOUSE && (evp)->type == E_AXIS)

void I_ConsoleRegister();

/**
 * Initialize the virtual input devices.
 *
 * @note There need not be actual physical devices available in order to use
 * these state tables.
 */
void I_InitAllDevices();

/**
 * Free the memory allocated for the input devices.
 */
void I_ShutdownAllDevices();

/**
 * Lookup an InputDevice by it's unique @a id.
 */
InputDevice &I_Device(int id);

/**
 * Lookup an InputDevice by it's unique @a id.
 *
 * @return  Pointer to the associated InputDevice; otherwise @c nullptr.
 */
InputDevice *I_DevicePtr(int id);

/**
 * Iterate through all the InputDevices.
 */
de::LoopResult I_ForAllDevices(std::function<de::LoopResult (InputDevice &)> func);

/**
 * Initializes the key mappings to the default values.
 */
void I_InitKeyMappings();

/**
 * Checks the current keyboard state, generates input events based on pressed/held
 * keys and posts them.
 */
void I_ReadKeyboard();

/**
 * Checks the current mouse state (axis, buttons and wheel).
 * Generates events and mickeys and posts them.
 */
void I_ReadMouse();

/**
 * Checks the current joystick state (axis, sliders, hat and buttons).
 * Generates events and posts them. Axis clamps and dead zone is done
 * here.
 */
void I_ReadJoystick();

void I_ReadHeadTracker();

/**
 * Clear the input event queue.
 */
void I_ClearEvents();

bool I_IgnoreEvents(bool yes = true);

/**
 * Process all incoming input for the given timestamp.
 * This is called only in the main thread, and also from the busy loop.
 *
 * This gets called at least 35 times per second. Usually more frequently
 * than that.
 */
void I_ProcessEvents(timespan_t ticLength);

void I_ProcessSharpEvents(timespan_t ticLength);

/**
 * @param ev  A copy is made.
 */
void I_PostEvent(ddevent_t *ev);

bool I_ConvertEvent(ddevent_t const *ddEvent, event_t *ev);

/**
 * Converts a libcore Event into an old-fashioned ddevent_t.
 *
 * @param event    Event instance.
 * @param ddEvent  ddevent_t instance.
 */
void I_ConvertEvent(de::Event const &event, ddevent_t *ddEvent);

/**
 * Update the input device state table.
 */
void I_TrackInput(ddevent_t *ev);

bool I_ShiftDown();

#ifdef DENG2_DEBUG
/**
 * Render a visual representation of the current state of all input devices.
 */
void Rend_DrawInputDeviceVisuals();
#else
#  define Rend_DrawInputDeviceVisuals()
#endif

#endif // CLIENT_CORE_INPUT_H
