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

//#include "resourcerecord.h"
#include "uri.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#if 0
/**
 * Given an id return the associated resource namespace object.
 */
ResourceNamespace* F_ToResourceNamespace(resourcenamespaceid_t rni);
#endif

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
uint F_FindResourceStr4(resourceclass_t rclass, ddstring_t const* searchPaths, ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix);
uint F_FindResourceStr3(resourceclass_t rclass, ddstring_t const* searchPaths, ddstring_t* foundPath, int flags/*, optionalSuffix = NULL*/);
uint F_FindResourceStr2(resourceclass_t rclass, ddstring_t const* searchPaths, ddstring_t* foundPath/*, flags = RLF_DEFAULT*/);
uint F_FindResourceStr(resourceclass_t rclass, ddstring_t const* searchPaths/*, foundPath = NULL*/);

uint F_FindResource5(resourceclass_t rclass, struct uri_s const** searchPaths, ddstring_t* foundPath, int flags, ddstring_t const* optionalSuffix);
uint F_FindResource4(resourceclass_t rclass, char const* searchPaths, ddstring_t* foundPath, int flags, char const* optionalSuffix);
uint F_FindResource3(resourceclass_t rclass, char const* searchPaths, ddstring_t* foundPath, int flags/*, optionalSuffix = NULL*/);
uint F_FindResource2(resourceclass_t rclass, char const* searchPaths, ddstring_t* foundPath/*, flags = RLF_DEFAULT*/);
uint F_FindResource(resourceclass_t rclass, char const* searchPaths/*, foundPath = NULL*/);

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
resourcenamespaceid_t F_SafeResourceNamespaceForName(char const* name);

/**
 * Same as F_SafeResourceNamespaceForName except will throw a fatal error if not
 * found and won't return.
 */
resourcenamespaceid_t F_ResourceNamespaceForName(char const* name);

/**
 * Attempts to determine which "type" should be attributed to a resource, solely
 * by examining the name (e.g., a file name/path).
 *
 * @return  Type determined for this resource else @c RT_NONE.
 */
resourcetype_t F_GuessResourceTypeByName(char const* name);

/**
 * Apply mapping for this namespace to the specified path (if enabled).
 *
 * This mapping will translate directives and symbolic identifiers into their default paths,
 * which themselves are determined using the current Game.
 *
 *  e.g.: "Models/my/cool/model.dmd" => "$(App.DataPath)/$(GamePlugin.Name)/models/my/cool/model.dmd"
 *
 * @param rni  Unique identifier of the namespace whose mappings to apply.
 * @param path  The path to be mapped (applied in-place).
 * @return  @c true iff mapping was applied to the path.
 */
boolean F_MapGameResourcePath(resourcenamespaceid_t rni, ddstring_t* path);

/**
 * Apply all resource namespace mappings to the specified path.
 *
 * @return  @c true iff the path was mapped.
 */
boolean F_ApplyGamePathMapping(ddstring_t* path);

/**
 * Convert a resourceclass_t constant into a string for error/debug messages.
 */
char const* F_ResourceClassStr(resourceclass_t rclass);

/**
 * Construct a new NULL terminated Uri list from the specified search path list.
 */
struct uri_s** F_CreateUriListStr2(resourceclass_t rclass, ddstring_t const* searchPaths, int* count);
struct uri_s** F_CreateUriListStr(resourceclass_t rclass, ddstring_t const* searchPaths);

struct uri_s** F_CreateUriList2(resourceclass_t rclass, char const* searchPaths, int* count);
struct uri_s** F_CreateUriList(resourceclass_t rclass, char const* searchPaths);

void F_DestroyUriList(struct uri_s** list);

ddstring_t** F_ResolvePathList2(resourceclass_t defaultResourceClass, ddstring_t const* pathList, size_t* count, char delimiter);
ddstring_t** F_ResolvePathList(resourceclass_t defaultResourceClass, ddstring_t const* pathList, size_t* count);

void F_DestroyStringList(ddstring_t** list);

#if _DEBUG
void F_PrintStringList(ddstring_t const** strings, size_t stringsCount);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_SYSTEM_RESOURCE_LOCATOR_H */
