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
#include "stringpool.h"

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
#define PCF_NO_BRANCH       0x1 /// Do not consider branches as possible candidates.
#define PCF_NO_LEAF         0x2 /// Do not consider leaves as possible candidates.
#define PCF_MATCH_PARENT    0x4 /// Only consider nodes whose parent matches that referenced.
#define PCF_MATCH_FULL      0x8 /// Whole path must match completely (i.e., path begins
                                /// from the same root point) otherwise allow partial
                                /// (i.e., relative) matches.
/**@}*/

/**
 * Path fragment info.
 */
typedef struct pathdirectorysearch_fragment_s {
    ushort hash;
    const char* from, *to;
    struct pathdirectorysearch_fragment_s* next;
} pathdirectorysearch_fragment_t;

/**
 * PathDirectorySearch. Can be allocated on the stack.
 */
/// Size of the fixed-length "small" search path (in characters) allocated with the search.
#define PATHDIRECTORYSEARCH_SMALL_PATH 256

/// Size of the fixed-length "small" fragment buffer allocated with the search.
#define PATHDIRECTORYSEARCH_SMALL_FRAGMENTBUFFER_SIZE 8

typedef struct {
    /// The search term.
    char _smallSearchPath[PATHDIRECTORYSEARCH_SMALL_PATH+1];
    char* _searchPath; // The long version.
    char _delimiter;

    /// @see pathComparisonFlags
    int _flags;

    /// Total number of fragments in the search term.
    uint _fragmentCount;

    /**
     * Fragment map of the search term. The map is split into two components.
     * The first PATHDIRECTORYSEARCH_SMALL_FRAGMENTBUFFER_SIZE elements are placed
     * into a fixed-size buffer allocated along with "this". Any additional fragments
     * are attached to "this" using a linked list.
     *
     * This optimized representation hopefully means that the majority of searches
     * can be fulfilled without dynamically allocating memory.
     */
    pathdirectorysearch_fragment_t _smallFragmentBuffer[PATHDIRECTORYSEARCH_SMALL_FRAGMENTBUFFER_SIZE];

    /// Head of the linked list of "extra" fragments, in reverse order.
    pathdirectorysearch_fragment_t* _extraFragments;
} pathdirectorysearch_t;

/**
 * Initialize the specified search from the given search term.
 * \note On C++ rewrite make this PathDirectory static
 *
 * \post The search term will have been subdivided into a fragment map and some or
 * all of the fragment hashes will have been calculated (dependant on the number of
 * discreet fragments).
 *
 * @param flags  @see pathComparisonFlags
 * @param path  Relative or absolute path to be searched for.
 * @param delimiter  Fragments of @a path are delimited by this character.
 * @return  Pointer to "this" instance for caller convenience.
 */
pathdirectorysearch_t* PathDirectory_InitSearch(pathdirectorysearch_t* search, int flags,
    const char* path, char delimiter);

/**
 * Destroy @a search releasing any resources acquired for it.
 * \note On C++ rewrite make this PathDirectory static.
 */
void PathDirectory_DestroySearch(pathdirectorysearch_t* search);

/// @return  @see pathComparisonFlags
int PathDirectorySearch_Flags(pathdirectorysearch_t* search);

/// @return  Number of path fragments in the search term.
uint PathDirectorySearch_Size(pathdirectorysearch_t* search);

/**
 * Retrieve the info for fragment @a idx within the search term. Note that
 * fragments are indexed in reverse order (compared to the logical, left-to-right
 * order of the search term).
 *
 * For example, if the search term is "c:/mystuff/myaddon.addon" the corresponding
 * fragment map will be: [0:{myaddon.addon}, 1:{mystuff}, 2:{c:}].
 *
 * \post Hash may have been calculated for the referenced fragment.
 *
 * @param idx  Reverse-index of the fragment to be retrieved.
 * @return  Processed fragment info else @c NULL if @a idx is invalid.
 */
const pathdirectorysearch_fragment_t* PathDirectorySearch_GetFragment(pathdirectorysearch_t* search, uint idx);

