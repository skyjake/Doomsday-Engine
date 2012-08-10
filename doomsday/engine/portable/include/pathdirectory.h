/**\file
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

#ifndef LIBDENG_PATHDIRECTORY_H
#define LIBDENG_PATHDIRECTORY_H

#include <de/str.h>
#include "pathmap.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup pathComparisonFlags  Path Comparison Flags
 * @ingroup base apiFlags
 */
///@{
#define PCF_NO_BRANCH       0x1 ///< Do not consider branches as possible candidates.
#define PCF_NO_LEAF         0x2 ///< Do not consider leaves as possible candidates.
#define PCF_MATCH_PARENT    0x4 ///< Only consider nodes whose parent matches that referenced.
#define PCF_MATCH_FULL      0x8 /**< Whole path must match completely (i.e., path begins
                                     from the same root point) otherwise allow partial
                                     (i.e., relative) matches. */
///@}

typedef enum {
    PT_ANY = -1,
    PATHDIRECTORYNODE_TYPE_FIRST = 0,
    PT_BRANCH = PATHDIRECTORYNODE_TYPE_FIRST,
    PT_LEAF,
    PATHDIRECTORYNODE_TYPE_COUNT
} pathdirectorynode_type_t;

// Helper macro for determining if the value v can be interpreted as a valid node type.
#define VALID_PATHDIRECTORYNODE_TYPE(v) (\
    (v) >= PATHDIRECTORYNODE_TYPE_FIRST && (v) < PATHDIRECTORYNODE_TYPE_COUNT)

struct pathdirectory_s; // The pathdirectory instance (opaque).

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <de/stringpool.h>

namespace de {

class PathDirectory;

class PathDirectoryNode
{
public:
    PathDirectoryNode(de::PathDirectory& directory, pathdirectorynode_type_t type,
                      de::PathDirectoryNode* parent, StringPoolId internId,
                      void* userData=NULL);
    ~PathDirectoryNode();

    /// @return  PathDirectory which owns this node.
    PathDirectory* directory() const { return directory_; }

    /// @return  Parent of this directory node else @c NULL
    PathDirectoryNode* parent() const { return parent_; }

    /// @return  Type of this directory node.
    pathdirectorynode_type_t type() const { return type_; }

    /// @return  Hash for this directory node path fragment.
    ushort hash() const;

    /**
     * @param flags  @see pathComparisonFlags
     * @param searchPattern  Fragment mapped search pattern (path).
     * @return  @c true iff the directory matched this.
     */
    int matchDirectory(int flags, PathMap* candidatePath);

    /**
     * Attach user data to this. PathDirectoryNode is given ownership of @a data
     */
    void attachUserData(void* data);

    /**
     * Detach user data from this. Ownership of the data is relinquished to the caller.
     */
    void* detachUserData();

    /// @return  Data associated with this.
    void* userData() const;

    /// @return  Print-ready name for node @a type.
    static const ddstring_t* typeName(pathdirectorynode_type_t type);

/// @fixme should be private:
    StringPoolId internId() const;

public:
    typedef struct pathdirectorynode_userdatapair_s {
        StringPoolId internId;
        void* data;
    } pathdirectorynode_userdatapair_t;

    /// Next node in the hashed path bucket.
    PathDirectoryNode* next;

    /// Parent node in the user's logical hierarchy.
    PathDirectoryNode* parent_;

    /// Symbolic node type.
    pathdirectorynode_type_t type_;

    /// PathDirectory which owns this node.
    PathDirectory* directory_;

    /// User data present at this node.
    pathdirectorynode_userdatapair_t pair;
};

} // namespace de

extern "C" {
#endif // __cplusplus

struct pathdirectorynode_s; // The pathdirectorynode instance (opaque).
typedef struct pathdirectorynode_s PathDirectoryNode;

/**
 * PathDirectory. Data structure for modelling a hierarchical relationship tree of
 * string+value data pairs.
 *
 * Somewhat similar to a Prefix Tree (Trie) representationally although that is
 * where the similarity ends.
 *
 * @ingroup base
 */

// Number of buckets in the hash table.
#define PATHDIRECTORY_PATHHASH_SIZE 512

/// Identifier used with the search and iteration algorithms in place of a
/// hash when the caller does not wish to narrow the set of considered nodes.
#define PATHDIRECTORY_NOHASH PATHDIRECTORY_PATHHASH_SIZE

/**
 * @defgroup pathDirectoryFlags  Path Directory Flags
 */
///@{
#define PDF_ALLOW_DUPLICATE_LEAF  0x1 ///< There can be more than one leaf with a given name.
///@}

/**
 * Callback function type for PathDirectory::Iterate
 *
 * @param node          PathDirectoryNode being processed.
 * @param parameters    User data passed to this.
 *
 * @return  Non-zero if iteration should stop.
 */
typedef int (*pathdirectory_iteratecallback_t) (PathDirectoryNode* node, void* parameters);

/// Const variant.
typedef int (*pathdirectory_iterateconstcallback_t) (const PathDirectoryNode* node, void* parameters);

/**
 * Callback function type for PathDirectory::Search
 *
 * @param node          Right-most node in path.
 * @param flags         @ref pathComparisonFlags
 * @param mappedSearchPath  Fragment mapped search path.
 * @param parameters    User data passed to this when used as a search callback.
 *
 * @return  @c true iff the directory matched this.
 */
typedef int (*pathdirectory_searchcallback_t) (PathDirectoryNode* node, int flags,
    PathMap* mappedSearchPath, void* parameters);

#ifdef __cplusplus
} // extern "C"

