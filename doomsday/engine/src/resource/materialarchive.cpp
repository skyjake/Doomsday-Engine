/** @file materialarchive.h Material Archive.
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

struct materialarchive_record_t
{
    Uri *uri; ///< Percent encoded.
    material_t *material;
};

struct materialarchive_s
{
    int version;
    uint count;
    materialarchive_record_t *table;
    uint numFlats; /// Used with older versions.
    bool useSegments; /// Segment id DENG_ASSERTion (Hexen saves).
};

static MaterialArchive *create()
{
    return (MaterialArchive *) M_Malloc(sizeof(MaterialArchive));
}

static void clearTable(MaterialArchive *mArc)
{
    if(mArc->table)
    {
        for(uint i = 0; i < mArc->count; ++i)
        {
            Uri_Delete(mArc->table[i].uri);
        }
        M_Free(mArc->table);
        mArc->table = 0;
    }
    mArc->count = 0;
}

static void destroy(MaterialArchive *mArc)
{
    clearTable(mArc);
    M_Free(mArc);
}

static void init(MaterialArchive *mArc, int useSegments)
{
    mArc->version = MATERIALARCHIVE_VERSION;
    mArc->numFlats = 0;
    mArc->count = 0;
    mArc->table = 0;
    mArc->useSegments = useSegments;
}

static void insertSerialId(MaterialArchive *mArc, materialarchive_serialid_t /*serialId*/,
    Uri const *uri, material_t *material)
{
    mArc->table = (materialarchive_record_t *) M_Realloc(mArc->table, ++mArc->count * sizeof(materialarchive_record_t));
    materialarchive_record_t *rec = &mArc->table[mArc->count - 1];

    rec->uri = Uri_Dup(uri);
    rec->material = material;
}

static materialarchive_serialid_t insertSerialIdForMaterial(MaterialArchive *mArc,
    material_t *mat)
{
    Uri *uri = Materials_ComposeUri(Materials_Id(mat));
    // Insert a new element in the index.
    insertSerialId(mArc, mArc->count+1, uri, mat);
    Uri_Delete(uri);
    return mArc->count; // 1-based index.
}

static materialarchive_serialid_t getSerialIdForMaterial(MaterialArchive *mArc, material_t *mat)
{
    materialarchive_serialid_t id = 0;
    for(uint i = 0; i < mArc->count; ++i)
    {
        materialarchive_record_t const *rec = &mArc->table[i];
        if(rec->material == mat)
        {
            id = i + 1; // Yes. Return existing serial.
            break;
        }
    }
    return id;
}

static materialarchive_record_t *getRecord(MaterialArchive const *mArc,
    materialarchive_serialid_t serialId, int group)
{
    if(mArc->version < 1 && group == 1) // Group 1 = Flats:
        serialId += mArc->numFlats;

    materialarchive_record_t *rec = &mArc->table[serialId];
    if(!Str_CompareIgnoreCase(Uri_Path(rec->uri), UNKNOWN_MATERIALNAME))
        return 0;
    return rec;
}

static material_t *materialForSerialId(MaterialArchive const *mArc,
    materialarchive_serialid_t serialId, int group)
{
    DENG_ASSERT(serialId <= mArc->count+1);

    materialarchive_record_t *rec;
    if(serialId != 0 && (rec = getRecord(mArc, serialId-1, group)))
    {
        if(!rec->material)
            rec->material = Materials_ToMaterial(Materials_ResolveUri(rec->uri));
        return rec->material;
    }
    return NULL;
}

/**
 * Populate the archive using the global Materials list.
 */
static void populate(MaterialArchive *mArc)
{
    Uri *unknownMaterial = Uri_NewWithPath2(UNKNOWN_MATERIALNAME, RC_NULL);
    insertSerialId(mArc, 1, unknownMaterial, 0);
    Uri_Delete(unknownMaterial);

    uint num = Materials_Size();
    for(uint i = 1; i < num + 1; ++i)
    {
        /// @todo Assumes knowledge of how material ids are generated.
        /// Should be iterated by Materials using a callback function.
        insertSerialIdForMaterial(mArc, Materials_ToMaterial(i));
    }
}

static int writeRecord(MaterialArchive const * /*mArc*/, materialarchive_record_t *rec, Writer *writer)
{
    Uri_Write(rec->uri, writer);
    return true; // Continue iteration.
}

static int readRecord(MaterialArchive *mArc, materialarchive_record_t *rec, Reader *reader)
{
    if(!rec->uri)
    {
        rec->uri = Uri_New();
    }

    if(mArc->version >= 4)
    {
        Uri_Read(rec->uri, reader);
    }
    else if(mArc->version >= 2)
    {
        ddstring_t *path = Str_NewFromReader(reader);
        Uri_SetUri2(rec->uri, Str_Text(path), RC_NULL);
        if(mArc->version == 2)
        {
            // We must encode the path.
            Str_PercentEncode(Str_Set(path, Str_Text(Uri_Path(rec->uri))));
            Uri_SetPath(rec->uri, Str_Text(path));
        }
        Str_Delete(path);
    }
    else
    {
        char name[9];
        Reader_Read(reader, name, 8);
        name[8] = 0;

        ddstring_t path; Str_Init(&path);
        Str_PercentEncode(Str_StripRight(Str_Set(&path, name)));

        byte oldMNI = Reader_ReadByte(reader);
        switch(oldMNI % 4)
        {
        case 0: Uri_SetScheme(rec->uri, "Textures"); break;
        case 1: Uri_SetScheme(rec->uri, "Flats");    break;
        case 2: Uri_SetScheme(rec->uri, "Sprites");  break;
        case 3: Uri_SetScheme(rec->uri, "System");   break;
        }
        Uri_SetPath(rec->uri, Str_Text(&path));
        Str_Free(&path);
    }
    return true; // Continue iteration.
}

