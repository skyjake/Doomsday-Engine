/**
 * @file joystick_win32.cpp
 * Joystick input for Windows. @ingroup input
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "directinput.h"

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "dd_winit.h"

extern int novideo;

int joyDevice = 0; ///< cvar Joystick index to use.
byte useJoystick = false; ///< cvar Joystick input enabled?

static LPDIRECTINPUTDEVICE8 didJoy;
static DIDEVICEINSTANCE firstJoystick;

static int counter;

void Joystick_Register(void)
{
    C_VAR_INT("input-joy-device", &joyDevice, CVF_NO_MAX | CVF_PROTECTED, 0, 0);
    C_VAR_BYTE("input-joy", &useJoystick, 0, 0, 1);
}

static BOOL CALLBACK enumJoysticks(LPCDIDEVICEINSTANCE lpddi, void* ref)
{
    // The first joystick is used by default.
    if(!firstJoystick.dwSize)
        memcpy(&firstJoystick, lpddi, sizeof(*lpddi));

    if(counter == joyDevice)
    {
        // We'll use this one.
        memcpy(ref, lpddi, sizeof(*lpddi));
        return DIENUM_STOP;
    }
    counter++;
    return DIENUM_CONTINUE;
}

boolean Joystick_Init(void)
{
    int joyProp[] = {
        DIJOFS_X, DIJOFS_Y, DIJOFS_Z,
        DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ,
        DIJOFS_SLIDER(0), DIJOFS_SLIDER(1)
    };
    const char* axisName[] = {
        "X", "Y", "Z", "RX", "RY", "RZ", "Slider 1", "Slider 2"
    };

    if(isDedicated || CommandLine_Check("-nojoy")) return false;

    HWND hWnd = (HWND) Window_NativeHandle(Window_Main());
    if(!hWnd)
    {
        Con_Error("Joystick_Init: Main window not available, cannot continue.");
        return false;
    }

    LPDIRECTINPUT8 dInput = DirectInput_IVersion8();
    if(!dInput)
    {
        Con_Message("Joystick_Init: DirectInput version 8 interface not available, cannot continue.\n");
        return false;
    }

    // ddi will contain info for the joystick device.
    DIDEVICEINSTANCE ddi;
    memset(&firstJoystick, 0, sizeof(firstJoystick));
    memset(&ddi, 0, sizeof(ddi));
    counter = 0;

    // Find the joystick we want by doing an enumeration.
    dInput->EnumDevices(DI8DEVCLASS_GAMECTRL, enumJoysticks, &ddi, DIEDFL_ALLDEVICES);

    // Was the joystick we want found?
    if(!ddi.dwSize)
    {
        // Use the default joystick.
        if(!firstJoystick.dwSize)
            return false; // Not found.

        Con_Message("Joystick_Init: joydevice = %i, out of range.\n", joyDevice);
        // Use the first joystick that was found.
        memcpy(&ddi, &firstJoystick, sizeof(ddi));
    }

    // Show some info.
    Con_Message("Joystick_Init: %s\n", ddi.tszProductName);

    // Create the joystick device.
    HRESULT hr = dInput->CreateDevice(ddi.guidInstance, &didJoy, 0);
    if(FAILED(hr))
    {
        Con_Message("Joystick_Init: Failed to create device (0x%x).\n", hr);
        return false;
    }

    // Set data format.
    hr = didJoy->SetDataFormat(&c_dfDIJoystick);
    if(FAILED(hr))
    {
        Con_Message("Joystick_Init: Failed to set data format (0x%x).\n", hr);
        goto kill_joy;
    }

    // Set behavior.
    hr = didJoy->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
    if(FAILED(hr))
    {
        Con_Message("Joystick_Init: Failed to set co-op level (0x%x: %s).\n",
                    hr, DirectInput_ErrorMsg(hr));
        goto kill_joy;
    }

    // Set properties.
    for(int i = 0; i < sizeof(joyProp) / sizeof(joyProp[0]); ++i)
    {
        hr = didJoy->SetProperty(DIPROP_RANGE, DIPropRange(DIPH_BYOFFSET, joyProp[i], IJOY_AXISMIN, IJOY_AXISMAX));
        if(FAILED(hr))
        {
            if(verbose)
                Con_Message("Joystick_Init: Failed to set axis '%s' range (0x%x: %s).\n",
                            axisName[i], hr, DirectInput_ErrorMsg(hr));
        }
    }

    // Set no dead zone. We'll handle this ourselves thanks...
    hr = didJoy->SetProperty(DIPROP_DEADZONE, DIPropDWord(DIPH_DEVICE));
    if(FAILED(hr))
    {
        Con_Message("Joystick_Init: Failed to set dead zone (0x%x: %s).\n",
                    hr, DirectInput_ErrorMsg(hr));
    }

    // Set absolute mode.
    hr = didJoy->SetProperty(DIPROP_AXISMODE, DIPropDWord(DIPH_DEVICE, 0, DIPROPAXISMODE_ABS));
    if(FAILED(hr))
    {
        Con_Message("Joystick_Init: Failed to set absolute axis mode (0x%x: %s).\n",
                    hr, DirectInput_ErrorMsg(hr));
    }

    // Acquire it.
    didJoy->Acquire();

    // Initialization was successful.
    return true;

  kill_joy:
    I_SAFE_RELEASE(didJoy);
    return false;
}

void Joystick_Shutdown(void)
{
    DirectInput_KillDevice(&didJoy);
}

boolean Joystick_IsPresent(void)
{
    return (didJoy != 0);
}

void Joystick_GetState(joystate_t* state)
{
    static BOOL oldButtons[IJOY_MAXBUTTONS]; // Thats a lot of buttons.

    memset(state, 0, sizeof(*state));

    // Initialization has been done?
    if(!didJoy || !useJoystick) return;

    // Some joysticks must be polled.
    didJoy->Poll();

    DIJOYSTATE dijoy;
    DWORD tries = 1;
    BOOL acquired = FALSE;
    while(!acquired && tries > 0)
    {
        HRESULT hr = didJoy->GetDeviceState(sizeof(dijoy), &dijoy);
        if(SUCCEEDED(hr))
        {
            acquired = TRUE;
        }
        else if(tries > 0)
        {
            // Try to reacquire.
            didJoy->Acquire();
            tries--;
        }
    }

    if(!acquired)
        return; // The operation is a failure.

    state->numAxes = 8;
    state->axis[0] = (int) dijoy.lX;
    state->axis[1] = (int) dijoy.lY;
    state->axis[2] = (int) dijoy.lZ;
    state->axis[3] = (int) dijoy.lRx;
    state->axis[4] = (int) dijoy.lRy;
    state->axis[5] = (int) dijoy.lRz;
    state->axis[6] = (int) dijoy.rglSlider[0];
    state->axis[7] = (int) dijoy.rglSlider[1];

    state->numButtons = 32;
    for(DWORD i = 0; i < IJOY_MAXBUTTONS; ++i)
    {
        BOOL isDown = (dijoy.rgbButtons[i] & 0x80? TRUE : FALSE);

        state->buttonDowns[i] =
            state->buttonUps[i] = 0;
        if(isDown && !oldButtons[i])
            state->buttonDowns[i] = 1;
        else if(!isDown && oldButtons[i])
            state->buttonUps[i] = 1;

        oldButtons[i] = isDown;
    }

    state->numHats = 4;
    for(DWORD i = 0; i < IJOY_MAXHATS; ++i)
    {
        DWORD pov = dijoy.rgdwPOV[i];

        if((pov & 0xffff) == 0xffff)
            state->hatAngle[i] = IJOY_POV_CENTER;
        else
            state->hatAngle[i] = pov / 100.0f;
    }
}
