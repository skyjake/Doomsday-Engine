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
extern "C" {
#endif

/**
 * @defgroup resourceLocationFlags  Resource Location Flags
 *
 * Flags used with the F_Find family of functions which dictate the
 * logic used during resource location.
 * @ingroup flags
 */
///@{
#define RLF_MATCH_EXTENSION     0x1 /// If an extension is specified in the search term the found file should have it too.

/// Default flags.
#define RLF_DEFAULT             0
///@}

/**
 * Attempt to locate a named resource.
 *
 * @param classId        Class of resource being searched for (if known).
 *
 * @param searchPath    Path/name of the resource being searched for. Note that
 *                      the resource class (@a classId) specified significantly
 *                      alters search behavior. This allows text replacements of
 *                      symbolic escape sequences in the path, allowing access to
 *                      the engine's view of the virtual file system.
 *
 * @param foundPath     If found, the fully qualified path is written back here.
 *                      Can be @c NULL, changing this routine to only check that
 *                      the resource exists is readable.
 *
 * @param flags         @ref resourceLocationFlags
 *
 * @param optionalSuffix  If not @c NULL, append this suffix to search paths and
 *                      look for matches. If not found or not specified then search
 *                      for matches without a suffix.
 *
 * @return  @c true iff a resource was found.
 */
boolean F_Find4(resourceclassid_t classId, struct uri_s const* searchPath, ddstring_t* foundPath, int flags, char const* optionalSuffix);
boolean F_Find3(resourceclassid_t classId, struct uri_s const* searchPath, ddstring_t* foundPath, int flags/*, optionalSuffix = NULL*/);
boolean F_Find2(resourceclassid_t classId, struct uri_s const* searchPath, ddstring_t* foundPath/*, flags = RLF_DEFAULT*/);
boolean F_Find(resourceclassid_t classId, struct uri_s const* searchPath/*, foundPath = NULL*/);

/**
 * @return  If a resource is found, the index + 1 of the path from @a searchPaths
 *          that was used to find it; otherwise @c 0.
 */
uint F_FindFromList(resourceclassid_t classId, char const* searchPaths,
    ddstring_t* foundPath, int flags, char const* optionalSuffix);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_SYSTEM_RESOURCE_LOCATOR_H */
