/**
 * @file pathtree.h
 *
 * PathTree. Data structure for modelling a hierarchical relationship tree
 * of string+value data pairs.
 *
 * Somewhat similar to a Prefix Tree (Trie) representationally although that is
 * where the similarity ends.
 *
 * @ingroup base
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2009-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_PATHTREE_H
#define LIBDENG_PATHTREE_H

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
    PATHTREENODE_TYPE_FIRST = 0,
    PT_BRANCH = PATHTREENODE_TYPE_FIRST,
    PT_LEAF,
    PATHTREENODE_TYPE_COUNT
} PathTreeNodeType;

// Helper macro for determining if the value v can be interpreted as a valid node type.
#define VALID_PATHTREENODE_TYPE(v) (\
    (v) >= PATHTREENODE_TYPE_FIRST && (v) < PATHTREENODE_TYPE_COUNT)

struct pathtree_s; // The pathtree instance (opaque).

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <de/stringpool.h>

namespace de {

class PathTree;

class PathTreeNode
{
public:
    /// @return  Print-ready name for node @a type.
    static ddstring_t const* typeName(PathTreeNodeType type);

public:
    /// @todo ctor/dtor should be private or made callable only by de::PathTree
    PathTreeNode(PathTree& tree, PathTreeNodeType type,
                 StringPoolId internId, PathTreeNode* parent = NULL,
                 void* userData = NULL);
    ~PathTreeNode();

    /// @return  PathTree which owns this node.
    PathTree& tree() const;

    /// @return  Parent of this node else @c NULL
    PathTreeNode* parent() const;

    /// @return  Type of this node.
    PathTreeNodeType type() const;

    /// @return  Hash for this node path fragment.
    ushort hash() const;

    /// @return  User data pointer associated with this.
    void* userData() const;

    /**
     * Change the associated user data pointer.
     */
    PathTreeNode& setUserData(void* data);

    /**
     * @param flags          @ref pathComparisonFlags
     * @param candidatePath  Fragment mapped search pattern (path).
     *
     * @return  Non-zero iff the candidatePath matched this.
     *
     * @todo We need an alternative version of this whose candidate path is
     *       specified using another tree node (possibly from another PathTree).
     *       This would allow for further optimizations in the file system
     *       (among others) -ds
     */
    int comparePath(int flags, PathMap* candidatePath) const;

    /// @return  The path fragment which this node represents.
    ddstring_t const* pathFragment() const;

    /**
     * Composes and/or calculates the composed-length of the path for this node.
     *
     * @param path          If not @c NULL the composed path is written here.
     * @param length        If not @c NULL the length of the composed path is written here.
     * @param delimiter     Path is composed with fragments delimited by this character.
     *
     * @return  The composed path pointer specified with @a path, for caller's convenience.
     */
    ddstring_t* composePath(ddstring_t* path, int* length, char delimiter = '/') const;

    /// @todo FIXME: should be private:
    StringPoolId internId() const;

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif // __cplusplus

struct pathtreenode_s; // The pathtreenode instance (opaque).
typedef struct pathtreenode_s PathTreeNode;

// Number of buckets in the hash table.
#define PATHTREE_PATHHASH_SIZE          512

/// Identifier used with the search and iteration algorithms in place of a
/// hash when the caller does not wish to narrow the set of considered nodes.
#define PATHTREE_NOHASH                 PATHTREE_PATHHASH_SIZE

/**
 * @defgroup pathTreeFlags  Path Tree Flags
 */
///@{
#define PDF_ALLOW_DUPLICATE_LEAF        0x1 ///< There can be more than one leaf with a given name.
///@}

#ifdef __cplusplus
} // extern "C"

#include <QMultiHash>

namespace de {

/**
 * PathTree. Data structure for modelling a hierarchical relationship tree of
 * string + data value pairs.
 *
 * Path fragment delimiters are automatically extracted from any paths inserted
 * into the tree. Removing the delimiters both reduces the memory overhead for
 * the whole data set while also allowing their optimal dynamic replacement when
 * reconstructing the original paths. One potential use of this feature when
 * representing file path hierarchies is for "ambidextrously" recomposing paths
 * using either forward or backward slashes, irrespective of which delimiter is
 * used at path insertion time.
 *
 * Path fragment strings are "pooled" such that only one instance of a fragment
 * is included in the tree. Potentially, significantly reducing memory overhead
 * for the representing the complete hierarchy.
 */
class PathTree
{
public:
    typedef QMultiHash<ushort, PathTreeNode*> Nodes;

public:
    explicit PathTree(int flags = 0);
    virtual ~PathTree();

