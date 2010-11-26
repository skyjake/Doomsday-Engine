/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILEHASH_H
#define LIBDENG_FILEHASH_H

/**
 * File name/directory search hash.
 *
 * @ingroup fs
 */
typedef void* filehash_t;

/**
 * Initialize the file hash using the given list of paths (separated with semicolons).
 * A copy of the path list is taken allowing the hash to be easily rebuilt.
 */
filehash_t* FileHash_Create(const char* pathList);

/**
 * Empty the contents of the file hash and destroy it.
 */
void FileHash_Destroy(filehash_t* fh);

/**
 * @return                  Ptr to a copy of the path list used to generate the hash.
 */
const char* FileHash_PathList(filehash_t* fh);

/**
 * Finds a file from the hash.
 *
 * @param foundPath         The full path, returned.
 * @param name              Relative or an absolute path.
 * @param len               Size of @p foundPath in bytes.
 *
 * @return                  @c true, iff successful.
 */
boolean FileHash_Find(filehash_t* fh, char* foundPath, const char* name, size_t len);

/**
 * @return                  @c true iff the hash record set has been built, else @c false.
 */
boolean FileHash_HasRecordSet(filehash_t* fh);

#endif /* LIBDENG_FILEHASH_H */
