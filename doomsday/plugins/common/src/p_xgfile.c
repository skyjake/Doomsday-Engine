/**\file
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

/**
 * p_xgfile.c: Extended Generalized Line Types.
 *
 * Writes XG data to file. Parses DD_XGDATA lumps.
 */

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
# include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_xg.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef enum xgsegenum_e {
    XGSEG_END,
    XGSEG_LINE,
    XGSEG_SECTOR
} xgsegenum_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean xgDataLumps = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FILE* file;
static const byte* readptr;

static linetype_t*linetypes = 0;
static int num_linetypes = 0;

static sectortype_t* sectypes;
static int num_sectypes;

// CODE --------------------------------------------------------------------

static void WriteByte(byte b)
{
    fputc(b, file);
}

static void WriteShort(short s)
{
    s = SHORT(s);
    fwrite(&s, 2, 1, file);
}

static void WriteLong(long l)
{
    l = LONG(l);
    fwrite(&l, 4, 1, file);
}

static void WriteFloat(float f)
{
    f = FLOAT(f);
    fwrite(&f, 4, 1, file);
}

/*void Write(void *data, int len)
{
    fwrite(data, len, 1, file);
}
*/

static void WriteString(char *str)
{
    int                 len;

    if(!str)
    {
        WriteShort(0);
        return;
    }
    len = strlen(str);
    WriteShort(len);
    //Write(str, len);
    fwrite(str, len, 1, file);
}

static byte ReadByte(void)
{
    return *readptr++;
}

static short ReadShort(void)
{
    short               s = *(const short *) readptr;

    readptr += 2;
    // Swap the bytes.
    //s = (s<<8) + (s>>8);
    return SHORT(s);
}

static long ReadLong(void)
{
    long                l = *(const long *) readptr;

    readptr += 4;
    // Swap the bytes.
    //l = (l<<24) + (l>>24) + ((l & 0xff0000) >> 8) + ((l & 0xff00) << 8);
    return LONG(l);
}

static float ReadFloat(void)
{
    long                f = ReadLong();
    float               returnValue = 0;

    memcpy(&returnValue, &f, 4);
    return returnValue;
}

/**
 * I could just return a pointer to the string, but that risks losing
 * it somewhere. Now we can be absolutely sure it can't be lost.
 */
static void ReadString(char** str)
{
    int                 len = ReadShort();

    if(!len) // Null string?
    {
        *str = 0;
        return;
    }

    if(len < 0)
        Con_Error("ReadString: Bogus len!\n");

    // Allocate memory for the string.
    *str = Z_Malloc(len + 1, PU_GAMESTATIC, 0);
    memcpy(*str, readptr, len);
    readptr += len;
    (*str)[len] = 0;
}

