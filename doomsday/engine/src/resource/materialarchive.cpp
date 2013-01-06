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

#include "de_base.h"
#include "de_console.h"
#include "uri.hh"

#include "resource/materials.h"
#include "resource/materialarchive.h"

/// For identifying the archived format version. Written to disk.
#define MATERIALARCHIVE_VERSION (4)

#define ASEG_MATERIAL_ARCHIVE   112

// Used to denote unknown Material references in records. Written to disk.
#define UNKNOWN_MATERIALNAME    "DD_BADTX"

namespace de {

struct ArchiveRecord
{
    Uri uri; ///< Percent encoded.
    material_t *material_;
    bool foundMaterial;

    ArchiveRecord() : uri(), material_(0), foundMaterial(false)
    {}

    ArchiveRecord(Uri const &_uri)
        : uri(_uri), material_(0), foundMaterial(false)
    {}

    bool hasMaterial()
    {
        return !!material_;
    }

    material_t *material()
    {
        if(!foundMaterial)
        {
            setMaterial(App_Materials()->find(uri).material());
        }
        return material_;
    }

    void setMaterial(material_t *mat)
    {
        material_ = mat;
        foundMaterial = true;
    }

    void write(writer_s &writer) const
    {
        Uri_Write(reinterpret_cast<uri_s const *>(&uri), &writer);
    }

    void read(int version, reader_s &reader)
    {
        if(version >= 4)
        {
            // A serialized URI.
            Uri_Read(reinterpret_cast<uri_s *>(&uri), &reader);
        }
        else if(version == 3)
        {
            // A percent encoded textual URI.
            ddstring_t *_uri = Str_NewFromReader(&reader);
            uri.setUri(Str_Text(_uri), RC_NULL);
            Str_Delete(_uri);
        }
        else if(version == 2)
        {
            // An unencoded textual URI.
            ddstring_t *_uri = Str_NewFromReader(&reader);
            uri.setUri(QString(QByteArray(Str_Text(_uri), Str_Length(_uri)).toPercentEncoding()), RC_NULL);
            Str_Delete(_uri);
        }
        else
        {
            // A short textual name (unencoded) plus legacy material scheme id.
            char path[9];
            Reader_Read(&reader, path, 8);
            path[8] = 0;
            uri.setPath(QByteArray(path, qstrlen(path)).toPercentEncoding());

            byte oldMNI = Reader_ReadByte(&reader);
            switch(oldMNI % 4)
            {
            case 0: uri.setScheme("Textures"); break;
            case 1: uri.setScheme("Flats");    break;
            case 2: uri.setScheme("Sprites");  break;
            case 3: uri.setScheme("System");   break;
            }
        }
    }

    /**
     * Same as read except we are reading the old record format used by
     * Doomsday 1.8.6 and earlier.
     */
    void read_v186(String scheme, reader_s &reader)
    {
        char path[9];
        Reader_Read(&reader, path, 8);
        path[8] = 0;
        uri.setPath(QByteArray(path, qstrlen(path)).toPercentEncoding());
        uri.setScheme(scheme);
    }
};

/// A list of archive records.
typedef QList<ArchiveRecord *> Records;

struct MaterialArchive::Instance
{
    int version;
    Records table;
    int numFlats; /// Used with older versions.
    int useSegments; /// Segment id assertion (Hexen saves).

    Instance(int _useSegments)
        : version(MATERIALARCHIVE_VERSION),
          numFlats(0), useSegments(_useSegments)
    {}

    void clearTable()
    {
        DENG2_FOR_EACH(Records, i, table)
        {
            delete *i;
        }
    }

    ArchiveRecord &insertRecord(materialarchive_serialid_t /*serialId*/, Uri const &uri)
    {
        ArchiveRecord *rec = new ArchiveRecord(uri);
        table.push_back(rec);
        return *rec;
    }

    ArchiveRecord *record(materialarchive_serialid_t serialId, int group)
    {
        if(version < 1 && group == 1) // Group 1 = Flats:
            serialId += numFlats;

        DENG_ASSERT(serialId >= 0 && serialId < table.count());

        ArchiveRecord *rec = table[serialId];
        if(!rec->uri.path().toStringRef().compareWithoutCase(UNKNOWN_MATERIALNAME))
            return 0;
        return rec;
    }