#include <QMultiHash>

namespace de {

class PathDirectory
{
public:
    typedef QMultiHash<ushort, de::PathDirectoryNode*> NodeHash;

public:
    explicit PathDirectory(int flags=0);
    ~PathDirectory();

    /// @return  Number of unique paths in the directory.
    uint size() const { return size_; }

    /**
     * Clear the directory contents.
     */
    void clear();

    /**
     * Add a new path. Duplicates are automatically pruned however, note that their
     * associated user data value is replaced!
     *
     * @param path          New path to add to the directory.
     * @param delimiter     Fragments of the path are delimited by this character.
     * @param userData      User data to associate with the new path.
     */
    PathDirectoryNode* insert(const char* path, char delimiter, void* userData=NULL);

    /**
     * Find a node in the directory.
     *
     * @note This method essentially amounts to "interface sugar". A convenient
     *       shorthand of the call tree:
     *
     * <pre>
     *    PathMap search;
     *    PathMap_Initialize(&search, @a flags, @a searchPath, @a delimiter);
     *    foundNode = PathDirectory_Search("this", &search, PathDirectoryNode_MatchDirectory);
     *    PathMap_Destroy(&search);
     *    return foundNode;
     * </pre>
     *
     * @param flags         @ref pathComparisonFlags
     * @param path          Relative or absolute path to be searched for.
     * @param delimiter     Fragments of @a path are delimited by this character.
     *
     * @return  Found node else @c NULL.
     */
    PathDirectoryNode* find(int flags, const char* path, char delimiter);

    /**
     * Composes and/or calculates the composed-length of the relative path for a node.
     *
     * @param path          If not @c NULL the composed path is written here.
     * @param length        If not @c NULL the length of the composed path is written here.
     * @param delimiter     Path is composed with fragments delimited by this character.
     *
     * @return  The composed path pointer specified with @a path, for caller's convenience.
     */
    ddstring_t* composePath(const PathDirectoryNode* node, ddstring_t* path,
                            int* length, char delimiter);

    /// @return  The path fragment which @a node represents.
    const ddstring_t* pathFragment(const PathDirectoryNode* node);

    /**
     * Collate all paths in the directory into a list.
     *
     * @todo Does this really belong here (perhaps a class static non-member)?
     *
     * @param flags         @ref pathComparisonFlags
     * @param delimiter     Fragments of the path will be delimited by this character.
     * @param count         Number of visited paths is written back here.
     *
     * @return  The allocated list; it is the responsibility of the caller to Str_Free()
     *          each string in the list and free() the list itself.
     */
    ddstring_t* collectPaths(int flags, char delimiter, size_t* count);

    /**
     * Provides access to the PathDirectoryNode hash for efficent traversals.
     */
    /*const*/ NodeHash* nodeHash(pathdirectorynode_type_t type) const;

    /**
     * This is a hash function. It uses the path fragment string to generate
     * a somewhat-random number between @c 0 and @c PATHDIRECTORY_PATHHASH_SIZE
     *
     * @return  The generated hash key.
     */
    static ushort hashPathFragment(const char* fragment, size_t len, char delimiter);

    static void debugPrint(PathDirectory* pd, char delimiter);
    static void debugPrintHashDistribution(PathDirectory* pd);

private:
    void clearInternPool();

    PathDirectoryNode* findNode(PathDirectoryNode* parent, pathdirectorynode_type_t nodeType,
                                StringPoolId internId);

    StringPoolId internNameAndUpdateIdHashMap(const ddstring_t* name, ushort hash);

    /**
     * @return  [ a new | the ] directory node that matches the name and type and
     * which has the specified parent node.
     */
    PathDirectoryNode* direcNode(PathDirectoryNode* parent, pathdirectorynode_type_t nodeType,
                                 const ddstring_t* name, char delimiter, void* userData);

    /**
     * The path is split into as many nodes as necessary. Parent links are set.
     *
     * @return  The node that identifies the given path.
     */
    PathDirectoryNode* buildDirecNodes(const char* path, char delimiter);

    /**
     * @param pd         PathDirectory.
     * @param node       Node whose path to construct.
     * @param constructedPath  The constructed path is written here. Previous contents discarded.
     * @param delimiter  Character to use for separating fragments.
     *
     * @return @a constructedPath
     *
     * @todo This is a good candidate for result caching: the constructed path
     * could be saved and returned on subsequent calls. Are there any circumstances
     * in which the cached result becomes obsolete? -jk
     */
    ddstring_t* constructPath(const PathDirectoryNode* node, ddstring_t* constructedPath, char delimiter);

    static void collectPathsInHash(NodeHash& ph, char delimiter, ddstring_t** pathListAdr);

