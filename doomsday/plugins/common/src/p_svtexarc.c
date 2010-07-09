/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
#define MATERIAL_ARCHIVE_VERSION (1)

// Used to denote unknown Material references in records. Written to disk.
#define UNKNOWN_MATERIAL_NAME   "DD_BADTX"  

typedef struct materialarchive_record_s {
    char            name[9];
    material_namespace_t mnamespace;
} materialarchive_record_t;

static materialarchive_t* create(void)
{
    return malloc(sizeof(materialarchive_t));
}

static void destroy(materialarchive_t* mArc)
{
    if(mArc->table)
        free(mArc->table);
    free(mArc);
}

static void init(materialarchive_t* mArc)
{
    mArc->version = MATERIAL_ARCHIVE_VERSION;
    mArc->numFlats = 0;
    mArc->count = 0;
    mArc->table = 0;
}

static void insertSerialId(materialarchive_t* mArc, materialarchive_serialid_t serialId,
    const char* name, material_namespace_t mnamespace)
{
    assert(name);
    {
    materialarchive_record_t* rec;

    mArc->table = realloc(mArc->table, ++mArc->count * sizeof(materialarchive_record_t));
    rec = &mArc->table[mArc->count-1];

    memset(rec->name, 0, 9);
    dd_snprintf(rec->name, 9, "%s", name);
    rec->mnamespace = mnamespace;
    }
}

static materialarchive_serialid_t serialIdForMaterial(materialarchive_t* mArc,
    material_t* mat, boolean canCreate)
{
    assert(mat);
    {
    static const char* unknownMaterialName = UNKNOWN_MATERIAL_NAME;
    material_namespace_t mnamespace = P_GetIntp(mat, DMU_NAMESPACE);
    const char* name = Materials_GetName(mat);

    if(!name)
        name = unknownMaterialName;

    // Has this already been registered? 
    {uint i;
    for(i = 0; i < mArc->count; ++i)
    {
        const materialarchive_record_t* rec = &mArc->table[i];
        if(rec->mnamespace == mnamespace && !stricmp(rec->name, name))
            return i + 1; // Yes. Return existing serial.
    }}

    if(!canCreate)
        return 0; // Not found.

    // Insert a new element in the index.
    insertSerialId(mArc, mArc->count+1, name, mnamespace);
    return mArc->count; // 1-based index.
    }
}

static materialarchive_record_t* getRecord(const materialarchive_t* mArc,
    materialarchive_serialid_t serialId, int group)
{
    materialarchive_record_t* rec;

    if(mArc->version < 1 && group == MN_FLATS)
        serialId += mArc->numFlats;

    rec = &mArc->table[serialId];
    if(!strncmp(rec->name, UNKNOWN_MATERIAL_NAME, 8))
        return 0;
    return rec;
}

static materialnum_t materialForSerialId(const materialarchive_t* mArc,
    materialarchive_serialid_t serialId, int group)
{
    assert(serialId <= mArc->count+1);
    {
    materialarchive_record_t* rec;
    if(serialId != 0 && (rec = getRecord(mArc, serialId-1, group)))
        return Materials_NumForName(rec->name, rec->mnamespace);
    return 0;
    }
}

/**
 * Populate the archive using the global Materials list.
 */
static void populate(materialarchive_t* mArc)
{
    uint i, num = nummaterials;
    for(i = 1; i < num+1; ++i)
    {
        serialIdForMaterial(mArc, P_ToPtr(DMU_MATERIAL, i), true);
    }
}

static int writeRecord(materialarchive_record_t* rec)
{
    SV_Write(rec->name, 8);
    SV_WriteByte(rec->mnamespace);
    return true; // Continue iteration.
}

static int readRecord(materialarchive_record_t* rec)
{
    SV_Read(rec->name, 8);
    rec->name[8] = 0;
    rec->mnamespace = SV_ReadByte();
    return true; // Continue iteration.
}

/**
 * Same as readRecord except we are reading the old record format used
 * by Doomsday 1.8.6 and earlier.
 */
static int readRecord_v186(materialarchive_record_t* rec, material_namespace_t mnamespace)
{
    SV_Read(rec->name, 8);
    rec->name[8] = 0;
    rec->mnamespace = mnamespace;
    return true; // Continue iteration.
}

static void readMaterialGroup(materialarchive_t* mArc, material_namespace_t defaultNamespace)
{
    // Read the group header.
    uint i, num = SV_ReadShort();

    for(i = 0; i < num; ++i)
    {
        materialarchive_record_t temp;
        memset(&temp, 0, sizeof(temp));

        if(mArc->version >= 1)
            readRecord(&temp);
        else
            readRecord_v186(&temp, defaultNamespace);

        insertSerialId(mArc, mArc->count+1, temp.name, temp.mnamespace);
    }
}

static void writeMaterialGroup(const materialarchive_t* mArc)
{
    uint i;
    for(i = 0; i < mArc->count; ++i)
    {
        writeRecord(&mArc->table[i]);
    }
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

materialarchive_serialid_t MaterialArchive_Add(materialarchive_t* materialArchive, material_t* material)
{
    assert(materialArchive);
    return serialIdForMaterial(materialArchive, material, true);
}

/**
 * @return                  A new (unused) SerialId for the specified material.
 */
materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(materialarchive_t* materialArchive,
    material_t* material)
{
    assert(materialArchive);
    if(material)
        return serialIdForMaterial(materialArchive, material, false);
    return 0; // Invalid.   
}

materialnum_t MaterialArchive_Find(materialarchive_t* materialArchive,
    materialarchive_serialid_t serialId, int group)
{
    assert(materialArchive);
    return materialForSerialId(materialArchive, serialId, group);
}

void MaterialArchive_Write(materialarchive_t* materialArchive)
{
    assert(materialArchive);
    writeHeader(materialArchive);

    // Write the group header.
    SV_WriteShort(materialArchive->count);

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
    readMaterialGroup(materialArchive, (version >= 1? MN_ANY : MN_FLATS));

    if(materialArchive->version == 0)
    {   // The old format saved flats and textures in seperate groups.
        materialArchive->numFlats = materialArchive->count;
        readMaterialGroup(materialArchive, (version >= 1? MN_ANY : MN_TEXTURES));
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
        Con_Printf("%i [%u]: %s\n", (int)rec->mnamespace, i, rec->name);
    }
    Con_Printf("End");
    }
}
#endif
