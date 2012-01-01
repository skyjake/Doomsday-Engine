/**\file p_svtexarc.c
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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_saveg.h"

#include "p_svtexarc.h"

// For identifying the archived format version. Written to disk.
#define MATERIALARCHIVE_VERSION (2)

// Used to denote unknown Material references in records. Written to disk.
#define UNKNOWN_MATERIALNAME    "DD_BADTX"

typedef struct materialarchive_record_s {
    Uri* uri;
    material_t* material;
} materialarchive_record_t;

static materialarchive_t* create(void)
{
    return malloc(sizeof(materialarchive_t));
}

static void destroy(materialarchive_t* mArc)
{
    if(mArc->table)
    {
        uint i;
        for(i = 0; i < mArc->count; ++i)
            Uri_Delete(mArc->table[i].uri);
        free(mArc->table);
    }
    free(mArc);
}

static void init(materialarchive_t* mArc)
{
    mArc->version = MATERIALARCHIVE_VERSION;
    mArc->numFlats = 0;
    mArc->count = 0;
    mArc->table = 0;
}

static void insertSerialId(materialarchive_t* mArc, materialarchive_serialid_t serialId,
    const Uri* uri, material_t* material)
{
    materialarchive_record_t* rec;

    mArc->table = realloc(mArc->table, ++mArc->count * sizeof(materialarchive_record_t));
    rec = &mArc->table[mArc->count-1];

    rec->uri = Uri_NewCopy(uri);
    rec->material = material;
}

static materialarchive_serialid_t insertSerialIdForMaterial(materialarchive_t* mArc,
    material_t* mat)
{
    Uri* uri = Materials_ComposeUri(P_ToIndex(mat));
    // Insert a new element in the index.
    insertSerialId(mArc, mArc->count+1, uri, mat);
    Uri_Delete(uri);
    return mArc->count; // 1-based index.
}

static materialarchive_serialid_t getSerialIdForMaterial(materialarchive_t* mArc,
    material_t* mat)
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

static materialarchive_record_t* getRecord(const materialarchive_t* mArc,
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

static material_t* materialForSerialId(const materialarchive_t* mArc,
    materialarchive_serialid_t serialId, int group)
{
    materialarchive_record_t* rec;
    assert(serialId <= mArc->count+1);
    if(serialId != 0 && (rec = getRecord(mArc, serialId-1, group)))
    {
        if(!rec->material)
            rec->material = P_ToPtr(DMU_MATERIAL, Materials_ResolveUri(rec->uri));
        return rec->material;
    }
    return NULL;
}

/**
 * Populate the archive using the global Materials list.
 */
static void populate(materialarchive_t* mArc)
{
    Uri* unknownMaterial = Uri_NewWithPath2(UNKNOWN_MATERIALNAME, RC_NULL);
    insertSerialId(mArc, 1, unknownMaterial, 0);
    Uri_Delete(unknownMaterial);

    { uint i, num = nummaterials;
    for(i = 1; i < num+1; ++i)
    {
        insertSerialIdForMaterial(mArc, P_ToPtr(DMU_MATERIAL, i));
    }}
}

static int writeRecord(const materialarchive_t* mArc, materialarchive_record_t* rec)
{
    ddstring_t* path = Uri_ComposePath(rec->uri);
    int length = (int) Str_Length(path);
    SV_WriteLong(length);
    SV_Write(Str_Text(path), length);
    Str_Delete(path);
    return true; // Continue iteration.
}

static int readRecord(materialarchive_t* mArc, materialarchive_record_t* rec)
{
    if(mArc->version >= 2)
    {
        int length = SV_ReadLong();
        char* buf = malloc(length + 1);
        buf[length] = 0;
        SV_Read(buf, length);
        if(!rec->uri)
            rec->uri = Uri_New();
        Uri_SetUri3(rec->uri, buf, RC_NULL);
        free(buf);
    }
    else
    {
        char name[9];
        byte oldMNI;

        SV_Read(name, 8);
        name[8] = 0;

        if(!rec->uri)
            rec->uri = Uri_New();

        oldMNI = SV_ReadByte();
        switch(oldMNI % 4)
        {
        case 0: Uri_SetScheme(rec->uri, MN_TEXTURES_NAME); break;
        case 1: Uri_SetScheme(rec->uri, MN_FLATS_NAME);    break;
        case 2: Uri_SetScheme(rec->uri, MN_SPRITES_NAME);  break;
        case 3: Uri_SetScheme(rec->uri, MN_SYSTEM_NAME);   break;
        }
        Uri_SetPath(rec->uri, name);
    }
    return true; // Continue iteration.
}

