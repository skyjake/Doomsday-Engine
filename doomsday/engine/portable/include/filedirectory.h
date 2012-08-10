/**\file filedirectory.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILEDIRECTORY_H
#define LIBDENG_FILEDIRECTORY_H

#include "uri.h"
#include "pathdirectory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback function type for FileDirectory::Iterate
 *
 * @param node  PathDirectoryNode being processed.
 * @param parameters  User data passed to this.
 * @return  Non-zero if iteration should stop.
 */
typedef pathdirectory_iteratecallback_t filedirectory_iteratecallback_t;

/// Const variant.
typedef pathdirectory_iterateconstcallback_t filedirectory_iterateconstcallback_t;

/**
 * FileDirectory. Core system component representing a hierarchical
 * file path structure.
 *
 * A specialization of de::PathDirectory which implements automatic
 * population of the directory itself from the virtual file system.
 * Also, paths are resolved prior to pushing them into the directory.
 *
 * @todo Perhaps this should be derived from PathDirectory instead of
 * encapsulating an instance of it?
 *
 * @ingroup fs
 */
struct filedirectory_s; // The filedirectory instance (opaque).
typedef struct filedirectory_s FileDirectory;

FileDirectory* FileDirectory_New(const char* basePath);

/**
 * Construct a new FileDirectory instance, populating it with nodes
 * for the given search paths.
 *
 * @param basePath  Used with directories which represent the internal paths
 *                  natively as relative paths. @c NULL means only absolute
 *                  paths will be added to the directory (attempts to add
 *                  relative paths will fail silently).
 * @param searchPathList  List of search paths. @c NULL terminated.
 * @param flags  @see searchPathFlags
 */
FileDirectory* FileDirectory_NewWithPathListStr(const char* basePath, const ddstring_t* searchPathList, int flags);
FileDirectory* FileDirectory_NewWithPathList(const char* basePath, const char* searchPathList, int flags);

void FileDirectory_Delete(FileDirectory* fd);

/**
 * Clear the directory contents.
 */
void FileDirectory_Clear(FileDirectory* fd);

/**
 * Resolve and collate all paths in the directory into a list.
 *
 * @param fd    FileDirectory instance.
 * @param type  If a valid type, only paths of this type will be visited.
 * @param count  Number of visited paths is written back here.
 *
 * @return  Ptr to the allocated list; it is the responsibility of the caller to
 *      Str_Free each string in the list and Z_Free the list itself.
 */
ddstring_t* FileDirectory_AllPaths(FileDirectory* fd, pathdirectorynode_type_t type, size_t* count);

/**
 * Add a new set of paths. Duplicates are automatically pruned.
 *
 * @param fd    FileDirectory instance.
 * @param flags  @see searchPathFlags
 * @param paths  One or more paths.
 * @param pathsCount  Number of elements in @a paths.
 * @param callback  Callback function ptr.
 * @param parameters  Passed to the callback.
 */
void FileDirectory_AddPaths3(FileDirectory* fd, int flags, const Uri* const* paths, uint pathsCount,
    int (*callback) (PathDirectoryNode* node, void* parameters), void* parameters);
void FileDirectory_AddPaths2(FileDirectory* fd, int flags, const Uri* const* paths, uint pathsCount,
    int (*callback) (PathDirectoryNode* node, void* parameters)); /*parameters=NULL*/
void FileDirectory_AddPaths(FileDirectory* fd,  int flags, const Uri* const* paths, uint pathsCount); /*callback=NULL*/

/**
 * Add a new set of paths from a path list. Duplicates are automatically pruned.
 *
 * @param fd    FileDirectory instance.
 * @param flags  @see searchPathFlags
 * @param pathList  One or more paths separated by semicolons.
 * @param callback  Callback function ptr.
 * @param parameters  Passed to the callback.
 */
void FileDirectory_AddPathList3(FileDirectory* fd, int flags, const char* pathList,
    int (*callback) (PathDirectoryNode* node, void* parameters), void* parameters);
void FileDirectory_AddPathList2(FileDirectory* fd, int flags, const char* pathList,
    int (*callback) (PathDirectoryNode* node, void* parameters)); /*parameters=NULL*/
void FileDirectory_AddPathList(FileDirectory* fd, int flags, const char* pathList); /*callback=NULL*/

/**
 * Find a path in the directory.
 *
 * @param fd    FileDirectory instance.
 * @param type  If a valid path type only consider nodes of this type.
 * @param searchPath  Relative or absolute path.
 * @param searchDelimiter  Fragments of @a searchPath are delimited by this character.
 * @param foundPath  If not @c NULL, the full path of the node is written back here if found.
 * @param foundDelimiter  Delimiter to be used when composing @a foundPath.
 *
 * @return  @c true, iff successful.
 */
boolean FileDirectory_Find(FileDirectory* fd, pathdirectorynode_type_t type,
    const char* searchPath, char searchDelimiter, ddstring_t* foundPath, char foundDelimiter);

/**
 * Iterate over nodes in the directory making a callback for each.
 * Iteration ends when all nodes have been visited or a callback returns non-zero.
 *
 * @param fd    FileDirectory instance.
 * @param type  If a valid path type only process nodes of this type.
 * @param parent  If not @c NULL, only process child nodes of this node.
 * @param callback  Callback function ptr.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int FileDirectory_Iterate2(FileDirectory* fd, pathdirectorynode_type_t type, PathDirectoryNode* parent, ushort hash,
    filedirectory_iteratecallback_t callback, void* parameters);
int FileDirectory_Iterate(FileDirectory* fd, pathdirectorynode_type_t type, PathDirectoryNode* parent, ushort hash,
    filedirectory_iteratecallback_t callback); /*parameters=NULL*/

int FileDirectory_Iterate2_Const(const FileDirectory* fd, pathdirectorynode_type_t type, const PathDirectoryNode* parent, ushort hash,
    filedirectory_iterateconstcallback_t callback, void* parameters);
int FileDirectory_Iterate_Const(const FileDirectory* fd, pathdirectorynode_type_t type, const PathDirectoryNode* parent, ushort hash,
    filedirectory_iterateconstcallback_t callback); /*parameters=NULL*/

#if _DEBUG
void FileDirectory_Print(FileDirectory* fd);
void FileDirectory_PrintHashDistribution(FileDirectory* fd);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_FILEDIRECTORY_H */
