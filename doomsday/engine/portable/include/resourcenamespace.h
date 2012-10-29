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

// Number of entries in the hash table.
#define RESOURCENAMESPACE_HASHSIZE      512

/**
 * @defgroup searchPathFlags  Search Path Flags
 * @{
 */
#define SPF_NO_DESCEND                  0x1 ///< Do not decend into branches when populating paths.
/**@}*/

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

        typedef unsigned short NameHashKey;

        typedef NameHashKey (HashNameFunc) (ddstring_t const* name);

        struct SearchPath
        {
            int flags; /// @see searchPathFlags
            Uri* uri;

            SearchPath(int _flags, Uri const* _uri)
                : flags(_flags), uri(Uri_NewCopy(_uri))
            {}

            ~SearchPath()
            {
                Uri_Delete(uri);
            }
        };

        typedef QMultiMap<PathGroup, SearchPath> SearchPaths;
        typedef QList<PathTree::Node*> ResourceList;

    public:
        ResourceNamespace(HashNameFunc* hashNameFunc);
        ~ResourceNamespace();

        /**
         * Reset the namespace back to it's "empty" state (i.e., no known symbols).
         */
        void clear();

        /**
         * Add a new named resource into the namespace. Multiple names for a given
         * resource may coexist however duplicates are automatically pruned.
         *
         * @post Name hash may have been rebuilt.
         *
         * @param name          Name of the resource being added.
         * @param node          PathTree node which represents the resource.
         *
         * @return  @c true iff the namespace did not already contain this resource.
         */
        bool add(ddstring_t const* name, PathTree::Node& node);

        /**
         * Finds all resources in this namespace..
         *
         * @param name          If not @c NULL, only consider resources with this name.
         * @param found         Set of resources that match the result.
         *
         * @return  Number of found resources.
         */
        int findAll(ddstring_t const* name, ResourceList& found);

        /**
         * Add a new path to this namespace.
         *
         * @param group         Group to add this path to. @see SearchPathGroup
         * @param path          New unresolved path to add.
         * @param flags         @see searchPathFlags
         *
         * @return  @c true if the path is correctly formed and present in the namespace.
         */
        bool addSearchPath(PathGroup group, Uri const* path, int flags);

        /**
         * Clear search paths in @a group in this namespace.
         * @param group  Search path group to be cleared.
         */
        void clearSearchPaths(PathGroup group);

        /**
         * Clear all search paths in all groups in this namespace.
         */
        void clearSearchPaths();

        /**
         * Append to @a pathList all resolved search paths in @a group, in descending,
         * (i.e., most recent order), using @a delimiter to separate each.
         *
         * @param group         Group of paths to append.
         * @param pathList      String to receive the search path list.
         * @param delimiter     Discreet paths will be delimited by this character.
         *
         * @return  Same as @a pathList for caller convenience.
         */
        ddstring_t* listSearchPaths(PathGroup group, ddstring_t* pathList,
                                    char delimiter = ';') const;

        /**
         * Append to @a pathList all resolved search paths from all groups, firstly
         * in group order (from Override to Fallback) and in descending, (i.e., most
         * recent order), using @a delimiter to separate each.
         *
         * @param pathList      String to receive the search paths.
         * @param delimiter     Discreet paths will be delimited by this character.
         *
         * @return  Same as @a pathList for caller convenience.
         */
        ddstring_t* listSearchPaths(ddstring_t* pathList, char delimiter = ';') const
        {
            listSearchPaths(OverridePaths, pathList, delimiter);
            listSearchPaths(ExtraPaths,    pathList, delimiter);
            listSearchPaths(DefaultPaths,  pathList, delimiter);
            listSearchPaths(FallbackPaths, pathList, delimiter);
            return pathList;
        }

        /**
         * Provides access to the search paths for efficent traversals.
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
