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
    PATHDIRECTORY_NODETYPES_FIRST = 0,
    PT_BRANCH = PATHDIRECTORY_NODETYPES_FIRST,
    PT_LEAF,
    PATHDIRECTORY_NODETYPES_COUNT
} pathdirectory_nodetype_t;

#define VALID_PATHDIRECTORY_NODETYPE(t) ((t) >= PATHDIRECTORY_NODETYPES_FIRST && (t) < PATHDIRECTORY_NODETYPES_COUNT)

/**
 * PathDirectory. Data structure for modelling a hierarchical relationship
 * tree of string+value data pairs.
 *
 * Somewhat similar to a Prefix Tree (Trie) representationally although that
 * is where the similarity ends.
 *
 * @ingroup data
 */
// Number of entries in the hash table.
#define PATHDIRECTORY_PATHHASH_SIZE 512
typedef struct pathdirectory_node_s* pathdirectory_pathhash_t[PATHDIRECTORY_PATHHASH_SIZE];

typedef uint pathdirectory_internnameid_t;

typedef struct pathdirectory_s {
    /// Intern name list. Indices become class-private identifiers.
    uint _internNamesCount;
    struct pathdirectory_internname_s* _internNames;

    /// Sorted redirection table.
    pathdirectory_internnameid_t* _sortedNameIdMap;

    /// Path hash map.
    pathdirectory_pathhash_t* _pathHash;
} pathdirectory_t;

pathdirectory_t* PathDirectory_Construct(void);

void PathDirectory_Destruct(pathdirectory_t* pd);

/**
 * Clear the directory contents.
 */
void PathDirectory_Clear(pathdirectory_t* pd);

/**
 * Check if @a searchPath exists in the directory.
 *
 * @param searchPath        Relative or absolute path.
 * @param delimiter         Fragments of the path are delimited by this character.
 *
 * @return  Pointer to the associated node iff found else @c 0
 */
struct pathdirectory_node_s* PathDirectory_Find(pathdirectory_t* pd,
    const char* searchPath, char delimiter);

/**
 * Add a new path. Duplicates are automatically pruned however, note that their
 * associated value is replaced!
 *
 * @param path              New path to add to the directory.
 * @param delimiter         Fragments of the path are delimited by this character.
 * @param value             Associated data value.
 */
struct pathdirectory_node_s* PathDirectory_Insert2(pathdirectory_t* pd,
    const char* path, char delimiter, void* value);
struct pathdirectory_node_s* PathDirectory_Insert(pathdirectory_t* pd,
    const char* path, char delimiter);

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
int PathDirectory_Iterate2(pathdirectory_t* pd, pathdirectory_nodetype_t type,
    struct pathdirectory_node_s* parent,
    int (*callback) (struct pathdirectory_node_s* node, void* paramaters),
    void* paramaters);
int PathDirectory_Iterate(pathdirectory_t* pd, pathdirectory_nodetype_t type,
    struct pathdirectory_node_s* parent,
    int (*callback) (struct pathdirectory_node_s* node, void* paramaters));

int PathDirectory_Iterate2_Const(const pathdirectory_t* pd, pathdirectory_nodetype_t type,
    const struct pathdirectory_node_s* parent,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters),
    void* paramaters);
int PathDirectory_Iterate_Const(const pathdirectory_t* pd, pathdirectory_nodetype_t type,
    const struct pathdirectory_node_s* parent,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters));

/**
 * Collate all paths in the directory into a list.
 *
 * @todo Does this really belong here (perhaps a class static non-member)?
 *
 * @param type              If a valid type; only paths of this type will be visited.
 * @param delimiter         Fragments of the path will be delimited by this character.
 * @param count             Number of visited paths is written back here.
 *
 * @return  Ptr to the allocated list; it is the responsibility of the caller to
 *      Str_Free each string in the list and Z_Free the list itself.
 */
ddstring_t* PathDirectory_AllPaths(pathdirectory_t* pd, pathdirectory_nodetype_t type,
    char delimiter, size_t* count);

/**
 * @param searchPath        A relative path.
 * @param searchPathLen     Number of characters from @a searchPath to match. Normally
 *      equal to the full length of @a searchPath excluding any terminating '\0'.
 * @param delimiter         Delimiter used to separate fragments of @a searchPath.
 *
 * @return  @c true, if the path specified in the name begins from a directory in the search path.
 */
boolean PathDirectoryNode_MatchDirectory(const struct pathdirectory_node_s* node,
    const char* searchPath, size_t searchPathLen, char delimiter);

/**
 * Composes a relative path for the directory node.
 * @param path              Composed path is written here.
 * @param delimiter         Path is composed with fragments delimited by this character.
 */
void PathDirectoryNode_ComposePath(const struct pathdirectory_node_s* node, ddstring_t* path,
    char delimiter);

/// @return  Parent of this directory node else @c NULL
struct pathdirectory_node_s* PathDirectoryNode_Parent(const struct pathdirectory_node_s* node);

/// @return  Type of this directory node.
pathdirectory_nodetype_t PathDirectoryNode_Type(const struct pathdirectory_node_s* node);

/**
 * Attach user data to this. PathDirectoryNode is given ownership of @a data
 */
void PathDirectoryNode_AttachUserData(struct pathdirectory_node_s* node, void* data);

/**
 * Detach user data from this. Ownership of the data is relinquished to the caller.
 */
void* PathDirectoryNode_DetachUserData(struct pathdirectory_node_s* node);

/// @return  Data associated with this.
void* PathDirectoryNode_UserData(const struct pathdirectory_node_s* node);

#if _DEBUG
void PathDirectory_Print(pathdirectory_t* pd, char delimiter);
#endif

#endif /* LIBDENG_PATHDIRECTORY_H */
