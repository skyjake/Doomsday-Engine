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

struct material_s;

typedef unsigned short materialarchive_serialid_t;

#ifdef __cplusplus
namespace de {

    class MaterialArchive
    {
    public:
        /**
         * @param useSegments  If @c true, a serialized archive will be preceded by a segment id number.
         */
        MaterialArchive(int useSegments, bool populate = true);

        ~MaterialArchive();

        /**
         * @return A new (unused) SerialId for the specified material.
         */
        materialarchive_serialid_t findUniqueSerialId(struct material_s *mat) const;

        /**
         * Finds and returns a material with the identifier @a serialId.
         *
         * @param serialId  SerialId of a material.
         * @param group  Set to zero. Only used with the version 0 of MaterialArchive (now obsolete).
         *
         * @return  Pointer to a material instance. Ownership not given.
         */
        struct material_s *find(materialarchive_serialid_t serialId, int group) const;

        /**
         * Returns the number of materials in the archive.
         *
         * @param arc  MaterialArchive instance.
         */
        size_t count() const;

        /**
         * Serializes the state of the archive using @a writer.
         *
         * @param writer  Writer instance.
         */
        void write(writer_s &writer) const;

        /**
         * Deserializes the state of the archive from @a reader.
         *
         * @param forcedVersion  Version to interpret as, not actual format version. Use -1 to use whatever
         *                       version is encountered.
         * @param reader  Reader instance.
         */
        void read(int forcedVersion, reader_s &reader);

    private:
        struct Instance;
        Instance *d;
    };

} // namespace de
extern "C" {
#endif

/*
 * C Wrapper API:
 */

struct materialarchive_s;
typedef struct materialarchive_s MaterialArchive; // MaterialArchive instance

MaterialArchive *MaterialArchive_New(int useSegments);
MaterialArchive *MaterialArchive_NewEmpty(int useSegments);

void MaterialArchive_Delete(MaterialArchive *arc);
materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(MaterialArchive const *arc, struct material_s *mat);
struct material_s *MaterialArchive_Find(MaterialArchive const *arc, materialarchive_serialid_t serialId, int group);
size_t MaterialArchive_Count(MaterialArchive const *arc);
void MaterialArchive_Write(MaterialArchive const *arc, Writer *writer);
void MaterialArchive_Read(MaterialArchive *arc, int forcedVersion, Reader *reader);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_MATERIALARCHIVE_H
