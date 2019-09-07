/** @file p_xgfile.cpp  Extended generalized line types.
 *
 * DD_XGDATA lump reader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__

#include "common.h"
#include "p_xg.h"

#include <de/legacy/memory.h>
#include <cstdio>
#include <cstring>

using namespace de;
using namespace res;

enum xgsegenum_t
{
    XGSEG_END,
    XGSEG_LINE,
    XGSEG_SECTOR
};

dd_bool xgDataLumps;

static const byte *readptr;

static linetype_t *linetypes;
static int num_linetypes;

static sectortype_t *sectypes;
static int num_sectypes;

static byte ReadByte()
{
    return *readptr++;
}

static short ReadShort()
{
    short s = *(const short *) readptr;
    readptr += 2;
    // Swap the bytes.
    //s = (s<<8) + (s>>8);
    return DD_SHORT(s);
}

static long ReadLong()
{
    long l = *(const long *) readptr;
    readptr += 4;
    // Swap the bytes.
    //l = (l<<24) + (l>>24) + ((l & 0xff0000) >> 8) + ((l & 0xff00) << 8);
    return DD_LONG(l);
}

static float ReadFloat()
{
    long f = ReadLong();
    float returnValue = 0;
    std::memcpy(&returnValue, &f, 4);
    return returnValue;
}

/**
 * I could just return a pointer to the string, but that risks losing
 * it somewhere. Now we can be absolutely sure it can't be lost.
 */
static void ReadString(char **str)
{
    int len = ReadShort();
    if(!len) // Null string?
    {
        *str = 0;
        return;
    }

    if(len < 0)
        Con_Error("ReadString: Bogus len!\n");

    // Allocate memory for the string.
    *str = (char *) Z_Malloc(len + 1, PU_GAMESTATIC, 0);
    std::memcpy(*str, readptr, len);
    readptr += len;
    (*str)[len] = 0;
}

static uri_s *readTextureUrn()
{
    return Uri_NewWithPath2(Str_Text(Str_Appendf(AutoStr_NewStd(), "urn:Textures:%i", ReadShort())), RC_NULL);
}

void XG_ReadXGLump(lumpnum_t lumpNum)
{
    if(0 > lumpNum)
        return; // No such lump.

    xgDataLumps = true;

    App_Log(DE2_RES_MSG, "Reading XG types from DDXGDATA");

    const LumpIndex &lumps = CentralLumpIndex();
    File1 &lump = lumps[lumpNum];

    readptr = (byte *)lump.cache();

    // Allocate the arrays.

    num_linetypes = ReadShort();
    linetypes = (linetype_t *)Z_Calloc(sizeof(*linetypes) * num_linetypes, PU_GAMESTATIC, 0);

    num_sectypes = ReadShort();
    sectypes = (sectortype_t *)Z_Calloc(sizeof(*sectypes) * num_sectypes, PU_GAMESTATIC, 0);

    int lc = 0, sc = 0;

    dd_bool done = false;
    while(!done)
    {
        // Get next segment.
        switch(ReadByte())
        {
        case XGSEG_END:
            done = true;
            break;

        case XGSEG_LINE: {
            linetype_t *li = linetypes + lc++;

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
            for(int i = 0; i < DDLT_MAX_APARAMS; ++i)
            {
                li->aparm[i] = ReadLong();
            }
            li->tickerStart = ReadFloat();
            li->tickerEnd = ReadFloat();
            li->tickerInterval = ReadLong();
            li->actSound = ReadShort();
            li->deactSound = ReadShort();
            li->evChain = ReadShort();
            li->actChain = ReadShort();
            li->deactChain = ReadShort();
            li->wallSection = ReadByte();

            {
                uri_s *textureUrn = readTextureUrn();
                li->actMaterial = P_ToIndex(DD_MaterialForTextureUri(textureUrn));
                Uri_Delete(textureUrn);
            }

            {
                uri_s *textureUrn = readTextureUrn();
                li->deactMaterial = P_ToIndex(DD_MaterialForTextureUri(textureUrn));
                Uri_Delete(textureUrn);
            }

            ReadString(&li->actMsg);
            ReadString(&li->deactMsg);
            li->materialMoveAngle = ReadFloat();
            li->materialMoveSpeed = ReadFloat();
            for(int i = 0; i < DDLT_MAX_PARAMS; ++i)
            {
                li->iparm[i] = ReadLong();
            }
            for(int i = 0; i < DDLT_MAX_PARAMS; ++i)
            {
                li->fparm[i] = ReadFloat();
            }
            for(int i = 0; i < DDLT_MAX_SPARAMS; ++i)
            {
                ReadString(&li->sparm[i]);
            }
            break; }

        case XGSEG_SECTOR: {
            sectortype_t *sec = sectypes + sc++;

            // Read the def.
            sec->id = ReadShort();
            sec->flags = ReadLong();
            sec->actTag = ReadLong();
            for(int i = 0; i < DDLT_MAX_CHAINS; ++i)
            {
                sec->chain[i] = ReadLong();
            }
            for(int i = 0; i < DDLT_MAX_CHAINS; ++i)
            {
                sec->chainFlags[i] = ReadLong();
            }
            for(int i = 0; i < DDLT_MAX_CHAINS; ++i)
            {
                sec->start[i] = ReadFloat();
            }
            for(int i = 0; i < DDLT_MAX_CHAINS; ++i)
            {
                sec->end[i] = ReadFloat();
            }
            for(int i = 0; i < DDLT_MAX_CHAINS; ++i)
            {
                sec->interval[i][0] = ReadFloat();
                sec->interval[i][1] = ReadFloat();
            }
            for(int i = 0; i < DDLT_MAX_CHAINS; ++i)
            {
                sec->count[i] = ReadLong();
            }
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
            for(int i = 0; i < 3; ++i)
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
            break; }

        default:
            lump.unlock();
            Con_Error("XG_ReadXGLump: Bad segment!");
        }
    }

    lump.unlock();
}

void XG_ReadTypes()
{
    num_linetypes = 0;
    Z_Free(linetypes); linetypes = 0;

    num_sectypes = 0;
    Z_Free(sectypes); sectypes = 0;

    XG_ReadXGLump(CentralLumpIndex().findLast("DDXGDATA.lmp"));
}

linetype_t *XG_GetLumpLine(int id)
{
    for(int i = 0; i < num_linetypes; ++i)
    {
        if(linetypes[i].id == id)
            return linetypes + i;
    }
    return 0; // Not found.
}

sectortype_t *XG_GetLumpSector(int id)
{
    for(int i = 0; i < num_sectypes; ++i)
    {
        if(sectypes[i].id == id)
            return sectypes + i;
    }
    return 0; // Not found.
}

#endif
