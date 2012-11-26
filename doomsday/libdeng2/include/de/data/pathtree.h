/** @file pathtree.h Tree of Path/data pairs.
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

#ifndef LIBDENG2_PATHTREE_H
#define LIBDENG2_PATHTREE_H

#include <de/Error>
#include <de/String>
#include <de/Path>

#include <QList>
#include <QMultiHash>

namespace de
{

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
 * Somewhat similar to a Prefix Tree (Trie) representationally although
 * that is where the similarity ends.
 */
class DENG2_PUBLIC PathTree
{
    struct Instance; // needs to be friended by Node

public:
    class Node; // forward declaration

    /**
     * Flags that affect the properties of the tree.
     */
    enum Flag
    {
        MultiLeaf = 0x1     ///< There can be more than one leaf with a given name.
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /**
     * Flags used to alter the behavior of path comparisons.
     */
    enum ComparisonFlag
    {
        NoBranch    = 0x1,  ///< Do not consider branches as possible candidates.
        NoLeaf      = 0x2,  ///< Do not consider leaves as possible candidates.
        MatchParent = 0x4,  ///< Only consider nodes whose parent matches the provided reference node.
        MatchFull   = 0x8   /**< Whole path must match completely (i.e., path begins
                                 from the same root point) otherwise allow partial
                                 (i.e., relative) matches. */
    };
    Q_DECLARE_FLAGS(ComparisonFlags, ComparisonFlag)

    /// Identifier associated with each unique path segment.
    typedef duint32 SegmentId;

    /// Node type identifiers.
    enum NodeType
    {
        Branch,
        Leaf
    };

    /// @return  Print-ready name for node @a type.
    static String const &nodeTypeName(NodeType type);

    /**
     * Identifier used with the search and iteration algorithms in place of
     * a hash when the user does not wish to narrow the set of considered
     * nodes.
     */
    static Path::hash_type const no_hash;

#ifdef DENG2_DEBUG
    void debugPrint(QChar separator = '/') const;
    void debugPrintHashDistribution() const;
#endif

    /**
     * Parameters passed to the Node constructor. Using this makes it more
     * convenient to write Node-derived classes, as one doesn't have to
     * spell out all the arguments provided by PathTree.
     *
     * Not public as only PathTree itself constructs instances, others can
     * threat this as an opaque type.
     */
    struct NodeArgs
    {
        PathTree &tree;
        NodeType type;
        SegmentId segmentId;
        Node *parent;

        NodeArgs(PathTree &pt, NodeType nt, SegmentId id, Node *p = 0)
            : tree(pt), type(nt), segmentId(id), parent(p) {}
    };

    /**
     * Base class for all nodes of a PathTree. @ingroup data
     */
    class DENG2_PUBLIC Node
    {
    protected:
        Node(NodeArgs const &args);

        virtual ~Node();

    public:
        /// @return PathTree which owns this node.
        PathTree &tree() const;

        /// @return Parent of this node else @c NULL.
        Node *parent() const;

        /// @return @c true iff this node is a leaf.
        bool isLeaf() const;

        /// @return Type of this node.
        inline NodeType type() const {
            return isLeaf()? Leaf : Branch;
        }

        /// @return Name for this node's path segment.
        String const &name() const;

        /// @return Hash for this node's path segment.
        Path::hash_type hash() const;

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
        int comparePath(de::Path const &searchPattern, ComparisonFlags flags) const;

        /**
         * Composes the path for this node. The whole path is upwardly
         * reconstructed toward the root of the hierarchy.
         *
         * @param sep  Segments in the composed path hierarchy are separatered
         *             with this character. Paths to branches always include
         *             a terminating separator.
         *
         * @return The composed path.
         */
        Path composePath(QChar sep = '/') const;

        friend class PathTree;
        friend struct PathTree::Instance;

    protected:
        SegmentId segmentId() const;

    private:
        struct Instance;
        Instance *d;
    };

public:
    /// The requested entry could not be found in the hierarchy.
    DENG2_ERROR(NotFoundError);

    typedef QMultiHash<Path::hash_type, Node *> Nodes;
    typedef QList<String> FoundPaths;

public:
    explicit PathTree(Flags flags = 0);

    virtual ~PathTree();

    /// @return  @c true iff there are no paths in the hierarchy. Same as @c size() == 0
    bool empty() const;

    /// @return  Total number of unique paths in the hierarchy.
    int size() const;

    /// @return  Total number of unique paths in the hierarchy. Same as @ref size().
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
     * @return Tail node for the inserted path else @c NULL. For example, given
     *         the path @c "c:/somewhere/something" this is the node for the
     *         path segment "something".
     */
    Node *insert(Path const &path);

    /**
     * Removes matching nodes from the tree.
     *
     * @param path   Path to remove.
     * @param flags  Search behavior.
     *
     * @return @c true, if one or more nodes were removed; otherwise, @c false.
     */
    bool remove(Path const &path, ComparisonFlags flags = 0);

