/**
 * @file mouse_win32.cpp
 * Mouse driver that gets mouse input from DirectInput on Windows.
 * @ingroup input
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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
#include "sys_input.h"
#include "sys_system.h"
#include "con_main.h"
#include "window.h"
#include <de/c_wrapper.h>

static LPDIRECTINPUTDEVICE8 didMouse;
static boolean mouseTrapped;
static DIMOUSESTATE2 mstate; ///< Polled state.

static int Mouse_Win32_Init(void)
{
    if(ArgCheck("-nomouse") || novideo) return false;

    // We'll need a window handle for this.
    HWND hWnd = (HWND) Window_NativeHandle(Window_Main());
    if(!hWnd)
    {
        Con_Error("Mouse_Init: Main window not available, cannot init mouse.");
        return false;
    }

    HRESULT hr = -1;
    // Prefer the newer version 8 interface if available.
    if(LPDIRECTINPUT8 dInput = DirectInput_IVersion8())
    {
        hr = dInput->CreateDevice(GUID_SysMouse, &didMouse, 0);
    }
    else if(LPDIRECTINPUT dInput = DirectInput_IVersion3())
    {
        hr = dInput->CreateDevice(GUID_SysMouse, (LPDIRECTINPUTDEVICE*) &didMouse, 0);
    }

    if(FAILED(hr))
    {
        Con_Message("Mouse_Init: Failed to create device (0x%x: %s).\n",
                    hr, DirectInput_ErrorMsg(hr));
        return false;
    }

    // Set data format.
    hr = didMouse->SetDataFormat(&c_dfDIMouse2);
    if(FAILED(hr))
    {
        Con_Message("Mouse_Init: Failed to set data format (0x%x: %s).\n",
                    hr, DirectInput_ErrorMsg(hr));
        goto kill_mouse;
    }

    // Set behavior.
    hr = didMouse->SetCooperativeLevel(hWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    if(FAILED(hr))
    {
        Con_Message("Mouse_Init: Failed to set co-op level (0x%x: %s).\n",
                    hr, DirectInput_ErrorMsg(hr));
        goto kill_mouse;
    }

    // Acquire the device.
    //didMouse->Acquire();
    //mouseTrapped = true;

    // We will be told when to trap the mouse.
    mouseTrapped = false;

    // Init was successful.
    return true;

  kill_mouse:
    I_SAFE_RELEASE(didMouse);
    return false;
}

static void Mouse_Win32_Shutdown(void)
{
    DirectInput_KillDevice(&didMouse);
}

static void Mouse_Win32_Poll(void)
{
    if(!mouseTrapped)
    {
        // We are not supposed to be reading the mouse right now.
        return;
    }

    // Try to get the mouse state.
    int tries = 1;
    BOOL acquired = false;
    while(!acquired && tries > 0)
    {
        HRESULT hr = didMouse->GetDeviceState(sizeof(mstate), &mstate);
        if(SUCCEEDED(hr))
        {
            acquired = true;
        }
        else if(tries > 0)
        {
            // Try to reacquire.
            didMouse->Acquire();
            tries--;
        }
    }

    if(!acquired)
    {
        // The operation is a failure.
        memset(&mstate, 0, sizeof(mstate));
    }
}

static void Mouse_Win32_GetState(mousestate_t* state)
{
    /**
     * We need to map the mouse buttons as follows:
     *         DX  : Deng
     * (left)   0  >  0
     * (right)  1  >  2
     * (center) 2  >  1
     * (b4)     3  >  5
     * (b5)     4  >  6
     * (b6)     5  >  7
     * (b7)     6  >  8
     * (b8)     7  >  9
     */
    static const int buttonMap[] = { 0, 2, 1, 5, 6, 7, 8, 9 };
    static BOOL oldButtons[8];
    static int oldZ;

    memset(state, 0, sizeof(*state));
    if(!mouseTrapped)
    {
        // We are not supposed to be reading the mouse right now.
        return;
    }

    // Fill in the state structure.
    state->axis[IMA_POINTER].x = (int) mstate.lX;
    state->axis[IMA_POINTER].y = (int) mstate.lY;

    // If this is called again before re-polling, we don't want to return
    // these deltas again.
    mstate.lX = 0;
    mstate.lY = 0;

    for(DWORD i = 0; i < 8; ++i)
    {
        BOOL isDown = (mstate.rgbButtons[i] & 0x80? TRUE : FALSE);
        int id;

        id = buttonMap[i];

        state->buttonDowns[id] =
            state->buttonUps[id] = 0;
        if(isDown && !oldButtons[i])
            state->buttonDowns[id] = 1;
        else if(!isDown && oldButtons[i])
            state->buttonUps[id] = 1;

        oldButtons[i] = isDown;
    }

    // Handle mouse wheel (convert to buttons).
    if(mstate.lZ == 0)
    {
        state->buttonDowns[3] = state->buttonDowns[4] = 0;

        if(oldZ > 0)
            state->buttonUps[3] = 1;
        else if(oldZ < 0)
            state->buttonUps[4] = 1;
    }
    else
    {
        if(mstate.lZ > 0 && !(oldZ > 0))
        {
            state->buttonDowns[3] = 1;
            if(state->buttonDowns[4])
                state->buttonUps[3] = 1;
        }
        else if(mstate.lZ < 0 && !(oldZ < 0))
        {
            state->buttonDowns[4] = 1;
            if(state->buttonDowns[3])
                state->buttonUps[4] = 1;
        }
    }

    oldZ = (int) mstate.lZ;
}

static void Mouse_Win32_Trap(boolean enabled)
{
    assert(didMouse);

    mouseTrapped = (enabled != 0);
    if(enabled)
    {
        didMouse->Acquire();
    }
    else
    {
        didMouse->Unacquire();
    }
}

// The global interface.
extern "C" {
mouseinterface_t win32Mouse = {
    Mouse_Win32_Init,
    Mouse_Win32_Shutdown,
    Mouse_Win32_Poll,
    Mouse_Win32_GetState,
    Mouse_Win32_Trap
};
}
