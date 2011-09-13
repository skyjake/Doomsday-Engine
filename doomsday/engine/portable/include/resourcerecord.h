/**\file resourcerecord.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010-2011 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RESOURCERECORD_H
#define LIBDENG_RESOURCERECORD_H

#include "uri.h"
#include "dd_string.h"

/**
 * Resource Record.  Used to record high-level metadata for a known resource.
 *
 * @ingroup core
 */
typedef struct resourcerecord_s {
    /// Class of resource.
    resourceclass_t _rclass;

    /// @see resourceFlags.
    int _rflags;

    /// Array of known potential names from lowest precedence to highest.
    int _namesCount;
    ddstring_t** _names;

    /// Vector of resource identifier keys (e.g., file or lump names), used for identification purposes.
    ddstring_t** _identityKeys;

    /// Paths to use when attempting to locate this resource.
    Uri** _searchPaths;

    /// Id+1 of the search path used to locate this resource (in _searchPaths) if found. Set during resource location.
    uint _searchPathUsed;

    /// Fully resolved absolute path to the located resource if found. Set during resource location.
    ddstring_t _foundPath;
} resourcerecord_t;

resourcerecord_t* ResourceRecord_NewWithName(resourceclass_t rclass, int rflags, const ddstring_t* name);
resourcerecord_t* ResourceRecord_New(resourceclass_t rclass, int rflags);
void ResourceRecord_Delete(resourcerecord_t* rec);

/**
 * Add a new name to the list of known names for this resource.
 *
 * @param name          New name for this resource. Newer names have precedence.
 */
void ResourceRecord_AddName(resourcerecord_t* rec, const ddstring_t* name);

/**
 * Add a new subrecord identity key to the list for this resource.
 *
 * @param identityKey   New identity key (e.g., a lump/file name) to add to this resource.
 */
void ResourceRecord_AddIdentityKey(resourcerecord_t* rec, const ddstring_t* identityKey);

/**
 * Attempt to resolve a path to this resource.
 *
 * @return  Path to a known resource which meets the specification of this record.
 */
const ddstring_t* ResourceRecord_ResolvedPath(resourcerecord_t* rec, boolean canLocate);

void ResourceRecord_Print(resourcerecord_t* rec, boolean printStatus);

/**
 * @return  String list of paths separated (and terminated) with semicolons ';'.
 */
ddstring_t* ResourceRecord_SearchPathsAsStringList(resourcerecord_t* rec);

/**
 * Accessor methods.
 */

/// @return  ResourceClass associated with this resource.
resourceclass_t ResourceRecord_ResourceClass(resourcerecord_t* rec);

/// @return  ResourceFlags for this resource.
int ResourceRecord_ResourceFlags(resourcerecord_t* rec);

/// @return  Array of IdentityKey(s) associated with subrecords of this resource.
ddstring_t* const* ResourceRecord_IdentityKeys(resourcerecord_t* rec);

Uri* const* ResourceRecord_SearchPaths(resourcerecord_t* rec);

#endif /* LIBDENG_RESOURCERECORD_H */
