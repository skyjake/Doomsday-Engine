/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * dd_input.c: Input Subsystem
 *
 * Base-level, system independent input code.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_system.h"
#include "de_play.h"
#include "de_misc.h"
#include "de_ui.h"

#include "gl_main.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_INPUT_RATE	35	// Hz
#define MAXEVENTS			256	// Size of the input event buffer.
#define WHEEL_THRESHOLD		10

#define KBDQUESIZE		32
#define MAX_DOWNKEYS	32	

#define CLAMP(x) DD_JoyAxisClamp(&x) //x = (x < -100? -100 : x > 100? 100 : x)

// TYPES -------------------------------------------------------------------

typedef struct repeater_s {
	int key;				// The key code (0 if not in use).
	int	timer;				// How's the time?
	int count;				// How many times has been repeated?
} repeater_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int			inputRate = DEFAULT_INPUT_RATE;

event_t		events[MAXEVENTS];
int			eventHead;
int			eventTail;
int			eventCount;

//int			mouseFilter = 0;		// No filtering by default.
//int			mouseInverseY = false;
//int			mouseWheelSensi = 10;	// I'm shooting in the dark here.
//int			joySensitivity = 5;
//int			joyDeadZone = 10;

inputdev_t	inputDevices[NUM_INPUT_DEVICES];

// The initial and secondary repeater delays (tics).
int			repWait1 = 15, repWait2 = 3; 
//int			mouseDisableX = false, mouseDisableY = false;
boolean		shiftDown = false, altDown = false;
boolean		showScanCodes = false;

// A customizable mapping of the scantokey array.
char		keyMapPath[256] = "}Data\\KeyMaps\\";
byte		keyMappings[256];
byte		shiftKeyMappings[256], altKeyMappings[256];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static byte scanToKey[256] =	
{
//	0				1			2				3				4			5					6				7
//	8				9			A				B				C			D					E				F
// 0
	0  ,			27, 		'1',			'2',			'3',		'4',				'5',			'6',
	'7',			'8',		'9',			'0',			'-',		'=',				DDKEY_BACKSPACE,9,			// 0
// 1
	'q',			'w',		'e',			'r',			't',		'y',				'u',			'i',
	'o',			'p',		'[',			']',			13 ,		DDKEY_RCTRL,		'a',			's',		// 1
// 2
	'd',			'f',		'g',			'h',			'j',		'k',				'l',			';',
	39 ,			'`',		DDKEY_RSHIFT,	92,				'z',		'x',				'c',			'v',		// 2
// 3
	'b',			'n',		'm',			',',			'.',		'/',				DDKEY_RSHIFT,	'*',
	DDKEY_RALT,		' ',		0  ,			DDKEY_F1,		DDKEY_F2,	DDKEY_F3,			DDKEY_F4,		DDKEY_F5,   // 3
// 4
	DDKEY_F6,		DDKEY_F7,	DDKEY_F8,		DDKEY_F9,		DDKEY_F10,	DDKEY_NUMLOCK,		DDKEY_SCROLL,	DDKEY_NUMPAD7,
	DDKEY_NUMPAD8,	DDKEY_NUMPAD9, '-',			DDKEY_NUMPAD4,	DDKEY_NUMPAD5, DDKEY_NUMPAD6,	'+',			DDKEY_NUMPAD1, // 4
// 5
	DDKEY_NUMPAD2,	DDKEY_NUMPAD3, DDKEY_NUMPAD0, DDKEY_DECIMAL,0,			0, 					0,				DDKEY_F11,
	DDKEY_F12,		0  ,		0  ,			0  ,			DDKEY_BACKSLASH, 0,				0  ,			0,			// 5
// 6
	0  ,			0  ,		0  ,			0  ,			0  ,		0  ,				0  ,			0,
	0  ,			0  ,		0  ,			0  ,			0,			0  ,				0  ,			0,			// 6
// 7
	0  ,			0  ,		0  ,			0  ,			0,			0  ,				0  ,			0,
	0  ,			0  ,		0  ,			0,				0  ,		0  ,				0  ,			0,			// 7
// 8
	0  ,			0  ,		0  ,			0  ,			0,			0  ,				0  ,			0,
	0,				0  ,		0  ,			0,				0  ,		0  ,				0  ,			0,			// 8
// 9
	0  ,			0  ,		0  ,			0  ,			0,			0  ,				0  ,			0,
	0  ,			0  ,		0  ,			0,				DDKEY_ENTER, DDKEY_RCTRL,		0  ,			0,			// 9
// A
	0  ,			0  ,		0  ,			0  ,			0,			0  ,				0  ,			0,
	0  ,			0  ,		0  ,			0,				0  ,		0  ,				0  ,			0,			// A
// B
	0  ,			0  ,		0  ,			0  ,			0,			'/',				0  ,			0,
	DDKEY_RALT,		0  ,		0  ,			0,				0  ,		0  ,				0  ,			0,			// B
// C
	0  ,			0  ,		0  ,			0  ,			0,			DDKEY_PAUSE,		0  ,			DDKEY_HOME,
	DDKEY_UPARROW,	DDKEY_PGUP,	0  ,			DDKEY_LEFTARROW,0  ,		DDKEY_RIGHTARROW,	0  ,			DDKEY_END,	// C
// D
	DDKEY_DOWNARROW,DDKEY_PGDN, DDKEY_INS,		DDKEY_DEL,		0,			0  ,				0  ,			0,
	0  ,			0  ,		0  ,			0,				0  ,		0  ,				0  ,			0,			// D
// E	
	0  ,			0  ,		0  ,			0  ,			0,			0  ,				0  ,			0,
	0  ,			0  ,		0  ,			0,				0  ,		0  ,				0  ,			0,			// E
// F
	0  ,			0  ,		0  ,			0  ,			0,			0  ,				0  ,			0,
	0  ,			0  ,		0  ,			0,				0  ,		0  ,				0  ,			0			// F
//	0				1			2				3				4			5					6				7
//	8				9			A				B				C			D					E				F
};

