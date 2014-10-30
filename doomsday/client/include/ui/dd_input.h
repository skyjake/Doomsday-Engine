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

#define NUMKKEYS            256

#include <de/ddstring.h>
#include <de/smoother.h>
#include <de/Event>

#include "api_event.h"

#ifdef DENG2_DEBUG
#  include <de/point.h> // For the debug visual.
#endif

// Input devices:
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

// Input device control types:
enum inputdev_controltype_t
{
    IDC_KEY,
    IDC_AXIS,
    IDC_HAT,
    NUM_INPUT_DEVICE_CONTROL_TYPES
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

// Binding association. How the device axis/key/etc. relates to binding contexts.
struct inputdevassoc_t
{
    struct bcontext_s *bContext;
    struct bcontext_s *prevBContext;
    int flags;
};

// Association flags.
#define IDAF_EXPIRED    0x1  /** The state has expired. The device is considered to remain
                                 in default state until the flag gets cleared (which happens when
                                 the real device state returns to its default). */
#define IDAF_TRIGGERED  0x2  /** The state has been triggered. This is cleared when someone checks
                                 the device state. (Only for toggles.) */

// Input device axis types:
enum
{
    IDAT_STICK,   ///< Joysticks, gamepads
    IDAT_POINTER  ///< Mouse
};

// Input device axis flags:
#define IDA_DISABLED    0x1  ///< Axis is always zero.
#define IDA_INVERT      0x2  ///< Real input data should be inverted.
#define IDA_RAW         0x4  ///< Do not smooth the input values; always use latest received value.

struct inputdevaxis_t
{
    char name[20];          ///< Symbolic name of the axis.
    int type;               ///< Type of the axis (pointer or stick).
    int flags;
    coord_t position;       ///< Current translated position of the axis (-1..1) including any filtering.
    coord_t realPosition;   ///< The actual latest position of the axis (-1..1).
    float scale;            ///< Scaling factor for real input values.
    float deadZone;         ///< Dead zone, in (0..1) range.
    coord_t sharpPosition;  ///< Current sharp (accumulated) position, entered into the Smoother.
    Smoother *smoother;     ///< Smoother for the input values.
    coord_t prevSmoothPos;  ///< Previous evaluated smooth position (needed for producing deltas).
    uint time;              ///< Timestamp for the latest update that changed the position.
    inputdevassoc_t assoc;  ///< Binding association.
};

struct inputdevkey_t
{
    char isDown;            ///< True/False for each key.
    uint time;
    inputdevassoc_t assoc;  ///< Binding association.
    char const *name;       ///< Symbolic name.
};

struct inputdevhat_t
{
    int pos;                ///< Position of each hat, -1 if centered.
    uint time;              ///< Timestamp for each hat for the latest change.
    inputdevassoc_t assoc;  ///< Binding association.
};

// Input device flags:
#define ID_ACTIVE 0x1       ///< The input device is active.

struct inputdev_t
{
    char niceName[40];     ///< Human-friendly name of the device.
    char name[20];         ///< Symbolic name of the device.
    int flags;
    uint numAxes;          ///< Number of axes in this input device.
    inputdevaxis_t *axes;
    uint numKeys;          ///< Number of keys for this input device.
    inputdevkey_t *keys;
    uint numHats;          ///< Number of hats.
    inputdevhat_t *hats;
};

extern int repWait1, repWait2;
extern dd_bool shiftDown, altDown;

void I_ConsoleRegister();

/**
 * Initializes the key mappings to the default values.
 */
void DD_InitInput();

dd_bool DD_IgnoreInput(dd_bool ignore);

/**
 * Checks the current keyboard state, generates input events based on pressed/held
 * keys and posts them.
 */
void DD_ReadKeyboard();

/**
 * Checks the current mouse state (axis, buttons and wheel).
 * Generates events and mickeys and posts them.
 */
void DD_ReadMouse();

/**
 * Checks the current joystick state (axis, sliders, hat and buttons).
 * Generates events and posts them. Axis clamps and dead zone is done
 * here.
 */
void DD_ReadJoystick();

void DD_ReadHeadTracker();

void DD_PostEvent(ddevent_t *ev);

/**
 * Process all incoming input for the given timestamp.
 * This is called only in the main thread, and also from the busy loop.
 *
 * This gets called at least 35 times per second. Usually more frequently
 * than that.
 */
void DD_ProcessEvents(timespan_t ticLength);

void DD_ProcessSharpEvents(timespan_t ticLength);

/**
 * Clear the input event queue.
 */
void DD_ClearEvents();

/**
 * Apply all active modifiers to the key.
 */
byte DD_ModKey(byte key);

bool DD_ConvertEvent(ddevent_t const *ddEvent, event_t *ev);

/**
 * Converts a libcore Event into an old-fashioned ddevent_t.
 *
 * @param event    Event instance.
 * @param ddEvent  ddevent_t instance.
 */
void DD_ConvertEvent(de::Event const &event, ddevent_t *ddEvent);

/**
 * Initialize the virtual input device state table.
 *
 * @note There need not be actual physical devices available in order to use
 * these state tables.
 */
void I_InitVirtualInputDevices();

/**
 * Free the memory allocated for the input devices.
 */
void I_ShutdownInputDevices();

void I_ClearDeviceContextAssociations();

void I_DeviceReset(uint ident);

void I_ResetAllDevices();

dd_bool I_ShiftDown();

enum InputDeviceGetMode {
    ActiveOrInactiveInputDevice,
    OnlyActiveInputDevice
};

/**
 * Retrieve a pointer to the input device state by identifier.
 *
 * @param ident  Input device identifier (index; @c IDEV_*).
 * @param mode   Finding behavior.
 *
 * @return  Ptr to the input device state OR @c nullptr.
 */
inputdev_t *I_GetDevice(uint ident, InputDeviceGetMode mode = ActiveOrInactiveInputDevice);

/**
 * Retrieve a pointer to the input device state by name.
 *
 * @param name  Input device name.
 * @param mode  Finding behavior.
 *
 * @return  Ptr to the input device state OR @c nullptr.
 */
inputdev_t *I_GetDeviceByName(char const *name, InputDeviceGetMode mode = ActiveOrInactiveInputDevice);

/**
 * Retrieve the user-friendly, print-ready, name for the device associated with
 * unique identifier @a ident.
 *
 * @return  String containing the name for this device. Always valid. This string
 *          should never be free'd by the caller.
 */
ddstring_t const *I_DeviceNameStr(uint ident);

float I_TransformAxis(inputdev_t *dev, uint axis, float rawPos);

/**
 * Check through the axes registered for the given device, see if there is
 * one identified by the given name.
 *
 * @return  @c false, if the string is invalid.
 */
dd_bool I_ParseDeviceAxis(char const *str, uint *deviceID, uint *axis);

/**
 * Retrieve the index of a device's axis by name.
 *
 * @param device  Ptr to input device info, to get the axis index from.
 * @param name    Ptr to string containing the name to be searched for.
 *
 * @return  Index of the device axis named; or -1, if not found.
 */
int I_GetAxisByName(inputdev_t *device, char const *name);

/**
 * Retrieve a ptr to the device axis specified by id.
 *
 * @param device  Ptr to input device info, to get the axis ptr from.
 * @param id      Axis index, to search for.
 *
 * @return  Ptr to the device axis OR @c nullptr, if not found.
 */
inputdevaxis_t *I_GetAxisByID(inputdev_t *device, uint id);

/**
 * Retrieve the index of a device's key by name.
 *
 * @param device  Ptr to input device info, to get the key index from.
 * @param name    Ptr to string containing the name to be searched for.
 *
 * @return  Index of the device key named; or -1, if not found.
 */
int I_GetKeyByName(inputdev_t *device, char const *name);

/**
 * Retrieve a ptr to the device key specified by id.
 *
 * @param device  Ptr to input device info, to get the key ptr from.
 * @param id      Key index, to search for.
 *
 * @return  Ptr to the device key OR @c nullptr, if not found.
 */
inputdevkey_t *I_GetKeyByID(inputdev_t *device, uint id);

/**
 * @return  The key state from the downKeys array.
 */
dd_bool I_IsKeyDown(inputdev_t *device, uint id);

/**
 * Retrieve a ptr to the device hat specified by id.
 *
 * @param device  Ptr to input device info, to get the hat ptr from.
 * @param id      Hat index, to search for.
 *
 * @return  Ptr to the device hat OR @c nullptr, if not found.
 */
inputdevhat_t *I_GetHatByID(inputdev_t *device, uint id);

/**
 * Change between normal and UI mousing modes.
 */
void I_SetUIMouseMode(dd_bool on);

/**
 * Update the input device state table.
 */
void I_TrackInput(ddevent_t *ev);

#ifdef DENG2_DEBUG
/**
 * Render a visual representation of the current state of all input devices.
 */
void Rend_AllInputDeviceStateVisuals();
#else
#  define Rend_AllInputDeviceStateVisuals()
#endif

#endif // CLIENT_CORE_INPUT_H
