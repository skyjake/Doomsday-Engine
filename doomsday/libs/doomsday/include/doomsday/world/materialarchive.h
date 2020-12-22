/** @file materialarchive.h Collection of identifier-material pairs.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_WORLD_MATERIALARCHIVE_H
#define LIBDOOMSDAY_WORLD_MATERIALARCHIVE_H

#include <de/error.h>
#include <dd_share.h>  // materialarchive_serialid_t

struct reader_s; // legacy
struct writer_s; // legacy

namespace world {

class Material;

/**
 * Collection of identifier-material pairs.
 *
 * Used when saving map state (savegames) or sharing world changes with clients.
 *
 * @ingroup resource
 */
class LIBDOOMSDAY_PUBLIC MaterialArchive
{
public:
    /// Base class for all deserialization errors. @ingroup errors
    DE_ERROR(ReadError);

public:
    /**
     * @param useSegments  If @c true, the serialized archive will be preceded
     *                     by a segment id number.
     * @param recordSymbolicMaterials  Add records for the symbolic materials
     *                     used to record special references in the serialized archive.
     */
    MaterialArchive(int useSegments, bool recordSymbolicMaterials = true);

    void addWorldMaterials();

    /**
     * Returns the number of materials in the archive.
     */
    int count() const;

    /**
     * Returns the number of materials in the archive.
     * Same as count()
     */
    inline int size() const { return count(); }

    /**
     * @return A new (unused) SerialId for the specified material.
     */
    materialarchive_serialid_t findUniqueSerialId(Material *mat) const;

    /**
     * Finds and returns a material with the identifier @a serialId.
     *
     * @param serialId  SerialId of a material.
     * @param group  Set to zero. Only used with the version 0 of MaterialArchive (now obsolete).
     *
     * @return  Pointer to a material instance. Ownership not given.
     */
    Material *find(materialarchive_serialid_t serialId, int group) const;

    /**
     * Insert the specified @a material into the archive. If this material
     * is already present the existing serial ID is returned and the archive
     * is unchanged.
     *
     * @param material  The material to be recorded.
     * @return  Unique SerialId of the recorded material.
     */
    materialarchive_serialid_t addRecord(const Material &material);

    /**
     * Serializes the state of the archive using @a writer.
     *
     * @param writer  Writer instance.
     */
    void write(writer_s &writer) const;

    /**
     * Deserializes the state of the archive from @a reader.
     *
     * @param reader  Reader instance.
     * @param forcedVersion  Version to interpret as, not actual format version.
     *                       Use -1 to use whatever version is encountered.
     */
    void read(reader_s &reader, int forcedVersion = -1);

private:
    DE_PRIVATE(d)
};

} // namespace world

#if 0
extern "C" {

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
materialarchive_serialid_t (*FindUniqueSerialId)(const MaterialArchive *arc, Material *mat);

/**
 * Finds and returns a material with the identifier @a serialId.
 *
 * @param arc  MaterialArchive instance.
 * @param serialId  SerialId of a material.
 * @param group  Set to zero. Only used with the version 0 of MaterialArchive (now obsolete).
 *
 * @return  Pointer to a material instance. Ownership not given.
 */
Material *(*Find)(const MaterialArchive *arc, materialarchive_serialid_t serialId, int group);

/**
 * Returns the number of materials in the archive.
 *
 * @param arc  MaterialArchive instance.
 */
int (*Count)(const MaterialArchive *arc);

/**
 * Serializes the state of the archive using @a writer.
 *
 * @param arc  MaterialArchive instance.
 * @param writer  Writer instance.
 */
void (*Write)(const MaterialArchive *arc, Writer *writer);

void (*Read)(MaterialArchive *arc, Reader *reader, int forcedVersion);

/**
 * Deserializes the state of the archive from @a reader.
 *
 * @param arc  MaterialArchive instance.
 * @param reader  Reader instance.
 * @param forcedVersion  Version to interpret as, not actual format version. Use -1 to use whatever
 *                       version is encountered.
 */

} // extern "C"
#endif

#endif /* LIBDOOMSDAY_WORLD_MATERIALARCHIVE_H */
