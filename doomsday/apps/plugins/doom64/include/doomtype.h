/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * doomtype_.h: Simple basic typedefs, isolated here to make it easier
 * separating modules.
 */

#ifndef __DOOMTYPE_H__
#define __DOOMTYPE_H__

#ifndef __JDOOM64__
#  error "Using jDoom64 headers without __JDOOM64__"
#endif

// Predefined with some OS.
#ifdef UNIX

#include <limits.h>

#define MAXCHAR     SCHAR_MAX
#define MAXSHORT    SHRT_MAX
#define MAXINT      INT_MAX
#define MAXLONG     LONG_MAX

#define MINCHAR     SCHAR_MIN
#define MINSHORT    SHRT_MIN
#define MININT      INT_MIN
#define MINLONG     LONG_MIN

#else /* not UNIX */

#define MAXCHAR     ((char)0x7f)
#define MAXSHORT    ((short)0x7fff)

// Max pos 32-bit int.
#define MAXINT      ((int)0x7fffffff)
#define MAXLONG     ((long)0x7fffffff)
#define MINCHAR     ((char)0x80)
#define MINSHORT    ((short)0x8000)

// Max negative 32-bit integer.
#define MININT      ((int)0x80000000)
#define MINLONG     ((long)0x80000000)
#endif

#endif
