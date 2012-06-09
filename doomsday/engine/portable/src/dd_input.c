/**
 * @file dd_input.c
 * Platform-independent input subsystem. @ingroup input
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>
#include <math.h>

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_infine.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_ui.h"

// For the debug visuals:
#if _DEBUG
#  include "de_graphics.h"
#  include "de_refresh.h"
#  include "de_render.h"
#endif

#include "con_busy.h"

#define DEFAULT_JOYSTICK_DEADZONE .05f // 5%

#define MAX_AXIS_FILTER 40

#define KBDQUESIZE      32
#define MAX_DOWNKEYS    16      // Most keyboards support 6 or 7.

typedef struct repeater_s {
    int key;                // The DDKEY code (0 if not in use).
    int native;             // Used to determine which key is repeating.
    char text[8];           // Text to insert.
    timespan_t timer;       // How's the time?
    int count;              // How many times has been repeated?
} repeater_t;

typedef struct {
    ddevent_t events[MAXEVENTS];
    int head;
    int tail;
} eventqueue_t;

#if 0
D_CMD(AxisPrintConfig);
D_CMD(AxisChangeOption);
D_CMD(AxisChangeValue);
#endif
D_CMD(DumpKeyMap);
D_CMD(KeyMap);
D_CMD(ListInputDevices);
D_CMD(ReleaseMouse);

static void postEvents(void);

// The initial and secondary repeater delays (tics).
int     repWait1 = 15, repWait2 = 3;
int     keyRepeatDelay1 = 430, keyRepeatDelay2 = 85;    // milliseconds
unsigned int  mouseFreq = 0;
boolean shiftDown = false, altDown = false;

inputdev_t inputDevices[NUM_INPUT_DEVICES];

//-------------------------------------------------------------------------

static boolean inputDisabledFully = false;
static boolean ignoreInput = false;

static byte shiftKeyMappings[NUMKKEYS], altKeyMappings[NUMKKEYS];

static eventqueue_t queue;
static eventqueue_t sharpQueue;

static char defaultShiftTable[96] = // Contains characters 32 to 127.
{
/* 32 */    ' ', 0, 0, 0, 0, 0, 0, '"',
/* 40 */    0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
/* 50 */    '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
/* 60 */    0, '+', 0, 0, 0, 'a', 'b', 'c', 'd', 'e',
/* 70 */    'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
/* 80 */    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
/* 90 */    'z', '{', '|', '}', 0, 0, '~', 'A', 'B', 'C',
/* 100 */   'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
/* 110 */   'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
/* 120 */   'X', 'Y', 'Z', 0, 0, 0, 0, 0
};

static repeater_t keyReps[MAX_DOWNKEYS];
static float oldPOV = IJOY_POV_CENTER;
static char* eventStrings[MAXEVENTS];
static boolean uiMouseMode = false; // Can mouse data be modified?

static byte useSharpInputEvents = true; ///< cvar

#if _DEBUG
static byte devRendKeyState = false; ///< cvar
static byte devRendMouseState = false; ///< cvar
static byte devRendJoyState = false; ///< cvar
#endif

//-------------------------------------------------------------------------

