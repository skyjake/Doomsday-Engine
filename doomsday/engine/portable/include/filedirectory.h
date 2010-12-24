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

#ifndef LIBDENG_FILEDIRECTORY_H
#define LIBDENG_FILEDIRECTORY_H

#include "sys_file.h"
#include "m_string.h"

typedef struct filedirectory_node_s {
    struct filedirectory_node_s* next;
    struct filedirectory_node_s* parent;
    filetype_t type;
    char* path;
    boolean processed;
} filedirectory_node_t;

/**
 * File Directory. File system component representing a physical directory.
 *
 * @ingroup fs
 */
typedef struct filedirectory_s {
    /// Copy of the entire path list.
    ddstring_t _pathList;

    /// @c true if the record set has been built.
    boolean _builtRecordSet;

    /// First and last directory nodes.
    struct filedirectory_node_s* _direcFirst, *_direcLast;
} filedirectory_t;

filedirectory_t* FileDirectory_Create2(const char* pathList);
filedirectory_t* FileDirectory_Create(void);

void FileDirectory_Destroy(filedirectory_t* fileDirectory);

/**
 * Clear the directory contents.
 */
void FileDirectory_Clear(filedirectory_t* fileDirectory);

/**
 * Resolve and collate all file paths in the directory into a list.
 *
 * @param type              If a valid file type only process nodes of this type.
 * @param count             Number of files in the list is written back here.
 * @return                  Ptr to the allocated list; it is the responsibility
 *                          of the caller to Str_Free each string in the list and
 *                          Z_Free the list itself.
 */
ddstring_t* FileDirectory_CollectFilePaths(filedirectory_t* fd, filetype_t type, size_t* count);

#if _DEBUG
void FileDirectory_PrintFileList(filedirectory_t* fileDirectory);
#endif

/**
 * Add a new set of file paths. Duplicates are automatically pruned.
 *
 * @param pathList          One or more paths separated by semicolons.
 * @param callback          Callback function ptr.
 * @param paramaters        Passed to the callback.
 */
void FileDirectory_AddPaths3(filedirectory_t* fileDirectory, const char* pathList,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters);
void FileDirectory_AddPaths2(filedirectory_t* fileDirectory, const char* pathList,
    int (*callback) (const filedirectory_node_t* node, void* paramaters));
void FileDirectory_AddPaths(filedirectory_t* fileDirectory, const char* pathList);

/**
 * Find a path in the directory.
 *
 * \note Poor performance: O(n)
 * \todo Implement a special-case name search algorithm to improve performance.
 *
 * @param searchPath        Relative or absolute path.
 * @param foundPath         If not @c NULL, the relative path is written back here if found.
 *
 * @return                  @c true, iff successful.
 */
boolean FileDirectory_Find2(filedirectory_t* fileDirectory, const char* searchPath, ddstring_t* foundPath);
boolean FileDirectory_Find(filedirectory_t* fileDirectory, const char* searchPath);

/**
 * Iterate over nodes in the directory making a callback for each.
 * Iteration ends when all nodes have been visited or a callback returns non-zero.
 *
 * @param type              If a valid file type only process nodes of this type.
 * @param parent            If not @c NULL, only process child nodes of this node.
 * @param callback          Callback function ptr.
 * @param paramaters        Passed to the callback.
 *
 * @return                  @c 0 iff iteration completed wholly.
 */
int FileDirectory_Iterate2(filedirectory_t* fileDirectory, filetype_t type, filedirectory_node_t* parent,
    int (*callback) (const filedirectory_node_t* node, void* paramaters), void* paramaters);
int FileDirectory_Iterate(filedirectory_t* fileDirectory, filetype_t type, filedirectory_node_t* parent,
    int (*callback) (const filedirectory_node_t* node, void* paramaters));

/**
 * @param name          A relative path.
 *
 * @return              @c true, if the path specified in the name begins
 *                      from a directory in the search path.
 */
boolean FileDirectoryNode_MatchDirectory(const filedirectory_node_t* direc, const char* name);

/**
 * Composes a relative path for the directory node.
 */
void FileDirectoryNode_ComposePath(const filedirectory_node_t* direc, ddstring_t* path);

#endif /* LIBDENG_FILEDIRECTORY_H */
