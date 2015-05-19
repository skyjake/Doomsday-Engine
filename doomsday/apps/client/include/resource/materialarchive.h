/** @file materialarchive.h Collection of identifier-material pairs.
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

#ifndef LIBDENG_RESOURCE_MATERIALARCHIVE_H
#define LIBDENG_RESOURCE_MATERIALARCHIVE_H

#include <de/Error>
#include <de/writer.h>
#include <de/reader.h>

class Material;

namespace de {

/**
 * Collection of identifier-material pairs.
 *
 * Used when saving map state (savegames) or sharing world changes with clients.
 *
 * @ingroup resource
 */
class MaterialArchive
{
public:
    /// Base class for all deserialization errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    /**
     * @param useSegments  If @c true, the serialized archive will be preceded
     *      by a segment id number.
     * @param recordSymbolicMaterials  Add records for the symbolic materials
     *      used to record special references in the serialized archive.
     */
    MaterialArchive(int useSegments, bool recordSymbolicMaterials = true);

    /**
     * Returns the number of materials in the archive.
     */
    int count() const;

    /**
     * Returns the number of materials in the archive.
     * Same as count()
     */
    inline int size() const {
        return count();
    }

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
    materialarchive_serialid_t addRecord(Material const &material);

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
    DENG2_PRIVATE(d)
};

} // namespace de

#endif /* LIBDENG_RESOURCE_MATERIALARCHIVE_H */