static char defaultShiftTable[96] =	// Contains characters 32 to 127.
{
/* 32 */	' ', 0, 0, 0, 0, 0, 0, '"',
/* 40 */	0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
/* 50 */	'@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
/* 60 */	0, '+', 0, 0, 0, 'a', 'b', 'c', 'd', 'e',
/* 70 */	'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
/* 80 */	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
/* 90 */	'z', '{', '|', '}', 0, 0, 0, 'A', 'B', 'C',
/* 100 */	'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
/* 110 */	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
/* 120 */	'X', 'Y', 'Z', 0, 0, 0, 0, 0
};

static boolean	stopInputThread = false;
static int		inputThreadHandle;
static int		eventQueueLock;

static repeater_t keyReps[MAX_DOWNKEYS];
//static int		oldMouseX, oldMouseY;
static int		oldMouseButtons = 0;
static int		oldJoyBState = 0;
static float	oldPOV = IJOY_POV_CENTER;

// CODE --------------------------------------------------------------------

/*
 * Allocate an array of bytes for the input devices keys.
 * The allocated memory is cleared to zero.
 */
void I_DeviceAllocKeys(inputdev_t *dev, int count)
{
	dev->numKeys = count;
	dev->keys = calloc(count, 1);
}

/*
 * Add a new axis to the input device.
 */
inputdevaxis_t *I_DeviceNewAxis(inputdev_t *dev, const char *name)
{
	inputdevaxis_t *axis;
	
	dev->axes = realloc(dev->axes, sizeof(inputdevaxis_t) * ++dev->numAxes);

	axis = dev->axes + dev->numAxes - 1;
	memset(axis, 0, sizeof(*axis));
	strcpy(axis->name, name);

	// Set reasonable defaults. The user's settings will be restored
	// later.
	axis->scale = 1/10.0f;
	axis->deadZone = 0;//5/100.0f;

	return axis;
}

/* 
 * Initialize the input device state table.
 */
