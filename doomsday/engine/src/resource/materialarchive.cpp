/** @file materialarchive.cpp Material Archive.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#define DENG_NO_API_MACROS_MATERIAL_ARCHIVE

#include "de_base.h"
#include "de_console.h"

#include <de/memory.h>

#include "resource/materials.h"

#include "api_materialarchive.h"
#include "api_material.h"

/// For identifying the archived format version. Written to disk.
#define MATERIALARCHIVE_VERSION (4)

#define ASEG_MATERIAL_ARCHIVE   112

// Used to denote unknown Material references in records. Written to disk.
#define UNKNOWN_MATERIALNAME    "DD_BADTX"

namespace de {

struct ArchiveRecord
{
    uri_s *uri; ///< Percent encoded.
    material_t *material;
};

struct MaterialArchive::Instance
{
    int version;
    uint count;
    ArchiveRecord *table;
    uint numFlats; /// Used with older versions.
    int useSegments; /// Segment id assertion (Hexen saves).

    Instance(int _useSegments)
        : version(MATERIALARCHIVE_VERSION),
          count(0), table(0), numFlats(0), useSegments(_useSegments)
    {}

    ~Instance()
    {
        clearTable();
    }

    void clearTable()
    {
        if(table)
        {
            for(uint i = 0; i < count; ++i)
            {
                Uri_Delete(table[i].uri);
            }
            M_Free(table); table = 0;
        }
        count = 0;
    }

    void insertSerialId(materialarchive_serialid_t /*serialId*/, de::Uri const &uri,
                        material_t *material)
    {
        table = (ArchiveRecord *) M_Realloc(table, ++count * sizeof(ArchiveRecord));
        ArchiveRecord *rec = &table[count - 1];

        rec->uri = Uri_Dup(reinterpret_cast<uri_s const *>(&uri));
        rec->material = material;
    }

    materialarchive_serialid_t insertSerialIdForMaterial(material_t *mat)
    {
        de::Uri uri = App_Materials()->toMaterialBind(Material_PrimaryBind(mat))->composeUri();
        // Insert a new element in the index.
        insertSerialId(count + 1, uri, mat);
        return count; // 1-based index.
    }

    materialarchive_serialid_t getSerialIdForMaterial(material_t &mat)
    {
        materialarchive_serialid_t id = 0;
        for(uint i = 0; i < count; ++i)
        {
            ArchiveRecord const *rec = &table[i];
            if(rec->material == &mat)
            {
                id = i + 1; // Yes. Return existing serial.
                break;
            }
        }
        return id;
    }

    ArchiveRecord *getRecord(materialarchive_serialid_t serialId, int group)
    {
        if(version < 1 && group == 1) // Group 1 = Flats:
            serialId += numFlats;

        ArchiveRecord *rec = &table[serialId];
        if(!Str_CompareIgnoreCase(Uri_Path(rec->uri), UNKNOWN_MATERIALNAME))
            return 0;
        return rec;
    }

    material_t *materialForSerialId(materialarchive_serialid_t serialId, int group)
    {
        DENG_ASSERT(serialId <= count + 1);

        ArchiveRecord *rec;
        if(serialId != 0 && (rec = getRecord(serialId - 1, group)))
        {
            if(!rec->material)
                rec->material = App_Materials()->find(*reinterpret_cast<de::Uri const *>(rec->uri)).material();
            return rec->material;
        }
        return 0;
    }

    /**
     * Populate the archive using the global Materials list.
     */
    void populate()
    {
        uri_s *unknownMaterial = Uri_NewWithPath2(UNKNOWN_MATERIALNAME, RC_NULL);
        insertSerialId(1, *reinterpret_cast<de::Uri const *>(unknownMaterial), 0);
        Uri_Delete(unknownMaterial);

        uint num = App_Materials()->count();
        for(uint i = 1; i < num + 1; ++i)
        {
            /// @todo Assumes knowledge of how material ids are generated.
            /// Should be iterated by Materials using a callback function.
            insertSerialIdForMaterial(App_Materials()->toMaterialBind(i)->material());
        }
    }

    int writeRecord(ArchiveRecord &rec, writer_s &writer)
    {
        Uri_Write(rec.uri, &writer);
        return true; // Continue iteration.
    }

    int readRecord(ArchiveRecord &rec, reader_s &reader)
    {
        if(!rec.uri)
        {
            rec.uri = Uri_New();
        }

        if(version >= 4)
        {
            Uri_Read(rec.uri, &reader);
        }
        else if(version >= 2)
        {
            ddstring_t *path = Str_NewFromReader(&reader);
            Uri_SetUri2(rec.uri, Str_Text(path), RC_NULL);
            if(version == 2)
            {
                // We must encode the path.
                Str_PercentEncode(Str_Set(path, Str_Text(Uri_Path(rec.uri))));
                Uri_SetPath(rec.uri, Str_Text(path));
            }
            Str_Delete(path);
        }
        else
        {
            char name[9];
            Reader_Read(&reader, name, 8);
            name[8] = 0;

            ddstring_t path; Str_Init(&path);
            Str_PercentEncode(Str_StripRight(Str_Set(&path, name)));

            byte oldMNI = Reader_ReadByte(&reader);
            switch(oldMNI % 4)
            {
            case 0: Uri_SetScheme(rec.uri, "Textures"); break;
            case 1: Uri_SetScheme(rec.uri, "Flats");    break;
            case 2: Uri_SetScheme(rec.uri, "Sprites");  break;
            case 3: Uri_SetScheme(rec.uri, "System");   break;
            }
            Uri_SetPath(rec.uri, Str_Text(&path));
            Str_Free(&path);
        }
        return true; // Continue iteration.
    }

    /**
     * Same as readRecord except we are reading the old record format used
     * by Doomsday 1.8.6 and earlier.
     */
    int readRecord_v186(ArchiveRecord &rec, char const *scheme, reader_s &reader)
    {
        char buf[9];
        Reader_Read(&reader, buf, 8);
        buf[8] = 0;

        if(!rec.uri)
            rec.uri = Uri_New();

        ddstring_t path; Str_Init(&path);
        Str_PercentEncode(Str_StripRight(Str_Appendf(&path, "%s", buf)));
        Uri_SetPath(rec.uri, Str_Text(&path));
        Uri_SetScheme(rec.uri, scheme);

        Str_Free(&path);
        return true; // Continue iteration.
    }

    void readMaterialGroup(char const *defaultScheme, reader_s &reader)
    {
        // Read the group header.
        uint num = Reader_ReadUInt16(&reader);
        for(uint i = 0; i < num; ++i)
        {
            ArchiveRecord temp;
            std::memset(&temp, 0, sizeof(temp));

            if(version >= 1)
                readRecord(temp, reader);
            else
                readRecord_v186(temp, version <= 1? defaultScheme : 0, reader);

            insertSerialId(count + 1, reinterpret_cast<de::Uri &>(temp.uri), 0);
            if(temp.uri)
                Uri_Delete(temp.uri);
        }
    }

    void writeMaterialGroup(writer_s &writer)
    {
        // Write the group header.
        Writer_WriteUInt16(&writer, count);

        for(uint i = 0; i < count; ++i)
        {
            writeRecord(table[i], writer);
        }
    }

    void beginSegment(int seg, writer_s &writer)
    {
        if(useSegments)
        {
            Writer_WriteUInt32(&writer, seg);
        }
    }

    void assertSegment(int seg, reader_s &reader)
    {
        if(useSegments)
        {
            int i = Reader_ReadUInt32(&reader);
            if(i != seg)
            {
                Con_Error("MaterialArchive: Expected ASEG_MATERIAL_ARCHIVE (%i), but got %i.\n",
                          ASEG_MATERIAL_ARCHIVE, i);
            }
        }
    }

    void writeHeader(writer_s &writer)
    {
        beginSegment(ASEG_MATERIAL_ARCHIVE, writer);
        Writer_WriteByte(&writer, version);
    }

    void readHeader(reader_s &reader)
    {
        assertSegment(ASEG_MATERIAL_ARCHIVE, reader);
        version = Reader_ReadByte(&reader);
    }
};

