/** @file api_materialarchive.h Collection of identifier-material pairs.
 * @ingroup resource
 *
 * Used when saving map state (savegames) or sharing world changes with clients.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "apis.h"
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

DENG_API_TYPEDEF(MaterialArchive)
{
    de_api_t api;

    /**
     * @param useSegments  If @c true, a serialized archive will be preceded by a segment id number.
     */
    MaterialArchive *(*New)(int useSegments);

    /**
     * @param useSegments  If @c true, a serialized archive will be preceded by a segment id number.
     */
    MaterialArchive *(*NewEmpty)(int useSegments);

    void (*Delete)(MaterialArchive *arc);

    /**
     * @return A new (unused) SerialId for the specified material.
     */
    materialarchive_serialid_t (*FindUniqueSerialId)(MaterialArchive const *arc, struct material_s *mat);

    /**
     * Finds and returns a material with the identifier @a serialId.
     *
     * @param arc  MaterialArchive instance.
     * @param serialId  SerialId of a material.
     * @param group  Set to zero. Only used with the version 0 of MaterialArchive (now obsolete).
     *
     * @return  Pointer to a material instance. Ownership not given.
     */
    struct material_s *(*Find)(MaterialArchive const *arc, materialarchive_serialid_t serialId, int group);

    /**
     * Returns the number of materials in the archive.
     *
     * @param arc  MaterialArchive instance.
     */
    int (*Count)(MaterialArchive const *arc);

    /**
     * Serializes the state of the archive using @a writer.
     *
     * @param arc  MaterialArchive instance.
     * @param writer  Writer instance.
     */
    void (*Write)(MaterialArchive const *arc, Writer *writer);

    /**
     * Deserializes the state of the archive from @a reader.
     *
     * @param arc  MaterialArchive instance.
     * @param reader  Reader instance.
     * @param forcedVersion  Version to interpret as, not actual format version. Use -1 to use whatever
     *                       version is encountered.
     */
    void (*Read)(MaterialArchive *arc, Reader *reader, int forcedVersion);
}
DENG_API_T(MaterialArchive);

#ifndef DENG_NO_API_MACROS_MATERIAL_ARCHIVE
#define MaterialArchive_New                 _api_MaterialArchive.New
#define MaterialArchive_NewEmpty            _api_MaterialArchive.NewEmpty
#define MaterialArchive_Delete              _api_MaterialArchive.Delete
#define MaterialArchive_FindUniqueSerialId  _api_MaterialArchive.FindUniqueSerialId
#define MaterialArchive_Find                _api_MaterialArchive.Find
#define MaterialArchive_Count               _api_MaterialArchive.Count
#define MaterialArchive_Write               _api_MaterialArchive.Write
#define MaterialArchive_Read                _api_MaterialArchive.Read
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(MaterialArchive);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_MATERIALARCHIVE_H
