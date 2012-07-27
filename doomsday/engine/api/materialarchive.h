/**
 * @file materialarchive.h
 * Collection of identifier-material pairs. @ingroup resource
 *
 * Used when saving map state (savegames) or sharing world changes with clients.
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_MATERIALARCHIVE_H
#define LIBDENG_MATERIALARCHIVE_H

#include <de/writer.h>
#include <de/reader.h>

#ifdef __cplusplus
extern "C" {
#endif

struct material_s;

struct materialarchive_s;

/**
 * MaterialArchive instance.
 */
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
 * Finds and returns a material with the identifier @a serialId.
 *
 * @param arc  MaterialArchive instance.
 * @param serialId  SerialId of a material.
 * @param group  Set to zero. Only used with the version 0 of MaterialArchive (now obsolete).
 *
 * @return  Pointer to a material instance. Ownership not given.
 */
struct material_s* MaterialArchive_Find(MaterialArchive* arc, materialarchive_serialid_t serialId, int group);

/**
 * Returns the number of materials in the archive.
 *
 * @param arc  MaterialArchive instance.
 */
size_t MaterialArchive_Count(MaterialArchive* arc);

/**
 * Serializes the state of the archive using @a writer.
 *
 * @param arc  MaterialArchive instance.
 * @param writer  Writer instance.
 */
void MaterialArchive_Write(MaterialArchive* arc, Writer* writer);

/**
 * Deserializes the state of the archive from @a reader.
 *
 * @param arc  MaterialArchive instance.
 * @param forcedVersion  Version to interpret as, not actual format version. Use -1 to use whatever
 *                       version is encountered.
 * @param reader  Reader instance.
 */
void MaterialArchive_Read(MaterialArchive* arc, int forcedVersion, Reader* reader);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_MATERIALARCHIVE_H
