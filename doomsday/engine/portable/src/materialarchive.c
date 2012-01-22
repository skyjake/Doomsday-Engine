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

#include "de_base.h"
#include "de_console.h"
#include "materials.h"
#include "materialarchive.h"

/// For identifying the archived format version. Written to disk.
#define MATERIALARCHIVE_VERSION (4)

#define ASEG_MATERIAL_ARCHIVE   112

// Used to denote unknown Material references in records. Written to disk.
#define UNKNOWN_MATERIALNAME    "DD_BADTX"

typedef struct materialarchive_record_s {
    Uri* uri; ///< Percent encoded.
    material_t* material;
} materialarchive_record_t;

struct materialarchive_s {
    int version;
    uint count;
    materialarchive_record_t* table;
    uint numFlats;          /// Used with older versions.
    boolean useSegments;    /// Segment id assertion (Hexen saves).
};

static MaterialArchive* create(void)
{
    return malloc(sizeof(MaterialArchive));
}

static void clearTable(MaterialArchive* mArc)
{
    if(mArc->table)
    {
        uint i;
        for(i = 0; i < mArc->count; ++i)
        {
            Uri_Delete(mArc->table[i].uri);
        }
        free(mArc->table);
        mArc->table = 0;
    }
    mArc->count = 0;
}

static void destroy(MaterialArchive* mArc)
{
    clearTable(mArc);
    free(mArc);
}

static void init(MaterialArchive* mArc, int useSegments)
{
    mArc->version = MATERIALARCHIVE_VERSION;
    mArc->numFlats = 0;
    mArc->count = 0;
    mArc->table = 0;
    mArc->useSegments = useSegments;
}

static void insertSerialId(MaterialArchive* mArc, materialarchive_serialid_t serialId,
    const Uri* uri, material_t* material)
{
    materialarchive_record_t* rec;

    mArc->table = realloc(mArc->table, ++mArc->count * sizeof(materialarchive_record_t));
    rec = &mArc->table[mArc->count-1];

    rec->uri = Uri_NewCopy(uri);
    rec->material = material;
}

static materialarchive_serialid_t insertSerialIdForMaterial(MaterialArchive* mArc,
    material_t* mat)
{
    Uri* uri = Materials_ComposeUri(Materials_Id(mat));
    // Insert a new element in the index.
    insertSerialId(mArc, mArc->count+1, uri, mat);
    Uri_Delete(uri);
    return mArc->count; // 1-based index.
}

static materialarchive_serialid_t getSerialIdForMaterial(MaterialArchive* mArc, material_t* mat)
{
    materialarchive_serialid_t id = 0;
    uint i;
    for(i = 0; i < mArc->count; ++i)
    {
        const materialarchive_record_t* rec = &mArc->table[i];
        if(rec->material == mat)
        {
            id = i + 1; // Yes. Return existing serial.
            break;
        }
    }
    return id;
}

static materialarchive_record_t* getRecord(const MaterialArchive* mArc,
    materialarchive_serialid_t serialId, int group)
{
    materialarchive_record_t* rec;

    if(mArc->version < 1 && group == 1) // Group 1 = Flats:
        serialId += mArc->numFlats;
    rec = &mArc->table[serialId];
    if(!Str_CompareIgnoreCase(Uri_Path(rec->uri), UNKNOWN_MATERIALNAME))
        return 0;
    return rec;
}

