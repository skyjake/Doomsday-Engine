/**
 * @file displaymode_macx.h
 * Changing and enumerating available display modes for Mac OS X. @ingroup gl
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

#ifndef LIBDENG_DISPLAYMODE_MACX_H
#define LIBDENG_DISPLAYMODE_MACX_H

#include "displaymode.h"

#ifdef __cplusplus
extern "C" {
#endif

void DisplayMode_Native_Init(void);

int DisplayMode_Native_Count(void);

void DisplayMode_Native_GetMode(int index, DisplayMode* mode);

void DisplayMode_Native_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // LIBDENG_DISPLAYMODE_MACX_H
