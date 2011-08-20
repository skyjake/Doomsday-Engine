/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_SYSTEM_RESOURCENAMESPACE_H
#define LIBDENG_SYSTEM_RESOURCENAMESPACE_H

#include "dd_string.h"
#include "uri.h"

typedef struct resourcenamespace_namehash_node_s {
    struct resourcenamespace_namehash_node_s* next;
    void* data;
} resourcenamespace_namehash_node_t;

/**
 * Name search hash.
 */
// Number of entries in the hash table.
#define RESOURCENAMESPACE_HASHSIZE 512
typedef unsigned short resourcenamespace_namehash_key_t;

typedef struct {
    resourcenamespace_namehash_node_t* first;
    resourcenamespace_namehash_node_t* last;
} resourcenamespace_hashentry_t;
typedef resourcenamespace_hashentry_t resourcenamespace_namehash_t[RESOURCENAMESPACE_HASHSIZE];

typedef enum {
    SPG_OVERRIDE = 0, // Override paths
    SPG_EXTRA, // Extra/runtime paths
    SPG_DEFAULT = 0, // Default paths
    SPG_FALLBACK, // Last-resort paths
    SEARCHPATHGROUP_COUNT
} resourcenamespace_searchpathgroup_t;

#define VALID_RESOURCENAMESPACE_SEARCHPATHGROUP(g) ((g) >= SPG_DEFAULT && (g) < SEARCHPATHGROUP_COUNT)

/**
 * @defGroup ResourceNamespaceFlags Resource Namespace Flags
 * @ingroup core.
 */
/*@{*/
#define RNF_USE_VMAP            0x01 // Map resources in packages.
#define RNF_IS_DIRTY            0x80 // Filehash needs to be (re)built (avoid allocating an empty name hash).
/*@}*/

/**
 * Resource Namespace.
 *
 * @ingroup core
 */
#define RESOURCENAMESPACE_MINNAMELENGTH URI_MINSCHEMELENGTH
typedef struct resourcenamespace_s {
    /// Unique symbolic name of this namespace (e.g., "Models").
    /// Must be at least @c RESOURCENAMESPACE_MINNAMELENGTH characters long.
    ddstring_t _name;

    /// @see ResourceNamespaceFlags
    byte _flags;

    /// Sets of search paths known by this namespace.
    /// Each set is in order of greatest-importance, right to left.
    Uri** _searchPaths[SEARCHPATHGROUP_COUNT];
    uint _searchPathsCount[SEARCHPATHGROUP_COUNT];

    /// Path hash table.
    filedirectory_t* _directory;
    ddstring_t* (*_composeHashName) (const ddstring_t* path);
    resourcenamespace_namehash_key_t (*_hashName) (const ddstring_t* name);
    resourcenamespace_namehash_t _pathHash;
} resourcenamespace_t;

resourcenamespace_t* ResourceNamespace_New(const char* name,
    filedirectory_t* directory, ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name));
resourcenamespace_t* ResourceNamespace_New2(const char* name,
    filedirectory_t* directory, ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name), byte flags);

void ResourceNamespace_Delete(resourcenamespace_t* rn);

/**
 * Reset the namespace back to it's "empty" state (i.e., no known symbols).
 */
void ResourceNamespace_Reset(resourcenamespace_t* rnamespace);

/**
 * Add a new path to this namespace.
 *
 * @param path  New unresolved path to add.
 * @param group  Group to add this path to. @see resourcenamespace_searchpathgroup_t
 * @return  @c true if the path is correctly formed and present in the namespace.
 */
boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rn, const Uri* path,
    resourcenamespace_searchpathgroup_t group);

/**
 * Clear search paths in this namespace.
 * @param group  Search path group to be cleared.
 */
void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rn, resourcenamespace_searchpathgroup_t group);

/**
 * Find a path to a named resource in the namespace.
 *
 * \post Name hash may have been rebuilt.
 *
 * @param searchPath    Relative or absolute path.
 * @param foundPath     If not @c NULL and a path is found, it is written back here.
 *
 * @return  Ptr to the name hash to use when searching.
 */
boolean ResourceNamespace_Find2(resourcenamespace_t* rnamespace, const ddstring_t* searchPath, ddstring_t* foundPath);
boolean ResourceNamespace_Find(resourcenamespace_t* rnamespace, const ddstring_t* searchPath);

/**
 * Apply mapping for this namespace to the specified path (if enabled).
 *
 * This mapping will translate directives and symbolic identifiers into their default paths,
 * which themselves are determined using the current GameInfo.
 *
 *  e.g.: "Models:my/cool/model.dmd" -> "}data/<GameInfo::IdentityKey>/models/my/cool/model.dmd"
 *
 * @param path          Ptr to the path to be mapped.
 * @return  @c true iff mapping was applied to the path.
 */
boolean ResourceNamespace_MapPath(resourcenamespace_t* rnamespace, ddstring_t* path);

/**
 * Accessor methods.
 */
/// @return  Ptr to a string containing the symbolic name.
const ddstring_t* ResourceNamespace_Name(const resourcenamespace_t* rnamespace);

/// @return  Ptr to the path directory used with this namespace.
filedirectory_t* ResourceNamespace_Directory(const resourcenamespace_t* rnamespace);

#endif /* LIBDENG_SYSTEM_RESOURCENAMESPACE_H */
