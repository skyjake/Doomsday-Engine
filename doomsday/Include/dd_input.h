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
 * dd_input.h: Input Subsystem
 */

#ifndef __DOOMSDAY_BASEINPUT_H__
#define __DOOMSDAY_BASEINPUT_H__

#include "dd_share.h"	// For event_t.
#include "con_decl.h"

// Input devices.
enum
{
	IDEV_KEYBOARD = 0,
	IDEV_MOUSE,
	IDEV_JOY1,
	IDEV_JOY2,
	IDEV_JOY3,
	IDEV_JOY4,
	NUM_INPUT_DEVICES		// Theoretical maximum.
};	

// Input device axis types.
enum
{
	IDAT_STICK = 0,			// joysticks, gamepads
	IDAT_POINTER = 1		// mouse
};

// Input device axis flags.
#define IDA_DISABLED 0x1	// Axis is always zero.
#define IDA_INVERT 0x2		// Real input data should be inverted.
#define IDA_FILTER 0x4		// Average with prev. position.

typedef struct inputdevaxis_s {
	char name[20];			// Symbolic name of the axis.
	int type;				// Type of the axis (pointer or stick).
	int flags;
	float position;			// Current translated position of the axis (-1..1).
	float scale;			// Scaling factor for real input values.
	float deadZone;			
} inputdevaxis_t;

// Input device flags.
#define ID_ACTIVE 0x1		// The input device is active.

typedef struct inputdev_s {
	int flags;
	char name[20];			// Symbolic name of the device.
	int numAxes;			// Number of axes in this input device.
	inputdevaxis_t *axes;
	int numKeys;			// Number of keys for this input device.
	char *keys;				// True/False for each key.
} inputdev_t;

extern int		repWait1, repWait2;
extern int		mouseFilter;
extern int		mouseDisableX, mouseDisableY;
extern int		mouseInverseY;
extern int		mouseWheelSensi;
extern int		joySensitivity;
extern int		joyDeadZone;
extern boolean	showScanCodes;
extern boolean	shiftDown, altDown;

void DD_InitInput(void);
void DD_ShutdownInput(void);
void DD_StartInput(void);
void DD_StopInput(void);

void DD_ReadKeyboard(void);
void DD_ReadMouse(void);
void DD_ReadJoystick(void);

void DD_PostEvent(event_t *ev);
void DD_ProcessEvents(void);
void DD_ClearEvents(void);
void DD_ClearKeyRepeaters(void);
byte DD_ScanToKey(byte scan);
byte DD_KeyToScan(byte key);
byte DD_ModKey(byte key);

void I_InitInputDevices(void);
void I_ShutdownInputDevices(void);
inputdev_t *I_GetDevice(int ident);
boolean I_ParseDeviceAxis(const char *name, inputdev_t **device, int *axis);

D_CMD( KeyMap );
D_CMD( DumpKeyMap );

#endif 