#if 0 // No longer supported.
void XG_WriteTypes(FILE* f)
{
    int                 i, k;
    int                 linecount = 0, sectorcount = 0;
    char                buff[6];
    linetype_t          line;
    sectortype_t        sec;

    file = f;

    // The first four four bytes are a header.
    // They will be updated with the real counts afterwards.
    WriteShort(0); // Number of lines & sectors (two shorts).
    WriteShort(0);

    // This is a very simple way to get the definitions.
    for(i = 1; i < 65536; ++i)
    {
        sprintf(buff, "%i", i);
        if(!Def_Get(DD_DEF_LINE_TYPE, buff, &line))
            continue;

        linecount++;

        // Write marker.
        WriteByte(XGSEG_LINE);

        WriteShort(line.id);
        WriteLong(line.flags);
        WriteLong(line.flags2);
        WriteLong(line.flags3);
        WriteShort(line.lineClass);
        WriteByte(line.actType);
        WriteShort(line.actCount);
        WriteFloat(line.actTime);
        WriteLong(line.actTag);
        for(k = 0; k < DDLT_MAX_APARAMS; ++k)
            WriteLong(line.aparm[k]);
        WriteFloat(line.tickerStart);
        WriteFloat(line.tickerEnd);
        WriteLong(line.tickerInterval);
        WriteShort(line.actSound);
        WriteShort(line.deactSound);
        WriteShort(line.evChain);
        WriteShort(line.actChain);
        WriteShort(line.deactChain);
        WriteByte(line.wallSection);
        WriteShort(line.actMaterial);
        WriteShort(line.deactMaterial);
        WriteString(line.actMsg);
        WriteString(line.deactMsg);
        WriteFloat(line.materialMoveAngle);
        WriteFloat(line.materialMoveSpeed);
        for(k = 0; k < DDLT_MAX_PARAMS; ++k)
            WriteLong(line.iparm[k]);
        for(k = 0; k < DDLT_MAX_PARAMS; ++k)
            WriteFloat(line.fparm[k]);
        for(k = 0; k < DDLT_MAX_SPARAMS; k++)
            WriteString(line.sparm[k]);
    }

    // Then the sectors.
    for(i = 1; i < 65536; ++i)
    {
        sprintf(buff, "%i", i);
        if(!Def_Get(DD_DEF_SECTOR_TYPE, buff, &sec))
            continue;

        sectorcount++;

        // Write marker.
        WriteByte(XGSEG_SECTOR);

        WriteShort(sec.id);
        WriteLong(sec.flags);
        WriteLong(sec.actTag);
        for(k = 0; k < DDLT_MAX_CHAINS; ++k)
            WriteLong(sec.chain[k]);
        for(k = 0; k < DDLT_MAX_CHAINS; ++k)
            WriteLong(sec.chainFlags[k]);
        for(k = 0; k < DDLT_MAX_CHAINS; ++k)
            WriteFloat(sec.start[k]);
        for(k = 0; k < DDLT_MAX_CHAINS; ++k)
            WriteFloat(sec.end[k]);
        for(k = 0; k < DDLT_MAX_CHAINS; ++k)
        {
            WriteFloat(sec.interval[k][0]);
            WriteFloat(sec.interval[k][1]);
        }
        for(k = 0; k < DDLT_MAX_CHAINS; ++k)
            WriteLong(sec.count[k]);
        WriteShort(sec.ambientSound);
        WriteFloat(sec.soundInterval[0]);
        WriteFloat(sec.soundInterval[1]);
        WriteFloat(sec.materialMoveAngle[0]);
        WriteFloat(sec.materialMoveAngle[1]);
        WriteFloat(sec.materialMoveSpeed[0]);
        WriteFloat(sec.materialMoveSpeed[1]);
        WriteFloat(sec.windAngle);
        WriteFloat(sec.windSpeed);
        WriteFloat(sec.verticalWind);
        WriteFloat(sec.gravity);
        WriteFloat(sec.friction);
        WriteString(sec.lightFunc);
        WriteShort(sec.lightInterval[0]);
        WriteShort(sec.lightInterval[1]);
        WriteString(sec.colFunc[0]);
        WriteString(sec.colFunc[1]);
        WriteString(sec.colFunc[2]);
        for(k = 0; k < 3; ++k)
        {
            WriteShort(sec.colInterval[k][0]);
            WriteShort(sec.colInterval[k][1]);
        }
        WriteString(sec.floorFunc);
        WriteFloat(sec.floorMul);
        WriteFloat(sec.floorOff);
        WriteShort(sec.floorInterval[0]);
        WriteShort(sec.floorInterval[1]);
        WriteString(sec.ceilFunc);
        WriteFloat(sec.ceilMul);
        WriteFloat(sec.ceilOff);
        WriteShort(sec.ceilInterval[0]);
        WriteShort(sec.ceilInterval[1]);
    }

    // Write the end marker.
    WriteByte(XGSEG_END);

    // Update header.
    rewind(file);
    WriteShort(linecount);
    WriteShort(sectorcount);
}
#endif

