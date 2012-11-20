/**
 * @file displaymode_native.h
 * Changing and enumerating available display modes using native APIs. @ingroup gl
 *
 * These routines are only intended to be used privately by the DisplayMode
 * class. Never call these directly unless you are sure you want to access
 * platform specific low-level functionality.
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

#ifndef LIBDENG_DISPLAYMODE_NATIVE_H
#define LIBDENG_DISPLAYMODE_NATIVE_H

#include "displaymode.h"

#ifdef __cplusplus
extern "C" {
#endif

void DisplayMode_Native_Init(void);

void DisplayMode_Native_Shutdown(void);

int DisplayMode_Native_Count(void);

void DisplayMode_Native_GetMode(int index, DisplayMode* mode);

void DisplayMode_Native_GetCurrentMode(DisplayMode* mode);

int DisplayMode_Native_Change(const DisplayMode* mode, boolean shouldCapture);

void DisplayMode_Native_GetColorTransfer(displaycolortransfer_t* colors);

void DisplayMode_Native_SetColorTransfer(const displaycolortransfer_t* colors);

#ifdef MACOSX
void DisplayMode_Native_Raise(void* handle);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_DISPLAYMODE_NATIVE_H
