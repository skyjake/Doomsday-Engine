/** @file materialarchive.h Material Archive.
 * Collection of identifier-material pairs.
 *
 * Used when saving map state (savegames) or sharing world changes with clients.
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifdef __cplusplus

#include <de/Error>
#include <de/writer.h>
#include <de/reader.h>

class Material;

namespace de {

class MaterialArchive
{
public:
    /// Base class for all deserialization errors. @ingroup errors
    DENG2_ERROR(ReadError);

public:
    /**
     * @param useSegments  If @c true, a serialized archive will be preceded by a segment id number.
     */
    MaterialArchive(int useSegments, bool populate = true);

    ~MaterialArchive();

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
    struct Instance;
    Instance *d;
};

} // namespace de

#endif // __cplusplus

#endif /* LIBDENG_RESOURCE_MATERIALARCHIVE_H */