MaterialArchive::MaterialArchive(int useSegments, bool populate)
{
    d = new Instance(useSegments);
    if(populate)
    {
        d->populate();
    }
}

MaterialArchive::~MaterialArchive()
{
    delete d;
}

materialarchive_serialid_t MaterialArchive::findUniqueSerialId(material_t *material)
{
    if(material)
        return d->getSerialIdForMaterial(*material);
    return 0; // Invalid.
}

material_t *MaterialArchive::find(materialarchive_serialid_t serialId, int group)
{
    return d->materialForSerialId(serialId, group);
}

size_t MaterialArchive::count()
{
    return d->count;
}

void MaterialArchive::write(writer_s &writer)
{
    d->writeHeader(writer);
    d->writeMaterialGroup(writer);
}

void MaterialArchive::read(int forcedVersion, reader_s &reader)
{
    d->clearTable();

    d->readHeader(reader);

    // Are we interpreting a specific version?
    if(forcedVersion >= 0)
    {
        d->version = forcedVersion;
    }

    d->count = 0;
    d->readMaterialGroup((forcedVersion >= 1? "" : "Flats:"), reader);

    if(d->version == 0)
    {
        // The old format saved flats and textures in seperate groups.
        d->numFlats = d->count;
        d->readMaterialGroup((forcedVersion >= 1? "" : "Textures:"), reader);
    }
}

} // namespace de

