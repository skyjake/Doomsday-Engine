/**
 * @file directinput.c
 * DirectInput for Windows. @ingroup input
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

#include "directinput.h"
#include "dd_winit.h"
#include "con_main.h"

static LPDIRECTINPUT8 dInput;

const char* DirectInput_ErrorMsg(HRESULT hr)
{
    return hr == DI_OK ? "OK" : hr == DIERR_GENERIC ? "Generic error" : hr ==
        DI_PROPNOEFFECT ? "Property has no effect" : hr ==
        DIERR_INVALIDPARAM ? "Invalid parameter" : hr ==
        DIERR_NOTINITIALIZED ? "Not initialized" : hr ==
        DIERR_UNSUPPORTED ? "Unsupported" : hr ==
        DIERR_NOTFOUND ? "Not found" : "?";
}

int DirectInput_Init(void)
{
    HRESULT hr;

    if(dInput) return true;

    // We'll create the DirectInput object. The only required input device
    // is the keyboard. The others are optional.
    if(FAILED(hr = CoCreateInstance(&CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER,
                                    &IID_IDirectInput8, &dInput)) ||
       FAILED(hr = IDirectInput8_Initialize(dInput, app.hInstance, DIRECTINPUT_VERSION)))
    {
        Con_Message("DirectInput 8 init failed (0x%x).\n", hr);
        // Try DInput3 instead.
        // I'm not sure if this works correctly.
        if(FAILED(hr = CoCreateInstance(&CLSID_DirectInput, NULL, CLSCTX_INPROC_SERVER,
                                        &IID_IDirectInput2W, &dInput)) ||
           FAILED(hr = IDirectInput2_Initialize(dInput, app.hInstance, 0x0300)))
        {
            Con_Message("Failed to create DirectInput 3 object (0x%x).\n", hr);
            return false;
        }
        Con_Message("Using DirectInput 3.\n");
    }

    if(!dInput)
    {
        Con_Message(" DirectInput init failed.\n");
        return false;
    }

    return true;
}

void DirectInput_Shutdown(void)
{
    if(!dInput) return;

    // Release DirectInput.
    IDirectInput_Release(dInput);
    dInput = 0;
}

LPDIRECTINPUT8 DirectInput_Instance()
{
    assert(dInput != 0);
    return dInput;
}

HRESULT DirectInput_SetProperty(LPDIRECTINPUTDEVICE8 dev, REFGUID property, DWORD how, DWORD obj, DWORD data)
{
    DIPROPDWORD dipdw;

    dipdw.diph.dwSize = sizeof(dipdw);
    dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
    dipdw.diph.dwObj = obj;
    dipdw.diph.dwHow = how;
    dipdw.dwData = data;
    return IDirectInputDevice8_SetProperty(dev, property, &dipdw.diph);
}

HRESULT DirectInput_SetRangeProperty(LPDIRECTINPUTDEVICE8 dev, REFGUID property, DWORD how, DWORD obj, int min, int max)
{
    DIPROPRANGE dipr;

    dipr.diph.dwSize = sizeof(dipr);
    dipr.diph.dwHeaderSize = sizeof(dipr.diph);
    dipr.diph.dwObj = obj;
    dipr.diph.dwHow = how;
    dipr.lMin = min;
    dipr.lMax = max;
    return IDirectInputDevice8_SetProperty(dev, property, &dipr.diph);
}

void DirectInput_KillDevice(LPDIRECTINPUTDEVICE8 *dev)
{
    if(*dev) IDirectInputDevice8_Unacquire(*dev);
    I_SAFE_RELEASE(*dev);
}
