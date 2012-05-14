/**
 * @file dd_types.h
 * Type definitions.
 *
 * @ingroup base
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2008 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef LIBDENG_TYPES_H
#define LIBDENG_TYPES_H

#if defined(__x86_64__) || defined(__x86_64) || defined(_LP64)
#  define __64BIT__
#endif

/** In Win32 TCHAR and related macros change size depending on if they are
 using wide characters (unicode utf-16le) or ansi for functions and strings.
 on Linux and OSX we use utf-8 by default - which conveniently maps to the
 CHAR type. As a general guide, any other Win32 T style functions used,
 should also be re-implemented here in an ANSI style form for use on Linux,
 OSX, and any other UNIX style operating systems or build environments */
#ifndef WIN32
#define TCHAR CHAR
#define _T(__v__) __v__
#endif

#ifndef _MSC_VER
#include <stdint.h> // Not MSVC so use C99 standard header
#else
/* MSVC must define them ouselves.
ISO C9x Integer types - not all of them though, just what we need
If we need more the this, best of taking the header from MinGW for MSVC users */

/* 7.18.1.1  Exact-width integer types */
typedef signed char int8_t;
typedef unsigned char   uint8_t;
typedef short  int16_t;
typedef unsigned short  uint16_t;
typedef int  int32_t;
typedef unsigned   uint32_t;
typedef long long  int64_t;
typedef unsigned long long   uint64_t;

/* 7.18.1.2  Minimum-width integer types */
typedef signed char int_least8_t;
typedef unsigned char   uint_least8_t;
typedef short  int_least16_t;
typedef unsigned short  uint_least16_t;
typedef int  int_least32_t;
typedef unsigned   uint_least32_t;
typedef long long  int_least64_t;
typedef unsigned long long   uint_least64_t;

/*  7.18.1.3  Fastest minimum-width integer types
 *  Not actually guaranteed to be fastest for all purposes
 *  Here we use the exact-width types for 8 and 16-bit ints.
 */
typedef char int_fast8_t;
typedef unsigned char uint_fast8_t;
typedef short  int_fast16_t;
typedef unsigned short  uint_fast16_t;
typedef int  int_fast32_t;
typedef unsigned  int  uint_fast32_t;
typedef long long  int_fast64_t;
typedef unsigned long long   uint_fast64_t;

/* 7.18.1.5  Greatest-width integer types */
typedef long long  intmax_t;
typedef unsigned long long   uintmax_t;

#endif

// The C_DECL macro, used with functions.
#ifndef C_DECL
#  ifdef WIN32
#    define C_DECL __cdecl
#  endif
#  ifdef UNIX
#    define C_DECL
#  endif
#endif

#ifndef UNIX
typedef unsigned int    uint;
typedef unsigned short  ushort;
#else
#  include <sys/types.h>
//typedef uint32_t        uint;
//typedef uint16_t        ushort;
#endif
typedef unsigned char   byte;
typedef int32_t         fixed_t;
typedef uint32_t        angle_t;
typedef uint32_t        ident_t;
typedef int32_t         gameid_t;

typedef uint32_t        fontid_t;
typedef uint32_t        materialid_t;
typedef int             patchid_t;
typedef int32_t         spritenum_t;
typedef uint16_t        nodeindex_t;
typedef uint16_t        thid_t;
typedef double          timespan_t;

/// All points in the map coordinate space should be defined using this type.
typedef double          coord_t;

typedef enum slopetype_e {
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;

/// \todo Should be a public typedef of a type defined by de::LumpDirectory.
typedef int32_t         lumpnum_t;
#define LUMPNAME_T_MAXLEN 9
#define LUMPNAME_T_LASTINDEX 8
typedef char            lumpname_t[LUMPNAME_T_MAXLEN];

// Pointer-integer conversion (used for legacy code).
#ifdef __64BIT__
typedef int64_t         int_from_pointer_t;
#else
typedef int32_t         int_from_pointer_t;
#endif
#define PTR2INT(x)      ( (int_from_pointer_t) ((void*)(x)) )
#define INT2PTR(type,x) ( (type*) ((int_from_pointer_t)(x)) )

/// \todo dj: Refactor me away
#define FILENAME_T_MAXLEN 256
#define FILENAME_T_LASTINDEX 255
typedef char            filename_t[FILENAME_T_MAXLEN];

typedef void (*con_textfilter_t) (char* text);

/**
 * Resource Class.
 *
 * @ingroup fs
 */
typedef enum {
    RC_NULL = -2,           ///< Not a real class, used internally during resource locator init.
    RC_UNKNOWN = -1,        ///< Attempt to guess the class using heuristic evaluation of the path.
    RESOURCECLASS_FIRST = 0,
    RC_PACKAGE = RESOURCECLASS_FIRST,
    RC_DEFINITION,
    RC_GRAPHIC,
    RC_MODEL,
    RC_SOUND,
    RC_MUSIC,
    RC_FONT,
    RESOURCECLASS_COUNT
} resourceclass_t;

#define VALID_RESOURCE_CLASS(n)     ((n) >= RESOURCECLASS_FIRST && (n) < RESOURCECLASS_COUNT)

#ifndef __BYTEBOOL__
#define __BYTEBOOL__
#  ifndef __cplusplus
#    undef false
#    define false           0
#    undef true
#    define true            1
#  else
#    define CPP_BOOL(x)     ((x) != 0)
#  endif
typedef int                 ddboolean_t;
#endif
#define boolean             ddboolean_t

#define BAMS_BITS   16

#if BAMS_BITS == 32
typedef uint32_t binangle_t;
#  define BANG_TO_ANGLE(bang)     ((angle_t)bang)
#elif BAMS_BITS == 16
typedef uint16_t binangle_t;
#  define BANG_TO_ANGLE(bang)     ((angle_t)bang << 16)
#else
typedef unsigned char binangle_t;
#  define BANG_TO_ANGLE(bang)     ((angle_t)bang << 24)
#endif

#define ANGLE_TO_BANG(angle)      ((binangle_t)((angle_t)angle >> BAMS_BITS))

#define DDMAXCHAR   ((char)0x7f)
#define DDMAXSHORT  ((int16_t)0x7fff)
#define DDMAXUSHORT ((uint16_t)0xffff)
#define DDMAXINT    ((int32_t)0x7fffffff)   ///< Max positive 32-bit int
#define DDMAXLONG   ((int32_t)0x7fffffff)
#define DDMAXFLOAT  ((float)1E+37)

#define DDMINCHAR   ((char)0x80)
#define DDMINSHORT  ((int16_t)0x8000)
#define DDMININT    ((int32_t)0x80000000)   ///< Min negative 32-bit integer
#define DDMINLONG   ((int32_t)0x80000000)
#define DDMINFLOAT  ((float)-(1E+37))

#ifdef _DEBUG
#  define DENG_DEBUG_ONLY(x)    x
#else
#  define DENG_DEBUG_ONLY(x)
#endif

// Forward declarations for map data types. The contents of these structs is
// declared in p_maptypes.h.
struct bspnode_s;
struct vertex_s;
struct linedef_s;
struct side_s;
struct hedge_s;
struct bspleaf_s;
struct sector_s;
struct polyblock_s;
struct polyobj_s;
struct plane_s;
struct surface_s;
struct material_s;

#include "dd_string.h"
#include "uri.h"

#endif /* LIBDENG_TYPES_H */
