/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Input Subsystem.
 */

#ifndef LIBDENG_CORE_INPUT_H
#define LIBDENG_CORE_INPUT_H

#define NUMKKEYS            256

#include <de/smoother.h>
#include <de/ddstring.h>
#include "api_event.h"

#if _DEBUG
#  include <de/point.h> // For the debug visual.
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Input devices.
enum
{
    IDEV_KEYBOARD = 0,
    IDEV_MOUSE,
    IDEV_JOY1,
    IDEV_JOY2,
    IDEV_JOY3,
    IDEV_JOY4,
    NUM_INPUT_DEVICES       // Theoretical maximum.
};

// Input device control types
typedef enum {
    IDC_KEY,
    IDC_AXIS,
    IDC_HAT,
    NUM_INPUT_DEVICE_CONTROL_TYPES
} inputdev_controltype_t;

typedef enum ddeventtype_e {
    E_TOGGLE,               // Two-state device
    E_AXIS,                 // Axis position
    E_ANGLE,                // Hat angle
    E_SYMBOLIC,             // Symbolic event
    E_FOCUS                 // Window focus
} ddeventtype_t;

typedef enum ddeevent_togglestate_e {
    ETOG_DOWN,
    ETOG_UP,
    ETOG_REPEAT
} ddevent_togglestate_t;

typedef enum ddevent_axistype_e {
    EAXIS_ABSOLUTE,         // Absolute position on the axis
    EAXIS_RELATIVE          // Offset relative to the previous position
} ddevent_axistype_t;

// These are used internally, a cutdown version containing
// only need-to-know metadata is sent down the games' responder chain.
typedef struct ddevent_s {
    uint            device; // e.g. IDEV_KEYBOARD
    ddeventtype_t   type;   // E_TOGGLE, E_AXIS, E_ANGLE, or E_SYMBOLIC
    union {
        struct {
            int             id;         // Button/key index number
            ddevent_togglestate_t state;// State of the toggle
            char            text[8];    // For characters, latin1-encoded text to insert (or empty).
        } toggle;
        struct {
            int             id;         // Axis index number
            float           pos;        // Position of the axis
            ddevent_axistype_t type;    // Type of the axis (absolute or relative)
        } axis;
        struct {
            int             id;         // Angle index number
            float           pos;        // Angle, or negative if centered
        } angle;
        struct {
            int             id;         // Console that originated the event.
            const char*     name;       // Symbolic name of the event.
        } symbolic;
        struct {
            boolean         gained;     // Gained or lost focus.
            int             inWindow;   // Window where the focus change occurred (index).
        } focus;
    };
} ddevent_t;

// Convenience macros.
#define IS_TOGGLE_DOWN(evp)            (evp->type == E_TOGGLE && evp->toggle.state == ETOG_DOWN)
#define IS_TOGGLE_DOWN_ID(evp, togid)  (evp->type == E_TOGGLE && evp->toggle.state == ETOG_DOWN && evp->toggle.id == togid)
#define IS_TOGGLE_UP(evp)              (evp->type == E_TOGGLE && evp->toggle.state == ETOG_UP)
#define IS_TOGGLE_REPEAT(evp)          (evp->type == E_TOGGLE && evp->toggle.state == ETOG_REPEAT)
#define IS_KEY_TOGGLE(evp)             (evp->device == IDEV_KEYBOARD && evp->type == E_TOGGLE)
#define IS_KEY_DOWN(evp)               (evp->device == IDEV_KEYBOARD && evp->type == E_TOGGLE && evp->toggle.state == ETOG_DOWN)
#define IS_KEY_PRESS(evp)              (evp->device == IDEV_KEYBOARD && evp->type == E_TOGGLE && evp->toggle.state != ETOG_UP)
#define IS_MOUSE_DOWN(evp)             (evp->device == IDEV_MOUSE && IS_TOGGLE_DOWN(evp))
#define IS_MOUSE_UP(evp)               (evp->device == IDEV_MOUSE && IS_TOGGLE_UP(evp))
#define IS_MOUSE_MOTION(evp)           (evp->device == IDEV_MOUSE && evp->type == E_AXIS)

// Binding association. How the device axis/key/etc. relates to binding contexts.
typedef struct inputdevassoc_s {
    struct bcontext_s* bContext;
    struct bcontext_s* prevBContext;
    int     flags;
} inputdevassoc_t;

// Association flags.
#define IDAF_EXPIRED    0x1 // The state has expired. The device is considered to remain
                            // in default state until the flag gets cleared (which happens when
                            // the real device state returns to its default).
#define IDAF_TRIGGERED  0x2 // The state has been triggered. This is cleared when someone checks
                            // the device state. (Only for toggles.)

// Input device axis types.
enum
{
    IDAT_STICK = 0,         // Joysticks, gamepads
    IDAT_POINTER = 1        // Mouse
};

// Input device axis flags.
#define IDA_DISABLED 0x1    // Axis is always zero.
#define IDA_INVERT 0x2      // Real input data should be inverted.

typedef struct inputdevaxis_s {
    char    name[20];       ///< Symbolic name of the axis.
    int     type;           ///< Type of the axis (pointer or stick).
    int     flags;
    coord_t position;       ///< Current translated position of the axis (-1..1) including any filtering.
    coord_t realPosition;   ///< The actual latest position of the axis (-1..1).
    float   scale;          ///< Scaling factor for real input values.
    float   deadZone;       ///< Dead zone, in (0..1) range.
    coord_t sharpPosition;  ///< Current sharp (accumulated) position, entered into the Smoother.
    Smoother* smoother;     ///< Smoother for the input values.
    coord_t prevSmoothPos;  ///< Previous evaluated smooth position (needed for producing deltas).
    uint    time;           ///< Timestamp for the latest update that changed the position.
    inputdevassoc_t assoc;  ///< Binding association.
} inputdevaxis_t;

