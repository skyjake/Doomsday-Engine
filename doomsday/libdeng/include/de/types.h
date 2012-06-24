/**
 * @file types.h
 * Types for libdeng.
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

#ifndef LIBDENG_BASIC_TYPES_H
#define LIBDENG_BASIC_TYPES_H

#include "libdeng.h"

enum
{
    VX = 0,
    VY = 1,
    VZ = 2
};

#ifndef __cplusplus
#  ifndef true
#     define true   1
#  endif
#  ifndef false
#     define false  0
#  endif
#endif

#define CPP_BOOL(x)     ((x) != 0)

/*
 * Basic types.
 */

/// All points in the map coordinate space should be defined using this type.
typedef double  coord_t;

typedef int     ddboolean_t;
#define boolean ddboolean_t

/// @todo move this to a separate header for logic utility macros
#define INRANGE_OF(x, y, r) ((x) >= (y) - (r) && (x) <= (y) + (r))

#endif // LIBDENG_BASIC_TYPES_H
