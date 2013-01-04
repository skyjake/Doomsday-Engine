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

#ifndef ENGINE_TYPES_H
#define ENGINE_TYPES_H

#include <de/types.h> // libdeng types

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

// The C_DECL macro, used with functions.
#ifndef C_DECL
#  ifdef WIN32
#    define C_DECL __cdecl
#  endif
#  ifdef UNIX
#    define C_DECL
#  endif
#endif

/// @todo Should be a public typedef of a type defined by de::LumpIndex.
typedef int32_t         lumpnum_t;
#define LUMPNAME_T_MAXLEN 9
#define LUMPNAME_T_LASTINDEX 8
typedef char            lumpname_t[LUMPNAME_T_MAXLEN];

/// \todo dj: Refactor me away
#define FILENAME_T_MAXLEN 256
#define FILENAME_T_LASTINDEX 255
typedef char            filename_t[FILENAME_T_MAXLEN];

typedef void (*con_textfilter_t) (char* text);

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

#include <de/str.h>
#include <de/fixedpoint.h>
#include "resourceclass.h"
#include "filetype.h"
#include "api_uri.h"

#endif /* ENGINE_TYPES_H */
