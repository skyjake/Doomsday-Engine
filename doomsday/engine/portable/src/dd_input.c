/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

/*
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

#define MAX_AXIS_FILTER .6     // Guess

#define KBDQUESIZE      32
#define MAX_DOWNKEYS    16      // Most keyboards support 6 or 7.
#define NUMKKEYS        256

#define CLAMP(x) DD_JoyAxisClamp(&x)    //x = (x < -100? -100 : x > 100? 100 : x)

// TYPES -------------------------------------------------------------------

typedef struct repeater_s {
    int     key;                // The H2 key code (0 if not in use).
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

/*
int     mouseFilter = 1;        // Filtering on by default.
int     mouseInverseY = false;
int     mouseWheelSensi = 10;   // I'm shooting in the dark here.
int     joySensitivity = 5;
int     joyDeadZone = 10;
*/
boolean allowMouseMod = true; // can mouse data be modified?

// The initial and secondary repeater delays (tics).
int     repWait1 = 15, repWait2 = 3;
int     keyRepeatDelay1 = 430, keyRepeatDelay2 = 85;    // milliseconds
//int     mouseDisableX = false, mouseDisableY = false;
unsigned int  mouseFreq = 0;
boolean shiftDown = false, altDown = false;
byte    showScanCodes = false;

// A customizable mapping of the scantokey array.
char    keyMapPath[NUMKKEYS] = "}Data\\KeyMaps\\";
byte    keyMappings[NUMKKEYS];
byte    shiftKeyMappings[NUMKKEYS], altKeyMappings[NUMKKEYS];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*static*/ inputdev_t inputDevices[NUM_INPUT_DEVICES];

static byte showMouseInfo = false;

static ddevent_t events[MAXEVENTS];
static int eventhead;
static int eventtail;

/* *INDENT-OFF* */
static byte scantokey[NUMKKEYS] =
{
//  0               1           2               3               4           5                   6               7
//  8               9           A               B               C           D                   E               F
// 0
    0  ,            27,         '1',            '2',            '3',        '4',                '5',            '6',
    '7',            '8',        '9',            '0',            '-',        '=',                DDKEY_BACKSPACE,9,          // 0
// 1
    'q',            'w',        'e',            'r',            't',        'y',                'u',            'i',
    'o',            'p',        '[',            ']',            13 ,        DDKEY_RCTRL,        'a',            's',        // 1
// 2
    'd',            'f',        'g',            'h',            'j',        'k',                'l',            ';',
    39 ,            '`',        DDKEY_RSHIFT,   92,             'z',        'x',                'c',            'v',        // 2
// 3
    'b',            'n',        'm',            ',',            '.',        '/',                DDKEY_RSHIFT,   '*',
    DDKEY_RALT,     ' ',        0  ,            DDKEY_F1,       DDKEY_F2,   DDKEY_F3,           DDKEY_F4,       DDKEY_F5,   // 3
// 4
    DDKEY_F6,       DDKEY_F7,   DDKEY_F8,       DDKEY_F9,       DDKEY_F10,  DDKEY_NUMLOCK,      DDKEY_SCROLL,   DDKEY_NUMPAD7,
    DDKEY_NUMPAD8,  DDKEY_NUMPAD9, '-',         DDKEY_NUMPAD4,  DDKEY_NUMPAD5, DDKEY_NUMPAD6,   '+',            DDKEY_NUMPAD1, // 4
// 5
    DDKEY_NUMPAD2,  DDKEY_NUMPAD3, DDKEY_NUMPAD0, DDKEY_DECIMAL,0,          0,                  0,              DDKEY_F11,
    DDKEY_F12,      0  ,        0  ,            0  ,            DDKEY_BACKSLASH, 0,             0  ,            0,          // 5
// 6
    0  ,            0  ,        0  ,            0  ,            0  ,        0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,          // 6
// 7
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // 7
// 8
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0,              0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // 8
// 9
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              DDKEY_ENTER, DDKEY_RCTRL,       0  ,            0,          // 9
// A
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // A
// B
    0  ,            0  ,        0  ,            0  ,            0,          '/',                0  ,            0,
    DDKEY_RALT,     0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // B
// C
    0  ,            0  ,        0  ,            0  ,            0,          DDKEY_PAUSE,        0  ,            DDKEY_HOME,
    DDKEY_UPARROW,  DDKEY_PGUP, 0  ,            DDKEY_LEFTARROW,0  ,        DDKEY_RIGHTARROW,   0  ,            DDKEY_END,  // C
// D
    DDKEY_DOWNARROW,DDKEY_PGDN, DDKEY_INS,      DDKEY_DEL,      0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // D
// E
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0,          // E
// F
    0  ,            0  ,        0  ,            0  ,            0,          0  ,                0  ,            0,
    0  ,            0  ,        0  ,            0,              0  ,        0  ,                0  ,            0           // F
//  0               1           2               3               4           5                   6               7
//  8               9           A               B               C           D                   E               F
};

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
static int oldMouseButtons = 0;
static int oldJoyBState = 0;
static float oldPOV = IJOY_POV_CENTER;

