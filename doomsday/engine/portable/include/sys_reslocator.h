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

#ifndef LIBDENG_SYSTEM_RESOURCE_LOCATOR_H
#define LIBDENG_SYSTEM_RESOURCE_LOCATOR_H

#include "m_string.h"

/**
 * Resource Type. Unique identifer attributable to resources (e.g., files).
 *
 * @ingroup core
 */
typedef enum {
    RT_NONE = 0,
    RT_FIRST = 1,
    RT_ZIP = RT_FIRST,
    RT_WAD,
    RT_DED,
    RT_PNG,
    RT_TGA,
    RT_PCX,
    RT_DMD,
    RT_MD2,
    RT_WAV,
    RT_OGG,
    RT_MP3,
    RT_MOD,
    RT_MID,
    RT_DEH,
    RT_LAST_INDEX
} resourcetype_t;

#define NUM_RESOURCE_TYPES          (RT_LAST_INDEX-1)
#define VALID_RESOURCE_TYPE(v)      ((v) >= RT_FIRST && (v) < RT_LAST_INDEX)

struct resourcenamespace_s;

/**
 * Unique identifier associated with resource namespaces managed by the resource locator.
 *
 * @ingroup core
 * @see ResourceNamespace
 */
typedef uint resourcenamespaceid_t;

/**
 * \post Initial/default search paths registered, namespaces initialized and queries may begin.
 * \note May be called to re-initialize the locator back to default state.
 */
void F_InitResourceLocator(void);

/**
 * \post All resource namespaces are emptied and search paths cleared. Queries no longer possible.
 */
void F_ShutdownResourceLocator(void);

/**
 * @return              Number of resource namespaces.
 */
uint F_NumResourceNamespaces(void);

/**
 * @return              @c true iff the value can be interpreted as a valid resource namespace id.
 */
boolean F_IsValidResourceNamespaceId(int value);

/**
 * Given an id return the associated resource namespace object.
 */
struct resourcenamespace_s* F_ToResourceNamespace(resourcenamespaceid_t rni);

/**
 * Attempt to locate a named resource.
 *
 * @param rclass        Class of resource being searched for (if known).
 *
 * @param searchPath    Path/name of the resource being searched for. Note that
 *                      the resource class (@a rclass) specified significantly
 *                      alters search behavior. This allows text replacements of
 *                      symbolic escape sequences in the path, allowing access to
 *                      the engine's view of the virtual file system.
 *
 * @param foundPath     If found, the fully qualified path is written back here.
 *                      Can be @c NULL, changing this routine to only check that
 *                      resource exists is readable.
 *
 * @param optionalSuffix If not @c NULL, append this suffix to search paths and
 *                      look for matches. If not found or not specified then search
 *                      for matches without a suffix.
 *
 * @return              @c true, iff a resource was found.
 */
const char* F_FindResource3(resourceclass_t rclass, const char* searchPath, ddstring_t* foundPath, const char* optionalSuffix);
const char* F_FindResource2(resourceclass_t rclass, const char* searchPath, ddstring_t* foundPath);
const char* F_FindResource(resourceclass_t rclass, const char* searchPath);

/**
 * @return              Default class associated with resources of type @a type.
 */
resourceclass_t F_DefaultResourceClassForType(resourcetype_t type);

/**
 * @return              Unique identifier of the default namespace associated with @a rclass.
 */
resourcenamespaceid_t F_DefaultResourceNamespaceForClass(resourceclass_t rclass);

/**
 * @return              Unique identifier of the resource namespace associated
 *                      with @a name, else @c 0 (not found).
 */
resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name);

/**
 * Same as F_SafeResourceNamespaceForName except will throw a fatal error if not found and won't return.
 */
resourcenamespaceid_t F_ResourceNamespaceForName(const char* name);

/**
 * Attempts to determine which "type" should be attributed to a resource, solely
 * by examining the name (e.g., a file name/path).
 *
 * @return              Type determined for this resource else @c RT_NONE.
 */
resourcetype_t F_GuessResourceTypeByName(const char* name);

/**
 * Apply all resource namespace mappings to the specified path.
 *
 * @return              @c true iff the path was mapped.
 */
boolean F_ApplyPathMapping(ddstring_t* path);

/**
 * @return              @c false if the resource uri cannot be fully resolved
 *                      (e.g., due to incomplete symbol definitions).
 */
boolean F_ResolveURI(ddstring_t* translatedPath, const ddstring_t* uri);

// Utility routines:

typedef struct directory2_s {
    int drive;
    ddstring_t path;
} directory2_t;

void F_FileDir(const ddstring_t* str, directory2_t* dir);

/**
 * Convert the symbolic path into a real path.
 * \todo dj: This seems rather redundant; refactor callers.
 */
void F_ResolveSymbolicPath(ddstring_t* dest, const ddstring_t* src);

/**
 * @return              @c true iff the path can be made into a relative path. 
 */
boolean F_IsRelativeToBasePath(const ddstring_t* path);

/**
 * Attempt to remove the base path if found at the beginning of the path.
 *
 * @param src           Possibly absolute path.
 * @param dest          Potential base-relative path written here.
 *
 * @return              @c true iff the base path was found and removed.
 */
boolean F_RemoveBasePath(ddstring_t* dest, const ddstring_t* src);

/**
 * Attempt to prepend the base path. If @a src is already absolute do nothing.
 *
 * @param dest          Expanded path written here.
 * @param src           Original path.
 *
 * @return              @c true iff the path was expanded.
 */
boolean F_PrependBasePath(ddstring_t* dest, const ddstring_t* src);

/**
 * Expands relative path directives like '>'.
 *
 * \note Despite appearances this function is *not* an alternative version
 * of M_TranslatePath accepting ddstring_t arguments. Key differences:
 *
 * ! Handles '~' on UNIX-based platforms.
 * ! No other transform applied to @a src path.
 *
 * @param dest          Expanded path written here.
 * @param src           Original path.
 *
 * @return              @c true iff the path was expanded.
 */
boolean F_ExpandBasePath(ddstring_t* dest, const ddstring_t* src);

/**
 * @return              A prettier copy of the original path.
 */
const ddstring_t* F_PrettyPath(const ddstring_t* path);

/**
 * Convert a resourceclass_t constant into a string for error/debug messages.
 */
const char* F_ResourceClassStr(resourceclass_t rclass);

#endif /* LIBDENG_SYSTEM_RESOURCE_LOCATOR_H */
