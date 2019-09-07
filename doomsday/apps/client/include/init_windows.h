/** @file dd_winit.h  Win32 Initialization.
 *
 * @authors Copyright © 2003-2017 Jaakko Kernen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DE_WINIT_H
#define DE_WINIT_H

#define WIN32_LEAN_AND_MEAN

#include "dd_pinit.h"
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

dd_bool DD_Win32_Init(void);
void DD_Shutdown(void);

const char *DD_Win32_GetLastErrorMessage(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_WINIT_H */