// CODE --------------------------------------------------------------------

void DD_RegisterInput(void)
{
    // Cvars
    C_VAR_INT("input-key-delay1", &keyRepeatDelay1, CVF_NO_MAX, 50, 0);
    C_VAR_INT("input-key-delay2", &keyRepeatDelay2, CVF_NO_MAX, 20, 0);
    C_VAR_BYTE("input-key-show-scancodes", &showScanCodes, 0, 0, 1);

//    C_VAR_INT("input-joy-sensi", &joySensitivity, 0, 0, 9);
//    C_VAR_INT("input-joy-deadzone", &joyDeadZone, 0, 0, 90);

//    C_VAR_INT("input-mouse-wheel-sensi", &mouseWheelSensi, CVF_NO_MAX, 0, 0);
//    C_VAR_INT("input-mouse-x-disable", &mouseDisableX, 0, 0, 1);
//    C_VAR_INT("input-mouse-y-disable", &mouseDisableY, 0, 0, 1);
//    C_VAR_INT("input-mouse-y-inverse", &mouseInverseY, 0, 0, 1);
//    C_VAR_INT("input-mouse-filter", &mouseFilter, 0, 0, MAX_MOUSE_FILTER-1);
    C_VAR_INT("input-mouse-frequency", &mouseFreq, CVF_NO_MAX, 0, 0);

    C_VAR_BYTE("input-info-mouse", &showMouseInfo, 0, 0, 1);

    // Ccmds
    C_CMD("dumpkeymap", "s", DumpKeyMap);
    C_CMD("keymap", "s", KeyMap);
    C_CMD("listinputdevices", "", ListInputDevices);
    C_CMD_FLAGS("setaxis", "s",      AxisPrintConfig, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("setaxis", "ss",     AxisChangeOption, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("setaxis", "sss",    AxisChangeValue, CMDF_NO_DEDICATED);
}

/**
 * Allocate an array of bytes for the input devices keys.
 * The allocated memory is cleared to zero.
 */
static void I_DeviceAllocKeys(inputdev_t *dev, uint count)
{
	dev->numKeys = count;
	dev->keys = M_Calloc(count);
}

/**
 * Add a new axis to the input device.
 */
static inputdevaxis_t *I_DeviceNewAxis(inputdev_t *dev, const char *name)
{
	inputdevaxis_t *axis;
	
	dev->axes = M_Realloc(dev->axes, sizeof(inputdevaxis_t) * ++dev->numAxes);

	axis = &dev->axes[dev->numAxes - 1];
	memset(axis, 0, sizeof(*axis));
	strcpy(axis->name, name);

	axis->type = IDAT_STICK;
	
	// Set reasonable defaults. The user's settings will be restored
	// later.
	axis->scale = 1/10.0f;
	axis->deadZone = 0;//5/100.0f;

	return axis;
}

/**
 * Initialize the input device state table.
 */
void I_InitInputDevices(void)
{
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

	// The wheel is translated to keys, so there is no need to
	// create an axis for it.
	axis = I_DeviceNewAxis(dev, "x");
    axis->type = IDAT_POINTER;
    axis->filter = 1; // On by default;
	axis = I_DeviceNewAxis(dev, "y");
    axis->type = IDAT_POINTER;
    axis->filter = 1; // On by default;

	if(I_MousePresent())
		dev->flags = ID_ACTIVE;

	dev = &inputDevices[IDEV_JOY1];
	strcpy(dev->name, "joy");
	I_DeviceAllocKeys(dev, IJOY_MAXBUTTONS);

	// We support eight axes.
	I_DeviceNewAxis(dev, "x");
	I_DeviceNewAxis(dev, "y");
	I_DeviceNewAxis(dev, "z");
	I_DeviceNewAxis(dev, "rx");
	I_DeviceNewAxis(dev, "ry");
	I_DeviceNewAxis(dev, "rz");
    I_DeviceNewAxis(dev, "slider1");
    I_DeviceNewAxis(dev, "slider2");

	// The joystick may not be active.
	if(I_JoystickPresent())
		dev->flags = ID_ACTIVE;
}

/**
 * Free the memory allocated for the input devices.
 */
void I_ShutdownInputDevices(void)
{
	uint        i;
	inputdev_t *dev;

	for(i = 0; i < NUM_INPUT_DEVICES; ++i)
	{
		dev = &inputDevices[i];
		if(dev->keys) 
            M_Free(dev->keys);
		if(dev->axes)
            M_Free(dev->axes);
	}
}

/**
 * Retrieve a pointer to the input device state by identifier.
 *
 * @param ident         Intput device identifier (index).
 * @param ifactive      Only return if the device is active.
 *
 * @return              Ptr to the input device state OR <code>NULL</code>.
 */
inputdev_t *I_GetDevice(uint ident, boolean ifactive)
{
    inputdev_t *dev = &inputDevices[ident];

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
 * @return              Ptr to the input device state OR <code>NULL</code>.
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
 * @return              Ptr to the device axis OR <code>NULL</code> if not
 *                      found.
 */
inputdevaxis_t *I_GetAxisByID(inputdev_t *device, uint id)
{
    if(!device || id > device->numAxes - 1)
        return NULL;

    return &device->axes[id];
}

/**
 * Retrieve the index + 1 of a device's axis by name.
 *
 * @param device        Ptr to input device info, to get the axis index from.
 * @param name          Ptr to string containing the name to be searched for.
 *
 * @return              Index of the device axis named OR <code>0</code> if
 *                      not found.
 */
static uint I_GetAxisByName(inputdev_t *device, const char *name)
{
	uint         i;

	for(i = 0; i < device->numAxes; ++i)
	{
		if(!stricmp(device->axes[i].name, name))
			return i + 1;
	}
	return 0;
}

/**
 * Check through the axes registered for the given device, see if there is
 * one identified by the given name.
 *
 * @return              <code>false</code> if the string is invalid.
 */
boolean I_ParseDeviceAxis(const char *str, uint *deviceID, uint *axis)
{
	char        name[30], *ptr;
    inputdev_t *device;

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
        uint    a = I_GetAxisByName(device, ptr + 1);
        if((*axis = a) == 0)
            return false;

        *axis = a - 1; // Axis indices are base 1.
    }
   
	return true;
}

/**
 * Update an input device axis.  Transformation is applied.
 */
static void I_UpdateAxis(inputdev_t *dev, uint axis, float pos,
                         timespan_t ticLength)
{
	inputdevaxis_t *a = &dev->axes[axis];

	// Disabled axes are always zero.
	if(a->flags & IDA_DISABLED)
	{
		a->position = 0;
		return;
	}

	// Apply scaling, deadzone and clamping.
	pos *= a->scale;
	if(a->type == IDAT_STICK) // Pointer axes are exempt.
	{
		if(fabs(pos) <= a->deadZone)
		{
			a->position = 0;
			return;
		}
		pos += a->deadZone * (pos > 0? -1 : 1);	// Remove the dead zone.
		pos *= 1.0f/(1.0f - a->deadZone);		// Normalize.
		if(pos < -1.0f)
            pos = -1.0f;
		if(pos > 1.0f)
            pos = 1.0f;
	}
	
	if(a->flags & IDA_INVERT)
	{
		// Invert the axis position.
		pos = -pos;
	}

    a->realPosition = pos;
/* if(a->filter > 0)
	{
        // Filtering ensures that events are sent more evenly on each frame.
        float   target;
        int     dir;
        float   avail;
        int     used;

        dir = (pos > 0? 1 : pos < 0? -1 : 0);
        if(pos > a->position)
            avail = pos - a->position;
        else if(a->position > pos)
            avail = a->position - pos;
        else
            avail = 0;

        // Determine the target velocity.
        target = avail * (MAX_AXIS_FILTER - a->filter);

        // Determine the amount of mickeys to send. It depends on the
        // current mouse velocity, and how much time has passed.
        used = target * ticLength;

        // Don't go over the available number of update frames.
        if(used > avail)
            used = pos;

        // This is the new (filtered) axis position.
        pos = dir * used;
	}
	else*/
	{
		// This is the new axis position.
		pos = a->realPosition;
	}

    a->position = pos;
}

/**
 * Update the input device state table.
 */
static void I_TrackInput(ddevent_t *ev, timespan_t ticLength)
{
	inputdev_t *dev;

    if((dev = I_GetDevice(ev->deviceID, true)) == NULL)
        return;

    // Track the state of Shift and Alt.
    if(ev->deviceID == IDEV_KEYBOARD)
    {
        if(ev->controlID == DDKEY_RSHIFT)
        {
            if(ev->data1 == EVS_DOWN)
                shiftDown = true;
            else if(ev->data1 == EVS_UP)
                shiftDown = false;
        }
        else if(ev->controlID == DDKEY_RALT)
        {
            if(ev->data1 == EVS_DOWN)
                altDown = true;
            else if(ev->data1 == EVS_UP)
                altDown = false;
        }
    }

    // Update the state table.
    if(ev->isAxis)
    {
        I_UpdateAxis(dev, ev->controlID, ev->data1, ticLength);
    }
    else
    {
        dev->keys[ev->controlID] =
            (ev->data1 == EVS_DOWN || ev->data1 == EVS_REPEAT);
    }
}

/**
 * @return          The key state from the downKeys array.
 */
boolean I_IsDeviceKeyDown(uint ident, uint code)
{
    inputdev_t *dev;

    if((dev = I_GetDevice(ident, true)) != NULL)
    {
        if(code >= dev->numKeys)
            return false;

        return dev->keys[code];
    }

    return false;
}

/**
 * Dumps the key mapping table to filename
 */
void DD_DumpKeyMappings(char *fileName)
{
    FILE   *file;
    int     i;

    file = fopen(fileName, "wt");
    for(i = 0; i < 256; ++i)
    {
        fprintf(file, "%03i\t", i);
        fprintf(file, !isspace(keyMappings[i]) &&
                isprint(keyMappings[i]) ? "%c\n" : "%03i\n", keyMappings[i]);
    }

    fprintf(file, "\n+Shift\n");
    for(i = 0; i < 256; ++i)
    {
        if(shiftKeyMappings[i] == i)
            continue;
        fprintf(file, !isspace(i) && isprint(i) ? "%c\t" : "%03i\t", i);
        fprintf(file, !isspace(shiftKeyMappings[i]) &&
                isprint(shiftKeyMappings[i]) ? "%c\n" : "%03i\n",
                shiftKeyMappings[i]);
    }

    fprintf(file, "-Shift\n\n+Alt\n");
    for(i = 0; i < 256; ++i)
    {
        if(altKeyMappings[i] == i)
            continue;
        fprintf(file, !isspace(i) && isprint(i) ? "%c\t" : "%03i\t", i);
        fprintf(file, !isspace(altKeyMappings[i]) &&
                isprint(altKeyMappings[i]) ? "%c\n" : "%03i\n",
                altKeyMappings[i]);
    }
    fclose(file);
}

/**
 * Sets the key mappings to the default values
 */
void DD_DefaultKeyMapping(void)
{
    int     i;

    for(i = 0; i < 256; ++i)
    {
        keyMappings[i] = scantokey[i];
        shiftKeyMappings[i] = i >= 32 && i <= 127 &&
            defaultShiftTable[i - 32] ? defaultShiftTable[i - 32] : i;
        altKeyMappings[i] = i;
    }
}

/**
 * Initializes the key mappings to the default values.
 */
void DD_InitInput(void)
{
    DD_DefaultKeyMapping();
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
 * Clear the input event queue.
 */
void DD_ClearEvents(void)
{
    eventhead = eventtail;
}

/**
 * Called by the I/O functions when input is detected.
 */
void DD_PostEvent(ddevent_t *ev)
{
    events[eventhead++] = *ev;
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

/**
 * Send all the events of the given timestamp down the responder chain.
 * This gets called at least 35 times per second. Usually more frequently
 * than that.
 */
void DD_ProcessEvents(timespan_t ticLength)
{
    ddevent_t *ddev;
    event_t     ev;

    DD_ReadMouse();
    DD_ReadJoystick();
    DD_ReadKeyboard();

    while((ddev = DD_GetEvent()) != NULL)
    {
        if(ignoreInput)
            continue;

        // Copy the essentials into a cutdown version for the game.
        // Ensure the format stays the same for future compatibility!
        switch(ddev->deviceID)
        {
        case IDEV_KEYBOARD:
            ev.type = EV_KEY;
            break;

        case IDEV_MOUSE:
            if(ddev->isAxis)
                ev.type = EV_MOUSE_AXIS;
            else
                ev.type = EV_MOUSE_BUTTON;
            break;

        case IDEV_JOY1:
        case IDEV_JOY2:
        case IDEV_JOY3:
        case IDEV_JOY4:
            // \fixme What about POV?
            if(ddev->isAxis)
                ev.type = EV_JOY_AXIS;
            else
            {
                if(ddev->controlID == 6 || ddev->controlID == 7)
                    ev.type = EV_JOY_SLIDER;
                else
                    ev.type = EV_JOY_BUTTON;
            }
            break;

        default:
#if _DEBUG
Con_Error("DD_ProcessEvents: Unknown deviceID in ddevent_t");
#endif
            break;
        }

        if(!ddev->isAxis)
        {
            ev.state = ddev->data1;
            ev.data1 = ddev->controlID;
        }
        else
        {
            ev.state = 0;
            ev.data1 = ddev->data1;
        }
        ev.data2 = 0;
        ev.data3 = 0;
        ev.data4 = 0;
        ev.data5 = 0;
        ev.data6 = 0;

        // Update the state of the input device tracking table.
		I_TrackInput(ddev, ticLength);

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
 * Converts as a scan code to the keymap key id.
 */
byte DD_ScanToKey(byte scan)
{
    return keyMappings[scan];
}

/*
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

/*
 * Converts a keymap key id to a scan code.
 */
byte DD_KeyToScan(byte key)
{
    int     i;

    for(i = 0; i < NUMKKEYS; ++i)
        if(keyMappings[i] == key)
            return i;
    return 0;
}

/*
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
    ddevent_t ev;
    keyevent_t keyevs[KBDQUESIZE];
    int     i, k, numkeyevs;

    if(isDedicated)
    {
        // In dedicated mode, all input events come from the console.
        Sys_ConPostEvents();
        return;
    }

    // Check the repeaters.
    ev.deviceID = IDEV_KEYBOARD;
    ev.isAxis = false;
    ev.data1 = EVS_REPEAT;

    ev.noclass = true; // Don't specify a class
    ev.useclass = 0; // initialize with something
    for(i = 0; i < MAX_DOWNKEYS; ++i)
    {
        repeater_t *rep = keyReps + i;

        if(!rep->key)
            continue;
        ev.controlID = rep->key;

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

    // Read the keyboard events.
    numkeyevs = I_GetKeyEvents(keyevs, KBDQUESIZE);

    // Translate them to Doomsday keys.
    for(i = 0; i < numkeyevs; ++i)
    {
        keyevent_t *ke = keyevs + i;

        // Check the type of the event.
        if(ke->event == IKE_KEY_DOWN)   // Key pressed?
        {
            ev.data1 = EVS_DOWN;
        }
        else if(ke->event == IKE_KEY_UP) // Key released?
        {
            ev.data1 = EVS_UP;
        }

        // Use the table to translate the scancode to a ddkey.
#ifdef WIN32
        ev.controlID = DD_ScanToKey(ke->code);
#endif
#ifdef UNIX
        ev.controlID = ke->code;
#endif

        // Should we print a message in the console?
        if(showScanCodes && ev.data1 == EVS_DOWN)
            Con_Printf("Scancode: %i (0x%x)\n", ev.controlID, ev.controlID);

        // Maintain the repeater table.
        if(ev.data1 == EVS_DOWN)
        {
            // Find an empty repeater.
            for(k = 0; k < MAX_DOWNKEYS; ++k)
                if(!keyReps[k].key)
                {
                    keyReps[k].key = ev.controlID;
                    keyReps[k].timer = sysTime;
                    keyReps[k].count = 0;
                    break;
                }
        }
        else if(ev.data1 == EVS_UP)
        {
            // Clear any repeaters with this key.
            for(k = 0; k < MAX_DOWNKEYS; ++k)
                if(keyReps[k].key == (int) ev.controlID)
                    keyReps[k].key = 0;
        }

        // Post the event.
        DD_PostEvent(&ev);
    }
}

/**
 * Checks the current mouse state (axis, buttons and wheel).
 * Generates events and mickeys and posts them.
 */
void DD_ReadMouse(void)
{
    int         change;
    ddevent_t   ev;
    mousestate_t mouse;
    int         xpos, ypos, zpos;

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

    ev.deviceID = IDEV_MOUSE;
    ev.isAxis = true;
    xpos = mouse.x * DD_MICKEY_ACCURACY;
    ypos = mouse.y * DD_MICKEY_ACCURACY;
    zpos = mouse.z * DD_MICKEY_ACCURACY;
    ev.noclass = true; // Don't specify a class
    ev.useclass = 0; // initialize with something

    // Mouse axis data may be modified if not in UI mode.
/*
    if(allowMouseMod)
    {
        if(mouseDisableX)
            xpos = 0;
        if(mouseDisableY)
            ypos = 0;
        if(!mouseInverseY)
            ypos = -ypos;
    }
    */
    if(!allowMouseMod) // In UI mode.
    {
        int         winWidth, winHeight;

        if(!DD_GetWindowDimensions(windowIDX, NULL, NULL, &winWidth, &winHeight))
        {
            Con_Error("DD_ReadMouse: Failed retrieving window dimensions.");
            return;
        }

        // Scale the movement depending on screen resolution.
        xpos *= MAX_OF(1, winWidth / 800.0f);
        ypos *= MAX_OF(1, winHeight / 600.0f);
    }
    else
    {
        ypos = -ypos;
    }

    // Post an event per axis.
    // Don't post empty events.
    if(xpos)
    {
        ev.data1 = xpos;
        ev.controlID = 0;
        DD_PostEvent(&ev);
        ev.data1 = 0;
    }
    if(ypos)
    {
        ev.data1 = ypos;
        ev.controlID = 1;
        DD_PostEvent(&ev);
        ev.data1 = 0;
    }

    // Insert the possible mouse Z axis into the button flags.
    if(abs(zpos) >= 10 /*mouseWheelSensi*/)
    {
        mouse.buttons |= zpos > 0 ? DDMB_MWHEELUP : DDMB_MWHEELDOWN;
    }

    // Check the buttons and send the appropriate events.
    change = oldMouseButtons ^ mouse.buttons;   // The change mask.
    // Send the relevant events.
    if((ev.controlID = mouse.buttons & change))
    {
        ev.isAxis = false;
        ev.data1 = EVS_DOWN;
        DD_PostEvent(&ev);
    }
    if((ev.controlID = oldMouseButtons & change))
    {
        ev.isAxis = false;
        ev.data1 = EVS_UP;
        DD_PostEvent(&ev);
    }
    oldMouseButtons = mouse.buttons;
}

/**
 * Applies the dead zone and clamps the value to -100...100.
 */
#if 0 // currently unused
void DD_JoyAxisClamp(int *val)
{
    if(abs(*val) < joyDeadZone)
    {
        // In the dead zone, just go to zero.
        *val = 0;
        return;
    }
    // Remove the dead zone.
    *val += *val > 0 ? -joyDeadZone : joyDeadZone;
    // Normalize.
    *val *= 100.0f / (100 - joyDeadZone);
    // Clamp.
    if(*val > 100)
        *val = 100;
    if(*val < -100)
        *val = -100;
}
#endif

/*
 * Checks the current joystick state (axis, sliders, hat and buttons).
 * Generates events and posts them. Axis clamps and dead zone is done
 * here.
 */
void DD_ReadJoystick(void)
{
    ddevent_t ev;
    joystate_t state;
    int     i, bstate;
    //int     div = 100 - joySensitivity * 10;

    if(!I_JoystickPresent())
        return;

    I_GetJoystickState(&state);

    bstate = 0;
    // Check the buttons.
    for(i = 0; i < IJOY_MAXBUTTONS; ++i)
        if(state.buttons[i])
            bstate |= 1 << i;   // Set the bits.

    ev.deviceID = IDEV_JOY1;
    ev.isAxis = false;
    ev.noclass = true; // Don't specify a class
    ev.useclass = 0; // initialize with something

    // Check for button state changes.
    i = oldJoyBState ^ bstate;  // The change mask.
    // Send the relevant events.
    if((ev.controlID = bstate & i))
    {
        ev.data1 = EVS_DOWN;
        DD_PostEvent(&ev);
    }
    if((ev.controlID = oldJoyBState & i))
    {
        ev.data1 = EVS_UP;
        DD_PostEvent(&ev);
    }
    oldJoyBState = bstate;

    // Check for a POV change.
    if(state.povAngle != oldPOV)
    {
        if(oldPOV != IJOY_POV_CENTER)
        {
            // Send a notification that the existing POV angle is no
            // longer active.
            ev.data1 = EVS_UP;
            ev.controlID = (int) (oldPOV / 45 + .5);    // Round off correctly w/.5.
            DD_PostEvent(&ev);
        }
        if(state.povAngle != IJOY_POV_CENTER)
        {
            // The new angle becomes active.
            ev.data1 = EVS_DOWN;
            ev.controlID = (int) (state.povAngle / 45 + .5);
            DD_PostEvent(&ev);
        }
        oldPOV = state.povAngle;
    }

    // Send joystick axis events, one per axis (XYZ and rotation-XYZ).
    ev.isAxis = true;

    // The input code returns the axis positions in the range -10000..10000.
    // The output axis data must be in range -100..100.
    // Increased sensitivity causes the axes to max out earlier.
    // Check that the divisor is valid.
/*  if(div < 10)
        div = 10;
    if(div > 100)
        div = 100;*/

    // fixme\ Check each axis position against dead zone and previous value.
    if(state.axis[0])
    {
        ev.data1 = state.axis[0];
        ev.controlID = 0;
        DD_PostEvent(&ev);
    }

    if(state.axis[1])
    {
        ev.data1 = state.axis[1];
        ev.controlID = 1;
        DD_PostEvent(&ev);
    }

    if(state.axis[2])
    {
        ev.data1 = state.axis[2];
        ev.controlID = 2;
        DD_PostEvent(&ev);
    }

    if(state.rotAxis[0])
    {
        ev.data1 = state.rotAxis[0];
        ev.controlID = 3;
        DD_PostEvent(&ev);
    }

    if(state.rotAxis[1])
    {
        ev.data1 = state.rotAxis[1];
        ev.controlID = 4;
        DD_PostEvent(&ev);
    }

    if(state.rotAxis[2])
    {
        ev.data1 = state.rotAxis[2];
        ev.controlID = 5;
        DD_PostEvent(&ev);
    }

    // The sliders.
    if(state.slider[0])
    {
        ev.data1 = state.slider[0];
        ev.controlID = 6;
        DD_PostEvent(&ev);
    }

    if(state.slider[1])
    {
        ev.data1 = state.slider[1];
        ev.controlID = 7;
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
            Con_Printf("  Axis #%i: %s\n", j, dev->axes[j].name);
    }
    return true;
}

/**
 * Console command to write the current keymap to a file.
 */
D_CMD(DumpKeyMap)
{
    DD_DumpKeyMappings(argv[1]);
    Con_Printf("The current keymap was dumped to %s.\n", argv[1]);
    return true;
}

/**
 * Console command to load a keymap file.
 */
D_CMD(KeyMap)
{
    DFILE  *file;
    char    line[512], *ptr;
    boolean shiftMode = false, altMode = false;
    int     key, mapTo, lineNumber = 0;

    // Try with and without .DKM.
    strcpy(line, argv[1]);
    if(!F_Access(line))
    {
        // Try the path.
        M_TranslatePath(keyMapPath, line);
        strcat(line, argv[1]);
        if(!F_Access(line))
        {
            strcpy(line, argv[1]);
            strcat(line, ".dkm");
            if(!F_Access(line))
            {
                M_TranslatePath(keyMapPath, line);
                strcat(line, argv[1]);
                strcat(line, ".dkm");
            }
        }
    }
    if(!(file = F_Open(line, "rt")))
    {
        Con_Printf("%s: file not found.\n", argv[1]);
        return false;
    }
    // Any missing entries are set to the default.
    DD_DefaultKeyMapping();
    do
    {
        lineNumber++;
        M_ReadLine(line, sizeof(line), file);
        ptr = M_SkipWhite(line);
        if(!*ptr || M_IsComment(ptr))
            continue;
        // Modifiers? Only shift is supported at the moment.
        if(!strnicmp(ptr + 1, "shift", 5))
        {
            shiftMode = (*ptr == '+');
            continue;
        }
        else if(!strnicmp(ptr + 1, "alt", 3))
        {
            altMode = (*ptr == '+');
            continue;
        }
        key = DD_KeyOrCode(ptr);
        if(key < 0 || key > 255)
        {
            Con_Printf("%s(%i): Invalid key %i.\n", argv[1], lineNumber, key);
            continue;
        }
        ptr = M_SkipWhite(M_FindWhite(ptr));
        mapTo = DD_KeyOrCode(ptr);
        // Check the mapping.
        if(mapTo < 0 || mapTo > 255)
        {
            Con_Printf("%s(%i): Invalid mapping %i.\n", argv[1], lineNumber,
                       mapTo);
            continue;
        }
        if(shiftMode)
            shiftKeyMappings[key] = mapTo;
        else if(altMode)
            altKeyMappings[key] = mapTo;
        else
            keyMappings[key] = mapTo;
    }
    while(!deof(file));
    F_Close(file);
    Con_Printf("Keymap %s loaded.\n", argv[1]);
    return true;
}
