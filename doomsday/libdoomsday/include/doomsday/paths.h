/** @file basepath.h  Application base path.
 *
 * @deprecated Should use de::App instead.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_BASEPATH_H
#define LIBDOOMSDAY_BASEPATH_H

#include "libdoomsday.h"
#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC char const *DD_BasePath();

LIBDOOMSDAY_PUBLIC char const *DD_RuntimePath();

LIBDOOMSDAY_PUBLIC char const *DD_BinPath();

LIBDOOMSDAY_PUBLIC void DD_SetBasePath(char const *path);

LIBDOOMSDAY_PUBLIC void DD_SetRuntimePath(char const *path);

LIBDOOMSDAY_PUBLIC void DD_SetBinPath(char const *path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOMSDAY_BASEPATH_H
