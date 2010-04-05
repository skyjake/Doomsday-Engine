/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * dd_input.c: System Independent Input
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"
#include "de_ui.h"

#include "gl_main.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_JOYSTICK_DEADZONE .05f // 5%

#define MAX_AXIS_FILTER 40

#define KBDQUESIZE      32
#define MAX_DOWNKEYS    16      // Most keyboards support 6 or 7.

// TYPES -------------------------------------------------------------------

typedef struct repeater_s {
    int     key;                // The DDKEY code (0 if not in use).
    timespan_t timer;           // How's the time?
    int     count;              // How many times has been repeated?
} repeater_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(AxisPrintConfig);
D_CMD(AxisChangeOption);
D_CMD(AxisChangeValue);
D_CMD(DumpKeyMap);
D_CMD(KeyMap);
D_CMD(ListInputDevices);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean ignoreInput = false;

int     mouseFilter = 1;        // Filtering on by default.

// The initial and secondary repeater delays (tics).
int     repWait1 = 15, repWait2 = 3;
int     keyRepeatDelay1 = 430, keyRepeatDelay2 = 85;    // milliseconds
unsigned int  mouseFreq = 0;
boolean shiftDown = false, altDown = false;

static byte shiftKeyMappings[NUMKKEYS], altKeyMappings[NUMKKEYS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*static*/ inputdev_t inputDevices[NUM_INPUT_DEVICES];

static ddevent_t events[MAXEVENTS];
static int eventhead;
static int eventtail;

static char defaultShiftTable[96] = // Contains characters 32 to 127.
{
/* 32 */    ' ', 0, 0, 0, 0, 0, 0, '"',
/* 40 */    0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
/* 50 */    '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
/* 60 */    0, '+', 0, 0, 0, 'a', 'b', 'c', 'd', 'e',
/* 70 */    'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
/* 80 */    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
/* 90 */    'z', '{', '|', '}', 0, 0, 0, 'A', 'B', 'C',
/* 100 */   'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
/* 110 */   'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
/* 120 */   'X', 'Y', 'Z', 0, 0, 0, 0, 0
};
/* *INDENT-ON* */

static repeater_t keyReps[MAX_DOWNKEYS];
//static int oldJoyBState = 0;
static float oldPOV = IJOY_POV_CENTER;
static char* eventStrings[MAXEVENTS];
static boolean uiMouseMode = false; // Can mouse data be modified?

// CODE --------------------------------------------------------------------

void DD_RegisterInput(void)
{
    // Cvars
    C_VAR_INT("input-key-delay1", &keyRepeatDelay1, CVF_NO_MAX, 50, 0);
    C_VAR_INT("input-key-delay2", &keyRepeatDelay2, CVF_NO_MAX, 20, 0);

    C_VAR_INT("input-mouse-filter", &mouseFilter, 0, 0, MAX_AXIS_FILTER - 1);
    C_VAR_INT("input-mouse-frequency", &mouseFreq, CVF_NO_MAX, 0, 0);

    // Ccmds
#if 0
    C_CMD("dumpkeymap", "s", DumpKeyMap);
    C_CMD("keymap", "s", KeyMap);
#endif
    C_CMD("listinputdevices", "", ListInputDevices);
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
    int         i;
    inputdev_t *dev;
    inputdevaxis_t *axis;

    memset(inputDevices, 0, sizeof(inputDevices));

    // The keyboard is always assumed to be present.
    // DDKEYs are used as key indices.
    dev = &inputDevices[IDEV_KEYBOARD];
    dev->flags = ID_ACTIVE;
    strcpy(dev->name, "key");
    I_DeviceAllocKeys(dev, 256);

    // The mouse may not be active.
    dev = &inputDevices[IDEV_MOUSE];
    strcpy(dev->name, "mouse");
    I_DeviceAllocKeys(dev, IMB_MAXBUTTONS);

    // The first five mouse buttons have symbolic names.
    dev->keys[0].name = "left";
    dev->keys[1].name = "middle";
    dev->keys[2].name = "right";
    dev->keys[3].name = "wheelup";
    dev->keys[4].name = "wheeldown";

    // The mouse wheel is translated to keys, so there is no need to
    // create an axis for it.
    axis = I_DeviceNewAxis(dev, "x", IDAT_POINTER);
    axis->filter = 1; // On by default.
    axis->scale = 1.f/1000;

    axis = I_DeviceNewAxis(dev, "y", IDAT_POINTER);
    axis->filter = 1; // On by default.
    axis->scale = 1.f/1000;

    // Register console variables for the axis settings.
    // CAUTION: Allocating new axes may invalidate the pointers here.
    C_VAR_FLOAT("input-mouse-x-scale", &dev->axes[0].scale, CVF_NO_MAX, 0, 0);
    C_VAR_INT("input-mouse-x-flags", &dev->axes[0].flags, 0, 0, 3);
    C_VAR_FLOAT("input-mouse-y-scale", &dev->axes[1].scale, CVF_NO_MAX, 0, 0);
    C_VAR_INT("input-mouse-y-flags", &dev->axes[1].flags, 0, 0, 3);

    if(I_MousePresent())
        dev->flags = ID_ACTIVE;

    // TODO: Add support for several joysticks.
    dev = &inputDevices[IDEV_JOY1];
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
    if(I_JoystickPresent())
        dev->flags = ID_ACTIVE;
}

/**
 * Free the memory allocated for the input devices.
 */
void I_ShutdownInputDevices(void)
{
    uint                i;
    inputdev_t*         dev;

    for(i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        dev = &inputDevices[i];

        if(dev->keys)
            M_Free(dev->keys);
        dev->keys = 0;

        if(dev->axes)
            M_Free(dev->axes);
        dev->axes = 0;

        if(dev->hats)
            M_Free(dev->hats);
        dev->hats = 0;
    }
}

void I_DeviceReset(uint ident)
{
    inputdev_t*         dev = &inputDevices[ident];
    int                 k;

    for(k = 0; k < dev->numAxes; ++k)
    {
        if(dev->axes[k].type == IDAT_POINTER)
        {
            // Clear the accumulation.
            dev->axes[k].position = 0;
        }
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

/**
 * Retrieve a ptr to the device axis specified by id.
 *
 * @param device        Ptr to input device info, to get the axis ptr from.
 * @param id            Axis index, to search for.
 *
 * @return              Ptr to the device axis OR @c NULL, if not
 *                      found.
 */
inputdevaxis_t *I_GetAxisByID(inputdev_t *device, uint id)
{
    if(!device || id > device->numAxes - 1)
        return NULL;

    return &device->axes[id-1];
}

/**
 * Retrieve the index of a device's axis by name.
 *
 * @param device        Ptr to input device info, to get the axis index from.
 * @param name          Ptr to string containing the name to be searched for.
 *
 * @return              Index of the device axis named; or -1, if not found.
 */
int I_GetAxisByName(inputdev_t *device, const char *name)
{
    uint         i;

    for(i = 0; i < device->numAxes; ++i)
    {
        if(!stricmp(device->axes[i].name, name))
            return i;
    }
    return -1;
}

int I_GetKeyByName(inputdev_t* device, const char* name)
{
    int         i;

    for(i = 0; i < device->numKeys; ++i)
    {
        if(device->keys[i].name && !stricmp(device->keys[i].name, name))
            return i;
    }
    return -1;
}

/**
 * Check through the axes registered for the given device, see if there is
 * one identified by the given name.
 *
 * @return              @c false, if the string is invalid.
 */
boolean I_ParseDeviceAxis(const char* str, uint* deviceID, uint* axis)
{
    char                name[30], *ptr;
    inputdev_t*         device;

    ptr = strchr(str, '-');
    if(!ptr)
        return false;

    // The name of the device.
    memset(name, 0, sizeof(name));
    strncpy(name, str, ptr - str);
    device = I_GetDeviceByName(name, false);
    if(device == NULL)
        return false;
    if(*deviceID)
        *deviceID = device - inputDevices;

    // The axis name.
    if(*axis)
    {
        int                 a = I_GetAxisByName(device, ptr + 1);
        if((*axis = a) < 0)
            return false;

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

/**
 * Update an input device axis.  Transformation is applied.
 */
static void I_UpdateAxis(inputdev_t *dev, uint axis, float pos, timespan_t ticLength)
{
    inputdevaxis_t *a = &dev->axes[axis];
    float oldRealPos = a->realPosition;
    float transformed = I_TransformAxis(dev, axis, pos);

    // The unfiltered position.
    a->realPosition = transformed;

    if(oldRealPos != a->realPosition)
    {
        // Mark down the time of the change.
        a->time = Sys_GetRealTime();
    }

    if(a->filter > 0)
    {
        pos = a->realPosition;
    }
    else
    {
        // This is the new axis position.
        pos = a->realPosition;
    }

    if(a->type == IDAT_STICK)
        a->position = pos; //a->realPosition;
    else // Cumulative.
        a->position += pos; //a->realPosition;

    // We can clear the expiration when it returns to default state.
    if(!a->position || a->type == IDAT_POINTER)
    {
        a->assoc.flags &= ~IDAF_EXPIRED;
    }

/*    if(verbose > 3)
    {
        Con_Message("I_UpdateAxis: device=%s axis=%i pos=%f\n",
                    dev->name, axis, pos);
    }*/
}

/**
 * Update the input device state table.
 */
void I_TrackInput(ddevent_t *ev, timespan_t ticLength)
{
    inputdev_t *dev;

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
                altDown = true;
            else if(ev->toggle.state == ETOG_UP)
                altDown = false;
        }
    }

    // Update the state table.
    if(ev->type == E_AXIS)
    {
        I_UpdateAxis(dev, ev->axis.id, ev->axis.pos, ticLength);
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

        // We can clear the expiration when the key is released.
        if(!key->isDown)
        {
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
    uint            i, j;
    inputdev_t     *dev;

    for(i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        dev = &inputDevices[i];

        // Keys.
        for(j = 0; j < dev->numKeys; ++j)
        {
            dev->keys[j].assoc.prevBContext = dev->keys[j].assoc.bContext;
            dev->keys[j].assoc.bContext = NULL;
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

/**
 * @return          The key state from the downKeys array.
 */
boolean I_IsDeviceKeyDown(uint ident, uint code)
{
    inputdev_t     *dev;

    if((dev = I_GetDevice(ident, true)) != NULL)
    {
        if(code >= dev->numKeys)
            return false;

        return dev->keys[code].isDown;
    }

    return false;
}

/**
 * @return          Either key number or the scan code for the given token.
 */
int DD_KeyOrCode(char *token)
{
    char   *end = M_FindWhite(token);

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
    int     i;

    for(i = 0; i < 256; ++i)
    {
        shiftKeyMappings[i] = i >= 32 && i <= 127 &&
            defaultShiftTable[i - 32] ? defaultShiftTable[i - 32] : i;
        altKeyMappings[i] = i;
    }
}

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

/**
 * Clear the input event queue.
 */
void DD_ClearEvents(void)
{
    eventhead = eventtail;

    DD_ClearEventStrings();
}

/**
 * Called by the I/O functions when input is detected.
 */
void DD_PostEvent(ddevent_t *ev)
{
    events[eventhead] = *ev;

    if(ev->type == E_SYMBOLIC)
    {
        // Allocate a throw-away string from our buffer.
        events[eventhead].symbolic.name = DD_AllocEventString(ev->symbolic.name);
    }

    eventhead++;
    eventhead &= MAXEVENTS - 1;
}

/**
 * Get the next event from the input event queue.  Returns NULL if no
 * more events are available.
 */
static ddevent_t *DD_GetEvent(void)
{
    ddevent_t *ev;

    if(eventhead == eventtail)
        return NULL;

    ev = &events[eventtail];
    eventtail = (eventtail + 1) & (MAXEVENTS - 1);

    return ev;
}

void DD_ConvertEvent(const ddevent_t* ddEvent, event_t* ev)
{
    // Copy the essentials into a cutdown version for the game.
    // Ensure the format stays the same for future compatibility!
    //
    // FIXME: This is probably broken! (DD_MICKEY_ACCURACY=1000 no longer used...)
    //
    memset(ev, 0, sizeof(ev));
    if(ddEvent->type == E_SYMBOLIC)
    {
        ev->type = EV_SYMBOLIC;
        ev->data1 = ((int64_t) ddEvent->symbolic.name) & 0xffffffff; // low dword
        ev->data2 = ((int64_t) ddEvent->symbolic.name) >> 32; // high dword
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

/**
 * Send all the events of the given timestamp down the responder chain.
 */
static void dispatchEvents(timespan_t ticLength)
{
    ddevent_t *ddev;
    event_t     ev;

    while((ddev = DD_GetEvent()) != NULL)
    {
        if(ignoreInput)
            continue;

        // Update the state of the input device tracking table.
        I_TrackInput(ddev, ticLength);

        DD_ConvertEvent(ddev, &ev);

        // Does the special responder use this event?
        if(gx.PrivilegedResponder)
            if(gx.PrivilegedResponder(&ev))
                continue;

        if(UI_Responder(ddev))
            continue;
        // The console.
        if(Con_Responder(ddev))
            continue;

        // The game responder only returns true if the bindings
        // can't be used (like when chatting).
        if(gx.G_Responder(&ev))
            continue;

        // The bindings responder.
        if(B_Responder(ddev))
            continue;

        // The "fallback" responder. Gets the event if no one else is
        // interested.
        if(gx.FallbackResponder)
            gx.FallbackResponder(&ev);
    }
}

/**
 * Poll all event sources (i.e., input devices) and post events.
 */
static void postEvents(timespan_t ticLength)
{
    DD_ReadKeyboard();

    if(!isDedicated)
    {   // In dedicated mode, we don't do mice or joysticks.
        DD_ReadMouse(ticLength);
        DD_ReadJoystick();
    }
}

/**
 * Process all incoming input for the given timestamp.
 *
 * This gets called at least 35 times per second. Usually more frequently
 * than that.
 */
void DD_ProcessEvents(timespan_t ticLength)
{
    // Poll all event sources (i.e., input devices) and post events.
    postEvents(ticLength);

    // Despatch all accumulated events down the responder chain.
    dispatchEvents(ticLength);
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
    return key;
}

/**
 * Clears the repeaters array.
 */
void DD_ClearKeyRepeaters(void)
{
    memset(keyReps, 0, sizeof(keyReps));
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

    for(i = 0; i < MAX_DOWNKEYS; ++i)
    {
        repeater_t *rep = keyReps + i;
        if(!rep->key)
            continue;

        ev.toggle.id = rep->key;

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
    if(isDedicated)
        numkeyevs = I_GetConsoleKeyEvents(keyevs, KBDQUESIZE);
    else
        numkeyevs = I_GetKeyEvents(keyevs, KBDQUESIZE);

    // Convert to ddevents and post them.
    for(n = 0; n < numkeyevs; ++n)
    {
        keyevent_t     *ke = &keyevs[n];

        // Check the type of the event.
        if(ke->event == IKE_KEY_DOWN)   // Key pressed?
        {
            ev.toggle.state = ETOG_DOWN;
        }
        else if(ke->event == IKE_KEY_UP) // Key released?
        {
            ev.toggle.state = ETOG_UP;
        }

        ev.toggle.id = ke->ddkey;

        // Maintain the repeater table.
        if(ev.toggle.state == ETOG_DOWN)
        {
            // Find an empty repeater.
            for(k = 0; k < MAX_DOWNKEYS; ++k)
                if(!keyReps[k].key)
                {
                    keyReps[k].key = ev.toggle.id;
                    keyReps[k].timer = sysTime;
                    keyReps[k].count = 0;
                    break;
                }
        }
        else if(ev.toggle.state == ETOG_UP)
        {
            // Clear any repeaters with this key.
            for(k = 0; k < MAX_DOWNKEYS; ++k)
                if(keyReps[k].key == ev.toggle.id)
                    keyReps[k].key = 0;
        }

        // Post the event.
        DD_PostEvent(&ev);
    }
}

float I_FilterMouse(float pos, float* accumulation, float ticLength)
{
    float   target;
    int     dir;
    float   avail;
    int     used;

    *accumulation += pos;
    dir = SIGN_OF(*accumulation);
    avail = fabs(*accumulation);

    // Determine the target velocity.
    target = avail * (MAX_AXIS_FILTER - mouseFilter);

    // Determine the amount of mickeys to send. It depends on the
    // current mouse velocity, and how much time has passed.
    used = target * ticLength;

    // Don't go over the available number of update frames.
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

/**
 * Change between normal and UI mousing modes.
 */
void I_SetUIMouseMode(boolean on)
{
    uiMouseMode = on;

#ifdef UNIX
    if(I_MousePresent())
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
}

/**
 * Checks the current mouse state (axis, buttons and wheel).
 * Generates events and mickeys and posts them.
 */
void DD_ReadMouse(timespan_t ticLength)
{
    ddevent_t       ev;
    mousestate_t    mouse;
    float           xpos, ypos;
    int             i;

    if(!I_MousePresent())
        return;

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
            I_GetMouseState(&mouse);
        }
    }
    else
    {
        // Get the mouse state.
        I_GetMouseState(&mouse);
    }

    ev.device = IDEV_MOUSE;
    ev.type = E_AXIS;
    ev.axis.type = EAXIS_RELATIVE;
    xpos = mouse.x;
    ypos = mouse.y;

    if(mouseFilter > 0)
    {
        // Filtering ensures that events are sent more evenly on each frame.
        static float accumulation[2] = { 0, 0 };
        xpos = I_FilterMouse(xpos, &accumulation[0], ticLength);
        ypos = I_FilterMouse(ypos, &accumulation[1], ticLength);
    }

    // Mouse axis data may be modified if not in UI mode.
/*
    if(uiMouseMode)
    {
        if(mouseDisableX)
            xpos = 0;
        if(mouseDisableY)
            ypos = 0;
        if(!mouseInverseY)
            ypos = -ypos;
    }
    */
    if(uiMouseMode)
    {
        // Scale the movement depending on screen resolution.
        xpos *= MAX_OF(1, theWindow->width / 800.0f);
        ypos *= MAX_OF(1, theWindow->height / 600.0f);
    }
    else
    {
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
                DD_PostEvent(&ev);
            }
            if(mouse.buttonUps[i]-- > 0)
            {
                ev.toggle.state = ETOG_UP;
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

    if(!I_JoystickPresent())
        return;

    I_GetJoystickState(&state);

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
            }
            if(state.buttonUps[i]-- > 0)
            {
                ev.toggle.state = ETOG_UP;
                DD_PostEvent(&ev);
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
                ev.angle.pos = (int) (state.hatAngle[0] / 45 + .5); // Round off correctly w/.5.
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

static void I_PrintAxisConfig(inputdev_t *device, inputdevaxis_t *axis)
{
    Con_Printf("%s-%s Config:\n"
               "  Type: %s\n"
               "  Filter: %i\n"
               "  Dead Zone: %g\n"
               "  Scale: %g\n"
               "  Flags: (%s%s)\n",
               device->name, axis->name,
               (axis->type == IDAT_STICK? "STICK" : "POINTER"),
               axis->filter, axis->deadZone, axis->scale,
               ((axis->flags & IDA_DISABLED)? "|disabled":""),
               ((axis->flags & IDA_INVERT)? "|inverted":""));
}

D_CMD(AxisPrintConfig)
{
    uint        deviceID, axisID;
    inputdev_t *device;
    inputdevaxis_t *axis;

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
    uint        deviceID, axisID;
    inputdev_t *device;
    inputdevaxis_t *axis;

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
    uint        deviceID, axisID;
    inputdev_t *device;
    inputdevaxis_t *axis;

    if(!I_ParseDeviceAxis(argv[1], &deviceID, &axisID))
    {
        Con_Printf("'%s' is not a valid device or device axis.\n", argv[1]);
        return false;
    }

    device = I_GetDevice(deviceID, false);
    axis   = I_GetAxisByID(device, axisID);

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

    // Unknown value name.
    return true;
}

/**
 * Console command to list all of the available input devices+axes.
 */
D_CMD(ListInputDevices)
{
    uint        i, j;
    inputdev_t *dev;

    Con_Printf("Input Devices:\n");
    for(i = 0; i < NUM_INPUT_DEVICES; ++i)
    {
        dev = &inputDevices[i];
        if(!dev->name || !(dev->flags & ID_ACTIVE))
            continue;

        Con_Printf("%s (%i keys, %i axes)\n", dev->name, dev->numKeys,
                   dev->numAxes);
        for(j = 0; j < dev->numAxes; ++j)
        {
            Con_Printf("  Axis #%i: %s\n", j, dev->axes[j].name);
            I_PrintAxisConfig(dev, &dev->axes[j]);
        }
    }
    return true;
}
