/**
 * @file displaymode_x11.cpp
 * X11 implementation of the DisplayMode native functionality. @ingroup gl
 *
 * Uses the XRandR extension to manipulate the display.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QDebug>

#include "de_platform.h"
#include "displaymode_native.h"

#include <assert.h>
#include <vector>

#if 0
static std::vector<DEVMODE> devModes;
static DEVMODE currentDevMode;
#endif

void DisplayMode_Native_Init(void)
{
    // Let's see which modes are available.

    // And which is the current mode?
}

void DisplayMode_Native_Shutdown(void)
{
#if 0
    devModes.clear();
#endif
}

int DisplayMode_Native_Count(void)
{
#if 0
    return devModes.size();
#endif
}

void DisplayMode_Native_GetMode(int index, DisplayMode* mode)
{
#if 0
    assert(index >= 0 && index < DisplayMode_Native_Count());
    *mode = devToDisplayMode(devModes[index]);
#endif
}

void DisplayMode_Native_GetCurrentMode(DisplayMode* mode)
{
#if 0
    *mode = devToDisplayMode(currentDevMode);
#endif
}

#if 0
static int findMode(const DisplayMode* mode)
{
    for(int i = 0; i < DisplayMode_Native_Count(); ++i)
    {
        DisplayMode d = devToDisplayMode(devModes[i]);
        if(DisplayMode_IsEqual(&d, mode))
        {
            return i;
        }
    }
    return -1;
}
#endif

int DisplayMode_Native_Change(const DisplayMode* mode, boolean shouldCapture)
{
    return true;
}
