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

struct pathdirectory_node_s;

typedef enum {
    PT_ANY = -1,
    PATHDIRECTORY_PATHTYPES_FIRST = 0,
    PT_DIRECTORY = PATHDIRECTORY_PATHTYPES_FIRST,
    PT_LEAF,
    PATHDIRECTORY_PATHTYPES_COUNT
} pathdirectory_pathtype_t;

#define VALID_PATHDIRECTORY_PATHTYPE(t) ((t) >= PATHDIRECTORY_PATHTYPES_FIRST || (t) < PATHDIRECTORY_PATHTYPES_COUNT)

/**
 * PathDirectory. Data structure for modelling a hierarchical relationship
 * tree of string+value data pairs.
 *
 * Somewhat similar to a Prefix Tree (Trie) representationally although
 * that is where the similarity ends.
 *
 * @ingroup data
 */
// Number of entries in the hash table.
#define PATHDIRECTORY_HASHSIZE 512

typedef struct pathdirectory_s {
    /// Path hash table.
    struct pathdirectory_node_s* _hashTable[PATHDIRECTORY_HASHSIZE];
} pathdirectory_t;

pathdirectory_t* PathDirectory_ConstructDefault(void);

void PathDirectory_Destruct(pathdirectory_t* pd);

/**
 * Clear the directory contents.
 */
void PathDirectory_Clear(pathdirectory_t* pd);

/**
 * Check if @a searchPath exists in the directory.
 *
 * @param searchPath        Relative or absolute path.
 *
 * @return  Pointer to the associated node iff found else @c 0
 */
const struct pathdirectory_node_s* PathDirectory_FindStr(pathdirectory_t* pd,
    const ddstring_t* searchPath);
const struct pathdirectory_node_s* PathDirectory_Find(pathdirectory_t* pd,
    const char* searchPath);

/**
 * Add a new path. Duplicates are automatically pruned however, note that their
 * associated value is replaced!
 *
 * @param path              New path to add to the directory.
 * @param value             Associated data value.
 * @param delimiter         Separates fragments of @a path (become tree nodes).
 */
void PathDirectory_Insert(pathdirectory_t* pd, const ddstring_t* path, void* value,
    char delimiter);

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
int PathDirectory_Iterate2(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    struct pathdirectory_node_s* parent,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters),
    void* paramaters);
int PathDirectory_Iterate(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    struct pathdirectory_node_s* parent,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters));

/**
 * Collate all paths in the directory into a list.
 *
 * @todo Does this really belong here (perhaps a class static non-member)?
 *
 * @param type              If a valid type; only paths of this type will be visited.
 * @param count             Number of visited paths is written back here.
 *
 * @return  Ptr to the allocated list; it is the responsibility of the caller to
 *      Str_Free each string in the list and Z_Free the list itself.
 */
ddstring_t* PathDirectory_AllPaths(pathdirectory_t* pd, pathdirectory_pathtype_t type,
    size_t* count);

#if _DEBUG
void PathDirectory_Print(pathdirectory_t* pd);
#endif

/**
 * @param searchPath        A relative path.
 * @param delimiter         Delimiter used to separate fragments of @a searchPath.
 *
 * @return  @c true, if the path specified in the name begins from a directory in the search path.
 */
boolean PathDirectoryNode_MatchDirectory(const struct pathdirectory_node_s* node,
    const ddstring_t* searchPath, char delimiter);

/**
 * Composes a relative path for the directory node.
 */
void PathDirectoryNode_ComposePath(const struct pathdirectory_node_s* node, ddstring_t* path);

#endif /* LIBDENG_PATHDIRECTORY_H */
