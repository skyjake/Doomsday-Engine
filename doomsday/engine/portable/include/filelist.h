/**\file filelist.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_FILELIST_H
#define LIBDENG_FILESYS_FILELIST_H

#include "dfile.h"

#ifdef __cplusplus
extern "C" {
#endif

struct abstractfile_s;
struct ddstring_s;

/**
 * FileList.  File system object designed to simplify the management of
 * a collection of files, treating the whole as a unit.
 *
 * The file collection is modelled as a managed, random access container
 * for file references. A single file may be referenced any number of times
 * within a collection while the references themselves are unique to the
 * collection which owns the reference.
 *
 * Ownership of files referenced within FileList are not considered owned
 * by either their reference(s) or the list itself.
 *
 * @ingroup fs
 */
struct filelist_s; // The file list instance (opaque).
typedef struct filelist_s FileList;

/// Construct a new (empty) FileList.
FileList* FileList_New(void);

/// Construct a new FileList, populating it with references @a files.
FileList* FileList_NewWithFiles(DFile** files, int count);

/**
 * Perform a deep-copy of the list, returning a fully cloned object.
 * New handles are constructed to the referenced files while the files themselves
 * can thusly be considered "shared" by the new list.
 */
FileList* FileList_NewCopy(FileList* fl);

/// Completely destroy the list, releasing all memory acquired for this instance.
void FileList_Delete(FileList* fl);

/// Remove all files from the list returning it to an empty, initial state.
void FileList_Clear(FileList* fl);

/// @return  Number of files present in the list.
int FileList_Size(FileList* fl);

/// @return  @c true iff there are no files present in the list.
boolean FileList_Empty(FileList* fl);

/// @return  @c NULL if empty else the reference at position @a idx.
DFile* FileList_Get(FileList* fl, int idx);

/// @return  @c NULL if empty else the reference at the front.
DFile* FileList_Front(FileList* fl);

/// @return  @c NULL if empty else the reference at the back.
DFile* FileList_Back(FileList* fl);

/// @return  @c NULL if empty else the file referenced at position @a idx.
struct abstractfile_s* FileList_GetFile(FileList* fl, int idx);

/// @return  @c NULL if empty else the file referenced at the front.
struct abstractfile_s* FileList_FrontFile(FileList* fl);

/// @return  @c NULL if empty else the file referenced at the back.
struct abstractfile_s* FileList_BackFile(FileList* fl);

/**
 * Push a new file reference onto the front.
 *
 * @param file  File reference being inserted into the list.
 * @return  Equal to @a file for convenience (chaining).
 */
DFile* FileList_AddFront(FileList* fl, DFile* file);

/**
 * Push a new file reference onto the end.
 *
 * @param file  File reference being inserted into the list.
 * @return  Equal to @a file for convenience (chaining).
 */
DFile* FileList_AddBack(FileList* fl, DFile* file);

/**
 * Remove the file reference at the front.
 * @return  Ptr to file referenced by the removed node if successful else @c NULL.
 */
DFile* FileList_RemoveFront(FileList* fl);

/**
 * Remove the file reference at the back.
 * @return  Ptr to file referenced by the removed node if successful else @c NULL.
 */
DFile* FileList_RemoveBack(FileList* fl);

/**
 * Remove file reference at position @a idx.
 *
 * @param idx  Index of the reference to be removed. Negative indexes allowed.
 * @return  Ptr to file referenced by the removed node if successful else @c NULL.
 */
DFile* FileList_RemoveAt(FileList* fl, int idx);

/**
 * @param size  If not @c NULL the number of elements in the resultant
 *      array is written back here (for convenience).
 * @return  Array of ptrs to files in this list or @c NULL if empty.
 *      Ownership of the array passes to the caller who should ensure to
 *      release it with free() when no longer needed.
 */
AbstractFile** FileList_ToArray(FileList* fl, int* size);

/**
 * @defgroup pathToStringFlags  Path To String Flags.
 * @{
 */
#define PTSF_QUOTED         0x1 /// Add double quotes around the path.
#define PTSF_TRANSFORM_EXCLUDE_DIR 0x2 /// Exclude the directory; e.g., c:/doom/myaddon.wad -> myaddon.wad
#define PTSF_TRANSFORM_EXCLUDE_EXT 0x4 /// Exclude the extension; e.g., c:/doom/myaddon.wad -> c:/doom/myaddon
/**@}*/

#define DEFAULT_PATHTOSTRINGFLAGS (PTSF_QUOTED)

/**
 * @param flags  @see pathToStringFlags
 * @param delimiter  If not @c NULL, path fragments in the resultant string
 *      will be delimited by this.
 * @param predicate  If not @c NULL, this predicate evaluator callback must
 *      return @c true for a given path to be included in the resultant string.
 * @param paramaters  Passed to the predicate evaluator callback.
 * @return  New string containing a concatenated, possibly delimited set
 *      of all file paths in the list. Ownership of the string passes to
 *      the caller who should ensure to release it with Str_Delete() when
 *      no longer needed. Always returns a valid (but perhaps zero-length)
 *      string object.
 */
struct ddstring_s* FileList_ToString4(FileList* fl, int flags, const char* delimiter, boolean (*predicate)(DFile* file, void* paramaters), void* paramaters);
struct ddstring_s* FileList_ToString3(FileList* fl, int flags, const char* delimiter); /* predicate = NULL, paramaters = NULL */
struct ddstring_s* FileList_ToString2(FileList* fl, int flags); /* delimiter = " " */
struct ddstring_s* FileList_ToString(FileList* fl); /* flags = DEFAULT_PATHTOSTRINGFLAGS */

#if _DEBUG
void FileList_Print(FileList* fl);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILESYS_FILELIST_H */
