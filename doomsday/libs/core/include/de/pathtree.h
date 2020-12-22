/** @file pathtree.h Tree of Path/data pairs.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_PATHTREE_H
#define LIBCORE_PATHTREE_H

#include "error.h"
#include "lockable.h"
#include "string.h"
#include "path.h"

#include <unordered_map>

namespace de {

/**
 * Data structure for modelling a hierarchical relationship tree of
 * Path + data value pairs. @ingroup data
 *
 * @em Segment is the term given to a components of a hierarchical path.
 * For example, the path <pre>"c:/somewhere/something"</pre> contains three
 * path segments: <pre>[ 0: "c:", 1: "somewhere", 2: "something" ]</pre>
 *
 * Segments are separated by @em separator @em characters. For instance,
 * UNIX file paths use forward slashes as separators.
 *
 * Internally, segments are "pooled" such that only one instance of a
 * segment is included in the model of the whole tree. Potentially, this
 * significantly reduces the memory overhead which would otherwise be
 * necessary to represent the complete hierarchy as a set of fully composed
 * paths.
 *
 * Separators are not included in the hierarchy model. Not including the
 * separators allows for optimal dynamic replacement when recomposing the
 * original paths (also reducing the memory overhead for the whole data
 * set). One potential use for this feature when representing file path
 * hierarchies is "ambidextrously" recomposing paths with either forward or
 * backward slashes, irrespective of the separator used at path insertion
 * time.
 *
 * @par Thread-safety
 *
 * The methods of PathTree automatically lock the tree. Access to the data in
 * the nodes is not automatically protected and is the responsibility of the
 * user.
 */
class DE_PUBLIC PathTree : public Lockable
{
    struct Impl; // needs to be friended by Node

public:
    class Node; // forward declaration

    typedef std::unordered_multimap<uint32_t, Node *> Nodes;
    typedef StringList FoundPaths;

    /**
     * Leaves and branches are stored in separate hashes.
     * Note that one can always unite the hashes (see QMultiHash).
     */
    struct DE_PUBLIC NodeHash {
        Nodes leaves;
        Nodes branches;
    };

    /**
     * Flags that affect the properties of the tree.
     */
    enum Flag
    {
        MultiLeaf = 0x1     ///< There can be more than one leaf with a given name.
    };

    /**
     * Flags used to alter the behavior of path comparisons.
     */
    enum ComparisonFlag
    {
        NoBranch    = 0x1,  ///< Do not consider branches as possible candidates.
        NoLeaf      = 0x2,  ///< Do not consider leaves as possible candidates.
        MatchParent = 0x4,  ///< Only consider nodes whose parent matches the provided reference node.
        MatchFull   = 0x8,  /**< Whole path must match completely (i.e., path begins
                                 from the same root point) otherwise allow partial
                                 (i.e., relative) matches. */
        RelinquishMatching = 0x10 /**< Matching node's ownership is relinquished; the node is
                                       removed from the tree. */
    };
    using ComparisonFlags = Flags;

    /// Identifier associated with each unique path segment.
    //typedef duint32 SegmentId;

    /// Node type identifiers.
    enum NodeType
    {
        Branch,
        Leaf
    };

    /// @return  Print-ready name for node @a type.
    static const String &nodeTypeName(NodeType type);

    /**
     * Identifier used with the search and iteration algorithms in place of
     * a hash when the user does not wish to narrow the set of considered
     * nodes.
     */
//    static Path::hash_type const no_hash;

#ifdef DE_DEBUG
    void debugPrint(Char separator = '/') const;
    void debugPrintHashDistribution() const;
#endif

    /**
     * Parameters passed to the Node constructor. Using this makes it more
     * convenient to write Node-derived classes, as one doesn't have to
     * spell out all the arguments provided by PathTree.
     *
     * Not public as only PathTree itself constructs instances, others can
     * treat this as an opaque type.
     */
    struct NodeArgs {
        PathTree &          tree;
        NodeType            type;
        LowercaseHashString segment;
        Node *              parent;

        NodeArgs(PathTree &pt, NodeType nt, const LowercaseHashString &segment, /*SegmentId id, */Node *p = 0)
            : tree(pt), type(nt), segment(segment), /*segmentId(id), */parent(p) {}
    };