/**
 * Same as readRecord except we are reading the old record format used
 * by Doomsday 1.8.6 and earlier.
 */
static int readRecord_v186(materialarchive_record_t *rec, char const *scheme, Reader *reader)
{
    char buf[9];
    Reader_Read(reader, buf, 8);
    buf[8] = 0;

    if(!rec->uri)
        rec->uri = Uri_New();

    ddstring_t path; Str_Init(&path);
    Str_PercentEncode(Str_StripRight(Str_Appendf(&path, "%s", buf)));
    Uri_SetPath(rec->uri, Str_Text(&path));
    Uri_SetScheme(rec->uri, scheme);

    Str_Free(&path);
    return true; // Continue iteration.
}

static void readMaterialGroup(MaterialArchive* mArc, const char* defaultScheme, Reader *reader)
{
    // Read the group header.
    uint num = Reader_ReadUInt16(reader);
    for(uint i = 0; i < num; ++i)
    {
        materialarchive_record_t temp;
        std::memset(&temp, 0, sizeof(temp));

        if(mArc->version >= 1)
            readRecord(mArc, &temp, reader);
        else
            readRecord_v186(&temp, mArc->version <= 1? defaultScheme : 0, reader);

        insertSerialId(mArc, mArc->count+1, temp.uri, 0);
        if(temp.uri)
            Uri_Delete(temp.uri);
    }
}

static void writeMaterialGroup(MaterialArchive const *mArc, Writer *writer)
{
    // Write the group header.
    Writer_WriteUInt16(writer, mArc->count);

    for(uint i = 0; i < mArc->count; ++i)
    {
        writeRecord(mArc, &mArc->table[i], writer);
    }
}

static void beginSegment(MaterialArchive const *mArc, int seg, Writer *writer)
{
    if(mArc->useSegments)
    {
        Writer_WriteUInt32(writer, seg);
    }
}

static void assertSegment(MaterialArchive *mArc, int seg, Reader *reader)
{
    if(mArc->useSegments)
    {
        int i = Reader_ReadUInt32(reader);
        if(i != seg)
        {
            Con_Error("MaterialArchive: Expected ASEG_MATERIAL_ARCHIVE (%i), but got %i.\n",
                      ASEG_MATERIAL_ARCHIVE, i);
        }
    }
}

static void writeHeader(MaterialArchive const *mArc, Writer *writer)
{
    beginSegment(mArc, ASEG_MATERIAL_ARCHIVE, writer);
    Writer_WriteByte(writer, mArc->version);
}

static void readHeader(MaterialArchive *mArc, Reader *reader)
{
    assertSegment(mArc, ASEG_MATERIAL_ARCHIVE, reader);
    mArc->version = Reader_ReadByte(reader);
}

MaterialArchive *MaterialArchive_New(int useSegments)
{
    MaterialArchive* mArc = create();
    init(mArc, useSegments);
    populate(mArc);
    return mArc;
}

MaterialArchive *MaterialArchive_NewEmpty(int useSegments)
{
    MaterialArchive *mArc = create();
    init(mArc, useSegments);
    return mArc;
}

void MaterialArchive_Delete(MaterialArchive *arc)
{
    if(arc)
    {
        destroy(arc);
    }
}

materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(MaterialArchive *arc, material_t *material)
{
    DENG_ASSERT(arc);
    if(material)
        return getSerialIdForMaterial(arc, material);
    return 0; // Invalid.
}

material_t *MaterialArchive_Find(MaterialArchive *arc, materialarchive_serialid_t serialId, int group)
{
    DENG_ASSERT(arc);
    return materialForSerialId(arc, serialId, group);
}

size_t MaterialArchive_Count(MaterialArchive *arc)
{
    DENG_ASSERT(arc);
    return arc->count;
}

void MaterialArchive_Write(MaterialArchive *arc, Writer *writer)
{
    DENG_ASSERT(arc);
    writeHeader(arc, writer);
    writeMaterialGroup(arc, writer);
}

void MaterialArchive_Read(MaterialArchive *arc, int forcedVersion, Reader *reader)
{
    DENG_ASSERT(arc);
    clearTable(arc);

    readHeader(arc, reader);

    // Are we interpreting a specific version?
    if(forcedVersion >= 0)
    {
        arc->version = forcedVersion;
    }

    arc->count = 0;
    readMaterialGroup(arc, (forcedVersion >= 1? "" : "Flats:"), reader);

    if(arc->version == 0)
    {
        // The old format saved flats and textures in seperate groups.
        arc->numFlats = arc->count;
        readMaterialGroup(arc, (forcedVersion >= 1? "" : "Textures:"), reader);
    }
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