void DD_RegisterInput(void)
{
    // Cvars
    C_VAR_INT("input-key-delay1", &keyRepeatDelay1, CVF_NO_MAX, 50, 0);
    C_VAR_INT("input-key-delay2", &keyRepeatDelay2, CVF_NO_MAX, 20, 0);
    C_VAR_BYTE("input-sharp", &useSharpInputEvents, 0, 0, 1);

#if _DEBUG
    C_VAR_BYTE("rend-dev-input-joy-state", &devRendJoyState, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-input-key-state", &devRendKeyState, CVF_NO_ARCHIVE, 0, 1);
    C_VAR_BYTE("rend-dev-input-mouse-state", &devRendMouseState, CVF_NO_ARCHIVE, 0, 1);
#endif

    // Ccmds
#if 0
    C_CMD("dumpkeymap", "s", DumpKeyMap);
    C_CMD("keymap", "s", KeyMap);
#endif
    C_CMD("listinputdevices", "", ListInputDevices);

    C_CMD("releasemouse", "", ReleaseMouse);

    //C_CMD_FLAGS("setaxis", "s",      AxisPrintConfig, CMDF_NO_DEDICATED);
    //C_CMD_FLAGS("setaxis", "ss",     AxisChangeOption, CMDF_NO_DEDICATED);
    //C_CMD_FLAGS("setaxis", "sss",    AxisChangeValue, CMDF_NO_DEDICATED);
}

/**
 * Allocate an array of bytes for the input devices keys.
 * The allocated memory is cleared to zero.
 */
static void I_DeviceAllocKeys(inputdev_t *dev, uint count)
{
    dev->numKeys = count;
    dev->keys = M_Calloc(count * sizeof(inputdevkey_t));
}

static void I_DeviceAllocHats(inputdev_t *dev, uint count)
{
    dev->numHats = count;
    dev->hats = M_Calloc(count * sizeof(inputdevhat_t));
}

/**
 * Add a new axis to the input device.
 */
static inputdevaxis_t *I_DeviceNewAxis(inputdev_t *dev, const char *name, uint type)
{
    inputdevaxis_t *axis;

    dev->axes = M_Realloc(dev->axes, sizeof(inputdevaxis_t) * ++dev->numAxes);

    axis = &dev->axes[dev->numAxes - 1];
    memset(axis, 0, sizeof(*axis));
    strcpy(axis->name, name);
    axis->type = type;
    axis->smoother = Smoother_New();
    Smoother_SetMaximumPastNowDelta(axis->smoother, 2*SECONDSPERTIC);

    // Set reasonable defaults. The user's settings will be restored
    // later.
    axis->scale = 1;
    axis->deadZone = 0;

    return axis;
}

/**
 * Initialize the input device state table.
 *
 * \note There need not be actual physical devices available in order to
 * use these state tables.
 */
void I_InitVirtualInputDevices(void)
{
    inputdev_t* dev;
    inputdevaxis_t* axis;
    int i;

    // Allow re-init.
    I_ShutdownInputDevices();

    memset(inputDevices, 0, sizeof(inputDevices));

    // The keyboard is always assumed to be present.
    // DDKEYs are used as key indices.
    dev = &inputDevices[IDEV_KEYBOARD];
    dev->flags = ID_ACTIVE;
    strcpy(dev->niceName, "Keyboard");
    strcpy(dev->name, "key");
    I_DeviceAllocKeys(dev, 256);

    // The mouse may not be active.
    dev = &inputDevices[IDEV_MOUSE];
    strcpy(dev->niceName, "Mouse");
    strcpy(dev->name, "mouse");
    I_DeviceAllocKeys(dev, IMB_MAXBUTTONS);

    // Some of the mouse buttons have symbolic names.
    dev->keys[IMB_LEFT].name = "left";
    dev->keys[IMB_MIDDLE].name = "middle";
    dev->keys[IMB_RIGHT].name = "right";
    dev->keys[IMB_MWHEELUP].name = "wheelup";
    dev->keys[IMB_MWHEELDOWN].name = "wheeldown";
    dev->keys[IMB_MWHEELLEFT].name = "wheelleft";
    dev->keys[IMB_MWHEELRIGHT].name = "wheelright";

    // The mouse wheel is translated to keys, so there is no need to
    // create an axis for it.
    axis = I_DeviceNewAxis(dev, "x", IDAT_POINTER);
    //axis->filter = 1; // On by default.
    axis->scale = 1.f/1000;

    axis = I_DeviceNewAxis(dev, "y", IDAT_POINTER);
    //axis->filter = 1; // On by default.
    axis->scale = 1.f/1000;

    // Register console variables for the axis settings.
    // CAUTION: Allocating new axes may invalidate the pointers here.
    C_VAR_FLOAT("input-mouse-x-scale", &dev->axes[0].scale, CVF_NO_MAX, 0, 0);
    C_VAR_INT("input-mouse-x-flags", &dev->axes[0].flags, 0, 0, 3);
    C_VAR_FLOAT("input-mouse-y-scale", &dev->axes[1].scale, CVF_NO_MAX, 0, 0);
    C_VAR_INT("input-mouse-y-flags", &dev->axes[1].flags, 0, 0, 3);
    //C_VAR_INT("input-mouse-filter", &dev->axes[0].filter, 0, 0, MAX_AXIS_FILTER - 1); // note: same filter used for Y axis

    if(Mouse_IsPresent())
        dev->flags = ID_ACTIVE;

    // TODO: Add support for several joysticks.
    dev = &inputDevices[IDEV_JOY1];
    strcpy(dev->niceName, "Joystick");
    strcpy(dev->name, "joy");
    I_DeviceAllocKeys(dev, IJOY_MAXBUTTONS);

    for(i = 0; i < IJOY_MAXAXES; ++i)
    {
        char name[32];
        if(i < 4)
        {
            strcpy(name, i == 0? "x" : i == 1? "y" : i == 2? "z" : "w");
        }
        else
        {
            sprintf(name, "axis%02i", i + 1);
        }
        axis = I_DeviceNewAxis(dev, name, IDAT_STICK);
        axis->scale = 1.0f / IJOY_AXISMAX;
        axis->deadZone = DEFAULT_JOYSTICK_DEADZONE;
    }

    // Register console variables for the axis settings.
    for(i = 0; i < IJOY_MAXAXES; ++i)
    {
        inputdevaxis_t* axis = &dev->axes[i];
        char varName[80];

        sprintf(varName, "input-joy-%s-scale", axis->name);
        C_VAR_FLOAT(varName, &axis->scale, CVF_NO_MAX, 0, 0);

        sprintf(varName, "input-joy-%s-flags", axis->name);
        C_VAR_INT(varName, &axis->flags, 0, 0, 3);

        sprintf(varName, "input-joy-%s-deadzone", axis->name);
        C_VAR_FLOAT(varName, &axis->deadZone, 0, 0, 1);
    }

    I_DeviceAllocHats(dev, IJOY_MAXHATS);
    for(i = 0; i < IJOY_MAXHATS; ++i)
    {
        dev->hats[i].pos = -1; // centered
    }

    // The joystick may not be active.
    if(Joystick_IsPresent())
        dev->flags = ID_ACTIVE;
}

/**
 * Free the memory allocated for the input devices.
 */
void I_ShutdownInputDevices(void)
{
    uint i, k;
    inputdev_t* dev;

    for(i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        dev = &inputDevices[i];

        if(dev->keys)
        {
            M_Free(dev->keys);
            dev->keys = 0;
        }

        if(dev->axes)
        {
            for(k = 0; k < dev->numAxes; ++k)
            {
                Smoother_Delete(dev->axes[k].smoother);
            }
            M_Free(dev->axes);
            dev->axes = 0;
        }

        if(dev->hats)
        {
            M_Free(dev->hats);
            dev->hats = 0;
        }
    }
}

void I_DeviceReset(uint ident)
{
    inputdev_t* dev = &inputDevices[ident];
    int k;

    DEBUG_VERBOSE_Message(("I_DeviceReset: %s.\n", Str_Text(I_DeviceNameStr(ident))));

    for(k = 0; k < (int)dev->numKeys && dev->keys; ++k)
    {
        if(dev->keys[k].isDown)
        {
            dev->keys[k].assoc.flags |= IDAF_EXPIRED;
        }
        else
        {
            dev->keys[k].isDown = false;
            dev->keys[k].time = 0;
            dev->keys[k].assoc.flags &= ~(IDAF_TRIGGERED | IDAF_EXPIRED);
        }
    }

    for(k = 0; k < (int)dev->numAxes && dev->axes; ++k)
    {
        if(dev->axes[k].type == IDAT_POINTER)
        {
            // Clear the accumulation.
            dev->axes[k].position = 0;
            dev->axes[k].sharpPosition = 0;
            dev->axes[k].prevSmoothPos = 0;
        }
        Smoother_Clear(dev->axes[k].smoother);
    }

    if(ident == IDEV_KEYBOARD)
    {
        altDown = shiftDown = false;
        DD_ClearKeyRepeaters();
    }
}

void I_ResetAllDevices(void)
{
    uint i;
    for(i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        I_DeviceReset(i);
    }
}

/**
 * Retrieve a pointer to the input device state by identifier.
 *
 * @param ident         Intput device identifier (index).
 * @param ifactive      Only return if the device is active.
 *
 * @return              Ptr to the input device state OR @c NULL,.
 */
inputdev_t *I_GetDevice(uint ident, boolean ifactive)
{
    inputdev_t*         dev = &inputDevices[ident];

    if(ifactive)
    {
        if(dev->flags & ID_ACTIVE)
            return dev;
        else
            return NULL;
    }

    return dev;
}

/**
 * Retrieve a pointer to the input device state by name.
 *
 * @param name          Input device name.
 * @param ifactive      Only return if the device is active.
 *
 * @return              Ptr to the input device state OR @c NULL,.
 */
inputdev_t *I_GetDeviceByName(const char *name, boolean ifactive)
{
    uint        i;
    inputdev_t *dev = NULL;
    boolean     found;

    i = 0;
    found = false;
    while(i < NUM_INPUT_DEVICES && !found)
    {
        if(!stricmp(inputDevices[i].name, name))
        {
            dev = &inputDevices[i];
            found = true;
        }
        else
            i++;
    }

    if(dev)
    {
        if(ifactive)
        {
            if(dev->flags & ID_ACTIVE)
                return dev;
            else
                return NULL;
        }
    }

    return dev;
}

const ddstring_t* I_DeviceNameStr(uint ident)
{
    static const ddstring_t names[1+NUM_INPUT_DEVICES] = {
        { "(invalid-identifier)" },
        { "keyboard" },
        { "mouse" },
        { "joystick" },
        { "joystick2" },
        { "joystick3" },
        { "joystick4" }
    };
    if(ident >= NUM_INPUT_DEVICES) return &names[0];
    return &names[1+ident];
}

inputdevaxis_t* I_GetAxisByID(inputdev_t* device, uint id)
{
    if(!device || id == 0 || id > device->numAxes - 1)
        return NULL;
    return &device->axes[id-1];
}

int I_GetAxisByName(inputdev_t* device, const char* name)
{
    uint i;
    for(i = 0; i < device->numAxes; ++i)
    {
        if(!stricmp(device->axes[i].name, name))
            return i;
    }
    return -1;
}

inputdevkey_t* I_GetKeyByID(inputdev_t* device, uint id)
{
    if(!device || id == 0 || id > device->numKeys - 1)
        return NULL;
    return &device->keys[id-1];
}

int I_GetKeyByName(inputdev_t* device, const char* name)
{
    int i;
    for(i = 0; i < (int)device->numKeys; ++i)
    {
        if(device->keys[i].name && !stricmp(device->keys[i].name, name))
            return i;
    }
    return -1;
}

inputdevhat_t* I_GetHatByID(inputdev_t* device, uint id)
{
    if(!device || id == 0 || id > device->numHats - 1)
        return NULL;
    return &device->hats[id-1];
}

boolean I_ParseDeviceAxis(const char* str, uint* deviceID, uint* axis)
{
    char name[30], *ptr;
    inputdev_t* device;

    ptr = strchr(str, '-');
    if(!ptr)
        return false;

    // The name of the device.
    memset(name, 0, sizeof(name));
    strncpy(name, str, ptr - str);
    device = I_GetDeviceByName(name, false);
    if(device == NULL)
        return false;
    if(deviceID)
        *deviceID = device - inputDevices;

    // The axis name.
    if(axis)
    {
        int a = I_GetAxisByName(device, ptr + 1);
        if(a < 0)
        {
            *axis = 0;
            return false;
        }
        *axis = a + 1; // Axis indices are base 1.
    }

    return true;
}

float I_TransformAxis(inputdev_t* dev, uint axis, float rawPos)
{
    float               pos = rawPos;
    inputdevaxis_t*     a = &dev->axes[axis];

    // Disabled axes are always zero.
    if(a->flags & IDA_DISABLED)
    {
        return 0;
    }

    // Apply scaling, deadzone and clamping.
    pos *= a->scale;
    if(a->type == IDAT_STICK) // Pointer axes are not dead-zoned or clamped.
    {
        if(fabs(pos) <= a->deadZone)
        {
            pos = 0;
        }
        else
        {
            pos -= a->deadZone * SIGN_OF(pos);  // Remove the dead zone.
            pos *= 1.0f/(1.0f - a->deadZone);       // Normalize.
            pos = MINMAX_OF(-1.0f, pos, 1.0f);
        }
    }

    if(a->flags & IDA_INVERT)
    {
        // Invert the axis position.
        pos = -pos;
    }

    return pos;
}

#if 0
static float filterAxis(int grade, float* accumulation, float ticLength)
{
    float   target;
    float   avail;
    float   used;
    int     dir;

    dir = SIGN_OF(*accumulation);
    avail = fabs(*accumulation);

    // Determine the target velocity.
    target = avail * (MAX_AXIS_FILTER - MINMAX_OF(1, grade, 39));

    /*
    // test: clamp
    if(target < -.7) target = -.7;
    else if(target > .7) target = .7;
    else target = 0;
    */

    // Determine the amount of mickeys to send. It depends on the
    // current mouse velocity, and how much time has passed.
    used = target * ticLength;

    // Don't go past the available motion.
    if(used > avail)
    {
        *accumulation = 0;
        used = avail;
    }
    else
    {
        if(*accumulation > 0)
            *accumulation -= used;
        else
            *accumulation += used;
    }

    // This is the new (filtered) axis position.
    return dir * used;
}
#endif

/**
 * Update an input device axis.  Transformation is applied.
 */
static void I_ApplyRealPositionToAxis(inputdev_t* dev, uint axis, float pos)
{
    inputdevaxis_t *a = &dev->axes[axis];
    float oldRealPos = a->realPosition;
    float transformed = I_TransformAxis(dev, axis, pos);

    // The unfiltered position.
    a->realPosition = transformed;

    if(oldRealPos != a->realPosition)
    {
        // Mark down the time of the change.
        a->time = DD_LatestRunTicsStartTime();
    }

    if(a->type == IDAT_STICK)
    {
        a->sharpPosition = a->realPosition;
    }
    else // Cumulative.
    {
        // Convert the delta to an absolute position for smoothing.
        a->sharpPosition += a->realPosition;
    }

    Smoother_AddPosXY(a->smoother, DD_LatestRunTicsStartTime(), a->sharpPosition, 0);
}

static void I_UpdateAxis(inputdev_t *dev, uint axis, timespan_t ticLength)
{
    inputdevaxis_t *a = &dev->axes[axis];

    Smoother_Advance(a->smoother, ticLength);

    if(a->type == IDAT_STICK)
    {
        // Absolute positions are straightforward to evaluate.
        Smoother_EvaluateComponent(a->smoother, 0, &a->position);
    }
    else if(a->type == IDAT_POINTER)
    {
        // Convert back into a delta.
        coord_t smoothPos = a->prevSmoothPos;
        Smoother_EvaluateComponent(a->smoother, 0, &smoothPos);
        a->position += smoothPos - a->prevSmoothPos;
        a->prevSmoothPos = smoothPos;
    }

    // We can clear the expiration when it returns to default state.
    if(FEQUAL(a->position, 0) || a->type == IDAT_POINTER)
    {
        a->assoc.flags &= ~IDAF_EXPIRED;
    }
}

boolean I_ShiftDown(void)
{
    return shiftDown;
}

/**
 * Update the input device state table.
 */
void I_TrackInput(ddevent_t *ev)
{
    inputdev_t *dev;

    if(ev->type == E_FOCUS || ev->type == E_SYMBOLIC)
        return; // Not a tracked device state.

    if((dev = I_GetDevice(ev->device, true)) == NULL)
        return;

    // Track the state of Shift and Alt.
    if(IS_KEY_TOGGLE(ev))
    {
        if(ev->toggle.id == DDKEY_RSHIFT)
        {
            if(ev->toggle.state == ETOG_DOWN)
                shiftDown = true;
            else if(ev->toggle.state == ETOG_UP)
                shiftDown = false;
        }
        else if(ev->toggle.id == DDKEY_RALT)
        {
            if(ev->toggle.state == ETOG_DOWN)
            {
                altDown = true;
                DEBUG_Message(("I_TrackInput: Alt down\n"));
            }
            else if(ev->toggle.state == ETOG_UP)
            {
                altDown = false;
                DEBUG_Message(("I_TrackInput: Alt up\n"));
            }
        }
    }

    // Update the state table.
    if(ev->type == E_AXIS)
    {
        I_ApplyRealPositionToAxis(dev, ev->axis.id, ev->axis.pos);
    }
    else if(ev->type == E_TOGGLE)
    {
        inputdevkey_t* key = &dev->keys[ev->toggle.id];

        key->isDown = (ev->toggle.state == ETOG_DOWN || ev->toggle.state == ETOG_REPEAT);

        // Mark down the time when the change occurs.
        if(ev->toggle.state == ETOG_DOWN || ev->toggle.state == ETOG_UP)
        {
            key->time = Sys_GetRealTime();
        }

        if(key->isDown)
        {
            // This will get cleared after the state is checked by someone.
            key->assoc.flags |= IDAF_TRIGGERED;
        }
        else
        {
            // We can clear the expiration when the key is released.
            key->assoc.flags &= ~IDAF_EXPIRED;
        }
    }
    else if(ev->type == E_ANGLE)
    {
        inputdevhat_t* hat = &dev->hats[ev->angle.id];

        hat->pos = ev->angle.pos;

        // Mark down the time when the change occurs.
        hat->time = Sys_GetRealTime();

        // We can clear the expiration when the hat is centered.
        if(hat->pos < 0)
        {
            hat->assoc.flags &= ~IDAF_EXPIRED;
        }
    }
}

void I_ClearDeviceContextAssociations(void)
{
    inputdev_t* dev;
    uint i, j;

    for(i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        dev = &inputDevices[i];

        // Keys.
        for(j = 0; j < dev->numKeys; ++j)
        {
            dev->keys[j].assoc.prevBContext = dev->keys[j].assoc.bContext;
            dev->keys[j].assoc.bContext = NULL;
            dev->keys[j].assoc.flags &= ~IDAF_TRIGGERED;
        }
        // Axes.
        for(j = 0; j < dev->numAxes; ++j)
        {
            dev->axes[j].assoc.prevBContext = dev->axes[j].assoc.bContext;
            dev->axes[j].assoc.bContext = NULL;
        }
        // Hats.
        for(j = 0; j < dev->numHats; ++j)
        {
            dev->hats[j].assoc.prevBContext = dev->hats[j].assoc.bContext;
            dev->hats[j].assoc.bContext = NULL;
        }
    }
}

boolean I_IsKeyDown(inputdev_t* dev, uint id)
{
    if(dev && id >= 0 && id < dev->numKeys)
    {
        return dev->keys[id].isDown;
    }
    return false;
}

/**
 * @return  Either key number or the scan code for the given token.
 */
int DD_KeyOrCode(char* token)
{
    char* end = M_FindWhite(token);

    if(end - token > 1)
    {
        // Longer than one character, it must be a number.
        return strtol(token, 0, !strnicmp(token, "0x", 2) ? 16 : 10);
    }
    // Direct mapping.
    return (unsigned char) *token;
}

/**
 * Initializes the key mappings to the default values.
 */
void DD_InitInput(void)
{
    int i;

    inputDisabledFully = CommandLine_Exists("-noinput");

    for(i = 0; i < 256; ++i)
    {
        if(i >= 32 && i <= 127)
            shiftKeyMappings[i] = defaultShiftTable[i - 32] ? defaultShiftTable[i - 32] : i;
        else
            shiftKeyMappings[i] = i;
        altKeyMappings[i] = i;
    }
}

/**
 * Returns a copy of the string @a str. The caller does not get ownership of
 * the string. The string is valid until it gets overwritten by a new
 * allocation. There are at most MAXEVENTS strings allocated at a time.
 *
 * These are intended for strings in ddevent_t that are valid during the
 * processing of an event.
 */
const char* DD_AllocEventString(const char* str)
{
    static int eventStringRover = 0;
    const char* returnValue = 0;

    free(eventStrings[eventStringRover]);
    returnValue = eventStrings[eventStringRover] = strdup(str);

    if(++eventStringRover >= MAXEVENTS)
    {
        eventStringRover = 0;
    }
    return returnValue;
}

void DD_ClearEventStrings(void)
{
    int i;

    for(i = 0; i < MAXEVENTS; ++i)
    {
        free(eventStrings[i]);
        eventStrings[i] = 0;
    }
}

static void clearQueue(eventqueue_t* q)
{
    q->head = q->tail;
}

boolean DD_IgnoreInput(boolean ignore)
{
    boolean old = ignoreInput;
    ignoreInput = ignore;
    DEBUG_Message(("DD_IgnoreInput: ignoring=%i\n", ignore));
    if(!ignore)
    {
        // Clear all the event buffers.
        postEvents();
        DD_ClearEvents();
        DD_ClearKeyRepeaters();
    }
    return old;
}

/**
 * Clear the input event queue.
 */
void DD_ClearEvents(void)
{
    clearQueue(&queue);
    clearQueue(&sharpQueue);

    DD_ClearEventStrings();
}

static void postToQueue(eventqueue_t* q, ddevent_t* ev)
{
    q->events[q->head] = *ev;

    if(ev->type == E_SYMBOLIC)
    {
        // Allocate a throw-away string from our buffer.
        q->events[q->head].symbolic.name = DD_AllocEventString(ev->symbolic.name);
    }

    q->head++;
    q->head &= MAXEVENTS - 1;
}

/**
 * Called by the I/O functions when input is detected.
 */
void DD_PostEvent(ddevent_t *ev)
{
    eventqueue_t* q = &queue;
    if(useSharpInputEvents && (ev->type == E_TOGGLE || ev->type == E_AXIS ||
                               ev->type == E_ANGLE))
    {
        q = &sharpQueue;
    }

    // Cleanup: make sure only keyboard toggles can have a text insert.
    if(ev->type == E_TOGGLE && ev->device != IDEV_KEYBOARD)
    {
        memset(ev->toggle.text, 0, sizeof(ev->toggle.text));
    }

    postToQueue(q, ev);

#ifdef LIBDENG_CAMERA_MOVEMENT_ANALYSIS
    if(ev->device == IDEV_KEYBOARD && ev->type == E_TOGGLE
            && ev->toggle.state == ETOG_DOWN)
    {
        // Restart timer on each key down.
        extern float devCameraMovementStartTime;
        extern float devCameraMovementStartTimeRealSecs;
        devCameraMovementStartTime = sysTime;
        devCameraMovementStartTimeRealSecs = Sys_GetRealSeconds();
    }
#endif
}

/**
 * Gets the next event from an input event queue.
 * @param q  Event queue.
 * @return @c NULL if no more events are available.
 */
static ddevent_t* nextFromQueue(eventqueue_t* q)
{
    ddevent_t *ev;

    if(q->head == q->tail)
        return NULL;

    ev = &q->events[q->tail];
    q->tail = (q->tail + 1) & (MAXEVENTS - 1);

    return ev;
}

void DD_ConvertEvent(const ddevent_t* ddEvent, event_t* ev)
{
    // Copy the essentials into a cutdown version for the game.
    // Ensure the format stays the same for future compatibility!
    //
    /// @todo This is probably broken! (DD_MICKEY_ACCURACY=1000 no longer used...)
    //
    memset(ev, 0, sizeof(*ev));
    if(ddEvent->type == E_SYMBOLIC)
    {
        ev->type = EV_SYMBOLIC;
#ifdef __64BIT__
        ASSERT_64BIT(ddEvent->symbolic.name);
        ev->data1 = (int)(((int64_t) ddEvent->symbolic.name) & 0xffffffff); // low dword
        ev->data2 = (int)(((int64_t) ddEvent->symbolic.name) >> 32); // high dword
#else
        ASSERT_NOT_64BIT(ddEvent->symbolic.name);

        ev->data1 = (int) ddEvent->symbolic.name;
        ev->data2 = 0;
#endif
    }
    else if(ddEvent->type == E_FOCUS)
    {
        ev->type = EV_FOCUS;
        ev->data1 = ddEvent->focus.gained;
        ev->data2 = ddEvent->focus.inWindow;
    }
    else
    {
        switch(ddEvent->device)
        {
        case IDEV_KEYBOARD:
            ev->type = EV_KEY;
            if(ddEvent->type == E_TOGGLE)
            {
                ev->state = ( ddEvent->toggle.state == ETOG_UP? EVS_UP :
                              ddEvent->toggle.state == ETOG_DOWN? EVS_DOWN :
                              EVS_REPEAT );
                ev->data1 = ddEvent->toggle.id;
            }
            break;

        case IDEV_MOUSE:
            if(ddEvent->type == E_AXIS)
            {
                ev->type = EV_MOUSE_AXIS;
            }
            else if(ddEvent->type == E_TOGGLE)
            {
                ev->type = EV_MOUSE_BUTTON;
                ev->data1 = ddEvent->toggle.id;
                ev->state = ( ddEvent->toggle.state == ETOG_UP? EVS_UP :
                              ddEvent->toggle.state == ETOG_DOWN? EVS_DOWN :
                              EVS_REPEAT );
            }
            break;

        case IDEV_JOY1:
        case IDEV_JOY2:
        case IDEV_JOY3:
        case IDEV_JOY4:
            if(ddEvent->type == E_AXIS)
            {
                int* data = &ev->data1;
                ev->type = EV_JOY_AXIS;
                ev->state = 0;
                if(ddEvent->axis.id >= 0 && ddEvent->axis.id < 6)
                {
                    data[ddEvent->axis.id] = ddEvent->axis.pos;
                }
                /// @todo  The other dataN's must contain up-to-date information
                /// as well. Read them from the current joystick status.
            }
            else if(ddEvent->type == E_TOGGLE)
            {
                ev->type = EV_JOY_BUTTON;
                ev->state = ( ddEvent->toggle.state == ETOG_UP? EVS_UP :
                              ddEvent->toggle.state == ETOG_DOWN? EVS_DOWN :
                              EVS_REPEAT );
                ev->data1 = ddEvent->toggle.id;
            }
            else if(ddEvent->type == E_ANGLE)
                ev->type = EV_POV;
            break;

        default:
#if _DEBUG
            Con_Error("DD_ProcessEvents: Unknown deviceID in ddevent_t");
#endif
            break;
        }
    }
}

static void updateDeviceAxes(timespan_t ticLength)
{
    int i;
    for(i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        uint k;
        for(k = 0; k < inputDevices[i].numAxes; ++k)
        {
            I_UpdateAxis(&inputDevices[i], k, ticLength);
        }
    }
}

/**
 * Send all the events of the given timestamp down the responder chain.
 */
static void dispatchEvents(eventqueue_t* q, timespan_t ticLength, boolean updateAxes)
{
    const boolean callGameResponders = DD_GameLoaded();
    ddevent_t* ddev;

    while((ddev = nextFromQueue(q)))
    {
        event_t ev;

        // Update the state of the input device tracking table.
        I_TrackInput(ddev);

        if(ignoreInput && ddev->type != E_FOCUS)
            continue;

        DD_ConvertEvent(ddev, &ev);

        if(callGameResponders)
        {
            // Does the game's special responder use this event? This is
            // intended for grabbing events when creating bindings in the
            // Controls menu.
            if(gx.PrivilegedResponder && gx.PrivilegedResponder(&ev))
                continue;
        }

        // The bindings responder.
        if(B_Responder(ddev)) continue;

        // The "fallback" responder. Gets the event if no one else is interested.
        if(callGameResponders && gx.FallbackResponder)
            gx.FallbackResponder(&ev);
    }

    if(updateAxes)
    {
        // Input events have modified input device state: update the axis positions.
        updateDeviceAxes(ticLength);
    }
}

/**
 * Poll all event sources (i.e., input devices) and post events.
 */
static void postEvents(void)
{
    if(inputDisabledFully) return;

    DD_ReadKeyboard();

    // In dedicated mode, we don't do mice or joysticks.
    if(!isDedicated)
    {
        DD_ReadMouse();
        DD_ReadJoystick();
    }
}

/**
 * Process all incoming input for the given timestamp.
 * This is called only in the main thread, and also from the busy loop.
 *
 * This gets called at least 35 times per second. Usually more frequently
 * than that.
 */
void DD_ProcessEvents(timespan_t ticLength)
{
    // Poll all event sources (i.e., input devices) and post events.
    postEvents();

    // Dispatch all accumulated events down the responder chain.
    dispatchEvents(&queue, ticLength, !useSharpInputEvents);
}

void DD_ProcessSharpEvents(timespan_t ticLength)
{
    // Sharp ticks may have some events queued on the side.
    if(DD_IsSharpTick() || !DD_IsFrameTimeAdvancing())
    {
        dispatchEvents(&sharpQueue, DD_IsFrameTimeAdvancing()? SECONDSPERTIC : ticLength, true);
    }
}

/**
 * Apply all active modifiers to the key.
 */
byte DD_ModKey(byte key)
{
    if(shiftDown)
        key = shiftKeyMappings[key];
    if(altDown)
        key = altKeyMappings[key];

    if(key >= DDKEY_NUMPAD7 && key <= DDKEY_NUMPAD0)
    {
        byte numPadKeys[10] = {
            '7', '8', '9', '4', '5', '6', '1', '2', '3', '0'
        };
        return numPadKeys[key - DDKEY_NUMPAD7];
    }
    else if(key == DDKEY_DIVIDE)
    {
        return '/';
    }
    else if(key == DDKEY_SUBTRACT)
    {
        return '-';
    }
    else if(key == DDKEY_ADD)
    {
        return '+';
    }
    else if(key == DDKEY_DECIMAL)
    {
        return '.';
    }
    else if(key == DDKEY_MULTIPLY)
    {
        return '*';
    }

    return key;
}

/**
 * Clears the repeaters array.
 */
void DD_ClearKeyRepeaters(void)
{
    memset(keyReps, 0, sizeof(keyReps));
}

void DD_ClearKeyRepeaterForKey(int ddkey, int native)
{
    int k;

    // Clear any repeaters with this key.
    for(k = 0; k < MAX_DOWNKEYS; ++k)
    {
        // Check the native code first, if provided.
        if(native >= 0)
        {
            if(native != keyReps[k].native)
                continue;
        }
        else
        {
            if(keyReps[k].key != ddkey) continue;
        }

        // Clear this.
        keyReps[k].native = -1;
        keyReps[k].key = 0;
        memset(keyReps[k].text, 0, sizeof(keyReps[k].text));
    }
}

/**
 * Checks the current keyboard state, generates input events
 * based on pressed/held keys and posts them.
 */
void DD_ReadKeyboard(void)
{
    uint            i, k;
    ddevent_t       ev;
    size_t          n, numkeyevs;
    keyevent_t      keyevs[KBDQUESIZE];

    // Check the repeaters.
    ev.device = IDEV_KEYBOARD;
    ev.type = E_TOGGLE;
    ev.toggle.state = ETOG_REPEAT;

    // Post key repeat events.
    /// @todo Move this to a ticker function?
    for(i = 0; i < MAX_DOWNKEYS; ++i)
    {
        repeater_t *rep = keyReps + i;
        if(!rep->key)
            continue;

        ev.toggle.id = rep->key;
        memcpy(ev.toggle.text, rep->text, sizeof(ev.toggle.text));

        if(!rep->count && sysTime - rep->timer >= keyRepeatDelay1 / 1000.0)
        {
            // The first time.
            rep->count++;
            rep->timer += keyRepeatDelay1 / 1000.0;
            DD_PostEvent(&ev);
        }

        if(rep->count)
        {
            while(sysTime - rep->timer >= keyRepeatDelay2 / 1000.0)
            {
                rep->count++;
                rep->timer += keyRepeatDelay2 / 1000.0;
                DD_PostEvent(&ev);
            }
        }
    }

    // Read the new keyboard events.
    if(novideo)
        numkeyevs = I_GetConsoleKeyEvents(keyevs, KBDQUESIZE);
    else
        numkeyevs = Keyboard_GetEvents(keyevs, KBDQUESIZE);

    // Convert to ddevents and post them.
    for(n = 0; n < numkeyevs; ++n)
    {
        keyevent_t *ke = &keyevs[n];

        // Check the type of the event.
        if(ke->type == IKE_DOWN)   // Key pressed?
        {
            ev.toggle.state = ETOG_DOWN;
        }
        else if(ke->type == IKE_UP) // Key released?
        {
            ev.toggle.state = ETOG_UP;
        }

        ev.toggle.id = ke->ddkey;

        // Text content to insert?
        assert(sizeof(ev.toggle.text) == sizeof(ke->text));
        memcpy(ev.toggle.text, ke->text, sizeof(ev.toggle.text));

        DEBUG_VERBOSE2_Message(("toggle.id: %i/%c [%s:%u]\n", ev.toggle.id, ev.toggle.id, ev.toggle.text, (uint)strlen(ev.toggle.text)));

        // Maintain the repeater table.
        if(ev.toggle.state == ETOG_DOWN)
        {
            // Find an empty repeater.
            for(k = 0; k < MAX_DOWNKEYS; ++k)
                if(!keyReps[k].key)
                {
                    keyReps[k].key = ev.toggle.id;
                    keyReps[k].native = ke->native;
                    memcpy(keyReps[k].text, ev.toggle.text, sizeof(ev.toggle.text));
                    keyReps[k].timer = sysTime;
                    keyReps[k].count = 0;
                    break;
                }
        }
        else if(ev.toggle.state == ETOG_UP)
        {
            DD_ClearKeyRepeaterForKey(ev.toggle.id, ke->native);
        }

        // Post the event.
        DD_PostEvent(&ev);
    }
}

/**
 * Change between normal and UI mousing modes.
 */
void I_SetUIMouseMode(boolean on)
{
    uiMouseMode = on;

    /// @todo  Update this after the Qt window management is working.

#if 0
#ifdef UNIX
    if(Mouse_IsPresent())
    {
        // Release mouse grab when in windowed mode.
        boolean isFullScreen = true;

        Sys_GetWindowFullscreen(1, &isFullScreen);
        if(!isFullScreen)
        {
            SDL_WM_GrabInput(on? SDL_GRAB_OFF : SDL_GRAB_ON);
        }
    }
#endif
#endif
}

/**
 * Checks the current mouse state (axis, buttons and wheel).
 * Generates events and mickeys and posts them.
 */
void DD_ReadMouse(void)
{
    ddevent_t       ev;
    mousestate_t    mouse;
    float           xpos, ypos;
    int             i;

    if(!Mouse_IsPresent())
        return;

#ifdef OLD_FILTER
    // Should we test the mouse input frequency?
    if(mouseFreq > 0)
    {
        static uint lastTime = 0;
        uint nowTime = Sys_GetRealTime();

        if(nowTime - lastTime < 1000/mouseFreq)
        {
            // Don't ask yet.
            memset(&mouse, 0, sizeof(mouse));
        }
        else
        {
            lastTime = nowTime;
            Mouse_GetState(&mouse);
        }
    }
    else
#endif
    {
        // Get the mouse state.
        Mouse_GetState(&mouse);
    }

    ev.device = IDEV_MOUSE;
    ev.type = E_AXIS;

    xpos = mouse.axis[IMA_POINTER].x;
    ypos = mouse.axis[IMA_POINTER].y;

    if(uiMouseMode)
    {
        ev.axis.type = EAXIS_ABSOLUTE;
    }
    else
    {
        ev.axis.type = EAXIS_RELATIVE;
        ypos = -ypos;
    }

    // Post an event per axis.
    // Don't post empty events.
    if(xpos)
    {
        ev.axis.id = 0;
        ev.axis.pos = xpos;
        DD_PostEvent(&ev);
    }
    if(ypos)
    {
        ev.axis.id = 1;
        ev.axis.pos = ypos;
        DD_PostEvent(&ev);
    }

    // Some very verbose output about mouse buttons.
    if(verbose >= 3)
    {
        for(i = 0; i < IMB_MAXBUTTONS; ++i)
            if(mouse.buttonDowns[i] || mouse.buttonUps[i])
                break;
        if(i < IMB_MAXBUTTONS)
        {
            for(i = 0; i < IMB_MAXBUTTONS; ++i)
                Con_Message("[%02i] %i/%i ", i, mouse.buttonDowns[i], mouse.buttonUps[i]);
            Con_Message("\n");
        }
    }

    // Post mouse button up and down events.
    ev.type = E_TOGGLE;
    for(i = 0; i < IMB_MAXBUTTONS; ++i)
    {
        ev.toggle.id = i;
        while(mouse.buttonDowns[i] > 0 || mouse.buttonUps[i] > 0)
        {
            if(mouse.buttonDowns[i]-- > 0)
            {
                ev.toggle.state = ETOG_DOWN;
                DEBUG_VERBOSE2_Message(("mb %i down\n", i));
                DD_PostEvent(&ev);
            }
            if(mouse.buttonUps[i]-- > 0)
            {
                ev.toggle.state = ETOG_UP;
                DEBUG_VERBOSE2_Message(("mb %i up\n", i));
                DD_PostEvent(&ev);
            }
        }
    }
}

/**
 * Checks the current joystick state (axis, sliders, hat and buttons).
 * Generates events and posts them. Axis clamps and dead zone is done
 * here.
 */
void DD_ReadJoystick(void)
{
    int             i;
    ddevent_t       ev;
    joystate_t      state;

    if(!Joystick_IsPresent())
        return;

    Joystick_GetState(&state);

    // Joystick buttons.
    ev.device = IDEV_JOY1;
    ev.type = E_TOGGLE;

    for(i = 0; i < state.numButtons; ++i)
    {
        ev.toggle.id = i;
        while(state.buttonDowns[i] > 0 || state.buttonUps[i] > 0)
        {
            if(state.buttonDowns[i]-- > 0)
            {
                ev.toggle.state = ETOG_DOWN;
                DD_PostEvent(&ev);
                DEBUG_VERBOSE2_Message(("Joy button %i down\n", i));
            }
            if(state.buttonUps[i]-- > 0)
            {
                ev.toggle.state = ETOG_UP;
                DD_PostEvent(&ev);
                DEBUG_VERBOSE2_Message(("Joy button %i up\n", i));
            }
        }
    }

    if(state.numHats > 0)
    {
        // Check for a POV change.
        // TODO: Some day, it would be nice to support multiple hats here. -jk
        if(state.hatAngle[0] != oldPOV)
        {
            ev.type = E_ANGLE;
            ev.angle.id = 0;

            if(state.hatAngle[0] < 0)
            {
                ev.angle.pos = -1;
            }
            else
            {
                // The new angle becomes active.
                ev.angle.pos = ROUND(state.hatAngle[0] / 45);
            }
            DD_PostEvent(&ev);

            oldPOV = state.hatAngle[0];
        }
    }

    // Send joystick axis events, one per axis.
    ev.type = E_AXIS;

    for(i = 0; i < state.numAxes; ++i)
    {
        ev.axis.id = i;
        ev.axis.pos = state.axis[i];
        ev.axis.type = EAXIS_ABSOLUTE;
        DD_PostEvent(&ev);
    }
}

#if _DEBUG
static void initDrawStateForVisual(const Point2Raw* origin)
{
    FR_PushAttrib();

    // Ignore zero offsets.
    if(origin && !(origin->x == 0 && origin->y == 0))
    {
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(origin->x, origin->y, 0);
    }
}

static void endDrawStateForVisual(const Point2Raw* origin)
{
    // Ignore zero offsets.
    if(origin && !(origin->x == 0 && origin->y == 0))
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    FR_PopAttrib();
}

void Rend_RenderKeyStateVisual(inputdev_t* device, uint keyID, const Point2Raw* _origin,
    RectRaw* geometry)
{
#define BORDER            4

    const Point2Raw textOffset = { BORDER, BORDER };
    const float upColor[] = { .3f, .3f, .3f, .6f };
    const float downColor[] = { .3f, .3f, 1, .6f };
    const float expiredMarkColor[] = { 1, 0, 0, 1 };
    const float triggeredMarkColor[] = { 1, 0, 1, 1 };
    const char* keyLabel = NULL;
    const inputdevkey_t* key;
    char keyLabelBuf[2];
    Point2Raw origin;
    RectRaw textGeom;
    ddstring_t label;

    if(geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    key = I_GetKeyByID(device, keyID+1);
    if(!key) return;

    origin.x = _origin? _origin->x : 0;
    origin.y = _origin? _origin->y : 0;

    Str_Init(&label);

    // Compose the key label.
    if(key->name)
    {
        // Use the symbolic name.
        keyLabel = key->name;
    }
    else if(device == I_GetDevice(IDEV_KEYBOARD, false))
    {
        // Perhaps a printable ASCII character?
        // Apply all active modifiers to the key.
        byte asciiCode = DD_ModKey((byte)keyID);
        if(asciiCode > 32 && asciiCode < 127)
        {
            keyLabelBuf[0] = asciiCode;
            keyLabelBuf[1] = '\0';
            keyLabel = keyLabelBuf;
        }

        // Is there symbolic name in the bindings system?
        if(!keyLabel)
        {
            keyLabel = B_ShortNameForKey2(keyID, false/*do not force lowercase*/);
        }
    }

    if(keyLabel)
        Str_Append(&label, keyLabel);
    else
        Str_Appendf(&label, "#%03u", keyID+1);

    initDrawStateForVisual(&origin);

    // Calculate the size of the visual according to the dimensions of the text.
    textGeom.origin.x = textGeom.origin.y = 0;
    FR_TextSize(&textGeom.size, Str_Text(&label));

    // Enlarge by BORDER pixels.
    textGeom.size.width  += BORDER * 2;
    textGeom.size.height += BORDER * 2;

    // Draw a background.
    glColor4fv(key->isDown? downColor : upColor);
    GL_DrawRect(&textGeom);

    // Draw the text.
    glEnable(GL_TEXTURE_2D);
    FR_DrawText(Str_Text(&label), &textOffset);
    glDisable(GL_TEXTURE_2D);

    // Mark expired?
    if(key->assoc.flags & IDAF_EXPIRED)
    {
        const int markSize = .5f + MIN_OF(textGeom.size.width, textGeom.size.height) / 3.f;

        glColor3fv(expiredMarkColor);
        glBegin(GL_TRIANGLES);
        glVertex2i(textGeom.size.width, 0);
        glVertex2i(textGeom.size.width, markSize);
        glVertex2i(textGeom.size.width-markSize, 0);
        glEnd();
    }

    // Mark triggered?
    if(key->assoc.flags & IDAF_TRIGGERED)
    {
        const int markSize = .5f + MIN_OF(textGeom.size.width, textGeom.size.height) / 3.f;

        glColor3fv(triggeredMarkColor);
        glBegin(GL_TRIANGLES);
        glVertex2i(0, 0);
        glVertex2i(markSize, 0);
        glVertex2i(0, markSize);
        glEnd();
    }

    endDrawStateForVisual(&origin);

    Str_Free(&label);

    if(geometry)
    {
        memcpy(&geometry->origin, &origin, sizeof(geometry->origin));
        memcpy(&geometry->size, &textGeom.size, sizeof(geometry->size));
    }

#undef BORDER
}

void Rend_RenderAxisStateVisual(inputdev_t* device, uint axisID, const Point2Raw* origin,
    RectRaw* geometry)
{
    const inputdevaxis_t* axis;

    if(geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    axis = I_GetAxisByID(device, axisID+1);
    if(!axis) return;

    initDrawStateForVisual(origin);

    endDrawStateForVisual(origin);
}

void Rend_RenderHatStateVisual(inputdev_t* device, uint hatID, const Point2Raw* origin,
    RectRaw* geometry)
{
    const inputdevhat_t* hat;

    if(geometry)
    {
        geometry->origin.x = geometry->origin.y = 0;
        geometry->size.width = geometry->size.height = 0;
    }

    hat = I_GetHatByID(device, hatID+1);
    if(!hat) return;

    initDrawStateForVisual(origin);

    endDrawStateForVisual(origin);
}

typedef struct {
    inputdev_controltype_t type;
    uint id;
} inputdev_layout_control_t;

typedef struct {
    inputdev_layout_control_t* controls;
    uint numControls;
} inputdev_layout_controlgroup_t;

// Defines the order of controls in the visual.
typedef struct {
    inputdev_layout_controlgroup_t* groups;
    uint numGroups;
} inputdev_layout_t;

static void drawControlGroup(inputdev_t* device, const inputdev_layout_controlgroup_t* group,
    Point2Raw* _origin, RectRaw* retGeometry)
{
#define SPACING               2

    Point2Raw origin;
    Rect* grpGeom = NULL;
    RectRaw ctrlGeom;
    uint i;

    if(retGeometry)
    {
        retGeometry->origin.x = retGeometry->origin.y = 0;
        retGeometry->size.width = retGeometry->size.height = 0;
    }

    if(!device || !group) return;

    origin.x = _origin? _origin->x : 0;
    origin.y = _origin? _origin->y : 0;

    for(i = 0; i < group->numControls; ++i)
    {
        const inputdev_layout_control_t* ctrl = group->controls + i;

        switch(ctrl->type)
        {
        case IDC_KEY:
            Rend_RenderKeyStateVisual(device, ctrl->id, &origin, &ctrlGeom);
            break;
        case IDC_AXIS:
            Rend_RenderAxisStateVisual(device, ctrl->id, &origin, &ctrlGeom);
            break;
        case IDC_HAT:
            Rend_RenderHatStateVisual(device, ctrl->id, &origin, &ctrlGeom);
            break;
        default:
            Con_Error("drawControlGroup: Unknown inputdev_controltype %i.", (int)ctrl->type);
            exit(1); // Unreachable.
        }

        if(ctrlGeom.size.width > 0 && ctrlGeom.size.height > 0)
        {
            origin.x += ctrlGeom.size.width + SPACING;

            if(grpGeom)
                Rect_UniteRaw(grpGeom, &ctrlGeom);
            else
                grpGeom = Rect_NewFromRaw(&ctrlGeom);
        }
    }

    if(grpGeom)
    {
        if(retGeometry)
        {
            Rect_Raw(grpGeom, retGeometry);
        }

        Rect_Delete(grpGeom);
    }

#undef SPACING
}

/**
 * Render a visual representation of the current state of the specified device.
 */
void Rend_RenderInputDeviceStateVisual(inputdev_t* device, const inputdev_layout_t* layout,
    const Point2Raw* origin, Size2Raw* retVisualDimensions)
{
#define SPACING               2

    Rect* visualGeom = NULL;
    Point2Raw offset;
    uint i;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(retVisualDimensions)
    {
        retVisualDimensions->width = retVisualDimensions->height = 0;
    }

    if(novideo || isDedicated) return; // Not for us.
    if(!device || !layout) return;

    // Init render state.
    FR_SetFont(fontFixed);
    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetColorAndAlpha(1, 1, 1, 1);
    initDrawStateForVisual(origin);

    offset.x = offset.y = 0;

    // Draw device name first.
    if(device->niceName)
    {
        Size2Raw size;

        glEnable(GL_TEXTURE_2D);
        FR_DrawText(device->niceName, NULL/*no offset*/);
        glDisable(GL_TEXTURE_2D);

        FR_TextSize(&size, device->niceName);
        visualGeom = Rect_NewWithOriginSize2(offset.x, offset.y, size.width, size.height);

        offset.y += size.height + SPACING;
    }

    // Draw control groups.
    for(i = 0; i < layout->numGroups; ++i)
    {
        const inputdev_layout_controlgroup_t* grp = layout->groups + i;
        RectRaw grpGeometry;

        drawControlGroup(device, grp, &offset, &grpGeometry);

        if(grpGeometry.size.width > 0 && grpGeometry.size.height > 0)
        {
            if(visualGeom)
                Rect_UniteRaw(visualGeom, &grpGeometry);
            else
                visualGeom = Rect_NewFromRaw(&grpGeometry);

            offset.y = Rect_Y(visualGeom) + Rect_Height(visualGeom) + SPACING;
        }
    }

    // Back to previous render state.
    endDrawStateForVisual(origin);
    FR_PopAttrib();

    // Return the united geometry dimensions?
    if(visualGeom && retVisualDimensions)
    {
        retVisualDimensions->width  = Rect_Width(visualGeom);
        retVisualDimensions->height = Rect_Height(visualGeom);
    }

#undef SPACING
}

void Rend_AllInputDeviceStateVisuals(void)
{
#define SPACING                    2
#define NUMITEMS(x)                (sizeof(x)/sizeof((x)[0]))

    // Keyboard (Standard US English layout):
    static inputdev_layout_control_t keyGroup1[] = {
        { IDC_KEY,  27 }, // escape
        { IDC_KEY, 132 }, // f1
        { IDC_KEY, 133 }, // f2
        { IDC_KEY, 134 }, // f3
        { IDC_KEY, 135 }, // f4
        { IDC_KEY, 136 }, // f5
        { IDC_KEY, 137 }, // f6
        { IDC_KEY, 138 }, // f7
        { IDC_KEY, 139 }, // f8
        { IDC_KEY, 140 }, // f9
        { IDC_KEY, 141 }, // f10
        { IDC_KEY, 142 }, // f11
        { IDC_KEY, 143 }  // f12
    };
    static inputdev_layout_control_t keyGroup2[] = {
        { IDC_KEY,  96 }, // tilde
        { IDC_KEY,  49 }, // 1
        { IDC_KEY,  50 }, // 2
        { IDC_KEY,  51 }, // 3
        { IDC_KEY,  52 }, // 4
        { IDC_KEY,  53 }, // 5
        { IDC_KEY,  54 }, // 6
        { IDC_KEY,  55 }, // 7
        { IDC_KEY,  56 }, // 8
        { IDC_KEY,  57 }, // 9
        { IDC_KEY,  48 }, // 0
        { IDC_KEY,  45 }, // -
        { IDC_KEY,  61 }, // =
        { IDC_KEY, 127 }  // backspace
    };
    static inputdev_layout_control_t keyGroup3[] = {
        { IDC_KEY,   9 }, // tab
        { IDC_KEY, 113 }, // q
        { IDC_KEY, 119 }, // w
        { IDC_KEY, 101 }, // e
        { IDC_KEY, 114 }, // r
        { IDC_KEY, 116 }, // t
        { IDC_KEY, 121 }, // y
        { IDC_KEY, 117 }, // u
        { IDC_KEY, 105 }, // i
        { IDC_KEY, 111 }, // o
        { IDC_KEY, 112 }, // p
        { IDC_KEY,  91 }, // {
        { IDC_KEY,  93 }, // }
        { IDC_KEY,  92 }, // bslash
    };
    static inputdev_layout_control_t keyGroup4[] = {
        { IDC_KEY, 145 }, // capslock
        { IDC_KEY,  97 }, // a
        { IDC_KEY, 115 }, // s
        { IDC_KEY, 100 }, // d
        { IDC_KEY, 102 }, // f
        { IDC_KEY, 103 }, // g
        { IDC_KEY, 104 }, // h
        { IDC_KEY, 106 }, // j
        { IDC_KEY, 107 }, // k
        { IDC_KEY, 108 }, // l
        { IDC_KEY,  59 }, // semicolon
        { IDC_KEY,  39 }, // apostrophe
        { IDC_KEY,  13 }  // return
    };
    static inputdev_layout_control_t keyGroup5[] = {
        { IDC_KEY, 159 }, // shift
        { IDC_KEY, 122 }, // z
        { IDC_KEY, 120 }, // x
        { IDC_KEY,  99 }, // c
        { IDC_KEY, 118 }, // v
        { IDC_KEY,  98 }, // b
        { IDC_KEY, 110 }, // n
        { IDC_KEY, 109 }, // m
        { IDC_KEY,  44 }, // comma
        { IDC_KEY,  46 }, // period
        { IDC_KEY,  47 }, // fslash
        { IDC_KEY, 159 }, // shift
    };
    static inputdev_layout_control_t keyGroup6[] = {
        { IDC_KEY, 160 }, // ctrl
        { IDC_KEY,   0 }, // ???
        { IDC_KEY, 161 }, // alt
        { IDC_KEY,  32 }, // space
        { IDC_KEY, 161 }, // alt
        { IDC_KEY,   0 }, // ???
        { IDC_KEY,   0 }, // ???
        { IDC_KEY, 160 }  // ctrl
    };
    static inputdev_layout_control_t keyGroup7[] = {
        { IDC_KEY, 170 }, // printscrn
        { IDC_KEY, 146 }, // scrolllock
        { IDC_KEY, 158 }  // pause
    };
    static inputdev_layout_control_t keyGroup8[] = {
        { IDC_KEY, 162 }, // insert
        { IDC_KEY, 166 }, // home
        { IDC_KEY, 164 }  // pageup
    };
    static inputdev_layout_control_t keyGroup9[] = {
        { IDC_KEY, 163 }, // delete
        { IDC_KEY, 167 }, // end
        { IDC_KEY, 165 }, // pagedown
    };
    static inputdev_layout_control_t keyGroup10[] = {
        { IDC_KEY, 130 }, // up
        { IDC_KEY, 129 }, // left
        { IDC_KEY, 131 }, // down
        { IDC_KEY, 128 }, // right
    };
    static inputdev_layout_control_t keyGroup11[] = {
        { IDC_KEY, 144 }, // numlock
        { IDC_KEY, 172 }, // divide
        { IDC_KEY, DDKEY_MULTIPLY }, // multiply
        { IDC_KEY, 168 }  // subtract
    };
    static inputdev_layout_control_t keyGroup12[] = {
        { IDC_KEY, 147 }, // pad 7
        { IDC_KEY, 148 }, // pad 8
        { IDC_KEY, 149 }, // pad 9
        { IDC_KEY, 169 }, // add
    };
    static inputdev_layout_control_t keyGroup13[] = {
        { IDC_KEY, 150 }, // pad 4
        { IDC_KEY, 151 }, // pad 5
        { IDC_KEY, 152 }  // pad 6
    };
    static inputdev_layout_control_t keyGroup14[] = {
        { IDC_KEY, 153 }, // pad 1
        { IDC_KEY, 154 }, // pad 2
        { IDC_KEY, 155 }, // pad 3
        { IDC_KEY, 171 }  // enter
    };
    static inputdev_layout_control_t keyGroup15[] = {
        { IDC_KEY, 156 }, // pad 0
        { IDC_KEY, 157 }  // pad .
    };
    static inputdev_layout_controlgroup_t keyGroups[] = {
        { keyGroup1, NUMITEMS(keyGroup1) },
        { keyGroup2, NUMITEMS(keyGroup2) },
        { keyGroup3, NUMITEMS(keyGroup3) },
        { keyGroup4, NUMITEMS(keyGroup4) },
        { keyGroup5, NUMITEMS(keyGroup5) },
        { keyGroup6, NUMITEMS(keyGroup6) },
        { keyGroup7, NUMITEMS(keyGroup7) },
        { keyGroup8, NUMITEMS(keyGroup8) },
        { keyGroup9, NUMITEMS(keyGroup9) },
        { keyGroup10, NUMITEMS(keyGroup10) },
        { keyGroup11, NUMITEMS(keyGroup11) },
        { keyGroup12, NUMITEMS(keyGroup12) },
        { keyGroup13, NUMITEMS(keyGroup13) },
        { keyGroup14, NUMITEMS(keyGroup14) },
        { keyGroup15, NUMITEMS(keyGroup15) }
    };
    static inputdev_layout_t keyLayout = { keyGroups, NUMITEMS(keyGroups) };

    // Mouse:
    static inputdev_layout_control_t mouseGroup1[] = {
        { IDC_AXIS, 0 }, // x-axis
        { IDC_AXIS, 1 }, // y-axis
    };
    static inputdev_layout_control_t mouseGroup2[] = {
        { IDC_KEY, 0 }, // left
        { IDC_KEY, 1 }, // middle
        { IDC_KEY, 2 }  // right
    };
    static inputdev_layout_control_t mouseGroup3[] = {
        { IDC_KEY, 3 }, // wheelup
        { IDC_KEY, 4 }  // wheeldown
    };
    static inputdev_layout_control_t mouseGroup4[] = {
        { IDC_KEY,  5 },
        { IDC_KEY,  6 },
        { IDC_KEY,  7 },
        { IDC_KEY,  8 },
        { IDC_KEY,  9 },
        { IDC_KEY, 10 },
        { IDC_KEY, 11 },
        { IDC_KEY, 12 },
        { IDC_KEY, 13 },
        { IDC_KEY, 14 },
        { IDC_KEY, 15 }
    };
    static inputdev_layout_controlgroup_t mouseGroups[] = {
        { mouseGroup1, NUMITEMS(mouseGroup1) },
        { mouseGroup2, NUMITEMS(mouseGroup2) },
        { mouseGroup3, NUMITEMS(mouseGroup3) },
        { mouseGroup4, NUMITEMS(mouseGroup4) }
    };
    static inputdev_layout_t mouseLayout = { mouseGroups, NUMITEMS(mouseGroups) };

    // Joystick:
    static inputdev_layout_control_t joyGroup1[] = {
        { IDC_AXIS, 0 }, // x-axis
        { IDC_AXIS, 1 }, // y-axis
        { IDC_AXIS, 2 }, // z-axis
        { IDC_AXIS, 3 }  // w-axis
    };
    static inputdev_layout_control_t joyGroup2[] = {
        { IDC_AXIS,  4 },
        { IDC_AXIS,  5 },
        { IDC_AXIS,  6 },
        { IDC_AXIS,  7 },
        { IDC_AXIS,  8 },
        { IDC_AXIS,  9 },
        { IDC_AXIS, 10 },
        { IDC_AXIS, 11 },
        { IDC_AXIS, 12 },
        { IDC_AXIS, 13 },
        { IDC_AXIS, 14 },
        { IDC_AXIS, 15 },
        { IDC_AXIS, 16 },
        { IDC_AXIS, 17 }
    };
    static inputdev_layout_control_t joyGroup3[] = {
        { IDC_AXIS, 18 },
        { IDC_AXIS, 19 },
        { IDC_AXIS, 20 },
        { IDC_AXIS, 21 },
        { IDC_AXIS, 22 },
        { IDC_AXIS, 23 },
        { IDC_AXIS, 24 },
        { IDC_AXIS, 25 },
        { IDC_AXIS, 26 },
        { IDC_AXIS, 27 },
        { IDC_AXIS, 28 },
        { IDC_AXIS, 29 },
        { IDC_AXIS, 30 },
        { IDC_AXIS, 31 }
    };
    static inputdev_layout_control_t joyGroup4[] = {
        { IDC_HAT,  0 },
        { IDC_HAT,  1 },
        { IDC_HAT,  2 },
        { IDC_HAT,  3 }
    };
    static inputdev_layout_control_t joyGroup5[] = {
        { IDC_KEY,  0 },
        { IDC_KEY,  1 },
        { IDC_KEY,  2 },
        { IDC_KEY,  3 },
        { IDC_KEY,  4 },
        { IDC_KEY,  5 },
        { IDC_KEY,  6 },
        { IDC_KEY,  7 }
    };
    static inputdev_layout_control_t joyGroup6[] = {
        { IDC_KEY,  8 },
        { IDC_KEY,  9 },
        { IDC_KEY, 10 },
        { IDC_KEY, 11 },
        { IDC_KEY, 12 },
        { IDC_KEY, 13 },
        { IDC_KEY, 14 },
        { IDC_KEY, 15 },
    };
    static inputdev_layout_control_t joyGroup7[] = {
        { IDC_KEY, 16 },
        { IDC_KEY, 17 },
        { IDC_KEY, 18 },
        { IDC_KEY, 19 },
        { IDC_KEY, 20 },
        { IDC_KEY, 21 },
        { IDC_KEY, 22 },
        { IDC_KEY, 23 },
    };
    static inputdev_layout_control_t joyGroup8[] = {
        { IDC_KEY, 24 },
        { IDC_KEY, 25 },
        { IDC_KEY, 26 },
        { IDC_KEY, 27 },
        { IDC_KEY, 28 },
        { IDC_KEY, 29 },
        { IDC_KEY, 30 },
        { IDC_KEY, 31 },
    };
    static inputdev_layout_controlgroup_t joyGroups[] = {
        { joyGroup1, NUMITEMS(joyGroup1) },
        { joyGroup2, NUMITEMS(joyGroup2) },
        { joyGroup3, NUMITEMS(joyGroup3) },
        { joyGroup4, NUMITEMS(joyGroup4) },
        { joyGroup5, NUMITEMS(joyGroup5) },
        { joyGroup6, NUMITEMS(joyGroup6) },
        { joyGroup7, NUMITEMS(joyGroup7) },
        { joyGroup8, NUMITEMS(joyGroup8) }
    };
    static inputdev_layout_t joyLayout = { joyGroups, NUMITEMS(joyGroups) };

    Point2Raw origin = { 2, 2 };
    Size2Raw dimensions;

    if(novideo || isDedicated) return; // Not for us.

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    // Disabled?
    if(!devRendKeyState && !devRendMouseState && !devRendJoyState) return;

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, Window_Width(theWindow), Window_Height(theWindow), 0, -1, 1);

    if(devRendKeyState)
    {
        Rend_RenderInputDeviceStateVisual(I_GetDevice(IDEV_KEYBOARD, false), &keyLayout, &origin, &dimensions);
        origin.y += dimensions.height + SPACING;
    }

    if(devRendMouseState)
    {
        Rend_RenderInputDeviceStateVisual(I_GetDevice(IDEV_MOUSE, false), &mouseLayout, &origin, &dimensions);
        origin.y += dimensions.height + SPACING;
    }

    if(devRendJoyState)
    {
        Rend_RenderInputDeviceStateVisual(I_GetDevice(IDEV_JOY1, false), &joyLayout, &origin, &dimensions);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

#undef NUMITEMS
#undef SPACING
}
#endif

static void I_PrintAxisConfig(inputdev_t* device, inputdevaxis_t* axis)
{
    Con_Printf("%s-%s Config:\n"
               "  Type: %s\n"
               //"  Filter: %i\n"
               "  Dead Zone: %g\n"
               "  Scale: %g\n"
               "  Flags: (%s%s)\n",
               device->name, axis->name,
               (axis->type == IDAT_STICK? "STICK" : "POINTER"),
               /*axis->filter,*/
               axis->deadZone, axis->scale,
               ((axis->flags & IDA_DISABLED)? "|disabled":""),
               ((axis->flags & IDA_INVERT)? "|inverted":""));
}

#if 0
D_CMD(AxisPrintConfig)
{
    uint deviceID, axisID;
    inputdev_t* device;
    inputdevaxis_t* axis;

    if(!I_ParseDeviceAxis(argv[1], &deviceID, &axisID))
    {
        Con_Printf("'%s' is not a valid device or device axis.\n", argv[1]);
        return false;
    }

    device = I_GetDevice(deviceID, false);
    axis   = I_GetAxisByID(device, axisID);
    I_PrintAxisConfig(device, axis);

    return true;
}

D_CMD(AxisChangeOption)
{
    uint deviceID, axisID;
    inputdev_t* device;
    inputdevaxis_t* axis;

    if(!I_ParseDeviceAxis(argv[1], &deviceID, &axisID))
    {
        Con_Printf("'%s' is not a valid device or device axis.\n", argv[1]);
        return false;
    }

    device = I_GetDevice(deviceID, false);
    axis   = I_GetAxisByID(device, axisID);

    // Options:
    if(!stricmp(argv[2], "disable") || !stricmp(argv[2], "off"))
    {
        axis->flags |= IDA_DISABLED;
    }
    else if(!stricmp(argv[2], "enable") || !stricmp(argv[2], "on"))
    {
        axis->flags &= ~IDA_DISABLED;
    }
    else if(!stricmp(argv[2], "invert")) // toggle
    {
        axis->flags ^= IDA_INVERT;
    }

    // Unknown option name.
    return true;
}

D_CMD(AxisChangeValue)
{
    uint deviceID, axisID;
    inputdevaxis_t* axis;
    inputdev_t* device;

    if(!I_ParseDeviceAxis(argv[1], &deviceID, &axisID))
    {
        Con_Printf("'%s' is not a valid device or device axis.\n", argv[1]);
        return false;
    }

    device = I_GetDevice(deviceID, false);
    axis   = I_GetAxisByID(device, axisID);
    if(axis)
    {
        // Values:
        if(!stricmp(argv[2], "filter"))
        {
            axis->filter = strtod(argv[3], 0);
        }
        else if(!stricmp(argv[2], "deadzone") || !stricmp(argv[2], "dead zone"))
        {
            axis->deadZone = strtod(argv[3], 0);
        }
        else if(!stricmp(argv[2], "scale"))
        {
            axis->scale = strtod(argv[3], 0);
        }
    }

    // Unknown value name?
    return true;
}
#endif

/**
 * Console command to list all of the available input devices+axes.
 */
D_CMD(ListInputDevices)
{
    inputdev_t* dev;
    uint i, j;

    Con_Printf("Input Devices:\n");
    for(i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        dev = &inputDevices[i];
        if(!dev->name || !(dev->flags & ID_ACTIVE))
            continue;

        Con_Printf("%s (%i keys, %i axes)\n", dev->name, dev->numKeys, dev->numAxes);

        for(j = 0; j < dev->numAxes; ++j)
        {
            Con_Printf("  Axis #%i: %s\n", j, dev->axes[j].name);
            I_PrintAxisConfig(dev, &dev->axes[j]);
        }
    }
    return true;
}

D_CMD(ReleaseMouse)
{
    Window_TrapMouse(Window_Main(), false);
    return true;
}