static material_t* materialForSerialId(const MaterialArchive* mArc,
    materialarchive_serialid_t serialId, int group)
{
    materialarchive_record_t* rec;
    assert(serialId <= mArc->count+1);
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
static void populate(MaterialArchive* mArc)
{
    Uri* unknownMaterial = Uri_NewWithPath2(UNKNOWN_MATERIALNAME, RC_NULL);
    insertSerialId(mArc, 1, unknownMaterial, 0);
    Uri_Delete(unknownMaterial);

    { uint i, num = Materials_Size();
    for(i = 1; i < num+1; ++i)
    {
        /// @todo Assumes knowledge of how material ids are generated.
        /// Should be iterated by Materials using a callback function.
        insertSerialIdForMaterial(mArc, Materials_ToMaterial(i));
    }}
}

static int writeRecord(const MaterialArchive* mArc, materialarchive_record_t* rec, Writer* writer)
{
    Uri_Write(rec->uri, writer);
    return true; // Continue iteration.
}

static int readRecord(MaterialArchive* mArc, materialarchive_record_t* rec, Reader* reader)
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
        ddstring_t* path = Str_NewFromReader(reader);
        Uri_SetUri3(rec->uri, Str_Text(path), RC_NULL);
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
        ddstring_t path;
        char name[9];
        byte oldMNI;

        Reader_Read(reader, name, 8);
        name[8] = 0;

        Str_Init(&path);
        Str_PercentEncode(Str_StripRight(Str_Set(&path, name)));

        oldMNI = Reader_ReadByte(reader);
        switch(oldMNI % 4)
        {
        case 0: Uri_SetScheme(rec->uri, MN_TEXTURES_NAME); break;
        case 1: Uri_SetScheme(rec->uri, MN_FLATS_NAME);    break;
        case 2: Uri_SetScheme(rec->uri, MN_SPRITES_NAME);  break;
        case 3: Uri_SetScheme(rec->uri, MN_SYSTEM_NAME);   break;
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
static int readRecord_v186(materialarchive_record_t* rec, const char* mnamespace, Reader* reader)
{
    char buf[9];
    ddstring_t path;
    Reader_Read(reader, buf, 8);
    buf[8] = 0;
    if(!rec->uri)
        rec->uri = Uri_New();
    Str_Init(&path);
    Str_PercentEncode(Str_StripRight(Str_Appendf(&path, "%s", buf)));
    Uri_SetPath(rec->uri, Str_Text(&path));
    Uri_SetScheme(rec->uri, mnamespace);
    Str_Free(&path);
    return true; // Continue iteration.
}

static void readMaterialGroup(MaterialArchive* mArc, const char* defaultNamespace, Reader *reader)
{
    // Read the group header.
    uint num = Reader_ReadUInt16(reader);
    uint i;
    for(i = 0; i < num; ++i)
    {
        materialarchive_record_t temp;
        memset(&temp, 0, sizeof(temp));

        if(mArc->version >= 1)
            readRecord(mArc, &temp, reader);
        else
            readRecord_v186(&temp, mArc->version <= 1? defaultNamespace : 0, reader);

        insertSerialId(mArc, mArc->count+1, temp.uri, 0);
        if(temp.uri)
            Uri_Delete(temp.uri);
    }
}

static void writeMaterialGroup(const MaterialArchive* mArc, Writer* writer)
{
    // Write the group header.
    Writer_WriteUInt16(writer, mArc->count);

    {uint i;
    for(i = 0; i < mArc->count; ++i)
    {
        writeRecord(mArc, &mArc->table[i], writer);
    }}
}

static void beginSegment(const MaterialArchive* mArc, int seg, Writer* writer)
{
    if(mArc->useSegments)
    {
        Writer_WriteUInt32(writer, seg);
    }
}

static void assertSegment(MaterialArchive* mArc, int seg, Reader* reader)
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

static void writeHeader(const MaterialArchive* mArc, Writer* writer)
{
    beginSegment(mArc, ASEG_MATERIAL_ARCHIVE, writer);
    Writer_WriteByte(writer, mArc->version);
}

static void readHeader(MaterialArchive* mArc, Reader* reader)
{
    assertSegment(mArc, ASEG_MATERIAL_ARCHIVE, reader);
    mArc->version = Reader_ReadByte(reader);
}

MaterialArchive* MaterialArchive_New(int useSegments)
{
    MaterialArchive* mArc = create();
    init(mArc, useSegments);
    populate(mArc);
    return mArc;
}

MaterialArchive* MaterialArchive_NewEmpty(int useSegments)
{
    MaterialArchive* mArc = create();
    init(mArc, useSegments);
    return mArc;
}

void MaterialArchive_Delete(MaterialArchive* arc)
{
    if(arc)
    {
        destroy(arc);
    }
}

materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(MaterialArchive* arc, struct material_s* material)
{
    assert(arc);
    if(material)
        return getSerialIdForMaterial(arc, material);
    return 0; // Invalid.
}

struct material_s* MaterialArchive_Find(MaterialArchive* arc, materialarchive_serialid_t serialId, int group)
{
    assert(arc);
    return materialForSerialId(arc, serialId, group);
}

size_t MaterialArchive_Count(MaterialArchive* arc)
{
    assert(arc);
    return arc->count;
}

void MaterialArchive_Write(MaterialArchive* arc, Writer* writer)
{
    assert(arc);
    writeHeader(arc, writer);
    writeMaterialGroup(arc, writer);
}

void MaterialArchive_Read(MaterialArchive* arc, int forcedVersion, Reader* reader)
{
    assert(arc);
    clearTable(arc);

    readHeader(arc, reader);

    // Are we interpreting a specific version?
    if(forcedVersion >= 0)
    {
        arc->version = forcedVersion;
    }

    arc->count = 0;
    readMaterialGroup(arc, (forcedVersion >= 1? "" : MN_FLATS_NAME":"), reader);

    if(arc->version == 0)
    {
        // The old format saved flats and textures in seperate groups.
        arc->numFlats = arc->count;
        readMaterialGroup(arc, (forcedVersion >= 1? "" : MN_TEXTURES_NAME":"), reader);
    }
}
