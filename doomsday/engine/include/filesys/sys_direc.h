/**\file sys_direc.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * Directory Utilities.
 */

#ifndef LIBDENG_DIREC_H
#define LIBDENG_DIREC_H

#include "dd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct directory_s {
#if defined(WIN32)
    int drive;
#endif
    filename_t path;
} directory_t;

/// Construct using the specified path.
directory_t* Dir_New(const char* path);

/// Construct using the current working directory path.
directory_t* Dir_NewFromCWD(void);

/**
 * Construct by extracting the path from @a path.
 * \note if not absolute then it will be interpeted as relative to the current
 * working directory path.
 */
directory_t* Dir_FromText(const char* path);

void Dir_Delete(directory_t* dir);

/**
 * @return  @c true if the directories @a a and @a b are considered equal
 *      (i.e., their paths match exactly).
 */
boolean Dir_IsEqual(directory_t* dir, directory_t* other);

/**
 * @return  "Raw" version of the present path.
 */
const char* Dir_Path(directory_t* dir);

/**
 * Change the path to that specified in @a path.
 * \note Path directives (such as '}' and '~' on Unix) are automatically expanded.
 */
void Dir_SetPath(directory_t* dir, const char* path);

/// Class-Static Members:

/**
 * Clean up given path. Whitespace is trimmed. Path separators are converted
 * into their system-specific form. On Unix '~' expansion is applied.
 */
void Dir_CleanPath(char* path, size_t len);
void Dir_CleanPathStr(ddstring_t* str);

/**
 * @return  Absolute path to the current working directory for the default drive.
 *      Always ends with a '/'. Path must be released with free()
 *      @c NULL if we are out of memory.
 */
char* Dir_CurrentPath(void);

/**
 * Extract just the file name including any extension from @a path.
 */
void Dir_FileName(char* name, const char* path, size_t len);

/**
 * Convert directory separators in @a path to their system-specifc form.
 */
void Dir_ToNativeSeparators(char* path, size_t len);

/**
 * Convert directory separators in @a path to our internal '/' form.
 */
void Dir_FixSeparators(char* path, size_t len);

/// @return  @c true if @a path is absolute.
int Dir_IsAbsolutePath(const char* path);

/**
 * Convert a path into an absolute path. If @a path is relative it is considered
 * relative to the current working directory. On Unix '~' expansion is applied.
 *
 * @param path  Path to be translated.
 * @param len  Length of the buffer used for @a path (in bytes).
 */
void Dir_MakeAbsolutePath(char* path, size_t len);

/**
 * Check that the given directory exists. If it doesn't, create it.
 * @return  @c true if successful.
 */
boolean Dir_mkpath(const char* path);

/**
 * Attempt to change the current working directory to the path defined.
 * @return  @c true if the change was successful.
 */
boolean Dir_SetCurrent(const char* path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_DIREC_H */
