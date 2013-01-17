/**
 * @file directinput.cpp
 * DirectInput for Windows. @ingroup input
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

#ifdef __CLIENT__

#include "directinput.h"
#include "dd_winit.h"
#include "con_main.h"

static LPDIRECTINPUT8 dInput;
static LPDIRECTINPUT dInput3;

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

    if(dInput || dInput3) return true;

    // Create the DirectInput interface instance. Try version 8 first.
    if(FAILED(hr = CoCreateInstance(CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IDirectInput8, (LPVOID*)&dInput)) ||
       FAILED(hr = dInput->Initialize(app.hInstance, DIRECTINPUT_VERSION)))
    {
        Con_Message("DirectInput 8 init failed (0x%x).\n", hr);

        // Try the older version 3 interface instead.
        // I'm not sure if this works correctly.
        if(FAILED(hr = CoCreateInstance(CLSID_DirectInput, NULL, CLSCTX_INPROC_SERVER,
                                        IID_IDirectInput2W, (LPVOID*)&dInput3)) ||
           FAILED(hr = dInput3->Initialize(app.hInstance, 0x0300)))
        {
            Con_Message("Failed to create DirectInput 3 object (0x%x).\n", hr);
            return false;
        }

        Con_Message("Using DirectInput 3.\n");
    }

    if(!dInput && !dInput3)
    {
        Con_Message(" DirectInput init failed.\n");
        return false;
    }

    return true;
}

void DirectInput_Shutdown(void)
{
    if(dInput)
    {
        IDirectInput_Release(dInput);
        dInput = 0;
    }
    if(dInput3)
    {
        IDirectInput_Release(dInput3);
        dInput3 = 0;
    }
}

LPDIRECTINPUT8 DirectInput_IVersion8()
{
    return dInput;
}

LPDIRECTINPUT DirectInput_IVersion3()
{
    return dInput3;
}

void DirectInput_KillDevice(LPDIRECTINPUTDEVICE8* dev)
{
    if(*dev) (*dev)->Unacquire();
    I_SAFE_RELEASE(*dev);
}

#endif // __CLIENT__
