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
 * sys_input.c: Game Controllers
 *
 * Keyboard, mouse and joystick input using DirectInput.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800

#include <stdlib.h>
#include <windows.h>
#include <dinput.h>
#include <assert.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define KEYBUFSIZE	32
#define INV(x, axis)	(joyInverseAxis[axis]? -x : x)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE hInstApp;
extern HWND hWndMain;
extern boolean novideo;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int joydevice = 0;				// Joystick index to use.
boolean usejoystick = false;	// Joystick input enabled?
int joyInverseAxis[8];			// Axis inversion (default: all false).

cvar_t inputCVars[] =
{
	"i_JoyDevice",		CVF_HIDE|CVF_NO_ARCHIVE|CVF_NO_MAX|CVF_PROTECTED, CVT_INT,	&joydevice,		0,	0,	"ID of joystick to use (if more than one).",
	"i_UseJoystick",	CVF_HIDE|CVF_NO_ARCHIVE,	CVT_BYTE,	&usejoystick,	0,	1,	"1=Enable joystick input.",
//--------------------------------------------------------------------------
	"input-joy-device",	CVF_NO_MAX|CVF_PROTECTED,	CVT_INT,	&joydevice,		0,	0,	"ID of joystick to use (if more than one).",
	"input-joy",		0,							CVT_BYTE,	&usejoystick,	0,	1,	"1=Enable joystick input.",
	"input-joy-x-inverse",			0,	CVT_INT,	&joyInverseAxis[0],	0, 1,	"1=Inverse joystick X axis.",
	"input-joy-y-inverse",			0,	CVT_INT,	&joyInverseAxis[1],	0, 1,	"1=Inverse joystick Y axis.",
	"input-joy-z-inverse",			0,	CVT_INT,	&joyInverseAxis[2],	0, 1,	"1=Inverse joystick Z axis.",
	"input-joy-rx-inverse",			0,	CVT_INT,	&joyInverseAxis[3],	0, 1,	"1=Inverse joystick RX axis.",
	"input-joy-ry-inverse",			0,	CVT_INT,	&joyInverseAxis[4],	0, 1,	"1=Inverse joystick RY axis.",
	"input-joy-rz-inverse",			0,	CVT_INT,	&joyInverseAxis[5],	0, 1,	"1=Inverse joystick RZ axis.",
	"input-joy-slider1-inverse",	0,	CVT_INT,	&joyInverseAxis[6], 0, 1,	"1=Inverse joystick slider 1.",
	"input-joy-slider2-inverse",	0,	CVT_INT,	&joyInverseAxis[7], 0, 1,	"1=Inverse joystick slider 2.",
	NULL
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initOk = false;		

static HRESULT hr;
static LPDIRECTINPUT8 dInput;
static LPDIRECTINPUTDEVICE8 didKeyb, didMouse;
static LPDIRECTINPUTDEVICE8 didJoy;
static DIDEVICEINSTANCE firstJoystick;

static int counter;

// CODE --------------------------------------------------------------------

//===========================================================================
// I_ErrorMsg
//===========================================================================
const char *I_ErrorMsg(HRESULT hr)
{
	return hr == DI_OK? "OK"
		: hr == DIERR_GENERIC? "Generic error"
		: hr == DI_PROPNOEFFECT? "Property has no effect"
		: hr == DIERR_INVALIDPARAM? "Invalid parameter"
		: hr == DIERR_NOTINITIALIZED? "Not initialized"
		: hr == DIERR_UNSUPPORTED? "Unsupported"
		: hr == DIERR_NOTFOUND? "Not found"
		: "?";
}

//===========================================================================
// I_SetProperty
//===========================================================================
HRESULT I_SetProperty(void *dev, REFGUID property, DWORD how,
						 DWORD obj, DWORD data)
{
	DIPROPDWORD dipdw;	

	dipdw.diph.dwSize = sizeof(dipdw);
	dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
	dipdw.diph.dwObj = obj;
	dipdw.diph.dwHow = how;
	dipdw.dwData = data;
	return IDirectInputDevice8_SetProperty( (LPDIRECTINPUTDEVICE8) dev, 
		property, &dipdw.diph);
}

//===========================================================================
// I_SetRangeProperty
//===========================================================================
HRESULT I_SetRangeProperty(void *dev, REFGUID property, DWORD how,
							  DWORD obj, int min, int max)
{
	DIPROPRANGE dipr;

	dipr.diph.dwSize = sizeof(dipr);
	dipr.diph.dwHeaderSize = sizeof(dipr.diph);
	dipr.diph.dwObj = obj;
	dipr.diph.dwHow = how;
	dipr.lMin = min;
	dipr.lMax = max;
	return IDirectInputDevice8_SetProperty( (LPDIRECTINPUTDEVICE8) dev,
		property, &dipr.diph);
}

//===========================================================================
// I_InitMouse
//===========================================================================
void I_InitMouse(void)
{
	if(ArgCheck("-nomouse") || novideo) return;

	hr = IDirectInput_CreateDevice(dInput, &GUID_SysMouse, &didMouse, 0);
	if(FAILED(hr))
	{
		Con_Message("I_InitMouse: failed to create device (0x%x).\n", hr);
		return;
	}
	
	// Set data format.
	hr = IDirectInputDevice_SetDataFormat(didMouse, &c_dfDIMouse2);
	if(FAILED(hr))
	{
		Con_Message("I_InitMouse: failed to set data format (0x%x).\n", hr);
		goto kill_mouse;
	}

	// Set behaviour.
	hr = IDirectInputDevice_SetCooperativeLevel(didMouse, hWndMain, 
		DISCL_FOREGROUND | DISCL_EXCLUSIVE);
	if(FAILED(hr))
	{
		Con_Message("I_InitMouse: failed to set co-op level (0x%x).\n", hr);
		goto kill_mouse;
	}

	// Acquire the device.
	IDirectInputDevice_Acquire(didMouse);

	// Init was successful.
	return;

kill_mouse:
	IDirectInputDevice_Release(didMouse);
	didMouse = 0;
}

//===========================================================================
// I_JoyEnum
//===========================================================================
BOOL CALLBACK I_JoyEnum(LPCDIDEVICEINSTANCE lpddi, void *ref)
{
	// The first joystick is used by default.
	if(!firstJoystick.dwSize)
		memcpy(&firstJoystick, lpddi, sizeof(*lpddi));

	if(counter == joydevice)
	{
		// We'll use this one.
		memcpy(ref, lpddi, sizeof(*lpddi));
		return DIENUM_STOP;
	}
	counter++;
	return DIENUM_CONTINUE;
}

//===========================================================================
// I_InitJoystick
//===========================================================================
void I_InitJoystick(void)
{
	DIDEVICEINSTANCE ddi;
	int i, joyProp[] =
	{
		DIJOFS_X, DIJOFS_Y, DIJOFS_Z,
		DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ,
		DIJOFS_SLIDER(0), DIJOFS_SLIDER(1)
	};
	const char *axisName[] =
	{
		"X", "Y", "Z", "RX", "RY", "RZ", "Slider 1", "Slider 2"
	};

	if(ArgCheck("-nojoy")) return;

	// ddi will contain info for the joystick device.
	memset(&firstJoystick, 0, sizeof(firstJoystick));
	memset(&ddi, 0, sizeof(ddi));
	counter = 0;

	// Find the joystick we want by doing an enumeration.
	IDirectInput_EnumDevices(dInput, DI8DEVCLASS_GAMECTRL, I_JoyEnum,
		&ddi, DIEDFL_ALLDEVICES);

	// Was the joystick we want found?
	if(!ddi.dwSize) 
	{
		// Use the default joystick.
		if(!firstJoystick.dwSize) return; // Not found.
		Con_Message("I_InitJoystick: joydevice = %i, out of range.\n", 
			joydevice);
		// Use the first joystick that was found.
		memcpy(&ddi, &firstJoystick, sizeof(ddi));
	}

	// Show some info.
	Con_Message("I_InitJoystick: %s\n", ddi.tszProductName);

	// Create the joystick device.
	hr = IDirectInput8_CreateDevice(dInput, &ddi.guidInstance, &didJoy, 0);
	if(FAILED(hr))
	{
		Con_Message("I_InitJoystick: failed to create device (0x%x).\n", hr);
		return;
	}

	// Get a DirectInputDevice2 interface.
	/*hr = IDirectInputDevice_QueryInterface(didJoy1, &IID_IDirectInputDevice2,
		(void**) &didJoy);
	
	// Release the old interface.
	IDirectInputDevice_Release(didJoy1);

	if(FAILED(hr))
	{
		Con_Message("I_InitJoystick: failed to get DirectInputDevice2 (0x%x).\n", hr);
		return;
	}*/

	// Set data format.
	if(FAILED(hr = IDirectInputDevice_SetDataFormat(didJoy, &c_dfDIJoystick)))
	{
		Con_Message("I_InitJoystick: failed to set data format (0x%x).\n", hr);
		goto kill_joy;
	}

	// Set behaviour.
	if(FAILED(hr = IDirectInputDevice_SetCooperativeLevel(didJoy, hWndMain,
		DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
	{
		Con_Message("I_InitJoystick: failed to set co-op level (0x%x: %s).\n", 
			hr, I_ErrorMsg(hr));
		goto kill_joy;
	}

	// Set properties.
	for(i = 0; i < sizeof(joyProp)/sizeof(joyProp[0]); i++)
	{
		if(FAILED(hr = I_SetRangeProperty(didJoy, DIPROP_RANGE, DIPH_BYOFFSET,
			joyProp[i], IJOY_AXISMIN, IJOY_AXISMAX)))
		{
			if(verbose) Con_Message("I_InitJoystick: failed to set %s "
				"range (0x%x: %s).\n", axisName[i], hr, I_ErrorMsg(hr));
			//goto kill_joy;
		}
	}

	// Set no dead zone.
	if(FAILED(hr = I_SetProperty(didJoy, DIPROP_DEADZONE, DIPH_DEVICE, 0, 0)))
	{
		Con_Message("I_InitJoystick: failed to set dead zone (0x%x: %s).\n", 
			hr, I_ErrorMsg(hr));
	}

	// Set absolute mode.
	if(FAILED(hr = I_SetProperty(didJoy, DIPROP_AXISMODE, DIPH_DEVICE,
		0, DIPROPAXISMODE_ABS)))
	{
		Con_Message("I_InitJoystick: failed to set absolute axis mode (0x%x: %s).\n",
			hr, I_ErrorMsg(hr));
	}

	// Acquire it.
	IDirectInputDevice_Acquire(didJoy);

	// Initialization was successful.
	return;

kill_joy:
	IDirectInputDevice_Release(didJoy);
	didJoy = 0;
}

//===========================================================================
// I_KillDevice
//===========================================================================
void I_KillDevice(LPDIRECTINPUTDEVICE8 *dev)
{
	if(!*dev) return;
	IDirectInputDevice8_Unacquire(*dev);
	IDirectInputDevice8_Release(*dev);
	*dev = 0;
}

#if 0
//===========================================================================
// I_KillDevice2
//===========================================================================
void I_KillDevice2(LPDIRECTINPUTDEVICE2 *dev)
{
	if(!*dev) return;
	IDirectInputDevice2_Unacquire(*dev);
	IDirectInputDevice2_Release(*dev);
	*dev = 0;
}
#endif

//===========================================================================
// I_Init
//	Initialize input. Returns true if successful.
//===========================================================================
int I_Init(void)
{
	if(initOk) return true;	// Already initialized.

	// We'll create the DirectInput object. The only required input device
	// is the keyboard. The others are optional.
	dInput = NULL;
	if(FAILED(hr = CoCreateInstance(&CLSID_DirectInput8, NULL,
		CLSCTX_INPROC_SERVER, &IID_IDirectInput8, &dInput)) 
		|| FAILED(hr = IDirectInput8_Initialize(dInput, hInstApp, 
			DIRECTINPUT_VERSION)))
	{
		Con_Message("I_Init: DirectInput 8 init failed (0x%x).\n", hr);
		// Try DInput3 instead.
		// I'm not sure if this works correctly.
		if(FAILED(hr = CoCreateInstance(&CLSID_DirectInput, NULL,
			CLSCTX_INPROC_SERVER, &IID_IDirectInput2W, &dInput))
			|| FAILED(hr = IDirectInput2_Initialize(dInput, hInstApp, 0x0300)))
		{
			Con_Message("I_Init: failed to create DirectInput 3 object (0x%x).\n", hr);
			return false;
		}
		Con_Message("I_Init: Using DirectInput 3.\n");
	}
	if(!dInput)
	{
		Con_Message("I_Init: DirectInput init failed.\n");
		return false;
	}

	// Create the keyboard device.
	hr = IDirectInput_CreateDevice(dInput, &GUID_SysKeyboard, &didKeyb, 0);
	if(FAILED(hr))
	{
		Con_Message("I_Init: failed to create keyboard device (0x%x).\n", hr);
		return false;
	}

	// Setup the keyboard input device.
	hr = IDirectInputDevice_SetDataFormat(didKeyb, &c_dfDIKeyboard);
	if(FAILED(hr))
	{
		Con_Message("I_Init: failed to set keyboard data format (0x%x).\n", hr);
		return false;
	}

	// Set behaviour.
	hr = IDirectInputDevice_SetCooperativeLevel(didKeyb, hWndMain, 
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if(FAILED(hr))
	{
		Con_Message("I_Init: failed to set keyboard co-op level (0x%x).\n", hr);
		return false;
	}

	// The input buffer size.
	hr = I_SetProperty(didKeyb, DIPROP_BUFFERSIZE, DIPH_DEVICE, 0, 
		KEYBUFSIZE);
	if(FAILED(hr))
	{
		Con_Message("I_Init: failed to set keyboard buffer size (0x%x).\n", hr);
		return false;
	}
	
	// Acquire the keyboard.
	IDirectInputDevice_Acquire(didKeyb);

	// Create the mouse and joystick devices. It doesn't matter if the init
	// fails for them.
	I_InitMouse();
	I_InitJoystick();
	initOk = true;
	return true;
}

//===========================================================================
// I_Shutdown
//===========================================================================
void I_Shutdown(void)
{
	if(!initOk) return;	// Not initialized.
	initOk = false;
	
	// Release all the input devices.
	I_KillDevice(&didKeyb);
	I_KillDevice(&didMouse);
	I_KillDevice(&didJoy);

	// Release DirectInput.
	IDirectInput_Release(dInput);
	dInput = 0;
}

//===========================================================================
// I_MousePresent
//===========================================================================
boolean I_MousePresent(void)
{
	return (didMouse != 0);
}

//===========================================================================
// I_JoystickPresent
//===========================================================================
boolean I_JoystickPresent(void)
{
	return (didJoy != 0);
}

//===========================================================================
// I_GetKeyEvents
//===========================================================================
int I_GetKeyEvents(keyevent_t *evbuf, int bufsize)
{
	DIDEVICEOBJECTDATA keyData[KEYBUFSIZE];
	int tries, i, num;

	if(!initOk) return 0;
	
	// Try two times to get the data.
	for(tries=1; tries>=0; tries--)
	{
		num = KEYBUFSIZE;
		hr = IDirectInputDevice_GetDeviceData(didKeyb, 
			sizeof(DIDEVICEOBJECTDATA), keyData, &num, 0);
		if(SUCCEEDED(hr)) break;
		
		// No tries left? The operation is a failure.
		if(!tries) return 0;		

		// Try to reacquire.
		IDirectInputDevice_Acquire(didKeyb);
	}

	// Get the events.
	for(i=0; i<num && i<bufsize; i++)
	{
		evbuf[i].event = (keyData[i].dwData & 0x80)? IKE_KEY_DOWN : IKE_KEY_UP;
		evbuf[i].code = (unsigned char) keyData[i].dwOfs;
	}
	return i;
}

//===========================================================================
// I_GetMouseState
//===========================================================================
void I_GetMouseState(mousestate_t *state)
{
	DIMOUSESTATE2 mstate;
	int i, tries;

	memset(state, 0, sizeof(*state));

	// Has the mouse been initialized?
	if(!didMouse || !initOk) return;

	// Try to get the mouse state.
	for(tries = 1; tries >= 0; tries--)
	{
		hr = IDirectInputDevice_GetDeviceState(didMouse, 
			sizeof(mstate), &mstate);
		if(SUCCEEDED(hr)) break;

		// Out of tries?
		if(!tries) return;

		// Try to reacquire.
		IDirectInputDevice_Acquire(didMouse);
	}

	// Fill in the state structure.
	state->x = mstate.lX;
	state->y = mstate.lY;
	state->z = mstate.lZ;
	
	// The buttons bitfield is ordered according to the numbering.
	for(i = 0; i < 8; i++)
		if(mstate.rgbButtons[i] & 0x80) state->buttons |= 1 << i;
}

//===========================================================================
// I_GetJoystickState
//===========================================================================
void I_GetJoystickState(joystate_t *state)
{
	int tries, i;
	int pov;
	DIJOYSTATE dijoy;

	memset(state, 0, sizeof(*state));

	// Initialization has not been done.
	if(!didJoy || !usejoystick || !initOk) return;

	// Some joysticks need to be polled.
	IDirectInputDevice8_Poll(didJoy);

	for(tries = 1; tries >= 0; tries--)
	{
		hr = IDirectInputDevice8_GetDeviceState(didJoy, 
			sizeof(dijoy), &dijoy);
		if(SUCCEEDED(hr)) break;

		// Out of tries?
		if(!tries) return;

		// Try to reacquire.
		IDirectInputDevice8_Acquire(didJoy);
	}

	state->axis[0] = INV(dijoy.lX, 0);
	state->axis[1] = INV(dijoy.lY, 1);
	state->axis[2] = INV(dijoy.lZ, 2);

	state->rotAxis[0] = INV(dijoy.lRx, 3);
	state->rotAxis[1] = INV(dijoy.lRy, 4);
	state->rotAxis[2] = INV(dijoy.lRz, 5);

	state->slider[0] = INV(dijoy.rglSlider[0], 6);
	state->slider[1] = INV(dijoy.rglSlider[1], 7);

	for(i = 0; i < IJOY_MAXBUTTONS; i++)
		state->buttons[i] = (dijoy.rgbButtons[i] & 0x80) != 0;
	pov = dijoy.rgdwPOV[0];
	if((pov & 0xffff) == 0xffff)
		state->povAngle = IJOY_POV_CENTER;
	else
		state->povAngle = pov / 100.0f;
}
