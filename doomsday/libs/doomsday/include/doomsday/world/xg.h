/** @file world/xg.h
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_WORLD_XG_H
#define LIBDOOMSDAY_WORLD_XG_H

#include "../libdoomsday.h"
#include "xgclass.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC void XG_GetGameClasses();

LIBDOOMSDAY_PUBLIC xgclass_t *XG_Class(int number);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDOOMSDAY_WORLD_XG_H
