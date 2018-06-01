/**
 * @file mouse_win32.h
 * Mouse driver that gets mouse input from DirectInput on Windows.
 * @ingroup input
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_MOUSE_WIN32_H
#define DE_MOUSE_WIN32_H

#include "ui/sys_input.h"

#ifdef __cplusplus
extern "C" {
#endif

extern mouseinterface_t win32Mouse;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_MOUSE_WIN32_H
