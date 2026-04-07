/** @file liblegacy.h  Common definitions for legacy support.
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBLEGACY_H
#define LIBLEGACY_H

/**
 * @defgroup legacy Legacy Support
 *
 * Common functionality for supporting old, mostly C based code tracing back to
 * the original id Software and Raven Software code bases.
 */

/**
 * @defgroup legacyMath Legacy Math Utilities
 * Math utilities for C based code. @ingroup legacy
 */

/**
 * @defgroup legacyData Legacy Data Types
 * Data types and structures for C based code. @ingroup legacy
 */

/**
 * @defgroup legacyFlags Flags
 * @ingroup legacy
 */

#if defined(__x86_64__) || defined(__x86_64) || defined(_LP64)
#  ifndef __64BIT__
#    define __64BIT__
#  endif
#endif

#include <assert.h>
#include <stddef.h>

#include <de/libcore.h>

#ifdef __cplusplus
#  define DE_EXTERN_C extern "C"
#else
#  define DE_EXTERN_C extern
#endif

#if defined (DE_STATIC_LINK)
#  define DE_ENTRYPOINT static
#else
#  ifdef _MSC_VER
#    define DE_ENTRYPOINT DE_EXTERN_C __declspec(dllexport)
#  else
#    define DE_ENTRYPOINT DE_EXTERN_C DE_PUBLIC
#  endif
#endif

#ifndef NDEBUG
#  if !defined (_DEBUG)
#    define _DEBUG 1
#  endif
#  define DE_DEBUG
#  if !defined (DE_ASSERT)
#    ifdef DE_USE_QT
#      define DE_ASSERT(x) Q_ASSERT(x)
#    else
#      define DE_ASSERT(x) assert(x)
#    endif
#  endif
#  define DE_DEBUG_ONLY(x) x
#else
#  define DE_NO_DEBUG
#  if !defined (DE_ASSERT)
#    define DE_ASSERT(x)
#    define DE_DEBUG_ONLY(x)
#  endif
#endif

#if !defined (DE_ASSERT_FAIL)
#  define DE_ASSERT_FAIL(msgCStr)  DE_ASSERT(msgCStr == NULL)
#endif

/**
 * Macro for hiding the warning about an unused parameter.
 */
#if !defined (DE_UNUSED)
#  define DE_UNUSED(x)      (void)x
#endif

/*
 * Utility macros.
 */

#define DD_PI           3.14159265359f
#define DD_PI_D         3.14159265358979323846
#define DEG2RAD(a)      (((a) * DD_PI_D) / 180.0)
#define RAD2DEG(a)      (((a) / DD_PI_D) * 180.0)
#define FLOATEPSILON    .000001f

/**
 * Converts a numerical value to a C++ bool type. Zero is the only accepted
 * 'false' value, any other value is considered 'true'.
 *
 * @param x  Any value that can be compared against zero.
 *
 * @return  @c true or @c false (C++ bool type).
 */
#define CPP_BOOL(x)         ((x) != 0)

#define INRANGE_OF(x, y, r) ((x) >= (y) - (r) && (x) <= (y) + (r))

#define MAX_OF(x, y)        ((x) > (y)? (x) : (y))

#define MIN_OF(x, y)        ((x) < (y)? (x) : (y))

#define MINMAX_OF(a, x, b)  ((x) < (a)? (a) : (x) > (b)? (b) : (x))

#define SIGN_OF(x)          ((x) > 0? +1 : (x) < 0? -1 : 0)

#define INRANGE_OF(x, y, r) ((x) >= (y) - (r) && (x) <= (y) + (r))

#define FEQUAL(x, y)        (INRANGE_OF(x, y, FLOATEPSILON))

#define IS_ZERO(x)          FEQUAL(x, 0)

#define NON_ZERO(x)         (!IS_ZERO(x))

#define ROUND(x)            ((int) (((x) < 0.0f)? ((x) - 0.5f) : ((x) + 0.5f)))

#ifdef ABS
#  undef ABS
#endif
#define ABS(x)              ((x) >= 0 ? (x) : -(x))

/// Ceiling of integer quotient of @a a divided by @a b.
#define CEILING(a, b)       ((a) % (b) == 0 ? (a)/(b) : (a)/(b)+1)

#define DE_ISSPACE(c)     ((c) == 0 || (c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

// Automatically define the basic types for convenience.
#include "legacy/types.h"

/*
 * Main interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the library. This must be the first function called before any other
 * functions in the library.
 */
DE_PUBLIC void Libdeng_Init(void);

/**
 * Shuts down the library. Frees any internal resources allocated by the library's
 * subsystems. Must be called when the library is no longer needed.
 */
DE_PUBLIC void Libdeng_Shutdown(void);

/**
 * Terminates the process immediately. Call this when a malloc fails to handle
 * terminating gracefully instead of crashing with null pointer access.
 */
DE_PUBLIC DE_NORETURN void Libdeng_BadAlloc(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBLEGACY_H
