/** @file world/valuetype.h
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_WORLD_VALUETYPE_H
#define LIBDOOMSDAY_WORLD_VALUETYPE_H

/// Value types.
typedef enum {
    DDVT_NONE = -1, ///< Not a read/writeable value type.
    DDVT_BOOL,
    DDVT_BYTE,
    DDVT_SHORT,
    DDVT_INT,       ///< 32 or 64 bit
    DDVT_UINT,
    DDVT_FIXED,
    DDVT_ANGLE,
    DDVT_FLOAT,
    DDVT_DOUBLE,
    DDVT_LONG,
    DDVT_ULONG,
    DDVT_PTR,
    DDVT_BLENDMODE
} valuetype_t;

#endif // LIBDOOMSDAY_WORLD_VALUETYPE_H

