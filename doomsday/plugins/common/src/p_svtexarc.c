/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_mapsetup.h"
#include "p_svtexarc.h"

// MACROS ------------------------------------------------------------------

#define BADTEXNAME  "DD_BADTX"  // string that will be written in the texture
                                // archives to denote a missing texture.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// Savegame I/O:
extern void    SV_Write(void *data, int len);
extern void    SV_WriteShort(short val);
extern void    SV_Read(void *data, int len);
extern short   SV_ReadShort();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

texarchive_t flat_archive;
texarchive_t tex_archive;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Called for every texture and flat in the level before saving by
 * Sv_InitTextureArchives.
 */
void SV_PrepareTexture(int tex, boolean isflat, texarchive_t * arc)
{
    int     c;
    char    name[9];

    // Get the name of the texture/flat.
    if(isflat)
   {
        if(R_FlatNameForNum > 0)
            strncpy(name, R_FlatNameForNum(tex), 8);
        else
            strncpy(name, BADTEXNAME, 8);

        name[8] = 0;
    }
    else
    {
        if(R_TextureNameForNum(tex))
            strncpy(name, R_TextureNameForNum(tex), 8);
        else
            strncpy(name, BADTEXNAME, 8);
        name[8] = 0;
    }
    // Has this already been registered?
    for(c = 0; c < arc->count; c++)
    {
        if(!stricmp(arc->table[c].name, name))
        {
            // Yes, skip it...
            break;
        }
    }
    if(c == arc->count)
        strcpy(arc->table[arc->count++].name, name);
}

/*
 * Initializes the texture and flat archives (translation tables).
 * Must be called before saving. The tables are written before any
 * world data is saved.
 */
void SV_InitTextureArchives(void)
{
    uint    i;

    // Init flats.
    flat_archive.count = 0;

    for(i = 0; i < numsectors; ++i)
    {
        SV_PrepareTexture(P_GetInt(DMU_SECTOR, i, DMU_FLOOR_TEXTURE), true, &flat_archive);
        SV_PrepareTexture(P_GetInt(DMU_SECTOR, i, DMU_CEILING_TEXTURE), true, &flat_archive);
    }
    // Init textures.
    tex_archive.count = 0;

    for(i = 0; i < numsides; ++i)
    {
        SV_PrepareTexture(P_GetInt(DMU_SIDE, i, DMU_MIDDLE_TEXTURE), false, &tex_archive);
        SV_PrepareTexture(P_GetInt(DMU_SIDE, i, DMU_TOP_TEXTURE), false, &tex_archive);
        SV_PrepareTexture(P_GetInt(DMU_SIDE, i, DMU_BOTTOM_TEXTURE), false, &tex_archive);
    }
}

unsigned short SV_SearchArchive(texarchive_t * arc, char *name)
{
    int     i;

    for(i = 0; i < arc->count; i++)
        if(!stricmp(arc->table[i].name, name))
            return i;
    // Not found?!!!
    return 0;
}

/*
 * Returns the archive number of the given texture.
 * It will be written to the savegame file.
 */
unsigned short SV_TextureArchiveNum(int texnum)
{
    char    name[9];

    if(R_TextureNameForNum(texnum))
        strncpy(name, R_TextureNameForNum(texnum), 8);
    else
        strncpy(name, BADTEXNAME, 8);
    name[8] = 0;
    return SV_SearchArchive(&tex_archive, name);
}

/*
 * Returns the archive number of the given flat.
 * It will be written to the savegame file.
 */
unsigned short SV_FlatArchiveNum(int flatnum)
{
    char name[9];

    if(R_FlatNameForNum(flatnum))
        strncpy(name, R_FlatNameForNum(flatnum), 8);
    else
        strncpy(name, BADTEXNAME, 8);
    name[8] = 0;
    return SV_SearchArchive(&flat_archive, name);
}

int SV_GetArchiveFlat(int archivenum)
{
    if(!strncmp(flat_archive.table[archivenum].name, BADTEXNAME, 8))
        return -1;
    else
        return R_FlatNumForName(flat_archive.table[archivenum].name);
}

int SV_GetArchiveTexture(int archivenum)
{
    if(!strncmp(tex_archive.table[archivenum].name, BADTEXNAME, 8))
        return -1;
    else
        return R_TextureNumForName(tex_archive.table[archivenum].name);
}

void SV_WriteTexArchive(texarchive_t *arc)
{
    int         i;

    SV_WriteShort(arc->count);
    for(i = 0; i < arc->count; ++i)
    {
        SV_Write(arc->table[i].name, 8);
    }
}

void SV_ReadTexArchive(texarchive_t *arc)
{
    int         i;

    arc->count = SV_ReadShort();
    for(i = 0; i < arc->count; ++i)
    {
        SV_Read(arc->table[i].name, 8);
        arc->table[i].name[8] = 0;
    }
}

void SV_WriteTextureArchive(void)
{
    SV_WriteTexArchive(&flat_archive);
    SV_WriteTexArchive(&tex_archive);
}

void SV_ReadTextureArchive(void)
{
    SV_ReadTexArchive(&flat_archive);
    SV_ReadTexArchive(&tex_archive);
}
