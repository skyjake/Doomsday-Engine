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

#include "resourcenamespace.h"
#include "abstractresource.h"
#include "filedirectory.h"

struct uri_s;

#define PACKAGES_RESOURCE_NAMESPACE_NAME    "Packages"
#define DEFINITIONS_RESOURCE_NAMESPACE_NAME "Defs"
#define GRAPHICS_RESOURCE_NAMESPACE_NAME    "Graphics"
#define MODELS_RESOURCE_NAMESPACE_NAME      "Models"
#define SOUNDS_RESOURCE_NAMESPACE_NAME      "Sfx"
#define MUSIC_RESOURCE_NAMESPACE_NAME       "Music"
#define TEXTURES_RESOURCE_NAMESPACE_NAME    "Textures"
#define FLATS_RESOURCE_NAMESPACE_NAME       "Flats"
#define PATCHES_RESOURCE_NAMESPACE_NAME     "Patches"
#define LIGHTMAPS_RESOURCE_NAMESPACE_NAME   "LightMaps"
#define FONTS_RESOURCE_NAMESPACE_NAME       "Fonts"

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
 * @{
 */
#define RLF_MATCH_EXTENSION     0x1 /// If an extension is specified in the search term the found file should have it too.

/// Default flags.
#define RLF_DEFAULT             0
/**@}*/

/**
 * \post Initial/default search paths registered, namespaces initialized and
 * queries may begin.
 * \note May be called to re-initialize the locator back to default state.
 */
void F_InitResourceLocator(void);

/**
 * \post All resource namespaces are emptied, search paths are cleared and
 * queries are no longer possible.
 */
void F_ShutdownResourceLocator(void);

void F_ResetAllResourceNamespaces(void);

void F_ResetResourceNamespace(resourcenamespaceid_t rni);

void F_CreateNamespacesForFileResourcePaths(void);

/**
 * @return  Newly created hash name. Ownership passes to the caller who should
 * ensure to release it with Str_Delete when done.
 */
ddstring_t* F_ComposeHashNameForFilePath(const ddstring_t* filePath);

/**
 * This is a hash function. It uses the resource name to generate a
 * somewhat-random number between 0 and RESOURCENAMESPACE_HASHSIZE.
 *
 * @return  The generated hash key.
 */
resourcenamespace_namehash_key_t F_HashKeyForAlphaNumericNameIgnoreCase(const ddstring_t* name);

#define F_HashKeyForFilePathHashName F_HashKeyForAlphaNumericNameIgnoreCase

resourcenamespace_t* F_CreateResourceNamespace(const char* name,
    FileDirectory* directory, ddstring_t* (*composeHashNameFunc) (const ddstring_t* path),
    resourcenamespace_namehash_key_t (*hashNameFunc) (const ddstring_t* name), byte flags);

/**
 * @param rni  Unique identifier of the namespace to add to.
 * @param flags  @see searchPathFlags
 * @param searchPath  Uri representing the search path to be added.
 * @param group  Group to add the new search path to.
 */
boolean F_AddSearchPathToResourceNamespace(resourcenamespaceid_t rni, int flags,
    const Uri* searchPath, resourcenamespace_searchpathgroup_t group);

const ddstring_t* F_ResourceNamespaceName(resourcenamespaceid_t rni);

/// @return  Number of resource namespaces.
uint F_NumResourceNamespaces(void);

/// @return  @c true iff @a value can be interpreted as a valid resource namespace id.
boolean F_IsValidResourceNamespaceId(int value);

/**
 * Given an id return the associated resource namespace object.
 */
resourcenamespace_t* F_ToResourceNamespace(resourcenamespaceid_t rni);

/**
 * Attempt to locate a known resource.
 *
 * @param record        Record of the resource being searched for.
 *
 * @param foundPath     If found, the fully qualified path is written back here.
 *                      Can be @c NULL, changing this routine to only check that
 *                      resource exists is readable.
 *
 * @return  The index+1 of the path in the list of search paths for this resource
 *     if found, else @c 0
 */
uint F_FindResourceForRecord(struct AbstractResource_s* rec, ddstring_t* foundPath);

uint F_FindResourceForRecord2(AbstractResource* rec, ddstring_t* foundPath, const Uri* const* searchPaths);

/**
 * Attempt to locate a named resource.
 *
 * @param rclass        Class of resource being searched for (if known).
 *
 * @param searchPaths   Paths/names of the resource being searched for. Note that
 *                      the resource class (@a rclass) specified significantly
 *                      alters search behavior. This allows text replacements of
 *                      symbolic escape sequences in the path, allowing access to
 *                      the engine's view of the virtual file system.
 *
 * @param foundPath     If found, the fully qualified path is written back here.
 *                      Can be @c NULL, changing this routine to only check that
 *                      resource exists is readable.
 *
 * @param flags         @see resourceLocationFlags
 *
 * @param optionalSuffix  If not @c NULL, append this suffix to search paths and
 *                      look for matches. If not found or not specified then search
 *                      for matches without a suffix.
 *
 * @return  The index+1 of the path in @a searchPaths if found, else @c 0
 */
uint F_FindResourceStr4(resourceclass_t rclass, const ddstring_t* searchPaths, ddstring_t* foundPath, int flags, const ddstring_t* optionalSuffix);
uint F_FindResourceStr3(resourceclass_t rclass, const ddstring_t* searchPaths, ddstring_t* foundPath, int flags); /*optionalSuffix=NULL*/
uint F_FindResourceStr2(resourceclass_t rclass, const ddstring_t* searchPaths, ddstring_t* foundPath); /*flags=RLF_DEFAULT*/
uint F_FindResourceStr(resourceclass_t rclass, const ddstring_t* searchPaths); /*foundPath=NULL*/

uint F_FindResource4(resourceclass_t rclass, const char* searchPaths, ddstring_t* foundPath, int flags, const char* optionalSuffix);
uint F_FindResource3(resourceclass_t rclass, const char* searchPaths, ddstring_t* foundPath, int flags); /*optionalSuffix=NULL*/
uint F_FindResource2(resourceclass_t rclass, const char* searchPaths, ddstring_t* foundPath); /*flags=RLF_DEFAULT*/
uint F_FindResource(resourceclass_t rclass, const char* searchPaths); /*foundPath=NULL*/

/**
 * @return  Default class associated with resources of type @a type.
 */
resourceclass_t F_DefaultResourceClassForType(resourcetype_t type);

/**
 * @return  Unique identifier of the default namespace associated with @a rclass.
 */
resourcenamespaceid_t F_DefaultResourceNamespaceForClass(resourceclass_t rclass);

/**
 * @return  Unique identifier of the resource namespace associated with @a name,
 *      else @c 0 (not found).
 */
resourcenamespaceid_t F_SafeResourceNamespaceForName(const char* name);

/**
 * Same as F_SafeResourceNamespaceForName except will throw a fatal error if not
 * found and won't return.
 */
resourcenamespaceid_t F_ResourceNamespaceForName(const char* name);

/**
 * Attempts to determine which "type" should be attributed to a resource, solely
 * by examining the name (e.g., a file name/path).
 *
 * @return  Type determined for this resource else @c RT_NONE.
 */
resourcetype_t F_GuessResourceTypeByName(const char* name);

/**
 * Apply mapping for this namespace to the specified path (if enabled).
 *
 * This mapping will translate directives and symbolic identifiers into their default paths,
 * which themselves are determined using the current Game.
 *
 *  e.g.: "Models:my/cool/model.dmd" -> "}data/<Game::IdentityKey>/models/my/cool/model.dmd"
 *
 * @param rni  Unique identifier of the namespace whose mappings to apply.
 * @param path  The path to be mapped (applied in-place).
 * @return  @c true iff mapping was applied to the path.
 */
boolean F_MapResourcePath(resourcenamespaceid_t rni, ddstring_t* path);

/**
 * Apply all resource namespace mappings to the specified path.
 *
 * @return  @c true iff the path was mapped.
 */
boolean F_ApplyPathMapping(ddstring_t* path);

const char* F_ParseSearchPath2(struct uri_s* dst, const char* src, char delim,
    resourceclass_t defaultResourceClass);
const char* F_ParseSearchPath(struct uri_s* dst, const char* src, char delim);

/**
 * Convert a resourceclass_t constant into a string for error/debug messages.
 */
const char* F_ResourceClassStr(resourceclass_t rclass);

/**
 * Construct a new NULL terminated Uri list from the specified search path list.
 */
Uri** F_CreateUriListStr2(resourceclass_t rclass, const ddstring_t* searchPaths, size_t* count);
Uri** F_CreateUriListStr(resourceclass_t rclass, const ddstring_t* searchPaths);

Uri** F_CreateUriList2(resourceclass_t rclass, const char* searchPaths, size_t* count);
Uri** F_CreateUriList(resourceclass_t rclass, const char* searchPaths);

void F_DestroyUriList(Uri** list);

ddstring_t** F_ResolvePathList2(resourceclass_t defaultResourceClass,
    const ddstring_t* pathList, size_t* count, char delimiter);
ddstring_t** F_ResolvePathList(resourceclass_t defaultResourceClass,
    const ddstring_t* pathList, size_t* count);

void F_DestroyStringList(ddstring_t** list);

#if _DEBUG
void F_PrintStringList(const ddstring_t** strings, size_t stringsCount);
#endif

#endif /* LIBDENG_SYSTEM_RESOURCE_LOCATOR_H */