    /// @return  Total number of unique paths in the hierarchy.
    uint size() const;

    /**
     * Clear the tree contents, free'ing all nodes.
     */
    void clear();

    /**
     * Add a new path into the hierarchy. Duplicate paths are automatically pruned
     * however, note that their associated user data value is replaced!
     *
     * @param path          New path to be added to the hierarchy.
     * @param delimiter     Fragments of @a path are delimited by this character.
     * @param userData      User data to associate with the tail node.
     *
     * @return Tail node for the inserted path else @c NULL. For example, given the
     *         path "c:/somewhere/something" and where @a delimiter @c= '/' this is
     *         the node for the path fragment "something".
     */
    PathTreeNode* insert(char const* path, char delimiter = '/', void* userData = NULL);

    /**
     * Find a node in the directory.
     *
     * @note This method essentially amounts to "interface sugar". A convenient
     *       shorthand of the call tree:
     *
     * <pre>
     *    PathMap search;
     *    PathMap_Initialize(&search, @a flags, @a searchPath, @a delimiter);
     *    foundNode = PathTree_Search("this", &search, PathTreeNode_ComparePath);
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
    PathTreeNode* find(int flags, char const* path, char delimiter = '/');

    /**
     * Composes and/or calculates the composed-length of the path for a node.
     *
     * @param path          If not @c NULL the composed path is written here.
     * @param length        If not @c NULL the length of the composed path is written here.
     * @param delimiter     Path is composed with fragments delimited by this character.
     *
     * @return  The composed path pointer specified with @a path, for caller's convenience.
     */
    ddstring_t* composePath(PathTreeNode const& node, ddstring_t* path,
                            int* length, char delimiter = '/');

    /// @return  The path fragment which @a node represents.
    ddstring_t const* pathFragment(PathTreeNode const& node);

    /**
     * Collate all paths in the tree into a list.
     *
     * @param count         Number of visited paths is written back here.
     * @param flags         @ref pathComparisonFlags
     * @param delimiter     Fragments of the path will be delimited by this character.
     *
     * @return  The allocated list; it is the responsibility of the caller to Str_Free()
     *          each string in the list and free() the list itself.
     */
    ddstring_t* collectPaths(size_t* count, int flags, char delimiter = '/');

    /**
     * Provides access to the PathTreeNodes for efficent traversals.
     */
    Nodes const& nodes(PathTreeNodeType type) const;

    inline Nodes const& leafNodes() const {
        return nodes(PT_LEAF);
    }

    inline Nodes const& branchNodes() const {
        return nodes(PT_BRANCH);
    }

public:
    /**
     * This is a hash function. It uses the path fragment string to generate
     * a somewhat-random number between @c 0 and @c PATHTREE_PATHHASH_SIZE
     *
     * @return  The generated hash key.
     */
    static ushort hashPathFragment(char const* fragment, size_t len, char delimiter = '/');

#if _DEBUG
    static void debugPrint(PathTree& pt, char delimiter = '/');
    static void debugPrintHashDistribution(PathTree& pt);
#endif

    /// @todo FIXME: Should be private:
    ushort hashForInternId(StringPoolId internId);

private:
    struct Instance;
    Instance* d;
};

} // namespace de