    /**
     * Destroy the tree's contents, free'ing all nodes.
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
    bool has(Path const &path, ComparisonFlags flags = 0);

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
    Node const &find(Path const &path, ComparisonFlags flags) const;

    /**
     * @copydoc find()
     */
    Node &find(Path const &path, ComparisonFlags flags);

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
    int findAllPaths(FoundPaths &found, ComparisonFlags flags = 0, QChar sep = '/') const;

    /**
     * Traverse the node hierarchy making a callback for visited node. Traversal
     * ends when all selected nodes have been visited or a callback returns a
     * non-zero value.
     *
     * @param flags       Path comparison flags.
     * @param parent      Used in combination with ComparisonFlag::MatchParent
     *                    to limit the traversal to only the child nodes of
     *                    this node.
     * @param hashKey     If not @c PathTree::no_hash only consider nodes whose
     *                    hashed name matches this.
     * @param callback    Callback function ptr.
     * @param parameters  Passed to the callback.
     *
     * @return  @c 0 iff iteration completed wholly.
     */
    int traverse(ComparisonFlags flags, Node const *parent, Path::hash_type hashKey,
                 int (*callback) (Node &node, void *parameters), void *parameters = 0) const;

    /**
     * Provides access to the nodes for efficent traversals.
     *
     * @param type  Type of nodes to return: Leaf or Branch.
     *
     * @return Collection of nodes.
     * @see PathTreeIterator
     */
    Nodes const &nodes(NodeType type) const;

    /**
     * Provides access to the leaf nodes for efficent traversals.
     * @return Collection of nodes.
     * @see PathTreeIterator
     */
    inline Nodes const &leafNodes() const {
        return nodes(Leaf);
    }

    /**
     * Provides access to the branch nodes for efficent traversals.
     * @return Collection of nodes.
     * @see PathTreeIterator
     */
    inline Nodes const &branchNodes() const {
        return nodes(Branch);
    }

    /*
     * Methods usually only needed by Node (or derivative classes).
     */

    /// @return The path segment associated with @a segmentId.
    String const &segmentName(SegmentId segmentId) const;

    /// @return Hash associated with @a segmentId.
    Path::hash_type segmentHash(SegmentId segmentId) const;

protected:
    /**
     * Construct a new Node instance. Derived classes can override this to
     * construct specialized nodes.
     *
     * @param args  Parameters for initializing the node.
     *
     * @return New node. Caller gets ownership.
     */
    virtual Node *newNode(NodeArgs const &args);

private:
    Instance *d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PathTree::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(PathTree::ComparisonFlags)

/**
 * Utility template for specialized PathTree classes. @ingroup data
 */
template <typename Type>
class PathTreeT : public PathTree
{
public:
    typedef Type Node; // shadow PathTree::Node
    typedef QMultiHash<Path::hash_type, Type *> Nodes;

public:
    explicit PathTreeT(Flags flags = 0) : PathTree(flags) {}

    inline Type *insert(Path const &path) {
        return static_cast<Type *>(PathTree::insert(path));
    }

    inline Type const &find(Path const &path, ComparisonFlags flags) const {
        return static_cast<Type const &>(PathTree::find(path, flags));
    }

    inline Type &find(Path const &path, ComparisonFlags flags) {
        return static_cast<Type &>(PathTree::find(path, flags));
    }

    inline int traverse(ComparisonFlags flags, Type const *parent, Path::hash_type hashKey,
                        int (*callback) (Type &node, void *parameters), void *parameters = 0) const {
        return PathTree::traverse(flags, parent, hashKey,
                                  reinterpret_cast<int (*)(PathTree::Node &, void *)>(callback),
                                  parameters);
    }

protected:
    PathTree::Node *newNode(NodeArgs const &args) {
        return new Type(args);
    }
};

/**
 * Iterator template for PathTree nodes. Can be used to iterate any set of
 * Nodes returned by a PathTree (PathTree::nodes(), PathTree::leafNodes(),
 * PathTree::branchNodes()). @ingroup data
 *
 * Example of using the iterator:
 * @code
 *  PathTreeIterator<MyTree> iter(myTree.leafNodes());
 *  while(iter.hasNext()) {
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
    PathTreeIterator(PathTree::Nodes const &nodes) : _nodes(nodes) {
        _next = _iter = _nodes.begin();
        if(_next != _nodes.end()) ++_next;
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
        if(_next != _nodes.end()) ++_next;
        return val;
    }

    Path::hash_type key() const {
        DENG2_ASSERT(_current != _nodes.end());
        return _current.key();
    }

    typename TreeType::Node &value() const {
        DENG2_ASSERT(_current != _nodes.end());
        return *static_cast<typename TreeType::Node *>(_current.value());
    }

private:
    PathTree::Nodes const &_nodes;
    PathTree::Nodes::const_iterator _iter, _next, _current;
};

/**
 * PathTree node with a custom integer value and a void pointer. @ingroup data
 */
class DENG2_PUBLIC UserDataNode : public PathTree::Node
{
public:
    UserDataNode(PathTree::NodeArgs const &args, void *userPointer = 0, int userValue = 0);

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

#endif /* LIBDENG2_PATHTREE_H */