    static PathDirectoryNode* newNode(PathDirectory* directory,
                                      pathdirectorynode_type_t type, PathDirectoryNode* parent,
                                      StringPoolId internId, void* userData);

    static void clearPathHash(NodeHash& ph);

public: ///@fixme should be private
    ushort hashForInternId(StringPoolId internId);

public:
    /// Path name fragment intern pool.
    StringPool* stringPool;

    int flags; /// @see pathDirectoryFlags

    /// Path node hashes.
    NodeHash* pathLeafHash;
    NodeHash* pathBranchHash;

    /// Total number of unique paths in the directory.
    uint size_;
};

} // namespace de

extern "C" {
#endif

/**
 * C wrapper API:
 */

typedef struct pathdirectory_s PathDirectory;

PathDirectory* PathDirectory_New(void);
PathDirectory* PathDirectory_NewWithFlags(int flags);
void PathDirectory_Delete(PathDirectory* pd);

uint PathDirectory_Size(PathDirectory* pd);
void PathDirectory_Clear(PathDirectory* pd);

PathDirectoryNode* PathDirectory_Insert2(PathDirectory* pd, const char* path, char delimiter, void* userData);
PathDirectoryNode* PathDirectory_Insert(PathDirectory* pd, const char* path, char delimiter); /*userData = NULL*/

/**
 * Perform a search of the nodes in the directory making a callback for each.
 * Pre-selection of nodes is determined by @a search. Iteration ends when all
 * selected nodes have been visited or a callback returns non-zero.
 *
 * This method essentially amounts to "interface sugar". A manual search of the
 * directory can be performed using nodes pre-selected by the caller or using
 * PathDirectory_Iterate().
 *
 * @param flags         @ref pathComparisonFlags
 * @param mappedSearchPath  Fragment mapped search path.
 * @param callback      Callback function ptr. The callback should only return
 *                      a non-zero value when the desired node has been found.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
PathDirectoryNode* PathDirectory_Search2(PathDirectory* pd, int flags,
    PathMap* mappedSearchPath, pathdirectory_searchcallback_t callback, void* parameters);
PathDirectoryNode* PathDirectory_Search(PathDirectory* pd, int flags,
    PathMap* mappedSearchPath, pathdirectory_searchcallback_t callback); /*parameters=NULL*/

PathDirectoryNode* PathDirectory_Find(PathDirectory* pd, int flags,
    const char* path, char delimiter);

/**
 * Iterate over nodes in the directory making a callback for each.
 * Iteration ends when all nodes have been visited or a callback returns non-zero.
 *
 * @param flags         @ref pathComparisonFlags
 * @param parent        Parent node reference, used when restricting processing
 *                      to the child nodes of this node. Only used when the flag
 *                      PCF_MATCH_PARENT is set in @a flags.
 * @param callback      Callback function ptr.
 * @param parameters    Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
int PathDirectory_Iterate2(PathDirectory* pd, int flags, PathDirectoryNode* parent,
    ushort hash, pathdirectory_iteratecallback_t callback, void* parameters);
int PathDirectory_Iterate(PathDirectory* pd, int flags, PathDirectoryNode* parent,
    ushort hash, pathdirectory_iteratecallback_t callback);

int PathDirectory_Iterate2_Const(const PathDirectory* pd, int flags, const PathDirectoryNode* parent,
    ushort hash, pathdirectory_iterateconstcallback_t callback, void* parameters);
int PathDirectory_Iterate_Const(const PathDirectory* pd, int flags, const PathDirectoryNode* parent,
    ushort hash, pathdirectory_iterateconstcallback_t callback);

ddstring_t* PathDirectory_ComposePath(PathDirectory* pd, const PathDirectoryNode* node,
    ddstring_t* path, int* length, char delimiter);

const ddstring_t* PathDirectory_GetFragment(PathDirectory* pd, const PathDirectoryNode* node);

ddstring_t* PathDirectory_CollectPaths(PathDirectory* pd, int flags, char delimiter, size_t* count);

ushort PathDirectory_HashPath(const char* path, size_t len, char delimiter);

#if _DEBUG
void PathDirectory_DebugPrint(PathDirectory* pd, char delimiter);
void PathDirectory_DebugPrintHashDistribution(PathDirectory* pd);
#endif

PathDirectory* PathDirectoryNode_Directory(const PathDirectoryNode* node);
PathDirectoryNode* PathDirectoryNode_Parent(const PathDirectoryNode* node);
pathdirectorynode_type_t PathDirectoryNode_Type(const PathDirectoryNode* node);
ushort PathDirectoryNode_Hash(const PathDirectoryNode* node);
int PathDirectoryNode_MatchDirectory(PathDirectoryNode* node, int flags,
    PathMap* candidatePath, void* parameters);
void PathDirectoryNode_AttachUserData(PathDirectoryNode* node, void* data);
void* PathDirectoryNode_DetachUserData(PathDirectoryNode* node);
void* PathDirectoryNode_UserData(const PathDirectoryNode* node);
const ddstring_t* PathDirectoryNode_TypeName(pathdirectorynode_type_t type);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_PATHDIRECTORY_H */
