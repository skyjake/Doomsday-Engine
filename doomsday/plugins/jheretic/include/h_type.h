/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/*
 * Simple basic typedefs, isolated here to make it easier
 * separating modules.
 */

#ifndef __H_TYPE__
#define __H_TYPE__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
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
