/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FILESYS_RESOURCE_LOCATOR_H
#define LIBDENG_FILESYS_RESOURCE_LOCATOR_H

struct resourcenamespace_s;

/**
 * Unique identifier associated with resource namespaces managed by the resource locator.
 *
 * @ingroup fs
 * @see ResourceNamespace
 */
typedef uint resourcenamespaceid_t;

/**
 * \post Initial/default search paths registered, namespaces initialized and queries may begin.
 */
void F_InitResourceLocator(void);

/**
 * \post All resource namespaces are emptied and search paths cleared. Queries no longer possible.
 */
void F_ShutdownResourceLocator(void);

/**
 * @return              @c true iff the value can be interpreted as a valid id.
 */
boolean F_IsValidResourceNamespaceId(int val);

/**
 * Given an id return the associated namespace object.
 */
struct resourcenamespace_s* F_ToResourceNamespace(resourcenamespaceid_t rni);

/**
 * @return              Number of resource namespaces.
 */
uint F_NumResourceNamespaces(void);

/**
 * @return              Unique identifier of the default namespace associated with @a rclass.
 */
resourcenamespaceid_t F_DefaultResourceNamespaceForClass(resourceclass_t rclass);

/**
 * @return              Unique identifier of the resource namespace associated with @a name,
 *                      else @c 0 if not found.
 */
resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name);

/**
 * Same as F_SafeResourceNamespaceForName except will throw a fatal error if not found and won't return.
 */
resourcenamespaceid_t F_ResourceNamespaceForName(const char* name);

/**
 * Clear "extra" resource search paths for all namespaces.
 */
void F_ClearResourceSearchPaths(void);

/**
 * Attempt to locate an external file for the specified resource.
 *
 * @param rclass        Class of resource being searched for (if known).
 *
 * @param foundPath     If found, the fully qualified path will be written back here.
 *                      Can be @c NULL, changing this routine to only check that file
 *                      exists on the physical file system and can be read.
 *
 * @param searchPath    Path/name of the resource being searched for. Note that
 *                      the resource @a type specified significantly alters search
 *                      behavior. This allows text replacements of symbolic escape
 *                      sequences in the path, allowing access to the engine's view
 *                      of the virtual file systmem.
 *
 * @param suffix        Optional name suffix. If not @c NULL, append to @p name
 *                      and look for matches. If not found or not specified then
 *                      search for matches to @p name.
 *
 * @param foundPathLen  Size of @p foundPath in bytes.
 *
 * @return              @c true, iff a file was found.
 */
boolean F_FindResource(resourceclass_t rclass, char* foundPath, const char* searchPath,
    const char* suffix, size_t foundPathLength);

/**
 * Convert a resourceclass_t constant into a string for error/debug messages.
 */
const char* F_ResourceClassStr(resourceclass_t rclass);

#endif /* LIBDENG_FILESYS_RESOURCE_LOCATOR_H */