#define TOINTERNAL(inst) \
    reinterpret_cast<de::MaterialArchive *>(inst)

#define TOINTERNAL_CONST(inst) \
    reinterpret_cast<de::MaterialArchive const*>(inst)

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::MaterialArchive *self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::MaterialArchive const *self = TOINTERNAL_CONST(inst)

#undef MaterialArchive_New
MaterialArchive *MaterialArchive_New(int useSegments)
{
    return reinterpret_cast<MaterialArchive *>(new de::MaterialArchive(useSegments));
}

#undef MaterialArchive_NewEmpty
MaterialArchive *MaterialArchive_NewEmpty(int useSegments)
{
    return reinterpret_cast<MaterialArchive *>(new de::MaterialArchive(useSegments, false /*don't populate*/));
}

#undef MaterialArchive_Delete
void MaterialArchive_Delete(MaterialArchive *arc)
{
    if(arc)
    {
        SELF(arc);
        delete self;
    }
}

#undef MaterialArchive_FindUniqueSerialId
materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(MaterialArchive *arc, struct material_s *mat)
{
    SELF(arc);
    return self->findUniqueSerialId(mat);
}

#undef MaterialArchive_Find
struct material_s *MaterialArchive_Find(MaterialArchive *arc, materialarchive_serialid_t serialId, int group)
{
    SELF(arc);
    return self->find(serialId, group);
}

#undef MaterialArchive_Count
size_t MaterialArchive_Count(MaterialArchive *arc)
{
    SELF(arc);
    return self->count();
}

#undef MaterialArchive_Write
void MaterialArchive_Write(MaterialArchive *arc, Writer *writer)
{
    SELF(arc);
    self->write(*writer);
}

#undef MaterialArchive_Read
void MaterialArchive_Read(MaterialArchive *arc, int forcedVersion, Reader *reader)
{
    SELF(arc);
    self->read(forcedVersion, *reader);
}

DENG_DECLARE_API(MaterialArchive) =
{
    { DE_API_MATERIAL_ARCHIVE },
    MaterialArchive_New,
    MaterialArchive_NewEmpty,
    MaterialArchive_Delete,
    MaterialArchive_FindUniqueSerialId,
    MaterialArchive_Find,
    MaterialArchive_Count,
    MaterialArchive_Write,
    MaterialArchive_Read
};
