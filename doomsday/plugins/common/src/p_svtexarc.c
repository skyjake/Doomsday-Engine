/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

/**
 * p_svtexarc.c: Archived texture names (save games).
 */

// HEADER FILES ------------------------------------------------------------

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
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_svtexarc.h"
#include "p_saveg.h"

// MACROS ------------------------------------------------------------------

#define MAX_ARCHIVED_MATERIALS  1024
#define BADTEXNAME  "DD_BADTX"  // string that will be written in the texture
                                // archives to denote a missing texture.

// TYPES -------------------------------------------------------------------

typedef struct {
    char            name[9];
    material_namespace_t mnamespace;
} materialentry_t;

typedef struct {
    //// \todo Remove fixed limit.
    materialentry_t table[MAX_ARCHIVED_MATERIALS];
    int             count, version;
} materialarchive_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

materialarchive_t matArchive;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int numFlats; // Used with older versions.

// CODE --------------------------------------------------------------------

static void initMaterialNameLUT(void)
{
    matArchive.count = 0;
    memset(matArchive.table, 0, sizeof(matArchive.table));
}

/**
 * Called for every material in the map before saving by
 * Sv_InitTextureArchives.
 */
void SV_PrepareMaterial(material_t* mat, materialarchive_t* arc)
{
    int                 c;
    char                name[9];
    const char*         matName;
    material_namespace_t mnamespace;

    if(!mat)
        return;
    matName = P_GetMaterialName(mat);
    mnamespace = P_GetIntp(mat, DMU_NAMESPACE);

    // Get the name of the material.
    if(matName)
        strncpy(name, matName, 8);
    else
        strncpy(name, BADTEXNAME, 8);

    name[8] = 0;

    // Has this already been registered?
    for(c = 0; c < arc->count; c++)
    {
        if(arc->table[c].mnamespace == mnamespace &&
           !stricmp(arc->table[c].name, name))
        {
            break;// Yes, skip it...
        }
    }

    if(c == arc->count)
    {
        strcpy(arc->table[arc->count++].name, name);
        arc->table[arc->count - 1].mnamespace = mnamespace;
    }
}

/**
 * Initializes the material archives (translation tables).
 * Must be called before saving. The table is written before any world data
 * is saved.
 *
 * \fixme: Poor design. What about the case where we wish to save a
 * reference to a material which is not currently used on any world surface
 * (for example, thinkers that will change a material at some later time)?
 */
void SV_InitMaterialArchives(void)
{
    uint                i;

    matArchive.version = MATERIAL_ARCHIVE_VERSION;

    initMaterialNameLUT();

    for(i = 0; i < numsectors; ++i)
    {
        SV_PrepareMaterial(P_GetPtr(DMU_SECTOR, i, DMU_FLOOR_MATERIAL), &matArchive);
        SV_PrepareMaterial(P_GetPtr(DMU_SECTOR, i, DMU_CEILING_MATERIAL), &matArchive);
    }

    for(i = 0; i < numsides; ++i)
    {
        SV_PrepareMaterial(P_GetPtr(DMU_SIDEDEF, i, DMU_MIDDLE_MATERIAL), &matArchive);
        SV_PrepareMaterial(P_GetPtr(DMU_SIDEDEF, i, DMU_TOP_MATERIAL), &matArchive);
        SV_PrepareMaterial(P_GetPtr(DMU_SIDEDEF, i, DMU_BOTTOM_MATERIAL), &matArchive);
    }
}

unsigned short SV_SearchArchive(materialarchive_t* arc, const char* name)
{
    int                 i;

    for(i = 0; i < arc->count; ++i)
    {
        if(!stricmp(arc->table[i].name, name))
            return i + 1;
    }

    // Not found?!!!
    return 0;
}

/**
 * @return              The archive number of the given material.
 */
unsigned short SV_MaterialArchiveNum(material_t* mat)
{
    char                name[9];

    if(!mat)
        return 0;

    if(P_GetMaterialName(mat))
        strncpy(name, P_GetMaterialName(mat), 8);
    else
        strncpy(name, BADTEXNAME, 8);
    name[8] = 0;

    return SV_SearchArchive(&matArchive, name);
}

material_t* SV_GetArchiveMaterial(int archivenum, int group)
{
    if(archivenum == 0)
        return NULL;
    archivenum--;

    if(matArchive.version < 1 && group == 1)
    {
        archivenum += numFlats;
    }

    if(!(archivenum < matArchive.count))
    {
#if _DEBUG
        Con_Error("SV_GetArchiveMaterial: Bad archivenum %i.", archivenum);
#endif
        return NULL;
    }

    if(!strncmp(matArchive.table[archivenum].name, BADTEXNAME, 8))
        return NULL;
    else
        return P_ToPtr(DMU_MATERIAL,
            Materials_NumForName(matArchive.table[archivenum].name,
                                 matArchive.table[archivenum].mnamespace));
}

void SV_WriteMaterialArchive(void)
{
    int                 i;

    SV_WriteByte(matArchive.version);
    SV_WriteShort(matArchive.count);

    for(i = 0; i < matArchive.count; ++i)
    {
        SV_Write(matArchive.table[i].name, 8);
        SV_WriteByte(matArchive.table[i].mnamespace);
    }
}

static void readMatArchive(materialarchive_t* arc,
                           material_namespace_t defaultGroup)
{
    int                 i, num;

    num = SV_ReadShort();
    for(i = arc->count; i < arc->count + num; ++i)
    {
        SV_Read(arc->table[i].name, 8);
        arc->table[i].name[8] = 0;
        if(arc->version >= 1)
            arc->table[i].mnamespace = SV_ReadByte();
        else
            arc->table[i].mnamespace = defaultGroup;
    }

    arc->count += num;
}

/**
 * @param version       Override support for/of a particular version.
 */
void SV_ReadMaterialArchive(int version)
{
    if(version >= 0)
    {
        matArchive.version = version;
        if(version != 0)
            SV_ReadByte();
    }
    else
    {
        matArchive.version = SV_ReadByte();
    }

    initMaterialNameLUT();

    readMatArchive(&matArchive, MN_FLATS);

    if(matArchive.version == 0)
    {   // The old format saved textures and flats in seperate blocks.
        numFlats = matArchive.count;

        readMatArchive(&matArchive, MN_FLATS);
    }
}
