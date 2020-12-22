/** @file de/findfile.h Win32-style native file finding.
 *
 * @author Copyright &copy; 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_FILESYS_FILEFINDER_H
#define DE_FILESYS_FILEFINDER_H

#include "types.h"
#include "str.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @addtogroup legacy
/// @{

// File attributes.
#define A_SUBDIR                0x1
#define A_RDONLY                0x2
#define A_HIDDEN                0x4
#define A_ARCH                  0x8

typedef struct finddata_s {
    void *finddata;
#if defined(UNIX)
    long date;
    long time;
    long size;
#else
    time_t date;
    time_t time;
    size_t size;
#endif
    ddstring_t name; // UTF-8 encoded
    long attrib;
} FindData;

/**
 * Initializes the file finder and locates the first matching file.
 * Directory names end in a directory separator character.
 *
 * @param findData  File finder.
 * @param pattern   File path pattern to find.
 *
 * @return  @c 0, if successful. If non-zero is returned, there were no
 * matching files.
 */
DE_PUBLIC int FindFile_FindFirst(FindData *findData, const char *pattern);

/**
 * Finds the next matching file. Directory names end in a directory
 * separator character.
 *
 * @param findData  File finder.
 *
 * @return  @c 0, if successful. If non-zero is returned, there were no
 * matching files.
 */
DE_PUBLIC int FindFile_FindNext(FindData *findData);

/**
 * This must be called after the file finding operation has been concluded.
 * Releases memory allocated for file finding.
 *
 * @param findData  File finder to release.
 */
DE_PUBLIC void FindFile_Finish(FindData *findData);

#ifdef UNIX

/**
 * Convert the given path to an absolute path. This behaves like the Win32 @c
 * _fullpath function. Do not use this in newly written code; it is intened for
 * supporting legacy code.
 *
 * @param full      Buffer where to write the full/absolute path.
 * @param original  Path to convert.
 * @param len       Length of the @a full buffer.
 */
DE_PUBLIC char* _fullpath(char* full, const char* original, int len);

/**
 * Split a path into components. This behaves like the Win32 @c _splitpath
 * function. Do not use this in newly written code; it is intended for
 * supporting legacy code.
 *
 * @param path   Path to split.
 * @param drive  Drive letter.
 * @param dir    Path.
 * @param name   File name.
 * @param ext    File extension.
 */
DE_PUBLIC void _splitpath(const char* path, char* drive, char* dir, char* name, char* ext);

#endif // UNIX

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DE_FILESYS_FILEFINDER_H */
