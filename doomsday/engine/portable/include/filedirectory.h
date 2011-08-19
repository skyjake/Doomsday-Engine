/**\file filedirectory.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2011 Daniel Swanson <danij@dengine.net>
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

#include "dd_string.h"
#include "dd_uri.h"

#include "pathdirectory.h"

// Default path delimiter is the platform-specific directory separator.
#define FILEDIRECTORY_DELIMITER         DIR_SEP_CHAR

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
typedef struct filedirectory_s {
    /// Path hash table.
    pathdirectory_t* _pathDirectory;
} filedirectory_t;

filedirectory_t* FileDirectory_New(void);
filedirectory_t* FileDirectory_NewWithPathListStr(const ddstring_t* pathList);
filedirectory_t* FileDirectory_NewWithPathList(const char* pathList);

void FileDirectory_Delete(filedirectory_t* fd);

/**
 * Clear the directory contents.
 */
void FileDirectory_Clear(filedirectory_t* fd);

/**
 * Resolve and collate all paths in the directory into a list.
 *
 * @param type  If a valid type, only paths of this type will be visited.
 * @param count  Number of visited paths is written back here.
 *
 * @return  Ptr to the allocated list; it is the responsibility of the caller to
 *      Str_Free each string in the list and Z_Free the list itself.
 */
ddstring_t* FileDirectory_AllPaths(filedirectory_t* fd, pathdirectory_nodetype_t type,
    size_t* count);

/**
 * Add a new set of paths. Duplicates are automatically pruned.
 *
 * @param paths  One or more paths.
 * @param pathsCount  Number of elements in @a paths.
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 */
void FileDirectory_AddPaths3(filedirectory_t* fd, const Uri* const* paths, uint pathsCount,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters), void* paramaters);
void FileDirectory_AddPaths2(filedirectory_t* fd, const Uri* const* paths, uint pathsCount,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters));
void FileDirectory_AddPaths(filedirectory_t* fd, const Uri* const* paths, uint pathsCount);

/**
 * Add a new set of paths from a path list. Duplicates are automatically pruned.
 *
 * @param pathList  One or more paths separated by semicolons.
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 */
void FileDirectory_AddPathList3(filedirectory_t* fd, const char* pathList,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters), void* paramaters);
void FileDirectory_AddPathList2(filedirectory_t* fd, const char* pathList,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters));
void FileDirectory_AddPathList(filedirectory_t* fd, const char* pathList);

/**
 * Find a path in the directory.
 *
 * @param type  If a valid path type only consider nodes of this type.
 * @param searchPath  Relative or absolute path.
 * @param foundPath  If not @c NULL, the full path of the node is written back here if found.
 *
 * @return  @c true, iff successful.
 */
boolean FileDirectory_Find(filedirectory_t* fd, pathdirectory_nodetype_t type,
    const char* searchPath, ddstring_t* foundPath);

/**
 * Iterate over nodes in the directory making a callback for each.
 * Iteration ends when all nodes have been visited or a callback returns non-zero.
 *
 * @param type  If a valid path type only process nodes of this type.
 * @param parent  If not @c NULL, only process child nodes of this node.
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int FileDirectory_Iterate2(filedirectory_t* fd, pathdirectory_nodetype_t type,
    struct pathdirectory_node_s* parent, ushort hash,
    int (*callback) (struct pathdirectory_node_s* node, void* paramaters),
    void* paramaters);
int FileDirectory_Iterate(filedirectory_t* fd, pathdirectory_nodetype_t type,
    struct pathdirectory_node_s* parent, ushort hash,
    int (*callback) (struct pathdirectory_node_s* node, void* paramaters));

int FileDirectory_Iterate2_Const(const filedirectory_t* fd, pathdirectory_nodetype_t type,
    const struct pathdirectory_node_s* parent, ushort hash,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters),
    void* paramaters);
int FileDirectory_Iterate_Const(const filedirectory_t* fd, pathdirectory_nodetype_t type,
    const struct pathdirectory_node_s* parent, ushort hash,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters));

void FileDirectory_Print(filedirectory_t* fd);
void FileDirectory_PrintHashDistribution(filedirectory_t* fd);

#endif /* LIBDENG_FILEDIRECTORY_H */
