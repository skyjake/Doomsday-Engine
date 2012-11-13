/**
 * @file resourcenamespace.h
 *
 * Resource Namespace. @ingroup resource
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCENAMESPACE_H
#define LIBDENG_RESOURCENAMESPACE_H

#include <QMultiMap>
#include "pathtree.h"
#include "uri.h"

/**
 * @defgroup searchPathFlags  Search Path Flags
 * @ingroup flags
 */
///@{
#define SPF_NO_DESCEND                  0x1 ///< Do not decend into branches when populating paths.
///@}

namespace de
{
    /**
     * Resource Namespace.
     *
     * @ingroup resource
     */
    class ResourceNamespace
    {
    public:
        /**
         * (Search) path groupings in descending priority.
         */
        enum PathGroup
        {
            /// 'Override' paths have the highest priority. These are usually
            /// set according to user specified paths, e.g., via the command line.
            OverridePaths,

            /// 'Extra' paths are those which are determined dynamically when some
            /// runtime resources are loaded. The DED module utilizes these to add
            /// new model search paths found when parsing definition files.
            ExtraPaths,

            /// Default paths are those which are known a priori. These are usually
            /// determined at compile time and are implicit paths relative to the
            /// virtual file system.
            DefaultPaths,

            /// Fallback (i.e., last-resort) paths have the lowest priority. These
            /// are usually set according to user specified paths, e.g., via the
            /// command line.
            FallbackPaths
        };

        struct SearchPath
        {
        public:
            /**
             * @param _flags   @ref searchPathFlags
             * @param _uri     Unresolved search URI (may include symbolic names or
             *                 other symbol references). SearchPath takes ownership.
             */
            SearchPath(int _flags, Uri& _uri);

            /**
             * Construct a copy from @a other. This is a "deep copy".
             */
            SearchPath(SearchPath const& other);

            ~SearchPath();

            SearchPath& operator = (SearchPath other);

            friend void swap(SearchPath& first, SearchPath& second); // nothrow

            /// @return  @ref searchPathFlags
            int flags() const;

            SearchPath& setFlags(int flags);

            /// @return  Unresolved URI.
            Uri const& uri() const;

        private:
            /// @see searchPathFlags
            int flags_;

            /// Unresolved search URI.
            Uri* uri_;
        };

        typedef QMultiMap<PathGroup, SearchPath> SearchPaths;
        typedef QList<PathTree::Node*> ResourceList;

    public:
        explicit ResourceNamespace(String symbolicName);
        ~ResourceNamespace();

        /// @return  Symbolic name of this namespace (e.g., "Models").
        String const& name() const;

        /**
         * Rebuild this namespace by re-scanning for resources on all search
         * paths and re-populating the internal database.
         *
         * @note Any manually added resources will not be present after this.
         */
        void rebuild();

        /**
         * Reset this namespace back to it's "empty" state (i.e., no resources).
         * The search path groups are unaffected.
         */
        void clear();

        /**
         * Manually add a resource to this namespace. Duplicates are pruned
         * automatically.
         *
         * @param node          PathTree node which represents the resource.
         *
         * @return  @c true iff this namespace did not already contain the resource.
         */
        bool add(PathTree::Node& node);

        /**
         * Finds all resources in this namespace.
         *
         * @param name    If not an empty string, only consider resources whose
         *                name begins with this. Case insensitive.
         * @param found   Set of resources which match the search.
         *
         * @return  Number of found resources.
         */
        int findAll(String name, ResourceList& found);

        /**
         * Add a new search path to this namespace. Newer paths have priority
         * over previously added paths.
         *
         * @param group     Group to add this path to. @see PathGroup
         * @param path      New unresolved path to add. A copy is made.
         * @param flags     @see searchPathFlags
         *
         * @return  @c true if @a path was well-formed and subsequently added.
         */
        bool addSearchPath(PathGroup group, Uri const& path, int flags);

        /**
         * Clear search paths in @a group from this namespace.
         *
         * @param group  Search path group to be cleared.
         */
        void clearSearchPaths(PathGroup group);

        /**
         * Clear all search paths in all groups in this namespace.
         */
        void clearSearchPaths();

        /**
         * Provides access to the search paths for efficient traversals.
         */
        SearchPaths const& searchPaths() const;

#if _DEBUG
        void debugPrint() const;
#endif

    private:
        struct Instance;
        Instance* d;
    };

} // namespace de

#endif /* LIBDENG_RESOURCENAMESPACE_H */