    /**
     * Base class for all nodes of a PathTree. @ingroup data
     */
    class DE_PUBLIC Node
    {
    public:
        typedef PathTree::NodeHash Children;

    protected:
        Node(const NodeArgs &args);

        virtual ~Node();

    public:
        /// @return PathTree which owns this node.
        PathTree &tree() const;

        /// @return Parent of this node. For nodes at the root level,
        /// the parent is the tree's special root node.
        Node &parent() const;

        /// Returns the children of a branch node. Note that leaf nodes
        /// have no children -- calling this for leaf nodes is not allowed.
        const Children &children() const;

        /**
         * Returns the children of a branch node. Note that leaf nodes
         * have no children -- calling this for leaf nodes is not allowed.
         *
         * @param type  Type of hash to return (leaves or branches).
         *
         * @return Hash of nodes.
         */
        const Nodes &childNodes(NodeType type) const;

        /// Determines if the node is at the root level of the tree
        /// (no other node is its parent).
        bool isAtRootLevel() const;

        /// @return @c true iff this node is a leaf.
        bool isLeaf() const;

        /// @return @c true iff this node is a branch.
        inline bool isBranch() const {
            return !isLeaf();
        }

        /// @return Type of this node.
        inline NodeType type() const {
            return isLeaf()? Leaf : Branch;
        }

        /// @return Name for this node's path segment.
        const String &name() const;

        /// @return Hash for this node's path segment.
//        Path::hash_type hash() const;

        const LowercaseHashString &key() const;

        /**
         * @param searchPattern  Mapped search pattern (path).
         * @param flags          Path comparison flags.
         *
         * @return Zero iff the candidate path matched this.
         *
         * @todo An alternative version of this whose candidate path is specified
         *       using another tree node (possibly from another PathTree), would
         *       allow for further optimizations elsewhere (in the file system
         *       for example) -ds
         *
         * @todo This logic should be encapsulated in de::Path or de::Path::Segment. -ds
         */
        int comparePath(const de::Path &searchPattern, ComparisonFlags flags) const;

        /**
         * Composes the path for this node. The whole path is upwardly
         * reconstructed toward the root of the hierarchy -- you should
         * consider the performance aspects, if calling this method very often.
         *
         * @param sep  Segments in the composed path hierarchy are separatered
         *             with this character. Paths to branches always include
         *             a terminating separator.
         *
         * @return The composed path.
         */
        Path path(Char sep = '/') const;

        DE_CAST_METHODS()

        friend class PathTree;
        friend struct PathTree::Impl;

    protected:
//        SegmentId segmentId() const;
        void addChild(Node &node);
        void removeChild(Node &node);
        Nodes &childNodes(NodeType type);

    private:
        DE_PRIVATE(d)
    };

public:
    /// The requested entry could not be found in the hierarchy.
    DE_ERROR(NotFoundError);

public:
    explicit PathTree(Flags flags = 0);

    virtual ~PathTree();

    /// @return  @c true iff there are no paths in the hierarchy.
    /// Same as <code>size() == 0</code>
    bool empty() const;

    inline bool isEmpty() const { return empty(); }

    /// Returns the flags that affect the properties of the tree.
    Flags flags() const;

    /// Total number of unique paths in the hierarchy.
    int size() const;

    /// Total number of unique paths in the hierarchy. Same as @ref size().
    inline int count() const {
        return size();
    }

    /**
     * Add a new path into the hierarchy. Duplicates are automatically pruned.
     * Separators in the path are completely ignored.
     *
     * @param path  New path to be added to the tree. Note that this path is
     *              NOT resolved before insertion, so any symbolics contained
     *              within will also be present in the name hierarchy.
     *
     * @return Tail node for the inserted path. For example, given
     *         the path @c "c:/somewhere/something" this is the node for the
     *         path segment "something".
     */
    Node &insert(const Path &path);

    /**
     * Removes matching nodes from the tree.
     *
     * @param path   Path to remove.
     * @param flags  Search behavior.
     *
     * @return @c true, if one or more nodes were removed; otherwise, @c false.
     */
    bool remove(const Path &path, ComparisonFlags flags = 0);

