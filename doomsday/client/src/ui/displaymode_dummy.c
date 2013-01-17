/**
 * @file displaymode_dummy.c
 * Dummy implementation of the DisplayMode native functionality. @ingroup gl
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/displaymode_native.h"
#include <de/libdeng1.h>

void DisplayMode_Native_Init(void)
{
}

void DisplayMode_Native_Shutdown(void)
{
}

int DisplayMode_Native_Count(void)
{
    return 0;
}

void DisplayMode_Native_GetMode(int index, DisplayMode* mode)
{
    DENG_UNUSED(index);
    DENG_UNUSED(mode);
}

void DisplayMode_Native_GetCurrentMode(DisplayMode* mode)
{
    DENG_UNUSED(mode);
}

int DisplayMode_Native_Change(const DisplayMode* mode, boolean shouldCapture)
{
    DENG_UNUSED(mode);
    DENG_UNUSED(shouldCapture);
    return true;
}

void DisplayMode_Native_GetColorTransfer(displaycolortransfer_t* colors)
{
    DENG_UNUSED(colors);
}

void DisplayMode_Native_SetColorTransfer(const displaycolortransfer_t* colors)
{
    DENG_UNUSED(colors);
}