typedef struct {
    struct pathdirectory_node_s* head[PATHDIRECTORY_NODETYPES_COUNT];
} pathdirectory_nodelist_t;

/**
 * PathDirectory. Data structure for modelling a hierarchical relationship tree of
 * string+value data pairs.
 *
 * Somewhat similar to a Prefix Tree (Trie) representationally although that is
 * where the similarity ends.
 *
 * @ingroup data
 */
// Number of entries in the hash table.
#define PATHDIRECTORY_PATHHASH_SIZE 512
typedef pathdirectory_nodelist_t pathdirectory_pathhash_t[PATHDIRECTORY_PATHHASH_SIZE];

/// Identifier used with the search and iteration algorithms in place of a hash
/// when the caller does not wish to narrow the set of considered nodes.
#define PATHDIRECTORY_NOHASH PATHDIRECTORY_PATHHASH_SIZE

typedef struct pathdirectory_s {
    /// Path name fragment intern pool.
    struct pathdirectory_internpool_s {
        StringPool* strings;
        ushort* idHashMap; // Index by @c StringPoolInternId-1
    } _internPool;

    /// Path hash map.
    pathdirectory_pathhash_t* _pathHash;

    /// Number of unique paths in the directory.
    uint _size;
} pathdirectory_t;

pathdirectory_t* PathDirectory_New(void);

void PathDirectory_Delete(pathdirectory_t* pd);

/// @return  Number of unique paths in the directory.
uint PathDirectory_Size(pathdirectory_t* pd);

/// @return  Print-ready name for node @a type.
const ddstring_t* PathDirectory_NodeTypeName(pathdirectory_nodetype_t type);

/**
 * Clear the directory contents.
 */
void PathDirectory_Clear(pathdirectory_t* pd);

/**
 * Add a new path. Duplicates are automatically pruned however, note that their
 * associated value is replaced!
 *
 * @param path  New path to add to the directory.
 * @param delimiter  Fragments of the path are delimited by this character.
 * @param userData  User data to associate with the new path.
 */
struct pathdirectory_node_s* PathDirectory_Insert2(pathdirectory_t* pd, const char* path, char delimiter, void* userData);
struct pathdirectory_node_s* PathDirectory_Insert(pathdirectory_t* pd, const char* path, char delimiter); /*userData = NULL*/

/**
 * Perform a search of the nodes in the directory making a callback for each.
 * Pre-selection of nodes is determined by @a search. Iteration ends when all
 * selected nodes have been visited or a callback returns non-zero.
 *
 * This method essentially amounts to "interface sugar". A manual search of the
 * directory can be performed using nodes pre-selected by the caller or via the
 * PathDirectory::Iterate method.
 *
 * @param search  Pre-initialized search term @see PathDirectory::InitSearch
 * @param callback  Callback function ptr. The callback should only return a
 *     non-zero value when the desired node has been found.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
struct pathdirectory_node_s* PathDirectory_Search2(pathdirectory_t* pd, pathdirectorysearch_t* search,
    int (*callback) (struct pathdirectory_node_s* node, pathdirectorysearch_t* search, void* paramaters), void* paramaters);
struct pathdirectory_node_s* PathDirectory_Search(pathdirectory_t* pd, pathdirectorysearch_t* search,
    int (*callback) (struct pathdirectory_node_s* node, pathdirectorysearch_t* search, void* paramaters)); /*paramaters=NULL*/

/**
 * Find a node in the directory.
 *
 * \note This method essentially amounts to "interface sugar". A convenient
 *     shorthand of the call tree:
 *
 *    pathdirectorysearch_t search
 *    PathDirectory_InitSearch(&search, @a flags, @a searchPath, @a delimiter)
 *    foundNode = PathDirectory_Search("this", &search, PathDirectoryNode_MatchDirectory)
 *    PathDirectory_DestroySearch(&search)
 *    return foundNode
 *
 * @param flags  @see pathComparisonFlags
 * @param path  Relative or absolute path to be searched for.
 * @param delimiter  Fragments of @a path are delimited by this character.
 * @return  Found node else @c NULL.
 */