extern "C" {
#endif

/**
 * C wrapper API:
 */

typedef struct pathtree_s PathTree;

PathTree* PathTree_New(void);
PathTree* PathTree_NewWithFlags(int flags);
void PathTree_Delete(PathTree* pt);

uint PathTree_Size(PathTree* pt);
void PathTree_Clear(PathTree* pt);

PathTreeNode* PathTree_Insert3(PathTree* pt, char const* path, char delimiter, void* userData);
PathTreeNode* PathTree_Insert2(PathTree* pt, char const* path, char delimiter); /*userData=NULL*/
PathTreeNode* PathTree_Insert(PathTree* pt, char const* path); /*delimiter='/'*/

/**
 * Callback function type for PathTree::Iterate
 *
 * @param node          PathTreeNode being processed.
 * @param parameters    User data passed to this.
 *
 * @return  Non-zero if iteration should stop.
 */
typedef int (*pathtree_iteratecallback_t) (PathTreeNode* node, void* parameters);

/// Const variant.
typedef int (*pathtree_iterateconstcallback_t) (PathTreeNode const* node, void* parameters);

/**
 * Callback function type for PathTree::Search
 *
 * @param node              Right-most node in path.
 * @param flags             @ref pathComparisonFlags
 * @param mappedSearchPath  Fragment mapped search path.
 * @param parameters        User data passed to this when used as a search callback.
 *
 * @return  @c true iff the directory matched this.
 */
typedef int (*pathtree_searchcallback_t) (PathTreeNode* node, int flags, PathMap* mappedSearchPath, void* parameters);

/**
 * Perform a search of the nodes in the hierarchy making a callback for each.
 * Pre-selection of nodes is determined by @a mappedSearchPath. Iteration ends when
 * all selected nodes have been visited or a callback returns non-zero.
 *
 * This method essentially amounts to "interface sugar". A manual search of the
 * hierarchy can be performed using nodes pre-selected by the caller or using
 * @see PathTree_Iterate().
 *
 * @param flags             @ref pathComparisonFlags
 * @param mappedSearchPath  Fragment mapped search path.
 * @param callback          Callback function ptr. The callback should only return
 *                          a non-zero value when the desired node has been found.
 * @param parameters        Passed to the callback.
 *
 * @return  @c 0 iff iteration completed wholly.
 */
PathTreeNode* PathTree_Search2(PathTree* pt, int flags, PathMap* mappedSearchPath, pathtree_searchcallback_t callback, void* parameters);
PathTreeNode* PathTree_Search (PathTree* pt, int flags, PathMap* mappedSearchPath, pathtree_searchcallback_t callback); /*parameters=NULL*/

PathTreeNode* PathTree_Find2(PathTree* pt, int flags, char const* path, char delimiter);
PathTreeNode* PathTree_Find(PathTree* pt, int flags, char const* path); /* delimiter='/' */

/**
 * Iterate over nodes in the hierarchy making a callback for each. Iteration ends
 * when all nodes have been visited or a callback returns non-zero.
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
int PathTree_Iterate2(PathTree* pt, int flags, PathTreeNode* parent, ushort hash, pathtree_iteratecallback_t callback, void* parameters);
int PathTree_Iterate (PathTree* pt, int flags, PathTreeNode* parent, ushort hash, pathtree_iteratecallback_t callback); /*parameters=NULL*/

int PathTree_Iterate2_Const(PathTree const* pt, int flags, PathTreeNode const* parent, ushort hash, pathtree_iterateconstcallback_t callback, void* parameters);
int PathTree_Iterate_Const (PathTree const* pt, int flags, PathTreeNode const* parent, ushort hash, pathtree_iterateconstcallback_t callback); /*parameters=NULL*/

ddstring_t* PathTree_ComposePath2(PathTree* pt, PathTreeNode const* node, ddstring_t* path, int* length, char delimiter);
ddstring_t* PathTree_ComposePath(PathTree* pt, PathTreeNode const* node, ddstring_t* path, int* length); /*delimiter='/'*/

ddstring_t const* PathTree_PathFragment(PathTree* pt, const PathTreeNode* node);

ddstring_t* PathTree_CollectPaths2(PathTree* pt, size_t* count, int flags, char delimiter);
ddstring_t* PathTree_CollectPaths(PathTree* pt, size_t* count, int flags); /*delimiter='/'*/

ushort PathTree_HashPathFragment2(char const* path, size_t len, char delimiter);
ushort PathTree_HashPathFragment(char const* path, size_t len);/*delimiter='/'*/

#if _DEBUG
void PathTree_DebugPrint(PathTree* pt, char delimiter);
void PathTree_DebugPrintHashDistribution(PathTree* pt);
#endif

PathTree* PathTreeNode_Tree(PathTreeNode const* node);
PathTreeNode* PathTreeNode_Parent(PathTreeNode const* node);
PathTreeNodeType PathTreeNode_Type(PathTreeNode const* node);
ushort PathTreeNode_Hash(PathTreeNode const* node);
int PathTreeNode_ComparePath(PathTreeNode* node, int flags, PathMap* candidatePath, void* parameters);
ddstring_t* PathTreeNode_ComposePath2(PathTreeNode const* node, ddstring_t* path, int* length, char delimiter);
ddstring_t* PathTreeNode_ComposePath(PathTreeNode const* node, ddstring_t* path, int* length); /*delimiter='/'*/
ddstring_t const* PathTreeNode_PathFragment(PathTreeNode const* node);
void* PathTreeNode_UserData(PathTreeNode const* node);
void PathTreeNode_SetUserData(PathTreeNode* node, void* data);

ddstring_t const* PathTreeNodeType_Name(PathTreeNodeType type);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_PATHTREE_H */
