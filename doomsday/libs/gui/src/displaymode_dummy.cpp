/** @file displaymode_dummy.cpp
 * Dummy implementation of the DisplayMode native functionality.
 * @ingroup gl
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/libcore.h>
#include <QGuiApplication>
#include <QScreen>

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
    DE_UNUSED(index);
    DE_UNUSED(mode);
}

void DisplayMode_Native_GetCurrentMode(DisplayMode *mode)
{
    const auto *scr = QGuiApplication::primaryScreen();
    
    mode->width       = scr->geometry().width();
    mode->height      = scr->geometry().height();
    mode->depth       = scr->depth();
    mode->refreshRate = scr->refreshRate();
}

int DisplayMode_Native_Change(DisplayMode const *mode, int shouldCapture)
{
    DE_UNUSED(mode);
    DE_UNUSED(shouldCapture);
    return true;
}

void DisplayMode_Native_GetColorTransfer(DisplayColorTransfer *colors)
{
    DE_UNUSED(colors);
}

void DisplayMode_Native_SetColorTransfer(DisplayColorTransfer const *colors)
{
    DE_UNUSED(colors);
}

#ifdef MACOSX
void DisplayMode_Native_Raise(void* handle)
{
    DE_UNUSED(handle);
}
#endif
