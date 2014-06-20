/** @file sys_direc.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Directory Utilities.
 */

#ifndef LIBDENG_DIREC_H
#define LIBDENG_DIREC_H

#include "../libdoomsday.h"
#include "dd_types.h"

#ifdef WIN32
#  define DENG_DIR_SEP_CHAR       '\\'
#  define DENG_DIR_SEP_STR        "\\"
#  define DENG_DIR_WRONG_SEP_CHAR '/'
#else
#  define DENG_DIR_SEP_CHAR        '/'
#  define DENG_DIR_SEP_STR         "/"
#  define DENG_DIR_WRONG_SEP_CHAR  '\\'
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define FILENAME_T_MAXLEN 256
#define FILENAME_T_LASTINDEX 255
typedef char filename_t[FILENAME_T_MAXLEN];

typedef struct directory_s {
#if defined(WIN32)
    int drive;
#endif
    filename_t path;
} directory_t;

/// Construct using the specified path.
LIBDOOMSDAY_PUBLIC directory_t* Dir_New(const char* path);

/// Construct using the current working directory path.
LIBDOOMSDAY_PUBLIC directory_t* Dir_NewFromCWD(void);

/**
 * Construct by extracting the path from @a path.
 * \note if not absolute then it will be interpeted as relative to the current
 * working directory path.
 */
LIBDOOMSDAY_PUBLIC directory_t* Dir_FromText(const char* path);

LIBDOOMSDAY_PUBLIC void Dir_Delete(directory_t* dir);

/**
 * @return  "Raw" version of the present path.
 */
LIBDOOMSDAY_PUBLIC const char* Dir_Path(directory_t* dir);

/// Class-Static Members:

/**
 * Clean up given path. Whitespace is trimmed. Path separators are converted
 * into their system-specific form. On Unix '~' expansion is applied.
 */
LIBDOOMSDAY_PUBLIC void Dir_CleanPath(char* path, size_t len);
LIBDOOMSDAY_PUBLIC void Dir_CleanPathStr(ddstring_t* str);

/**
 * @return  Absolute path to the current working directory for the default drive.
 *      Always ends with a '/'. Path must be released with M_Free()
 *      @c NULL if we are out of memory.
 */
LIBDOOMSDAY_PUBLIC char* Dir_CurrentPath(void);

/**
 * Convert a path into an absolute path. If @a path is relative it is considered
 * relative to the current working directory. On Unix '~' expansion is applied.
 *
 * @param path  Path to be translated.
 * @param len  Length of the buffer used for @a path (in bytes).
 */
LIBDOOMSDAY_PUBLIC void Dir_MakeAbsolutePath(char* path, size_t len);

/**
 * Check that the given directory exists. If it doesn't, create it.
 * @return  @c true if successful.
 */
LIBDOOMSDAY_PUBLIC dd_bool Dir_mkpath(const char* path);

/**
 * Attempt to change the current working directory to the path defined.
 * @return  @c true if the change was successful.
 */
LIBDOOMSDAY_PUBLIC dd_bool Dir_SetCurrent(const char* path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DIREC_H */
