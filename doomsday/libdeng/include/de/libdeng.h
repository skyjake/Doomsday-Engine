/**
 * @file libdeng.h
 * Common definitions for libdeng.
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

#ifndef LIBDENG_H
#define LIBDENG_H

#if defined(__cplusplus) && !defined(DENG_NO_QT)
#  define DENG_USE_QT
#endif

#if defined(__x86_64__) || defined(__x86_64) || defined(_LP64)
#  define __64BIT__
#endif

#ifdef DENG2_USE_QT
#  include <QtCore/qglobal.h>
#endif

#include <assert.h>

/*
 * The DENG_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols out of libdeng2.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __DENG__
// This is defined when compiling the library.
#    define DENG_PUBLIC __declspec(dllexport)
#  else
#    define DENG_PUBLIC __declspec(dllimport)
#  endif
#else
// No need to use any special declarators.
#  define DENG_PUBLIC
#endif

#ifndef NDEBUG
#  define DENG_DEBUG
#  ifdef DENG_USE_QT
#    define DENG_ASSERT(x) Q_ASSERT(x)
#  else
#    define DENG_ASSERT(x) assert(x)
#  endif
#  define DENG_DEBUG_ONLY(x) x
#else
#  define DENG_NO_DEBUG
#  define DENG_ASSERT(x)
#  define DENG_DEBUG_ONLY(x)
#endif

/*
 * Utility macros.
 */

#define CPP_BOOL(x)         ((x) != 0)

#define INRANGE_OF(x, y, r) ((x) >= (y) - (r) && (x) <= (y) + (r))

#define MAX_OF(x, y)        ((x) > (y)? (x) : (y))

#define MIN_OF(x, y)        ((x) < (y)? (x) : (y))

#define MINMAX_OF(a, x, b)  ((x) < (a)? (a) : (x) > (b)? (b) : (x))

#define SIGN_OF(x)          ((x) > 0? +1 : (x) < 0? -1 : 0)

#define INRANGE_OF(x, y, r) ((x) >= (y) - (r) && (x) <= (y) + (r))

#define FEQUAL(x, y)        (INRANGE_OF(x, y, FLOATEPSILON))

#define ROUND(x)            ((int) (((x) < 0.0f)? ((x) - 0.5f) : ((x) + 0.5f)))

#define ABS(x)              ((x) >= 0 ? (x) : -(x))

/// Ceiling of integer quotient of @a a divided by @a b.
#define CEILING(a, b)       ((a) % (b) == 0 ? (a)/(b) : (a)/(b)+1)

// Automatically define the basic types for convenience.
#include <de/types.h>

/*
 * Main interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the libdeng1 library. This must be the first function called
 * before any other libdeng1 functions.
 */
DENG_PUBLIC void Libdeng_Init(void);

/**
 * Shuts down the libdeng1 library. Frees any internal resources allocated by
 * the library's subsystems. Must be called when the library is no longer
 * needed.
 */
DENG_PUBLIC void Libdeng_Shutdown(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_H
