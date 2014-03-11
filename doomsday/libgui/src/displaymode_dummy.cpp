/** @file displaymode_dummy.cpp
 * Dummy implementation of the DisplayMode native functionality.
 * @ingroup gl
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/gui/displaymode_native.h"
#include <de/libdeng2.h>

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

void DisplayMode_Native_GetMode(int index, DisplayMode *mode)
{
    DENG2_UNUSED(index);
    DENG2_UNUSED(mode);
}

void DisplayMode_Native_GetCurrentMode(DisplayMode *mode)
{
    DENG2_UNUSED(mode);
}

int DisplayMode_Native_Change(DisplayMode const *mode, int shouldCapture)
{
    DENG2_UNUSED(mode);
    DENG2_UNUSED(shouldCapture);
    return true;
}

void DisplayMode_Native_GetColorTransfer(DisplayColorTransfer *colors)
{
    DENG2_UNUSED(colors);
}

void DisplayMode_Native_SetColorTransfer(DisplayColorTransfer const *colors)
{
    DENG2_UNUSED(colors);
}