void XG_ReadXGLump(lumpnum_t lumpNum)
{
    int lc = 0, sc = 0, i;
    sectortype_t* sec;
    linetype_t* li;
    boolean done = false;
    size_t len;
    uint8_t* buf;

    if(0 > lumpNum)
        return; // No such lump.

    xgDataLumps = true;

    Con_Message("XG_ReadTypes: Reading XG types from DDXGDATA.\n");

    len = W_LumpLength(lumpNum);
    buf = (uint8_t*) malloc(len);
    if(NULL == buf)
        Con_Error("XG_ReadTypes: Failed on allocation of %lu bytes for temporary buffer.", (unsigned long) len);
    W_ReadLump(lumpNum, buf);

    readptr = (byte*)buf;

    num_linetypes = ReadShort();
    num_sectypes = ReadShort();

    // Allocate the arrays.
    linetypes = Z_Calloc(sizeof(*linetypes) * num_linetypes, PU_GAMESTATIC, 0);
    sectypes = Z_Calloc(sizeof(*sectypes) * num_sectypes, PU_GAMESTATIC, 0);

    while(!done)
    {
        // Get next segment.
        switch(ReadByte())
        {
        case XGSEG_END:
            done = true;
            break;

        case XGSEG_LINE:
            li = linetypes + lc++;
            // Read the def.
            li->id = ReadShort();
            li->flags = ReadLong();
            li->flags2 = ReadLong();
            li->flags3 = ReadLong();
            li->lineClass = ReadShort();
            li->actType = ReadByte();
            li->actCount = ReadShort();
            li->actTime = ReadFloat();
            li->actTag = ReadLong();
            for(i = 0; i < DDLT_MAX_APARAMS; ++i)
                li->aparm[i] = ReadLong();
            li->tickerStart = ReadFloat();
            li->tickerEnd = ReadFloat();
            li->tickerInterval = ReadLong();
            li->actSound = ReadShort();
            li->deactSound = ReadShort();
            li->evChain = ReadShort();
            li->actChain = ReadShort();
            li->deactChain = ReadShort();
            li->wallSection = ReadByte();
            li->actMaterial = DD_MaterialForTextureUniqueId("Textures", ReadShort());
            li->deactMaterial = DD_MaterialForTextureUniqueId("Textures", ReadShort());
            ReadString(&li->actMsg);
            ReadString(&li->deactMsg);
            li->materialMoveAngle = ReadFloat();
            li->materialMoveSpeed = ReadFloat();
            for(i = 0; i < DDLT_MAX_PARAMS; ++i)
                li->iparm[i] = ReadLong();
            for(i = 0; i < DDLT_MAX_PARAMS; ++i)
                li->fparm[i] = ReadFloat();
            for(i = 0; i < DDLT_MAX_SPARAMS; ++i)
                ReadString(&li->sparm[i]);
            break;

        case XGSEG_SECTOR:
            sec = sectypes + sc++;
            // Read the def.
            sec->id = ReadShort();
            sec->flags = ReadLong();
            sec->actTag = ReadLong();
            for(i = 0; i < DDLT_MAX_CHAINS; ++i)
                sec->chain[i] = ReadLong();
            for(i = 0; i < DDLT_MAX_CHAINS; ++i)
                sec->chainFlags[i] = ReadLong();
            for(i = 0; i < DDLT_MAX_CHAINS; ++i)
                sec->start[i] = ReadFloat();
            for(i = 0; i < DDLT_MAX_CHAINS; ++i)
                sec->end[i] = ReadFloat();
            for(i = 0; i < DDLT_MAX_CHAINS; ++i)
            {
                sec->interval[i][0] = ReadFloat();
                sec->interval[i][1] = ReadFloat();
            }
            for(i = 0; i < DDLT_MAX_CHAINS; ++i)
                sec->count[i] = ReadLong();
            sec->ambientSound = ReadShort();
            sec->soundInterval[0] = ReadFloat();
            sec->soundInterval[1] = ReadFloat();
            sec->materialMoveAngle[0] = ReadFloat();
            sec->materialMoveAngle[1] = ReadFloat();
            sec->materialMoveSpeed[0] = ReadFloat();
            sec->materialMoveSpeed[1] = ReadFloat();
            sec->windAngle = ReadFloat();
            sec->windSpeed = ReadFloat();
            sec->verticalWind = ReadFloat();
            sec->gravity = ReadFloat();
            sec->friction = ReadFloat();
            ReadString(&sec->lightFunc);
            sec->lightInterval[0] = ReadShort();
            sec->lightInterval[1] = ReadShort();
            ReadString(&sec->colFunc[0]);
            ReadString(&sec->colFunc[1]);
            ReadString(&sec->colFunc[2]);
            for(i = 0; i < 3; ++i)
            {
                sec->colInterval[i][0] = ReadShort();
                sec->colInterval[i][1] = ReadShort();
            }
            ReadString(&sec->floorFunc);
            sec->floorMul = ReadFloat();
            sec->floorOff = ReadFloat();
            sec->floorInterval[0] = ReadShort();
            sec->floorInterval[1] = ReadShort();
            ReadString(&sec->ceilFunc);
            sec->ceilMul = ReadFloat();
            sec->ceilOff = ReadFloat();
            sec->ceilInterval[0] = ReadShort();
            sec->ceilInterval[1] = ReadShort();
            break;

        default:
            Con_Error("XG_ReadXGLump: Bad segment!\n");
        }
    }

    free(buf);
}

/**
 * See if any line or sector types are saved in a DDXGDATA lump.
 */
void XG_ReadTypes(void)
{
    num_linetypes = 0;
    num_sectypes = 0;
    if(linetypes)
        Z_Free(linetypes);
    if(sectypes)
        Z_Free(sectypes);
    linetypes = 0;
    sectypes = 0;

    XG_ReadXGLump(W_CheckLumpNumForName2("DDXGDATA", true));
}

linetype_t* XG_GetLumpLine(int id)
{
    int                 i;

    for(i = 0; i < num_linetypes; ++i)
        if(linetypes[i].id == id)
            return linetypes + i;

    return NULL; // Not found.
}

sectortype_t *XG_GetLumpSector(int id)
{
    int                 i;

    for(i = 0; i < num_sectypes; ++i)
        if(sectypes[i].id == id)
            return sectypes + i;

    return NULL; // Not found.
}

#endif