    /**
     * Destroy the tree's contents, freeing all nodes.
     */
    void clear();

    /**
     * Determines if a path exists in the tree.
     *
     * @param path   Path to look for.
     * @param flags  Search behavior.
     *
     * @return @c true, if the node exists; otherwise @c false.
     */
    bool has(const Path &path, ComparisonFlags flags = 0) const;

    /**
     * Find a single node in the hierarchy.
     *
     * @param path   Relative or absolute path to be searched for. Note that
     *               this path is NOT resolved before searching. This means
     *               that any symbolics contained within must also be present
     *               in the tree's name hierarchy.
     * @param flags  Comparison behavior flags.
     *
     * @return Found node.
     */
    const Node &find(const Path &path, ComparisonFlags flags) const;

    const Node *tryFind(const Path &path, ComparisonFlags flags) const;

    /**
     * @copydoc find()
     */
    Node &find(const Path &path, ComparisonFlags flags);

    Node *tryFind(const Path &path, ComparisonFlags flags);

    /**
     * Collate all referenced paths in the hierarchy into a list.
     *
     * @param found  Set of paths that match the result.
     * @param flags  Comparison behavior flags.
     * @param sep    Segments in the composed path will be separated
     *               with this character. Paths to branches always include
     *               a terminating separator.
     *
     * @return Number of paths found.
     */
    int findAllPaths(FoundPaths &found, ComparisonFlags flags = 0, Char sep = '/') const;

    /**
     * Traverse the node hierarchy making a callback for visited node. Traversal
     * ends when all selected nodes have been visited or a callback returns a
     * non-zero value.
     *
     * @param flags       Path comparison flags.
     * @param parent      Used in combination with ComparisonFlag::MatchParent
     *                    to limit the traversal to only the child nodes of
     *                    this node.
     * @param callback    Callback function ptr.
     * @param parameters  Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int traverse(ComparisonFlags flags, const Node *parent,
                 int (*callback) (Node &node, void *parameters), void *parameters = 0) const;

    /**
     * Provides access to the nodes for efficent traversals.
     *
     * @param type  Type of nodes to return: Leaf or Branch.
     *
     * @return Collection of nodes.
     * @see PathTreeIterator
     */
    const Nodes &nodes(NodeType type) const;

    /**
     * Provides access to the leaf nodes for efficent traversals.
     * @return Collection of nodes.
     * @see PathTreeIterator
     */
    inline const Nodes &leafNodes() const {
        return nodes(Leaf);
    }

    /**
     * Provides access to the branch nodes for efficent traversals.
     * @return Collection of nodes.
     * @see PathTreeIterator
     */
    inline const Nodes &branchNodes() const {
        return nodes(Branch);
    }

    /*
     * Methods usually only needed by Node (or derivative classes).
     */

//    /// @return The path segment associated with @a segmentId.
//    const String &segmentName(SegmentId segmentId) const;

//    /// @return Hash associated with @a segmentId.
//    Path::hash_type segmentHash(SegmentId segmentId) const;

    const Node &rootBranch() const;

protected:
    /**
     * Construct a new Node instance. Derived classes can override this to
     * construct specialized nodes.
     *
     * @param args  Parameters for initializing the node.
     *
     * @return New node. Caller gets ownership.
     */
    virtual Node *newNode(const NodeArgs &args);

private:
    Impl *d;
};

/**
 * Decode and then lexicographically compare the two manifest paths,
 * returning @c true if @a is less than @a b.
 */
template <typename PathTreeNodeType>
inline bool comparePathTreeNodePathsAscending(const PathTreeNodeType *a, const PathTreeNodeType *b)
{
    return String::fromPercentEncoding(a->path().toString()).compareWithoutCase(
               String::fromPercentEncoding(b->path().toString())) < 0;
}

/**
 * Iterator template for PathTree nodes. Can be used to iterate any set of
 * Nodes returned by a PathTree (PathTree::nodes(), PathTree::leafNodes(),
 * PathTree::branchNodes()). @ingroup data
 *
 * Example of using the iterator:
 * @code
 *  PathTreeIterator<MyTree> iter(myTree.leafNodes());
 *  while (iter.hasNext()) {
 *      MyTree::Node &node = iter.next();
 *      // ...
 *  }
 * @endcode
 *
 * @note Follows the Qt iterator conventions.
 */
