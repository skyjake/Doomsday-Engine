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

#include <QList>
#include <QMultiHash>
#include <de/Error>
#include <de/String>
#include <de/str.h>
#include "pathmap.h"

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

// Number of buckets in the hash table.
#define PATHTREE_PATHHASH_SIZE          512

/// Identifier used with the search and iteration algorithms in place of a
/// hash when the caller does not wish to narrow the set of considered nodes.
#define PATHTREE_NOHASH                 PATHTREE_PATHHASH_SIZE

/**
 * @defgroup pathTreeFlags  Path Tree Flags
 */
///@{
#define PATHTREE_MULTI_LEAF 0x1 ///< There can be more than one leaf with a given name.
///@}

namespace de
{
    /**
     * PathTree. Data structure for modelling a hierarchical relationship tree of
     * string + data value pairs.
     *
     * 'fragment' is the term given to a name in a hierarchical path. For example,
     * the path <pre>"c:/somewhere/something"</pre> contains three path fragments:
     * <pre>[ 0: "c:", 1: "somewhere", 2: "something" ]</pre>
     *
     * 'delimiter' is the term given to the separators between fragments. (Such as
     * forward slashes in a file path).
     *
     * Internally, fragments are "pooled" such that only one instance of a fragment
     * is included in the model of the whole tree. Potentially, this significantly
     * reduces the memory overhead which would otherwise be necessary to represent
     * the complete hierarchy as a set of fully composed paths.
     *
     * Delimiters are not included in the hierarchy model. Not including the delimiters
     * allows for optimal dynamic replacement when recomposing the original paths
     * (also reducing the memory overhead for the whole data set). One potential use
     * for this feature when representing file path hierarchies is "ambidextrously"
     * recomposing paths with either forward or backward slashes, irrespective of
     * the delimiter used at path insertion time.
     */
    class PathTree
    {
        struct Instance; // needs to be friended by Node

    public:
        /// Identifier associated with each unique path fragment.
        typedef unsigned int FragmentId;

        /// Node type identifiers.
        enum NodeType
        {
            Branch,
            Leaf
        };

        /// @return  Print-ready name for node @a type.
        static ddstring_t const* nodeTypeName(NodeType type);

        /**
         * This is a hash function. It uses the path fragment string to generate a
         * somewhat-random number between @c 0 and @c PATHTREE_PATHHASH_SIZE
         *
         * @return  The generated hash key.
         */
        static ushort hashPathFragment(char const* fragment, size_t len, char delimiter = '/');

#if _DEBUG
        static void debugPrint(PathTree& pathtree, char delimiter = '/');
        static void debugPrintHashDistribution(PathTree& pathtree);
#endif

        /**
         * Node is the base class for all nodes of a PathTree.
         */
        class Node
        {
        private:
            Node();
            Node(PathTree& tree, NodeType type, FragmentId fragmentId, Node* parent = 0,
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

            /// @return Name for this node's path fragment.
            ddstring_t const* name() const;

            /// @return Hash for this node's path fragment.
            ushort hash() const;

            /**
             * @param candidatePath  Mapped search pattern (path).
             * @param flags          @ref pathComparisonFlags
             *
             * @return Non-zero iff the candidate path matched this.
             *
             * @todo An alternative version of this whose candidate path is specified
             *       using another tree node (possibly from another PathTree), would
             *       allow for further optimizations elsewhere (in the file system
             *       for example) -ds
             */
            int comparePath(PathMap& candidatePath, int flags) const;

            /**
             * Composes the full path for this node. 'Composing' the path of a node is
             * to upwardly reconstruct the whole path toward the root of the hierarchy.
             *
             * @param delimiter Names in the composed path hierarchy will be delimited
             *                  with this character. Paths to branches always include
             *                  a terminating delimiter.
             *
             * @return The composed path.
             */
            String composePath(char delimiter = '/') const;

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
            FragmentId fragmentId() const;

        private:
            struct Instance;
            Instance* d;
        };

    public:
        /// The requested entry could not be found in the hierarchy.
        DENG2_ERROR(NotFoundError);

        typedef QMultiHash<ushort, Node*> Nodes;
        typedef QList<String> FoundPaths;

    public:
        explicit PathTree(int flags = 0);
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
         *
         * @param path          New path to be added to the tree.
         * @param delimiter     Names in the composed @a path hierarchy are delimited
         *                      with this character.
         *
         * @return Tail node for the inserted path else @c NULL. For example, given
         *         the path @c "c:/somewhere/something" and @a delimiter @c = '/'
         *         this is the node for the path fragment "something".
         */
        Node* insert(char const* path, char delimiter = '/');

        /**
         * Destroy the tree's contents, free'ing all nodes.
         */
        void clear();

        /**
         * Find a single node in the hierarchy.
         *
         * @param flags         @ref pathComparisonFlags
         * @param path          Relative or absolute path to be searched for.
         * @param delimiter     Names in the composed @a path hierarchy are delimited
         *                      with this character.
         *
         * @return Found node.
         *
         * @throws NotFoundError if the referenced node could not be found.
         */
        Node& find(int flags, char const* path, char delimiter = '/');

        /**
         * Collate all referenced paths in the hierarchy into a list.
         *
         * @param found         Set of paths that match the result.
         * @param flags         @ref pathComparisonFlags
         * @param delimiter     Names in the composed path hierarchy will be delimited
         *                      with this character. Paths to branches always include
         *                      a terminating delimiter.
         *
         * @return Number of paths found.
         */
        int findAllPaths(FoundPaths& found, int flags = 0, char delimiter = '/');

        /**
         * Iterate over nodes in the hierarchy making a callback for each. Iteration ends
         * when all nodes have been visited or a callback returns non-zero.
         *
         * @param flags         @ref pathComparisonFlags
         * @param parent        Parent node reference, used when restricting processing
         *                      to the child nodes of this node. Only used when the flag
         *                      PCF_MATCH_PARENT is set in @a flags.
         * @param hash          If not @c PATHTREE_NOHASH only consider nodes whose hashed
         *                      name matches this.
         * @param callback      Callback function ptr.
         * @param parameters    Passed to the callback.
         *
         * @return  @c 0 iff iteration completed wholly.
         */
        int iterate(int flags, Node* parent, ushort hash, int (*callback) (Node& node, void* parameters), void* parameters = 0);

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

        /// @return The path fragment associated with @a fragmentId.
        ddstring_t const* fragmentName(FragmentId fragmentId) const;

        /// @return Hash associated with @a fragmentId.
        ushort fragmentHash(FragmentId fragmentId) const;

    private:
        Instance* d;
    };

} // namespace de

#endif /* LIBDENG_PATHTREE_H */