typedef struct inputdevkey_s {
    char    isDown;         // True/False for each key.
    uint    time;
    inputdevassoc_t assoc;  // Binding association.
    const char* name;       // Symbolic name.
} inputdevkey_t;

typedef struct inputdevhat_s {
    int     pos;            // Position of each hat, -1 if centered.
    uint    time;           // Timestamp for each hat for the latest change.
    inputdevassoc_t assoc;  // Binding association.
} inputdevhat_t;

// Input device flags.
#define ID_ACTIVE 0x1       // The input device is active.

typedef struct inputdev_s {
    char    niceName[40];   // Human-friendly name of the device.
    char    name[20];       // Symbolic name of the device.
    int     flags;
    uint    numAxes;        // Number of axes in this input device.
    inputdevaxis_t *axes;
    uint    numKeys;        // Number of keys for this input device.
    inputdevkey_t *keys;
    uint    numHats;        // Number of hats.
    inputdevhat_t *hats;
} inputdev_t;

#ifdef __CLIENT__

extern int      repWait1, repWait2;
extern int      keyRepeatDelay1, keyRepeatDelay2;   // milliseconds
extern boolean  shiftDown, altDown;

void        DD_RegisterInput(void);
void        DD_InitInput(void);
void        DD_ShutdownInput(void);
void        DD_StartInput(void);
void        DD_StopInput(void);
boolean     DD_IgnoreInput(boolean ignore);

void        DD_ReadKeyboard(void);
void        DD_ReadMouse(void);
void        DD_ReadJoystick(void);

void        DD_PostEvent(ddevent_t *ev);
void        DD_ProcessEvents(timespan_t ticLength);
void        DD_ProcessSharpEvents(timespan_t ticLength);
void        DD_ClearEvents(void);
void        DD_ClearKeyRepeaterForKey(int ddkey, int native);
byte        DD_ModKey(byte key);
void        DD_ConvertEvent(const ddevent_t* ddEvent, event_t* ev);

void        I_InitVirtualInputDevices(void);
void        I_ShutdownInputDevices(void);
void        I_ClearDeviceContextAssociations(void);
void        I_DeviceReset(uint ident);
void        I_ResetAllDevices(void);
boolean     I_ShiftDown(void);

inputdev_t* I_GetDevice(uint ident, boolean ifactive);
inputdev_t* I_GetDeviceByName(const char* name, boolean ifactive);

/**
 * Retrieve the user-friendly, print-ready, name for the device associated with
 * unique identifier @a ident.
 *
 * @return  String containing the name for this device. Always valid. This string
 *          should never be free'd by the caller.
 */
const ddstring_t* I_DeviceNameStr(uint ident);

float I_TransformAxis(inputdev_t* dev, uint axis, float rawPos);

/**
 * Check through the axes registered for the given device, see if there is
 * one identified by the given name.
 *
 * @return  @c false, if the string is invalid.
 */
boolean I_ParseDeviceAxis(const char* str, uint* deviceID, uint* axis);

/**
 * Retrieve the index of a device's axis by name.
 *
 * @param device        Ptr to input device info, to get the axis index from.
 * @param name          Ptr to string containing the name to be searched for.
 *
 * @return  Index of the device axis named; or -1, if not found.
 */
int I_GetAxisByName(inputdev_t* device, const char* name);

/**
 * Retrieve a ptr to the device axis specified by id.
 *
 * @param device        Ptr to input device info, to get the axis ptr from.
 * @param id            Axis index, to search for.
 *
 * @return  Ptr to the device axis OR @c NULL, if not found.
 */
inputdevaxis_t* I_GetAxisByID(inputdev_t* device, uint id);

/**
 * Retrieve the index of a device's key by name.
 *
 * @param device        Ptr to input device info, to get the key index from.
 * @param name          Ptr to string containing the name to be searched for.
 *
 * @return  Index of the device key named; or -1, if not found.
 */
int I_GetKeyByName(inputdev_t* device, const char* name);

/**
 * Retrieve a ptr to the device key specified by id.
 *
 * @param device        Ptr to input device info, to get the key ptr from.
 * @param id            Key index, to search for.
 *
 * @return  Ptr to the device key OR @c NULL, if not found.
 */
inputdevkey_t* I_GetKeyByID(inputdev_t* device, uint id);

/**
 * @return  The key state from the downKeys array.
 */
boolean I_IsKeyDown(inputdev_t* device, uint id);

/**
 * Retrieve a ptr to the device hat specified by id.
 *
 * @param device        Ptr to input device info, to get the hat ptr from.
 * @param id            Hat index, to search for.
 *
 * @return  Ptr to the device hat OR @c NULL, if not found.
 */
inputdevhat_t* I_GetHatByID(inputdev_t* device, uint id);

void        I_SetUIMouseMode(boolean on);
void        I_TrackInput(ddevent_t *ev);

#if _DEBUG
/**
 * Render a visual representation of the current state of all input devices.
 */
void Rend_AllInputDeviceStateVisuals(void);
#else
#  define Rend_AllInputDeviceStateVisuals()
#endif

#endif // __CLIENT__

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_CORE_INPUT_H */