template <typename TreeType>
class PathTreeIterator
{
public:
    PathTreeIterator(const PathTree::Nodes &nodes) : _nodes(nodes) {
        _next = _iter = _nodes.begin();
        if (_next != _nodes.end()) ++_next;
        _current = _nodes.end();
    }

    inline bool hasNext() const {
        return _iter != _nodes.end();
    }

    /**
     * Advances the iterator over one node.
     *
     * @return The node that the iterator jumped over while advancing.
     */
    inline typename TreeType::Node &next() {
        _current = _iter;
        typename TreeType::Node &val = value();
        _iter = _next;
        if (_next != _nodes.end()) ++_next;
        return val;
    }

    uint32_t key() const {
        DE_ASSERT(_current != _nodes.end());
        return _current->first;
    }

    typename TreeType::Node &value() const {
        DE_ASSERT(_current != _nodes.end());
        return *static_cast<typename TreeType::Node *>(_current->second);
    }

private:
    const PathTree::Nodes &_nodes;
    PathTree::Nodes::const_iterator _iter, _next, _current;
};

/**
 * Utility template for specialized PathTree classes. @ingroup data
 */
template <typename Type>
class PathTreeT : public PathTree
{
public:
    typedef Type Node; // shadow PathTree::Node
    typedef std::unordered_multimap<uint32_t, Type *> Nodes;
    typedef List<Type *> FoundNodes;

public:
    explicit PathTreeT(Flags flags = 0) : PathTree(flags) {}

    inline Type &insert(const Path &path) {
        return static_cast<Type &>(PathTree::insert(path));
    }

    inline const Type &find(const Path &path, ComparisonFlags flags) const {
        return static_cast<const Type &>(PathTree::find(path, flags));
    }

    inline Type &find(const Path &path, ComparisonFlags flags) {
        return static_cast<Type &>(PathTree::find(path, flags));
    }

    inline const Type *tryFind(const Path &path, ComparisonFlags flags) const {
        return static_cast<const Type *>(PathTree::tryFind(path, flags));
    }

    inline Type *tryFind(const Path &path, ComparisonFlags flags) {
        return static_cast<Type *>(PathTree::tryFind(path, flags));
    }

    inline int findAll(FoundNodes &found, bool (*predicate)(const Type &node, void *context),
                       void *context = nullptr) const {
        int numFoundSoFar = found.size();
        PathTreeIterator<PathTreeT> iter(leafNodes());
        while (iter.hasNext())
        {
            Type &node = iter.next();
            if (predicate(node, context))
            {
                found << &node;
            }
        }
        return found.size() - numFoundSoFar;
    }

    inline int traverse(ComparisonFlags flags, const Type *parent,
                        int (*callback) (Type &node, void *context), void *context = nullptr) const {
        return PathTree::traverse(flags, parent,
                                  function_cast<int (*)(PathTree::Node &, void *)>(callback),
                                  context);
    }

protected:
    PathTree::Node *newNode(const NodeArgs &args) {
        return new Type(args);
    }
};

/**
 * PathTree node with a custom integer value and a void pointer. @ingroup data
 */
class DE_PUBLIC UserDataNode : public PathTree::Node
{
public:
    UserDataNode(const PathTree::NodeArgs &args, void *userPointer = 0, int userValue = 0);

    /**
     * Sets the user-specified custom pointer.
     *
     * @param ptr  Pointer to user's data. Ownership not transferred.
     *
     * @return Reference to this node.
     */
    UserDataNode &setUserPointer(void *ptr);

    /// @return User-specified custom pointer.
    void *userPointer() const;

    /**
     * Sets the user-specified custom value.
     *
     * @return Reference to this node.
     */
    UserDataNode &setUserValue(int value);

    /// @return User-specified custom value.
    int userValue() const;

private:
    /// User-specified data pointer associated with this node.
    void *_pointer;

    /// User-specified value associated with this node.
    int _value;
};

typedef PathTreeT<UserDataNode> UserDataPathTree;

} // namespace de

#endif /* LIBCORE_PATHTREE_H */
