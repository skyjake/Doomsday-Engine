/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * Zip/Pk3 Files
 */

#ifndef LIBDENG_FILESYS_PACKAGE_ZIP_H
#define LIBDENG_FILESYS_PACKAGE_ZIP_H

#include "sys_file.h"

struct ddstring_s;

// Zip entry indices are invalidated when a new Zip file is read.
typedef uint zipindex_t;

/**
 * Initializes the zip file database.
 */
void Zip_Init(void);

/**
 * Shuts down the zip file database and frees all resources.
 */
void Zip_Shutdown(void);

/**
 * Opens the file zip, reads the directory and stores the info for later access.
 *
 * @param prevOpened  If not @c NULL, all data will be read from there.
 */
boolean Zip_Open(const char* fileName, DFILE* prevOpened);

/**
 * @return  Size of a zipentry specified by index.
 */
size_t Zip_GetSize(zipindex_t index);

/**
 * @return  "Last modified" timestamp of the zip entry.
 */
uint Zip_GetLastModified(zipindex_t index);

/**
 * Reads a zipentry into the buffer. The caller must make sure that
 * the buffer is large enough. Zip_GetSize() returns the size.
 *
 * @return  Number of bytes read.
 */
size_t Zip_Read(zipindex_t index, void* buffer);

/**
 * Find a specific path in the zipentry list. Relative paths are converted
 * to absolute ones. A binary search is used (the entries have been sorted).
 *
 * \note Good performance: O(log n)
 *
 * @return  Non-zero if something is found.
 */
zipindex_t Zip_Find(const char* searchPath);

/**
 * Iterate over nodes in the Zip making a callback for each.
 * Iteration ends when all nodes have been visited or a callback returns non-zero.
 *
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int Zip_Iterate2(int (*callback) (const struct ddstring_s*, void*), void* paramaters);
int Zip_Iterate(int (*callback) (const struct ddstring_s*, void*));

#endif /* LIBDENG_FILESYS_PACKAGE_ZIP_H */
