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

struct filedirectory_node_s;

typedef enum {
    FDT_ANY = -1,
    FILEDIRECTORY_PATHTYPES_FIRST = 0,
    FDT_DIRECTORY = FILEDIRECTORY_PATHTYPES_FIRST,
    FDT_FILE,
    FILEDIRECTORY_PATHTYPES_COUNT
} filedirectory_pathtype_t;

#define VALID_FILEDIRECTORY_PATHTYPE(t)     ((t) >= FILEDIRECTORY_PATHTYPES_FIRST || (t) < FILEDIRECTORY_PATHTYPES_COUNT)

/**
 * FileDirectory. Core system component representing a hierarchical file path structure.
 *
 * A specialization of de::PathDirectory which implements automatic population of the
 * directory itself from the virtual file system. Also, paths are resolved prior to pushing
 * them into the directory.
 *
 * @todo Perhaps this should be derived from PathDirectory?
 *
 * @ingroup fs
 */
typedef struct filedirectory_s {
    /// First and last nodes in the directory.
    struct filedirectory_node_s* _head, *_tail;
} filedirectory_t;

filedirectory_t* FileDirectory_ConstructStr2(const ddstring_t* pathList, char delimiter);
filedirectory_t* FileDirectory_ConstructStr(const ddstring_t* pathList);

filedirectory_t* FileDirectory_Construct2(const char* pathList, char delimiter);
filedirectory_t* FileDirectory_Construct(const char* pathList);

filedirectory_t* FileDirectory_ConstructEmpty2(char delimiter);
filedirectory_t* FileDirectory_ConstructEmpty(void);

/// \note Same as FileDirectory_ConstructEmpty
filedirectory_t* FileDirectory_ConstructDefault(void);

void FileDirectory_Destruct(filedirectory_t* pd);

/**
 * Clear the directory contents.
 */
void FileDirectory_Clear(filedirectory_t* pd);

/**
 * Resolve and collate all paths in the directory into a list.
 *
 * @param type              If a valid type, only paths of this type will be visited.
 * @param count             Number of visited paths is written back here.
 *
 * @return  Ptr to the allocated list; it is the responsibility of the caller to
 * Str_Free each string in the list and Z_Free the list itself.
 */
ddstring_t* FileDirectory_AllPaths(filedirectory_t* pd, filedirectory_pathtype_t type,
    size_t* count);

/**
 * Add a new set of paths. Duplicates are automatically pruned.
 *
 * @param paths             One or more paths.
 * @param pathsCount        Number of elements in @a paths.
 * @param callback          Callback function ptr.
 * @param paramaters        Passed to the callback.
 */
void FileDirectory_AddPaths3(filedirectory_t* pd, const dduri_t* const* paths, uint pathsCount,
    int (*callback) (const struct filedirectory_node_s* node, void* paramaters), void* paramaters);
void FileDirectory_AddPaths2(filedirectory_t* pd, const dduri_t* const* paths, uint pathsCount,
    int (*callback) (const struct filedirectory_node_s* node, void* paramaters));
void FileDirectory_AddPaths(filedirectory_t* pd, const dduri_t* const* paths, uint pathsCount);

/**
 * Add a new set of paths from a path list. Duplicates are automatically pruned.
 *
 * @param pathList          One or more paths separated by semicolons.
 * @param callback          Callback function ptr.
 * @param paramaters        Passed to the callback.
 */
void FileDirectory_AddPathList3(filedirectory_t* pd, const char* pathList,
    int (*callback) (const struct filedirectory_node_s* node, void* paramaters), void* paramaters);
void FileDirectory_AddPathList2(filedirectory_t* pd, const char* pathList,
    int (*callback) (const struct filedirectory_node_s* node, void* paramaters));
void FileDirectory_AddPathList(filedirectory_t* pd, const char* pathList);

/**
 * Find a path in the directory.
 *
 * @param searchPath        Relative or absolute path.
 * @param foundPath         If not @c NULL, the full path of the node is written
 *                          back here if found.
 *
 * @return  @c true, iff successful.
 */
boolean FileDirectory_FindPath(filedirectory_t* fd, const char* searchPath, ddstring_t* foundPath);

/**
 * Iterate over nodes in the directory making a callback for each.
 * Iteration ends when all nodes have been visited or a callback returns non-zero.
 *
 * @param type              If a valid path type only process nodes of this type.
 * @param parent            If not @c NULL, only process child nodes of this node.
 * @param callback          Callback function ptr.
 * @param paramaters        Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int FileDirectory_Iterate2(filedirectory_t* pd, filedirectory_pathtype_t type,
    struct filedirectory_node_s* parent,
    int (*callback) (const struct filedirectory_node_s* node, void* paramaters),
    void* paramaters);
int FileDirectory_Iterate(filedirectory_t* pd, filedirectory_pathtype_t type,
    struct filedirectory_node_s* parent,
    int (*callback) (const struct filedirectory_node_s* node, void* paramaters));

/**
 * @param searchPath        A relative path.
 * @param delimiter         Delimiter used to separate fragments of @a searchPath.
 *
 * @return  @c true, if the path specified in the name begins from a directory in the search path.
 */
boolean FileDirectoryNode_MatchDirectory(const struct filedirectory_node_s* node,
    const ddstring_t* searchPath, char delimiter);

#if _DEBUG
void FileDirectory_Print(filedirectory_t* pd);
#endif

/**
 * Composes a relative path for the directory node.
 */
void FileDirectoryNode_ComposePath(const struct filedirectory_node_s* node, ddstring_t* path);

/// @return  Type of this directory node.
filedirectory_pathtype_t FileDirectoryNode_Type(const struct filedirectory_node_s* node);

#endif /* LIBDENG_FILEDIRECTORY_H */