struct pathdirectory_node_s* PathDirectory_Find(pathdirectory_t* pd, int flags,
    const char* path, char delimiter);

/**
 * Iterate over nodes in the directory making a callback for each.
 * Iteration ends when all nodes have been visited or a callback returns non-zero.
 *
 * @param flags  @see pathComparisonFlags
 * @param parent  Parent node reference, used when restricting processing to the
 *      child nodes of this node. Only used when the flag PCF_MATCH_PARENT is set
 *      in @a flags.
 * @param callback  Callback function ptr.
 * @param paramaters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int PathDirectory_Iterate2(pathdirectory_t* pd, int flags, struct pathdirectory_node_s* parent,
    ushort hash, int (*callback) (struct pathdirectory_node_s* node, void* paramaters), void* paramaters);
int PathDirectory_Iterate(pathdirectory_t* pd, int flags, struct pathdirectory_node_s* parent,
    ushort hash, int (*callback) (struct pathdirectory_node_s* node, void* paramaters));

int PathDirectory_Iterate2_Const(const pathdirectory_t* pd, int flags, const struct pathdirectory_node_s* parent,
    ushort hash, int (*callback) (const struct pathdirectory_node_s* node, void* paramaters), void* paramaters);
int PathDirectory_Iterate_Const(const pathdirectory_t* pd, int flags, const struct pathdirectory_node_s* parent,
    ushort hash, int (*callback) (const struct pathdirectory_node_s* node, void* paramaters));

/**
 * Composes and/or calculates the composed-length of the relative path for a node.
 *
 * @param path  If not @c NULL the composed path is written here.
 * @param length  If not @c NULL the length of the composed path is written here.
 * @param delimiter  Path is composed with fragments delimited by this character.
 *
 * @return  The composed path pointer specified with @a path, for caller's convenience.
 */
ddstring_t* PathDirectory_ComposePath(pathdirectory_t* pd, const struct pathdirectory_node_s* node,
    ddstring_t* path, int* length, char delimiter);

/// @return  The path fragment which @a node represents.
const ddstring_t* PathDirectory_GetFragment(pathdirectory_t* pd, const struct pathdirectory_node_s* node);

/**
 * Collate all paths in the directory into a list.
 *
 * \todo Does this really belong here (perhaps a class static non-member)?
 *
 * @param flags  @see pathComparisonFlags
 * @param delimiter  Fragments of the path will be delimited by this character.
 * @param count  Number of visited paths is written back here.
 *
 * @return  Ptr to the allocated list; it is the responsibility of the caller to
 *      Str_Free each string in the list and free() the list itself.
 */
ddstring_t* PathDirectory_CollectPaths(pathdirectory_t* pd, int flags, char delimiter, size_t* count);

#if _DEBUG
void PathDirectory_Print(pathdirectory_t* pd, char delimiter);
void PathDirectory_PrintHashDistribution(pathdirectory_t* pd);
#endif

/// @return  PathDirectory which owns this node.
pathdirectory_t* PathDirectoryNode_Directory(const struct pathdirectory_node_s* node);

/// @return  Parent of this directory node else @c NULL
struct pathdirectory_node_s* PathDirectoryNode_Parent(const struct pathdirectory_node_s* node);

/// @return  Type of this directory node.
pathdirectory_nodetype_t PathDirectoryNode_Type(const struct pathdirectory_node_s* node);

/// @return  Intern id for the string fragment owned by the PathDirectory of which this node is a child of.
StringPoolInternId PathDirectoryNode_InternId(const struct pathdirectory_node_s* node);

/**
 * @param node  Right-most node in path.
 * @param search  Pre-initialized search term @see PathDirectory::InitSearch
 * @param paramaters  User data passed to this when used as a search callback.
 *
 * @return  @c true iff the directory matched this.
 */
int PathDirectoryNode_MatchDirectory(const struct pathdirectory_node_s* node,
    pathdirectorysearch_t* search, void* paramaters);

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

#endif /* LIBDENG_PATHDIRECTORY_H */
