/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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
 * de_platform.h: Platform Independence
 *
 * Define either WIN32 or UNIX when compiling.
 *
 * Use this header file in source files which can be compiled on any
 * platform but still use some platform specific code.
 */

#ifndef __DOOMSDAY_PLATFORM__
#define __DOOMSDAY_PLATFORM__

#include "dd_types.h"

/*
 * The Win32 Platform
 */
#if defined(WIN32)

#if __cplusplus
#  include <QIODevice> // must be included before anything that defines open
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

#define INTEGER64 __int64

#define DIR_SEP_CHAR    '\\'
#define DIR_SEP_STR     "\\"
#define DIR_WRONG_SEP_CHAR  '/'

#define stricmp _stricmp
#define strnicmp _strnicmp
#define open _open
#define close _close
//#define read _read
//#define write _write
#define access _access
#define mkdir _mkdir
#define strlwr _strlwr
#define strupr _strupr
#define strdup _strdup
#define spawnlp _spawnlp

#ifdef __cplusplus
extern "C" {
#endif

const char* strcasestr(const char* text, const char* sub);

#ifdef __cplusplus
} extern "C"
#endif

#endif                          // WIN32

/*
 * The Unix Platform
 */
#if defined(UNIX)
#include <string.h>
#include <errno.h>

#ifdef __GNUC__
#  define GCC_VERSION (__GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__)
#endif

typedef long long int INTEGER64;
typedef unsigned int DWORD;

/*
 * Networking.
 */
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)

/*
 * File system routines.
 */
#define _getdrive()     (0)
#define _chdrive(x)
#define _getcwd         getcwd
#define _chdir          chdir

#define DIR_SEP_CHAR        '/'
#define DIR_SEP_STR         "/"
#define DIR_WRONG_SEP_CHAR  '\\'

#include "unix/sys_path.h"

#endif                          // UNIX

// Initialization code.
#ifdef WIN32
#  include "dd_winit.h"
#else
#  ifdef UNIX
#    include "dd_uinit.h"
#  endif
#endif

#endif                          // __DOOMSDAY_PLATFORM__