void I_InitInputDevices(void)
{
	inputdev_t *dev;

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
	I_DeviceNewAxis(dev, "x");
	I_DeviceNewAxis(dev, "y");
	
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

/*
 * Free the memory allocated for the input devices.
 */
void I_ShutdownInputDevices(void)
{
	int i;
	inputdev_t *dev;

	for(i = 0; i < NUM_INPUT_DEVICES; i++)
	{
		dev = inputDevices + i;
		if(dev->keys) free(dev->keys);
		if(dev->axes) free(dev->axes);
	}
}

/*
 * Return a pointer to the input device state, or NULL if the device
 * is inactive.
 */
inputdev_t *I_GetDevice(int ident)
{
	if(inputDevices[ident].flags & ID_ACTIVE)
		return inputDevices + ident;
	else
		return NULL;
}

/*
 * Return a pointer to the input device state, or NULL if the device
 * is inactive.
 */
inputdev_t *I_GetDeviceByName(const char *name)
{
	int i;

	for(i = 0; i < NUM_INPUT_DEVICES; i++)
	{
		if(!stricmp(inputDevices[i].name, name))
			return inputDevices + i;
	}
	return NULL;
}

/*
 * Returns the index of the axis.
 */
int I_GetAxisByName(inputdev_t *device, const char *name)
{
	int i;

	for(i = 0; i < device->numAxes; i++)
	{
		if(!stricmp(device->axes[i].name, name))
			return i;
	}
	return -1;
}

/*
 * Returns false if the string is invalid.
 */
boolean I_ParseDeviceAxis(const char *str, inputdev_t **device, int *axis)
{
	char name[30], *ptr;

	ptr = strchr(str, '-');
	if(!ptr) return false;

	// The name of the device.
	memset(name, 0, sizeof(name));
	strncpy(name, str, ptr - str);
	if((*device = I_GetDeviceByName(name)) == NULL) return false;

	// The axis name.
	if((*axis = I_GetAxisByName(*device, ptr + 1)) < 0) return false;
   
	return true;
}

/*
 * Update an input device axis.  Transformation is applied.
 */
void I_UpdateAxis(inputdev_t *dev, int axis, float pos)
{
	inputdevaxis_t *a = dev->axes + axis;

	// Disabled axes are always zero.
	if(a->flags & IDA_DISABLED)
	{
		a->position = 0;
		return;
	}

	// Apply scaling, deadzone and clamping.
	pos *= a->scale;
	if(fabs(pos) <= a->deadZone)
	{
		a->position = 0;
		return;
	}
	pos += a->deadZone * (pos > 0? -1 : 1);	// Remove the dead zone.
	pos *= 1.0f/(1.0f - a->deadZone);		// Normalize.
	if(pos < -1.0f) pos = -1.0f;
	if(pos > 1.0f) pos = 1.0f;
	
	if(a->flags & IDA_INVERT)
	{
		// Invert the axis position.
		pos = -pos;
	}

	if(a->flags & IDA_FILTER)
	{
		// Average with the previous position.
		a->position = (a->position + pos) / 2;
	}
	else
	{
		// This is the new axis position.
		a->position = pos;
	}
}

/*
 * Update the input device state table.
 */
void I_TrackInput(event_t *ev)
{
	inputdev_t *dev;
	int i;
	
	// Track the state of Shift and Alt.
	if(ev->data1 == DDKEY_RSHIFT)
	{
		if(ev->type == ev_keydown) shiftDown = true;
		else if(ev->type == ev_keyup) shiftDown = false;
	}
	if(ev->data1 == DDKEY_RALT)
	{
		if(ev->type == ev_keydown) altDown = true;
		else if(ev->type == ev_keyup) altDown = false;
	}

	// Update the state table.
	switch(ev->type)
	{
	case ev_keydown:
	case ev_keyup:
		inputDevices[IDEV_KEYBOARD].keys[ev->data1 & 0xff] =
			(ev->type == ev_keydown);			
		break;

	case ev_mousebdown:
	case ev_mousebup:
		if((dev = I_GetDevice(IDEV_MOUSE)) != NULL)
		{
			for(i = 0; i < IMB_MAXBUTTONS; i++)
			{
				if(ev->data1 & (1 << i))
					dev->keys[i] = (ev->type == ev_mousebdown);
			}
		}
		break;

	case ev_mouse:
		if((dev = I_GetDevice(IDEV_MOUSE)) != NULL)
		{
			I_UpdateAxis(dev, 0, ev->data1);

			// On the mouse Y axis, positive means upward.
			I_UpdateAxis(dev, 1, -ev->data2);
		}
		break;

	case ev_joybdown:
	case ev_joybup:
		if((dev = I_GetDevice(IDEV_JOY1)) != NULL)
		{
			for(i = 0; i < IJOY_MAXBUTTONS; i++)
			{
				if(ev->data1 & (1 << i))
					dev->keys[i] = (ev->type == ev_joybdown);
			}
		}
		break;

	case ev_joystick:
		if((dev = I_GetDevice(IDEV_JOY1)) != NULL)
		{
			I_UpdateAxis(dev, 0, ev->data1);
			I_UpdateAxis(dev, 1, ev->data2);
			I_UpdateAxis(dev, 2, ev->data3);
			I_UpdateAxis(dev, 3, ev->data4);
			I_UpdateAxis(dev, 4, ev->data5);
			I_UpdateAxis(dev, 5, ev->data6);
		}
		break;

	case ev_joyslider:
		if((dev = I_GetDevice(IDEV_JOY1)) != NULL)
		{
			I_UpdateAxis(dev, 6, ev->data1);
			I_UpdateAxis(dev, 7, ev->data2);
		}
		break;
		
	default:
		break;
	}
}

/*
 * Write the current key mapping (normal, shift, alt) into a text file.
 */
void DD_DumpKeyMappings(char *fileName)
{
	FILE *file;
	int i;

	file = fopen(fileName, "wt");
	for(i = 0; i < 256; i++)
	{
		fprintf(file, "%03i\t", i);
		fprintf(file, !isspace(keyMappings[i])
			&& isprint(keyMappings[i])? "%c\n" : "%03i\n", 
			keyMappings[i]);
	}

	fprintf(file, "\n+Shift\n");
	for(i = 0; i < 256; i++)
	{
		if(shiftKeyMappings[i] == i) continue;
		fprintf(file, !isspace(i) && isprint(i)? "%c\t" : "%03i\t", i);
		fprintf(file, !isspace(shiftKeyMappings[i])
			&& isprint(shiftKeyMappings[i])? "%c\n" : "%03i\n",
			shiftKeyMappings[i]);
	}

	fprintf(file, "-Shift\n\n+Alt\n");
	for(i = 0; i < 256; i++)
	{
		if(altKeyMappings[i] == i) continue;
		fprintf(file, !isspace(i) && isprint(i)? "%c\t" : "%03i\t", i);
		fprintf(file, !isspace(altKeyMappings[i])
			&& isprint(altKeyMappings[i])? "%c\n" : "%03i\n",
			altKeyMappings[i]);
	}
	fclose(file);
}

//===========================================================================
// DD_DefaultKeyMapping
//===========================================================================
void DD_DefaultKeyMapping(void)
{
	int i;

	for(i = 0; i < 256; i++)
	{
		keyMappings[i] = scanToKey[i];
		shiftKeyMappings[i] = i >= 32 && i <= 127 
			&& defaultShiftTable[i - 32]? defaultShiftTable[i - 32] : i;
		altKeyMappings[i] = i;
	}
}

//===========================================================================
// DD_KeyOrCode
//===========================================================================
int DD_KeyOrCode(char *token)
{
	char *end = M_FindWhite(token);

	if(end - token > 1)
	{
		// Longer than one character, it must be a number.
		return strtol(token, 0, !strnicmp(token, "0x", 2)? 16 : 10);
	}
	// Direct mapping.
	return (unsigned char) *token;
}

//===========================================================================
// CCmdDumpKeyMap
//===========================================================================
int CCmdDumpKeyMap(int argc, char **argv)
{
	if(argc != 2)
	{
		Con_Printf("Usage: %s (file)\n", argv[0]);
		return true;
	}
	DD_DumpKeyMappings(argv[1]);
	Con_Printf("The current keymap was dumped to %s.\n", argv[1]);
	return true;
}

/*
 * Load a keymap file.
 */
int CCmdKeyMap(int argc, char **argv)
{
	DFILE	*file;
	char	line[512], *ptr;
	boolean	shiftMode = false, altMode = false;
	int		key, mapTo, lineNumber = 0;

	if(argc != 2) 
	{
		Con_Printf("Usage: %s (dkm-file)\n", argv[0]);
		return true;
	}
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
		if(!*ptr || M_IsComment(ptr)) continue;
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
			Con_Printf("%s(%i): Invalid key %i.\n", argv[1], 
				lineNumber, key);
			continue;
		}
		ptr = M_SkipWhite(M_FindWhite(ptr));
		mapTo = DD_KeyOrCode(ptr);
		// Check the mapping.
		if(mapTo < 0 || mapTo > 255)
		{
			Con_Printf("%s(%i): Invalid mapping %i.\n", argv[1],
				lineNumber, mapTo);
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

/*
 * Send all the events of the given timestamp down the responder
 * chain.  This is called from the refresh thread.
 */
void DD_ProcessEvents(void)
{
	event_t *ev;

	// No new events will be posted until the currently queued ones 
	// have been processed.
	Sys_Lock(eventQueueLock);

	for(; eventTail != eventHead; eventTail = (++eventTail)&(MAXEVENTS-1))
	{
		ev = &events[eventTail];
		--eventCount;

		// Update the state of the input device tracking table.
		I_TrackInput(ev);
	
		// Does the special responder use this event?
		if(gx.PrivilegedResponder)
		{
			if(gx.PrivilegedResponder(ev)) continue;
		}

		if(UI_Responder(ev)) continue;

		// The console.
		if(Con_Responder(ev)) continue;

		// The menu.
		if(gx.MN_Responder(ev)) continue;

		// The game responder only returns true if the bindings 
		// can't be used (like when chatting).
		if(gx.G_Responder(ev)) continue;

		// The bindings responder.
		B_Responder(ev);
	}

	// New events can be posted again.
	Sys_Unlock(eventQueueLock);
}

/*
 * Clear the input event queue.
 */
void DD_ClearEvents(void)
{
	Sys_Lock(eventQueueLock);

	eventHead = eventTail;
	eventCount = 0;

	// New events can be posted again.
	Sys_Unlock(eventQueueLock);
}

/*
 * Called by the I/O functions when input is detected.
 */
void DD_PostEvent(event_t *ev)
{
	// We don't want to overflow the queue.
	if(eventCount == MAXEVENTS - 1) return;

	// Only one thread can access the queue at a time.
	Sys_Lock(eventQueueLock);

	events[eventHead] = *ev;
	eventHead = (eventHead + 1) & (MAXEVENTS - 1);
	eventCount++;

	Sys_Unlock(eventQueueLock);
}

//===========================================================================
// DD_ScanToKey
//===========================================================================
byte DD_ScanToKey(byte scan)
{
	return keyMappings[scan];
}

//===========================================================================
// DD_ModKey
//	Apply all active modifiers to the key.
//===========================================================================
byte DD_ModKey(byte key)
{
	if(shiftDown) key = shiftKeyMappings[key];
	if(altDown) key = altKeyMappings[key];
	return key;
}

//===========================================================================
// DD_KeyToScan
//===========================================================================
byte DD_KeyToScan(byte key)
{
	int	i;
	for(i = 0; i < 256; i++) if(keyMappings[i] == key) return i;
	return 0;
}

//===========================================================================
// DD_ClearKeyRepeaters
//===========================================================================
void DD_ClearKeyRepeaters(void)
{
	memset(keyReps, 0, sizeof(keyReps));
}

/*
 * Poll keyboard input and generate events.
 */
void DD_ReadKeyboard(void)
{
	event_t			ev;
	keyevent_t		keyevs[KBDQUESIZE];
	int				i, k, numkeyevs;

	if(isDedicated)
	{
		// In dedicated mode, all input events come from the console.
		Sys_ConPostEvents();
		return;
	}

	// Check the repeaters.
	ev.type = ev_keyrepeat;
	k = systics;	// The current time.
	for(i = 0; i < MAX_DOWNKEYS; i++)
	{
		repeater_t *rep = keyReps + i;
		if(!rep->key) continue;
		ev.data1 = rep->key;
		if(!rep->count && k - rep->timer >= repWait1)
		{
			// The first time.
			rep->count++;
			rep->timer += repWait1;
			DD_PostEvent(&ev);
		}
		if(rep->count)
		{
			while(k - rep->timer >= repWait2)
			{
				rep->count++;
				rep->timer += repWait2;
				DD_PostEvent(&ev);				
			}
		}
	}

	// Read the keyboard events.
	numkeyevs = I_GetKeyEvents(keyevs, KBDQUESIZE);

	// Translate them to Doomsday keys.
	for(i = 0; i < numkeyevs; i++)
	{
		keyevent_t *ke = keyevs + i;
		
		// Check the type of the event.
		if(ke->event == IKE_KEY_DOWN) // Key pressed?
			ev.type = ev_keydown;
		else if(ke->event == IKE_KEY_UP) // Key released?
			ev.type = ev_keyup;
		
		// Use the table to translate the scancode to a ddkey.
#ifdef WIN32
		ev.data1 = DD_ScanToKey(ke->code);
#endif
#ifdef UNIX
		ev.data1 = ke->code;
#endif

		// Should we print a message in the console?
		if(showScanCodes && ev.type == ev_keydown)
			Con_Printf("Scancode: %i (0x%x)\n", ev.data1, ev.data1);

		// Maintain the repeater table.
		if(ev.type == ev_keydown)
		{
			// Find an empty repeater.
			for(k = 0; k < MAX_DOWNKEYS; k++)
				if(!keyReps[k].key)
				{
					keyReps[k].key = ev.data1;
					keyReps[k].timer = systics;
					keyReps[k].count = 0;
					break;
				}
		}
		else if(ev.type == ev_keyup)
		{
			// Clear any repeaters with this key.
			for(k = 0; k < MAX_DOWNKEYS; k++)
				if(keyReps[k].key == ev.data1)
					keyReps[k].key = 0;
		}

		// Post the event.
		DD_PostEvent(&ev);
	}
}

/*
 * Poll mouse input and generate events.
 */
void DD_ReadMouse(void)
{
	event_t ev;
	mousestate_t mouse;
	int change;

	if(!I_MousePresent()) return;

	// Get the mouse state.
	I_GetMouseState(&mouse);

	ev.type = ev_mouse;
	ev.data1 = mouse.x;
	ev.data2 = mouse.y;
	ev.data3 = mouse.z;
	
	// Mouse axis data may be modified if not in UI mode.
/*	if(!ui_active)
	{
		if(mouseDisableX) ev.data1 = 0;
		if(mouseDisableY) ev.data2 = 0;
		if(!mouseInverseY) ev.data2 = -ev.data2;

		// Filtering calculates the average with previous (x,y) value.
		if(mouseFilter)
		{
			int oldX = ev.data1, oldY = ev.data2;
			ev.data1 = (ev.data1 + oldMouseX) / 2;
			ev.data2 = (ev.data2 + oldMouseY) / 2;
			oldMouseX = oldX;
			oldMouseY = oldY;
		}
	}*/
	
	if(ui_active) // In UI mode.
	{
		// Scale the movement depending on screen resolution.
		ev.data1 *= MAX_OF(1, screenWidth/800.0f);
		ev.data2 *= MAX_OF(1, screenHeight/600.0f);
	}

	DD_PostEvent (&ev);

	// Insert the possible mouse Z axis into the button flags.
	if(abs(ev.data3) >= WHEEL_THRESHOLD)
	{
		mouse.buttons |= ev.data3 > 0? DDMB_MWHEELUP : DDMB_MWHEELDOWN;
	}

	// Check the buttons and send the appropriate events.
	change = oldMouseButtons ^ mouse.buttons; // The change mask.

	// Send the relevant events.
	if((ev.data1 = mouse.buttons & change))
	{
		ev.type = ev_mousebdown;
		DD_PostEvent(&ev);
	}
	if((ev.data1 = oldMouseButtons & change))
	{
		ev.type = ev_mousebup;
		DD_PostEvent(&ev);
	}
	oldMouseButtons = mouse.buttons;
}

/*
 * Applies the dead zone and clamps the value to -100...100.
 */
/*void DD_JoyAxisClamp(int *val)
{
	if(abs(*val) < joyDeadZone)
	{
		// In the dead zone, just go to zero.
		*val = 0;
		return;
	}
	// Remove the dead zone.
	*val += *val > 0? -joyDeadZone : joyDeadZone;
	// Normalize.
	*val *= 100.0f/(100 - joyDeadZone);	
	// Clamp.
	if(*val > 100) *val = 100;
	if(*val < -100) *val = -100;
}*/

/*
 * Poll joystick input and generate events.
 */
void DD_ReadJoystick(void)
{
	event_t		ev;
	joystate_t	state;
	int			i, bstate;
	//int			div = 100 - joySensitivity*10;

	if(!I_JoystickPresent())
		return;

	I_GetJoystickState(&state);

	bstate = 0;
	// Check the buttons.
	for(i = 0; i < IJOY_MAXBUTTONS; i++) 
		if(state.buttons[i]) bstate |= 1<<i; // Set the bits.

	// Check for button state changes. 
	i = oldJoyBState ^ bstate; // The change mask.
	// Send the relevant events.
	if((ev.data1 = bstate & i))
	{
		ev.type = ev_joybdown;
		DD_PostEvent(&ev);
	}
	if((ev.data1 = oldJoyBState & i))
	{
		ev.type = ev_joybup;
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
			ev.type = ev_povup;
			ev.data1 = (int) (oldPOV/45 + .5);	// Round off correctly w/.5.
			DD_PostEvent(&ev);
		}
		if(state.povAngle != IJOY_POV_CENTER)
		{
			// The new angle becomes active.
			ev.type = ev_povdown;
			ev.data1 = (int) (state.povAngle/45 + .5);
			DD_PostEvent(&ev);
		}
		oldPOV = state.povAngle;
	}

	// Send the joystick movement event (XYZ and rotation-XYZ).
	ev.type = ev_joystick;

	// The input code returns the axis positions in the range -10000..10000.
	// The output axis data must be in range -100..100.
	// Increased sensitivity causes the axes to max out earlier.
	// Check that the divisor is valid.
/*	if(div < 10) div = 10;
	if(div > 100) div = 100;
*/
	ev.data1 = state.axis[0];
	ev.data2 = state.axis[1];
	ev.data3 = state.axis[2];
	ev.data4 = state.rotAxis[0];
	ev.data5 = state.rotAxis[1];
	ev.data6 = state.rotAxis[2];
/*	CLAMP(ev.data1);
	CLAMP(ev.data2);
	CLAMP(ev.data3);
	CLAMP(ev.data4);
	CLAMP(ev.data5);
	CLAMP(ev.data6);*/

	DD_PostEvent(&ev);

	// The sliders.
	memset(&ev, 0, sizeof(ev));
	ev.type = ev_joyslider;

	ev.data1 = state.slider[0];
	ev.data2 = state.slider[1];
/*	CLAMP(ev.data1);
	CLAMP(ev.data2);*/

	DD_PostEvent(&ev);
}

/*
 * The input thread is started right before the main loop is entered.
 * The thread's task is to poll controllers for input, generate input
 * events that the refresh thread will respond to, and generate ticcmds
 * for local players. The commands are generated by examining the current
 * state of the controls assigned for the player.
 *
 * The input thread operates at a fixed frequency. For historical reasons, 
 * 35 Hz seems appropriate.
 */
int DD_InputThread(void *parm)
{
	double nowTime = Sys_GetSeconds(), lastTime;
	int i;

	// This global variable will be set to true when the input thread 
	// should exit.
	stopInputThread = false;

	while(!stopInputThread)
	{
		lastTime = nowTime;
		
		// Poll controllers for input.
		DD_ReadMouse();
		DD_ReadJoystick();
		DD_ReadKeyboard();

		// Update the axis controls via the axis bindings.
		B_UpdateAxisControls();

		// Generate ticcmds for local players.
		for(i = 0; i < DDMAXPLAYERS; i++)
			if(Net_IsLocalPlayer(i))
			{
				// Build a command for this player.
				P_BuildCommand(i);
			}
		
		// Wait until enough time has passed.
		while((nowTime = Sys_GetSeconds()) - lastTime < 1.0/inputRate
			&& !stopInputThread)
		{
			// Let's sleep for a while.
			Sys_Sleep(350/inputRate);
		}

		lastTime = nowTime;
	}
	return 0;
}

/*
 * Start the input thread, which will periodically generate input events.
 */
void DD_StartInput(void)
{
	inputThreadHandle = Sys_StartThread(DD_InputThread, NULL, 0);
}

/*
 * Stop the input thread. Blocks until the thread has been terminated.
 */
void DD_StopInput(void)
{
	// Tell the input thread to immediately break the loop and exit.
	stopInputThread = true;

	// Wait for the input thread to stop.
	Sys_WaitThread(inputThreadHandle);
}

/*
 * Initializes the key mappings to the default values.
 */
void DD_InitInput(void)
{
	// A mutex is used to control access to the event queue. The input 
	// threads writes to the queue and the refresh queue reads events 
	// from it.
	eventQueueLock = Sys_CreateMutex("EventQueueMutex");

	DD_DefaultKeyMapping();
}

/*
 * Shut down the input subsystem. The input thread is stopped.
 */
void DD_ShutdownInput(void)
{
	DD_StopInput();

	Sys_DestroyMutex(eventQueueLock);
	eventQueueLock = 0;
}
