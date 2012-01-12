/**\file materialarchive.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MATERIALARCHIVE_H
#define LIBDENG_MATERIALARCHIVE_H

#include "writer.h"
#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

struct material_s;

struct materialarchive_s;
typedef struct materialarchive_s MaterialArchive;

typedef unsigned short materialarchive_serialid_t;

/**
 * @param useSegments  If @c true, a serialized archive will be preceded by a segment id number.
 */
MaterialArchive* MaterialArchive_New(int useSegments);

/**
 * @param useSegments  If @c true, a serialized archive will be preceded by a segment id number.
 */
MaterialArchive* MaterialArchive_NewEmpty(int useSegments);

void MaterialArchive_Delete(MaterialArchive* arc);

/**
 * @return A new (unused) SerialId for the specified material.
 */
materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(MaterialArchive* arc, struct material_s* mat);

/**
 * @param group  Set to zero. Only used with the version 0 of MaterialArchive (now obsolete).
 */
struct material_s* MaterialArchive_Find(MaterialArchive* arc, materialarchive_serialid_t serialId, int group);

/**
 * Returns the number of materials in the archive.
 */
size_t MaterialArchive_Count(MaterialArchive* arc);

void MaterialArchive_Write(MaterialArchive* arc, Writer* writer);

/**
 * @param forcedVersion  Version to interpret as, not actual format version. Use -1 to use whatever
 *                       version is encountered.
 */
void MaterialArchive_Read(MaterialArchive* arc, int forcedVersion, Reader* reader);

#ifdef __cplusplus
}
#endif

#endif // LIBDENG_MATERIALARCHIVE_H
