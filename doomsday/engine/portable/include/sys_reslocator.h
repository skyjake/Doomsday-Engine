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

#ifndef LIBDENG_SYSTEM_RESOURCE_LOCATOR_H
#define LIBDENG_SYSTEM_RESOURCE_LOCATOR_H

#include "dd_string.h"

struct resourcenamespace_s;
struct resourcerecord_s;
struct filedirectory_s;
struct dduri_s;

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

/// @return  Number of resource namespaces.
uint F_NumResourceNamespaces(void);

/// @return  @c true iff @a value can be interpreted as a valid resource namespace id.
boolean F_IsValidResourceNamespaceId(int value);

/**
 * Given an id return the associated resource namespace object.
 */
struct resourcenamespace_s* F_ToResourceNamespace(resourcenamespaceid_t rni);

/**
 * Attempt to locate a known resource.
 *
 * @param record        Record of the resource being searched for.
 *
 * @param foundPath     If found, the fully qualified path is written back here.
 *                      Can be @c NULL, changing this routine to only check that
 *                      resource exists is readable.
 *
 * @return  Non-zero iff a resource was found.
 */
uint F_FindResourceForRecord(struct resourcerecord_s* rec, ddstring_t* foundPath);

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
 * @param optionalSuffix  If not @c NULL, append this suffix to search paths and
 *                      look for matches. If not found or not specified then search
 *                      for matches without a suffix.
 *
 * @return  @c Non-zero iff a resource was found.
 */
uint F_FindResourceStr3(resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath, const ddstring_t* optionalSuffix);
uint F_FindResourceStr2(resourceclass_t rclass, const ddstring_t* searchPath,
    ddstring_t* foundPath);
uint F_FindResourceStr(resourceclass_t rclass, const ddstring_t* searchPath);

uint F_FindResource3(resourceclass_t rclass, const char* searchPath,
    ddstring_t* foundPath, const char* optionalSuffix);
uint F_FindResource2(resourceclass_t rclass, const char* searchPath,
    ddstring_t* foundPath);
uint F_FindResource(resourceclass_t rclass, const char* searchPath);

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
 *          else @c 0 (not found).
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
 * Apply all resource namespace mappings to the specified path.
 *
 * @return  @c true iff the path was mapped.
 */
boolean F_ApplyPathMapping(ddstring_t* path);

// Utility routines:

typedef struct directory2_s {
    int drive;
    ddstring_t path;
} directory2_t;

void F_FileDir(const ddstring_t* str, directory2_t* dir);
void F_FileName(ddstring_t* dst, const ddstring_t* src);
void F_FileNameAndExtension(ddstring_t* dst, const ddstring_t* src);

const char* F_ParseSearchPath2(struct dduri_s* dst, const char* src, char delim,
    resourceclass_t defaultResourceClass);
const char* F_ParseSearchPath(struct dduri_s* dst, const char* src, char delim);

/**
 * Converts directory slashes to the correct type of slash.
 * @return  @c true iff the path was modified.
 */
boolean F_FixSlashes(ddstring_t* dst, const ddstring_t* src);

/**
 * Convert the symbolic path into a real path.
 * \todo dj: This seems rather redundant; refactor callers.
 */
void F_ResolveSymbolicPath(ddstring_t* dst, const ddstring_t* src);

/**
 * @return  @c true, if the given path is absolute (starts with \ or / or the
 *          second character is a ':' (drive).
 */
boolean F_IsAbsolute(const ddstring_t* str);

/**
 * @return  @c true iff the path can be made into a relative path. 
 */
boolean F_IsRelativeToBasePath(const ddstring_t* path);

/**
 * Attempt to remove the base path if found at the beginning of the path.
 *
 * @param dst           Potential base-relative path written here.
 * @param src           Possibly absolute path.
 *
 * @return  @c true iff the base path was found and removed.
 */
boolean F_RemoveBasePath(ddstring_t* dst, const ddstring_t* src);

/**
 * Attempt to prepend the base path. If @a src is already absolute do nothing.
 *
 * @param dst           Expanded path written here.
 * @param src           Original path.
 *
 * @return  @c true iff the path was expanded.
 */
boolean F_PrependBasePath(ddstring_t* dst, const ddstring_t* src);

/**
 * Expands relative path directives like '>'.
 *
 * \note Despite appearances this function is *not* an alternative version
 * of M_TranslatePath accepting ddstring_t arguments. Key differences:
 *
 * ! Handles '~' on UNIX-based platforms.
 * ! No other transform applied to @a src path.
 *
 * @param dst           Expanded path written here.
 * @param src           Original path.
 *
 * @return  @c true iff the path was expanded.
 */
boolean F_ExpandBasePath(ddstring_t* dst, const ddstring_t* src);

/**
 * \important Not thread-safe!
 * @return  A prettier copy of the original path.
 */
const ddstring_t* F_PrettyPath(const ddstring_t* path);

/**
 * Convert a resourceclass_t constant into a string for error/debug messages.
 */
const char* F_ResourceClassStr(resourceclass_t rclass);

/**
 * Construct a new NULL terminated Uri list from the specified search path list.
 */
dduri_t** F_CreateUriListStr2(resourceclass_t rclass, const ddstring_t* searchPaths, size_t* count);
dduri_t** F_CreateUriListStr(resourceclass_t rclass, const ddstring_t* searchPaths);

dduri_t** F_CreateUriList2(resourceclass_t rclass, const char* searchPaths, size_t* count);
dduri_t** F_CreateUriList(resourceclass_t rclass, const char* searchPaths);

void F_DestroyUriList(dduri_t** list);

ddstring_t** F_ResolvePathList2(resourceclass_t defaultResourceClass,
    const ddstring_t* pathList, size_t* count, char delimiter);
ddstring_t** F_ResolvePathList(resourceclass_t defaultResourceClass,
    const ddstring_t* pathList, size_t* count);

void F_DestroyStringList(ddstring_t** list);

#if _DEBUG
void F_PrintStringList(const ddstring_t** strings, size_t stringsCount);
#endif

#endif /* LIBDENG_SYSTEM_RESOURCE_LOCATOR_H */
