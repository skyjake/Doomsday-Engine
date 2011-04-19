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

// Helper macro for determining if the value v can be interpreted as a valid node type.
#define VALID_PATHDIRECTORY_NODETYPE(v) (\
    (v) >= PATHDIRECTORY_NODETYPES_FIRST && (v) < PATHDIRECTORY_NODETYPES_COUNT)

/**
 * @defgroup pathComparisonFlags  Path Comparison Flags
 * @{
 */
#define PCF_NO_LEAF         0x1 /// Do not consider leaves as possible candidates.
#define PCF_NO_BRANCH       0x2 /// Do not consider branches as possible candidates.
#define PCF_MATCH_PARENT    0x4 /// Only consider nodes whose parent matches that referenced.
#define PCF_MATCH_FULL      0x8 /// Whole path must match completely (i.e., path begins
                                /// from the same root point) otherwise allow partial
                                /// (i.e., relative) matches.
/**@}*/

/**
 * PathDirectory. Data structure for modelling a hierarchical relationship tree of
 * string+value data pairs.
 *
 * Somewhat similar to a Prefix Tree (Trie) representationally although that is where
 * the similarity ends.
 *
 * @ingroup data
 */
// Number of entries in the hash table.
#define PATHDIRECTORY_PATHHASH_SIZE 512
typedef struct pathdirectory_node_s* pathdirectory_pathhash_t[PATHDIRECTORY_PATHHASH_SIZE];

// Intern name identifier. Used privately by this class and friends.
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
 * @param flags             @see pathComparisonFlags
 * @param searchPath        Relative or absolute path.
 * @param delimiter         Fragments of the path are delimited by this character.
 *
 * @return  Pointer to the associated node iff found else @c 0
 */
struct pathdirectory_node_s* PathDirectory_Find(pathdirectory_t* pd, int flags,
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
 * @param flags             @see pathComparisonFlags
 * @param parent            Parent node reference, used when restricting processing
 *      to the child nodes of this node. Only used when the flag PCF_MATCH_PARENT
 *      is set in @a flags.
 * @param callback          Callback function ptr.
 * @param paramaters        Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int PathDirectory_Iterate2(pathdirectory_t* pd, int flags, struct pathdirectory_node_s* parent,
    int (*callback) (struct pathdirectory_node_s* node, void* paramaters), void* paramaters);
int PathDirectory_Iterate(pathdirectory_t* pd, int flags, struct pathdirectory_node_s* parent,
    int (*callback) (struct pathdirectory_node_s* node, void* paramaters));

int PathDirectory_Iterate2_Const(const pathdirectory_t* pd, int flags, const struct pathdirectory_node_s* parent,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters), void* paramaters);
int PathDirectory_Iterate_Const(const pathdirectory_t* pd, int flags, const struct pathdirectory_node_s* parent,
    int (*callback) (const struct pathdirectory_node_s* node, void* paramaters));

/**
 * Collate all paths in the directory into a list.
 *
 * @todo Does this really belong here (perhaps a class static non-member)?
 *
 * @param flags             @see pathComparisonFlags
 * @param delimiter         Fragments of the path will be delimited by this character.
 * @param count             Number of visited paths is written back here.
 *
 * @return  Ptr to the allocated list; it is the responsibility of the caller to
 *      Str_Free each string in the list and free() the list itself.
 */
ddstring_t* PathDirectory_CollectPaths(pathdirectory_t* pd, int flags, char delimiter,
    size_t* count);

/**
 * @param flags             @see pathComparisonFlags
 * @param searchPath        A relative path.
 * @param searchPathLen     Number of characters from @a searchPath to match. Normally
 *      equal to the full length of @a searchPath excluding any terminating '\0'.
 * @param delimiter         Delimiter used to separate fragments of @a searchPath.
 *
 * @return  @c true iff @a searchPath matches this.
 */
boolean PathDirectoryNode_MatchDirectory(const struct pathdirectory_node_s* node,
    int flags, const char* searchPath, size_t searchPathLen, char delimiter);

/**
 * Composes a relative path for the directory node.
 * @param path              Composed path is written here.
 * @param delimiter         Path is composed with fragments delimited by this character.
 * @return  The composed path pointer specified with @a path, for caller's convenience.
 */
ddstring_t* PathDirectoryNode_ComposePath(const struct pathdirectory_node_s* node,
    ddstring_t* path, char delimiter);

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