/**
 * Same as readRecord except we are reading the old record format used
 * by Doomsday 1.8.6 and earlier.
 */
static int readRecord_v186(materialarchive_record_t* rec, const char* mnamespace)
{
    char buf[9];
    ddstring_t path;
    SV_Read(buf, 8);
    buf[8] = 0;
    if(!rec->uri)
        rec->uri = Uri_New();
    Str_Init(&path);
    Str_Appendf(&path, "%s:%s", mnamespace, buf);
    Uri_SetUri(rec->uri, &path);
    Str_Free(&path);
    return true; // Continue iteration.
}

static void readMaterialGroup(materialarchive_t* mArc, const char* defaultNamespace)
{
    // Read the group header.
    uint num = SV_ReadShort();

    {uint i;
    for(i = 0; i < num; ++i)
    {
        materialarchive_record_t temp;
        memset(&temp, 0, sizeof(temp));

        if(mArc->version >= 1)
            readRecord(mArc, &temp);
        else
            readRecord_v186(&temp, mArc->version <= 1? defaultNamespace : 0);

        insertSerialId(mArc, mArc->count+1, temp.uri, 0);
        if(temp.uri)
            Uri_Delete(temp.uri);
    }}
}

static void writeMaterialGroup(const materialarchive_t* mArc)
{
    // Write the group header.
    SV_WriteShort(mArc->count);

    {uint i;
    for(i = 0; i < mArc->count; ++i)
    {
        writeRecord(mArc, &mArc->table[i]);
    }}
}

static void writeHeader(const materialarchive_t* mArc)
{
    SV_BeginSegment(ASEG_MATERIAL_ARCHIVE);
    SV_WriteByte(mArc->version);
}

static void readHeader(materialarchive_t* mArc)
{
    SV_AssertSegment(ASEG_MATERIAL_ARCHIVE);
    mArc->version = SV_ReadByte();
}

materialarchive_t* P_CreateMaterialArchive(void)
{
    materialarchive_t* mArc = create();
    init(mArc);
    populate(mArc);
    return mArc;
}

materialarchive_t* P_CreateEmptyMaterialArchive(void)
{
    materialarchive_t* mArc = create();
    init(mArc);
    return mArc;
}

void P_DestroyMaterialArchive(materialarchive_t* materialArchive)
{
    if(!materialArchive)
        return;
    destroy(materialArchive);
}

/**
 * @return                  A new (unused) SerialId for the specified material.
 */
materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(materialarchive_t* materialArchive,
    material_t* material)
{
    assert(materialArchive);
    if(material)
        return getSerialIdForMaterial(materialArchive, material);
    return 0; // Invalid.
}

material_t* MaterialArchive_Find(materialarchive_t* materialArchive,
    materialarchive_serialid_t serialId, int group)
{
    assert(materialArchive);
    return materialForSerialId(materialArchive, serialId, group);
}

void MaterialArchive_Write(materialarchive_t* materialArchive)
{
    assert(materialArchive);
    writeHeader(materialArchive);
    writeMaterialGroup(materialArchive);
}

/**
 * @param version           Version to interpret as, not actual format version.
 */
void MaterialArchive_Read(materialarchive_t* materialArchive, int version)
{
    assert(materialArchive);

    readHeader(materialArchive);

    // Are we interpreting a specific version?
    if(version >= 0)
    {
        materialArchive->version = version;
    }

    materialArchive->count = 0;
    readMaterialGroup(materialArchive, (version >= 1? "" : MN_FLATS_NAME":"));

    if(materialArchive->version == 0)
    {   // The old format saved flats and textures in seperate groups.
        materialArchive->numFlats = materialArchive->count;
        readMaterialGroup(materialArchive, (version >= 1? "" : MN_TEXTURES_NAME":"));
    }
}

#if _DEBUG
void MaterialArchive_Print(const materialarchive_t* materialArchive)
{
    assert(materialArchive);
    {
    uint i;
    Con_Printf("MaterialArchiveRecords: num#%i\n", materialArchive->count);
    for(i = 0; i < materialArchive->count; ++i)
    {
        materialarchive_record_t* rec = &materialArchive->table[i];
        ddstring_t* path = rec->uri? Uri_ToString(rec->uri) : 0;
        Con_Printf("[%u]: %s\n", i, path? Str_Text(path) : "");
        Str_Delete(path);
    }
    Con_Printf("End");
    }
}
#endif

