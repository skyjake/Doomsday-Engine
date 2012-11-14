/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

/**
 * Routines for locating resources.
 */

#ifndef LIBDENG_SYSTEM_RESOURCE_LOCATOR_H
#define LIBDENG_SYSTEM_RESOURCE_LOCATOR_H

#include "dd_types.h"
#include "uri.h"

#ifdef __cplusplus
#include <de/String>

extern "C" {
#endif

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
    RT_JPG,
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
    RT_DFN,
    RT_LAST_INDEX
} resourcetype_t;

#define NUM_RESOURCE_TYPES          (RT_LAST_INDEX-1)
#define VALID_RESOURCE_TYPE(v)      ((v) >= RT_FIRST && (v) < RT_LAST_INDEX)

/**
 * Unique identifier associated with resource namespaces managed by the
 * resource locator.
 *
 * @ingroup core
 * @see ResourceNamespace
 */
typedef uint resourcenamespaceid_t;

/**
 * @defgroup resourceLocationFlags  Resource Location Flags
 *
 * Flags used with the F_FindResource family of functions which dictate the
 * logic used during resource location.
 * @ingroup flags
 */
///@{
#define RLF_MATCH_EXTENSION     0x1 /// If an extension is specified in the search term the found file should have it too.

/// Default flags.
#define RLF_DEFAULT             0
///@}

/**
 * @post Initial/default search paths registered, namespaces initialized and
 *       queries may begin.
 *
 * @note May be called to re-initialize the locator back to default state.
 */
void F_InitResourceLocator(void);

/**
 * @post All resource namespaces are emptied, search paths are cleared and
 *       queries are no longer possible.
 */
void F_ShutdownResourceLocator(void);

void F_ResetAllResourceNamespaces(void);

void F_ResetResourceNamespace(resourcenamespaceid_t rni);

void F_CreateNamespacesForFileResourcePaths(void);

/// @return  Number of resource namespaces.
uint F_NumResourceNamespaces(void);

/// @return  @c true iff @a value can be interpreted as a valid resource namespace id.
boolean F_IsValidResourceNamespaceId(int value);

/**
 * @param rni  Unique identifier of the namespace to add to.
 * @param flags  @see searchPathFlags
 * @param searchPath  Uri representing the search path to be added.
 */
boolean F_AddExtraSearchPathToResourceNamespace(resourcenamespaceid_t rni, int flags,
    struct uri_s const* searchPath);

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
 *                      the resource exists is readable.
 *
 * @param flags         @see resourceLocationFlags
 *
 * @param optionalSuffix  If not @c NULL, append this suffix to search paths and
 *                      look for matches. If not found or not specified then search
 *                      for matches without a suffix.
 *
 * @return  @c true iff a resource was found.
 */
boolean F_FindResource4(resourceclass_t rclass, struct uri_s const* searchPath, ddstring_t* foundPath, int flags, char const* optionalSuffix);
boolean F_FindResource3(resourceclass_t rclass, struct uri_s const* searchPath, ddstring_t* foundPath, int flags/*, optionalSuffix = NULL*/);
boolean F_FindResource2(resourceclass_t rclass, struct uri_s const* searchPath, ddstring_t* foundPath/*, flags = RLF_DEFAULT*/);
boolean F_FindResource(resourceclass_t rclass, struct uri_s const* searchPath/*, foundPath = NULL*/);

/**
 * @return  If a resource is found, the index + 1 of the path from @a searchPaths
 *          that was used to find it; otherwise @c 0.
 */
uint F_FindResourceFromList(resourceclass_t rclass, char const* searchPaths,
    ddstring_t* foundPath, int flags, char const* optionalSuffix);

/**
 * @return  Default class associated with resources of type @a type.
 */
resourceclass_t F_DefaultResourceClassForType(resourcetype_t type);

/**
 * @return  Unique identifier of the default namespace associated with @a rclass.
 */
resourcenamespaceid_t F_DefaultResourceNamespaceForClass(resourceclass_t rclass);

#ifdef __cplusplus
} // extern "C"

/**
 * @return  Unique identifier of the resource namespace associated with @a name,
 *      else @c 0 (not found).
 */
resourcenamespaceid_t F_SafeResourceNamespaceForName(de::String name);

/**
 * Attempts to determine which "type" should be attributed to a resource, solely
 * by examining the name (e.g., a file name/path).
 *
 * @return  Type determined for this resource else @c RT_NONE if not recognizable.
 */
resourcetype_t F_GuessResourceTypeByName(de::String name);

/**
 * Convert a resourceclass_t constant into a string for error/debug messages.
 */
de::String const& F_ResourceClassStr(resourceclass_t rclass);

#endif // __cplusplus

#endif /* LIBDENG_SYSTEM_RESOURCE_LOCATOR_H */
