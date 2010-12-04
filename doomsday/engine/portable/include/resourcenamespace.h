/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#include "m_string.h"

struct filehash_s;

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
typedef struct resourcenamespace_s {
    /// Unique symbolic name of this namespace (e.g., "Models").
    const char* _name;

    /// @see ResourceNamespaceFlags
    byte _flags;

    /// Command line options for overriding resource paths explicitly. Flag2 Takes precendence.
    const char* _overrideFlag, *_overrideFlag2;

    /// Resource search order (in order of greatest-importance, right to left) seperated by semicolon (e.g., "path1;path2;").
    const char* _searchPaths;

    /// Set of "extra" search paths (in order of greatest-importance, right to left) seperated by semicolon (e.g., "path1;path2;").
    ddstring_t _extraSearchPaths;

    struct filehash_s* _fileHash;
} resourcenamespace_t;

/**
 * Reset the namespace back to it's "empty" state (i.e., no known symbols).
 */
void ResourceNamespace_Reset(resourcenamespace_t* rnamespace);

/**
 * Add a new raw path to the list of "extra" search paths in this namespace.
 */
boolean ResourceNamespace_AddSearchPath(resourcenamespace_t* rnamespace, const char* newPath, boolean append);

/**
 * Clear "extra" resource search paths in this namespace.
 */
void ResourceNamespace_ClearSearchPaths(resourcenamespace_t* rnamespace);

/**
 * \post Name hash may have been rebuilt.
 * @return              Ptr to the name hash to use when searching.
 */
struct filehash_s* ResourceNamespace_Hash(resourcenamespace_t* rnamespace);

/**
 * Apply mapping for this namespace to the specified path (if enabled).
 *
 * This mapping will translate tokens like "<rnamespace::name>:" into their default paths,
 * which themselves are determined using the current GameInfo.
 *
 *  e.g.: "Models:my\\cool\\model.dmd" > "}data\\<gameinfo::identitykey>\\models\\my\\cool\\model.dmd"
 *
 * @param path          Ptr to the path to be mapped.
 * @return              @c true iff mapping was applied to the path.
 */
boolean ResourceNamespace_MapPath(resourcenamespace_t* rnamespace, ddstring_t* path);

/**
 * Accessor methods.
 */
/// @return             Ptr to a cstring containing the symbolic name.
const char* ResourceNamespace_Name(const resourcenamespace_t* rnamespace);

/// @return             Ptr to a string containing the list of "extra" (raw) search paths.
const ddstring_t* ResourceNamespace_ExtraSearchPaths(resourcenamespace_t* rnamespace);

#endif /* LIBDENG_SYSTEM_RESOURCENAMESPACE_H */
