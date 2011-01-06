/**\file
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

#ifndef LIBDENG_PATHDIRECTORY_H
#define LIBDENG_PATHDIRECTORY_H

#include "dd_string.h"

typedef enum {
    PT_INVALID = -1,
    PATHDIRECTORY_PATHTYPES_FIRST = 0,
    PT_DIRECTORY = PATHDIRECTORY_PATHTYPES_FIRST,
    PT_FILE,
    PATHDIRECTORY_PATHTYPES_COUNT
} pathdirectory_pathtype_t;

#define VALID_PATHDIRECTORY_PATHTYPE(t)     ((t) >= PATHDIRECTORY_PATHTYPES_FIRST || (t) < PATHDIRECTORY_PATHTYPES_COUNT)

typedef struct pathdirectory_node_s {
    struct pathdirectory_node_s* next;
    struct pathdirectory_node_s* parent;
    pathdirectory_pathtype_t type;
    ddstring_t path;
    boolean processed;
} pathdirectory_node_t;

/**
 * Path Directory. Virtual file system component representing a
 * hierarchical path structure.
 *
 * @ingroup fs
 */
typedef struct pathdirectory_s {
    /// Current path list.
    ddstring_t _pathList;

    /// @c true if the record set has been built.
    boolean _builtRecordSet;

    /// First and last nodes.
    struct pathdirectory_node_s* _head, *_tail;
} pathdirectory_t;

pathdirectory_t* PathDirectory_Construct2(const ddstring_t* pathList);
pathdirectory_t* PathDirectory_Construct(const char* pathList);
pathdirectory_t* PathDirectory_ConstructDefault(void);

void PathDirectory_Destruct(pathdirectory_t* pathDirectory);

/**
 * Clear the directory contents.
 */
void PathDirectory_Clear(pathdirectory_t* pathDirectory);

/**
 * Resolve and collate all paths in the directory into a list.
 *
 * @param type              If a valid path, only paths of this type will be visited.
 * @param count             Number of files in the list is written back here.
 * @return                  Ptr to the allocated list; it is the responsibility
 *                          of the caller to Str_Free each string in the list and
 *                          Z_Free the list itself.
 */
ddstring_t* PathDirectory_AllPaths2(pathdirectory_t* fd, pathdirectory_pathtype_t type, size_t* count);

/**
 * Add a new set of file paths. Duplicates are automatically pruned.
 *
 * @param pathList          One or more paths separated by semicolons.
 * @param callback          Callback function ptr.
 * @param paramaters        Passed to the callback.
 */
void PathDirectory_AddPaths3(pathdirectory_t* pathDirectory, const char* pathList,
    int (*callback) (const pathdirectory_node_t* node, void* paramaters), void* paramaters);
void PathDirectory_AddPaths2(pathdirectory_t* pathDirectory, const char* pathList,
    int (*callback) (const pathdirectory_node_t* node, void* paramaters));
void PathDirectory_AddPaths(pathdirectory_t* pathDirectory, const char* pathList);

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
boolean PathDirectory_Find2(pathdirectory_t* pathDirectory, const char* searchPath, ddstring_t* foundPath);
boolean PathDirectory_Find(pathdirectory_t* pathDirectory, const char* searchPath);

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
int PathDirectory_Iterate2(pathdirectory_t* pathDirectory, pathdirectory_pathtype_t type, pathdirectory_node_t* parent,
    int (*callback) (const pathdirectory_node_t* node, void* paramaters), void* paramaters);
int PathDirectory_Iterate(pathdirectory_t* pathDirectory, pathdirectory_pathtype_t type, pathdirectory_node_t* parent,
    int (*callback) (const pathdirectory_node_t* node, void* paramaters));

/**
 * @param name          A relative path.
 *
 * @return              @c true, if the path specified in the name begins
 *                      from a directory in the search path.
 */
boolean PathDirectoryNode_MatchDirectory(const pathdirectory_node_t* node, const ddstring_t* searchPath);

/**
 * Composes a relative path for the directory node.
 */
void PathDirectoryNode_ComposePath(const pathdirectory_node_t* node, ddstring_t* path);

#if _DEBUG
void PathDirectory_Print(pathdirectory_t* fd);
#endif

#endif /* LIBDENG_PATHDIRECTORY_H */
