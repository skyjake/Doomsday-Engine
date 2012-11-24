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
     * Path + data value pairs.
     *
     * @em Segment is the term given to a components of a hierarchical path.
     * For example, the path <pre>"c:/somewhere/something"</pre> contains three
     * path segments: <pre>[ 0: "c:", 1: "somewhere", 2: "something" ]</pre>
     *
     * @em Delimiter is the term given to the separators between segments.
     * (Such as forward slashes in an UNIX-style file path).
     *
     * Internally, segments are "pooled" such that only one instance of a
     * segment is included in the model of the whole tree. Potentially, this
     * significantly reduces the memory overhead which would otherwise be
     * necessary to represent the complete hierarchy as a set of fully composed
     * paths.
     *
     * Delimiters are not included in the hierarchy model. Not including the
     * delimiters allows for optimal dynamic replacement when recomposing the
     * original paths (also reducing the memory overhead for the whole data
     * set). One potential use for this feature when representing file path
     * hierarchies is "ambidextrously" recomposing paths with either forward or
     * backward slashes, irrespective of the delimiter used at path insertion
     * time.
     *
     * Somewhat similar to a Prefix Tree (Trie) representationally although
     * that is where the similarity ends.
     */
    class PathTree
    {
        struct Instance; // needs to be friended by Node

    public:
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
            MatchParent = 0x4,  ///< Only consider nodes whose parent matches that referenced.
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
        static String const& nodeTypeName(NodeType type);

        /**
         * Identifier used with the search and iteration algorithms in place of
         * a hash when the user does not wish to narrow the set of considered
         * nodes.
         */
        static Path::hash_type const no_hash;

#ifdef DENG2_DEBUG
        void debugPrint(QChar delimiter = '/') const;
        void debugPrintHashDistribution() const;
#endif

        /**
         * Node is the base class for all nodes of a PathTree.
         */
        class Node
        {
        private:
            Node();
            Node(PathTree& tree, NodeType type, SegmentId segmentId, Node* parent = 0,
                 void* userPointer = 0, int userValue = 0);
            virtual ~Node();

        public:
            /// @return PathTree which owns this node.
            PathTree& tree() const;

            /// @return Parent of this node else @c NULL.
            Node* parent() const;

            /// @return @c true iff this node is a leaf.
            bool isLeaf() const;

            /// @return Type of this node.
            inline NodeType type() const {
                return isLeaf()? Leaf : Branch;
            }

            /// @return Name for this node's path segment.
            String const& name() const;

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
            int comparePath(de::Path const& searchPattern, ComparisonFlags flags) const;

            /**
             * Composes the path for this node. The whole path is upwardly
             * reconstructed toward the root of the hierarchy.
             *
             * @param delimiter Names in the composed path hierarchy will be delimited
             *                  with this character. Paths to branches always include
             *                  a terminating delimiter.
             *
             * @return The composed uri.
             */
            Path composePath(QChar delimiter = '/') const;

            /**
             * Sets the user-specified custom pointer.
             */
            Node& setUserPointer(void* ptr);

            /// @return User-specified custom pointer.
            void* userPointer() const;

            /**
             * Sets the user-specified custom pointer.
             */
            Node& setUserValue(int value);

            /// @return User-specified custom value.
            int userValue() const;

            friend class PathTree;
            friend struct PathTree::Instance;

        private:
            SegmentId segmentId() const;

        private:
            struct Instance;
            Instance* d;
        };

    public:
        /// The requested entry could not be found in the hierarchy.
        DENG2_ERROR(NotFoundError);

        typedef QMultiHash<Path::hash_type, Node*> Nodes;
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
         * Delimiters in the path are completely ignored.
         *
         * @param path  New path to be added to the tree. Note that this path is
         *              NOT resolved before insertion, so any symbolics contained
         *              within will also be present in the name hierarchy.
         *
         * @return Tail node for the inserted path else @c NULL. For example, given
         *         the path @c "c:/somewhere/something" this is the node for the
         *         path segment "something".
         */
        Node* insert(Path const& path);

        /**
         * Destroy the tree's contents, free'ing all nodes.
         */
        void clear();

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
        Node const& find(Path const& path, ComparisonFlags flags) const;

        /**
         * @copydoc Node const& find(Path const& path, ComparisonFlags flags) const
         */
        Node &find(Path const &path, ComparisonFlags flags);

        /**
         * Collate all referenced paths in the hierarchy into a list.
         *
         * @param found      Set of paths that match the result.
         * @param flags      Comparison behavior flags.
         * @param delimiter  Names in the composed path hierarchy will be delimited
         *                   with this character. Paths to branches always include
         *                   a terminating delimiter.
         *
         * @return Number of paths found.
         */
        int findAllPaths(FoundPaths& found, ComparisonFlags flags = 0, QChar delimiter = '/') const;

        /**
         * Traverse the node hierarchy making a callback for visited node. Traversal
         * ends when all selected nodes have been visited or a callback returns a
         * non-zero value.
         *
         * @param flags       Path comparison flags.
         * @param parent      Used in combination with @a flags= PCF_MATCH_PARENT
         *                    to limit the traversal to only the child nodes of
         *                    this node.
         * @param hashKey     If not @c PathTree::no_hash only consider nodes whose
         *                    hashed name matches this.
         * @param callback    Callback function ptr.
         * @param parameters  Passed to the callback.
         *
         * @return  @c 0 iff iteration completed wholly.
         */
        int traverse(ComparisonFlags flags, Node* parent, Path::hash_type hashKey,
                     int (*callback) (Node& node, void* parameters), void* parameters = 0) const;

        /**
         * Provides access to the nodes for efficent traversals.
         */
        Nodes const& nodes(NodeType type) const;

        inline Nodes const& leafNodes() const {
            return nodes(Leaf);
        }

        inline Nodes const& branchNodes() const {
            return nodes(Branch);
        }

        /*
         * Methods usually only needed by Node (or derivative classes).
         */

        /// @return The path segment associated with @a segmentId.
        String const& segmentName(SegmentId segmentId) const;

        /// @return Hash associated with @a segmentId.
        Path::hash_type segmentHash(SegmentId segmentId) const;

    private:
        Instance* d;
    };

    Q_DECLARE_OPERATORS_FOR_FLAGS(PathTree::Flags)
    Q_DECLARE_OPERATORS_FOR_FLAGS(PathTree::ComparisonFlags)

} // namespace de

#endif /* LIBDENG2_PATHTREE_H */
