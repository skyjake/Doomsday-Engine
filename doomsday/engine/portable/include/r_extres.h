/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * sys_extres.h: External resource file locator and associated name hash.
 */

#ifndef LIBDENG_FILESYS_EXTRES_H
#define LIBDENG_FILESYS_EXTRES_H

/**
 * \post Initial/default search paths registered.
 */
void F_InitResourceLocator(void);

void F_ShutdownResourceLocator(void);

/**
 * Convert a ddresourceclass_t constant into a string for error/debug messages.
 */
const char* F_ResourceClassStr(ddresourceclass_t rc);

/**
 * Attempt to locate an external file for the specified resource.
 *
 * @param resType       Type of resource being searched for (if known).
 * @param resClass      Class specifier; alters search behavior including locations to be searched.
 * @param foundPath     If a file is found, the fully qualified path will be written back to here.
 *                      Can be @c NULL, which makes the routine just check for the existence of the file.
 * @param searchPath    Path/name of the resource being searched for.
 * @param optionalSuffix An optional name suffix. If not @c NULL, append to @p name and look for matches.
 *                      If not found or not specified then search for matches to @p name.
 * @param foundPathLen  Size of @p fileName in bytes.
 *
 * @return              @c true, iff a file was found.
 */
boolean F_FindResource2(resourcetype_t resType, ddresourceclass_t resClass, char* foundPath, const char* searchPath, const char* optionalSuffix, size_t foundPathLen);

/**
 * Same as F_FindResource2 except that the resource class is chosen automatically, using a set of logical defaults.
 */
boolean F_FindResource(resourcetype_t resType, char* foundPath, const char* searchPath, const char* optionalSuffix, size_t foundPathLen);

#endif /* LIBDENG_FILESYS_EXTRES_H */