    /**
     * Populate the archive using the global Materials list.
     */
    void populate()
    {
        ArchiveRecord &record = insertRecord(1, Uri(Path(UNKNOWN_MATERIALNAME)));
        record.setMaterial(0);

        /// @todo Assumes knowledge of how material ids are generated.
        /// Should be iterated by Materials using a callback function.
        uint num = App_Materials()->count();
        for(uint i = 1; i < num + 1; ++i)
        {
            MaterialBind *bind = App_Materials()->toMaterialBind(i);
            ArchiveRecord &record = insertRecord(table.count() + 1, bind->composeUri());
            record.setMaterial(bind->material());
        }
    }

    void readGroup(String defaultScheme, reader_s &reader)
    {
        // Read the group header.
        uint num = Reader_ReadUInt16(&reader);
        for(uint i = 0; i < num; ++i)
        {
            ArchiveRecord temp;

            if(version >= 1)
                temp.read(version, reader);
            else
                temp.read_v186(defaultScheme, reader);

            insertRecord(table.count() + 1, temp.uri);
        }
    }

    void writeGroup(writer_s &writer)
    {
        // Write the group header.
        Writer_WriteUInt16(&writer, table.count());

        DENG2_FOR_EACH_CONST(Records, i, table)
        {
            (*i)->write(writer);
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
        if(!useSegments) return;

        int i = Reader_ReadUInt32(&reader);
        if(i != seg)
        {
            throw MaterialArchive::ReadError("MaterialArchive::assertSegment",
                                             QString("Expected ASEG_MATERIAL_ARCHIVE (%1), but got %2")
                                                 .arg(ASEG_MATERIAL_ARCHIVE).arg(i));
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
    d->clearTable();
    delete d;
}

materialarchive_serialid_t MaterialArchive::findUniqueSerialId(material_t *material) const
{
    if(!material) return 0; // Invalid.
    materialarchive_serialid_t id = 0;
    for(int i = 0; i < d->table.count(); ++i)
    {
        ArchiveRecord *rec = d->table[i];
        // Lookup the material for the record's uri.
        if(rec->material() == material)
        {
            id = i + 1; // Yes. Return existing serial.
            break;
        }
    }
    return id;
}

material_t *MaterialArchive::find(materialarchive_serialid_t serialId, int group) const
{
    if(serialId <= 0 || serialId > d->table.count() + 1) return 0; // Invalid.
    // Lookup the material for the record's uri.
    return d->record(serialId - 1, group)->material();
}

int MaterialArchive::count() const
{
    return d->table.count();
}

void MaterialArchive::write(writer_s &writer) const
{
    d->writeHeader(writer);
    d->writeGroup(writer);
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

    d->readGroup((forcedVersion >= 1? "" : "Flats:"), reader);

    if(d->version == 0)
    {
        // The old format saved flats and textures in seperate groups.
        d->numFlats = d->table.count();
        d->readGroup((forcedVersion >= 1? "" : "Textures:"), reader);
    }
}

} // namespace de

/*
 * C Wrapper API:
 */

#define DENG_NO_API_MACROS_MATERIAL_ARCHIVE

#include "api_materialarchive.h"

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
materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(MaterialArchive const *arc, struct material_s *mat)
{
    SELF_CONST(arc);
    return self->findUniqueSerialId(mat);
}

#undef MaterialArchive_Find
struct material_s *MaterialArchive_Find(MaterialArchive const *arc, materialarchive_serialid_t serialId, int group)
{
    SELF_CONST(arc);
    return self->find(serialId, group);
}

#undef MaterialArchive_Count
int MaterialArchive_Count(MaterialArchive const *arc)
{
    SELF_CONST(arc);
    return self->count();
}

#undef MaterialArchive_Write
void MaterialArchive_Write(MaterialArchive const *arc, Writer *writer)
{
    SELF_CONST(arc);
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
