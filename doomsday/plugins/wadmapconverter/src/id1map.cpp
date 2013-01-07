/**
 * @file id1map.cpp @ingroup wadmapconverter
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "wadmapconverter.h"
#include "id1map_load.h"
#include "id1map_util.h"
#include <de/libdeng2.h>
#include <de/Log>
#include <de/Error>
#include <de/memory.h>
#include <de/timer.h>

static Reader* bufferLump(MapLumpInfo* info);
static void clearReadBuffer(void);

Id1Map::Id1Map(MapFormatId format)
    : mapFormat(format), numVertexes(0), vertexes(0)
{}

Id1Map::~Id1Map()
{
    if(vertexes)
    {
        M_Free(vertexes);
        vertexes = 0;
    }

    DENG2_FOR_EACH(Polyobjs, i, polyobjs)
    {
        M_Free((i)->lineIndices);
    }
}

MaterialDictId Id1Map::addMaterialToDictionary(char const* name, MaterialDictGroup group)
{
    DENG2_ASSERT(name);

    // Prepare the encoded URI for insertion into the dictionary.
    AutoStr* uriCString;
    if(mapFormat == MF_DOOM64)
    {
        // Doom64 maps reference materials with unique ids.
        int uniqueId = *((int*) name);
        //char name[9];
        //sprintf(name, "UNK%05i", uniqueId); name[8] = '\0';

        Uri *textureUrn = Uri_NewWithPath2(Str_Text(Str_Appendf(AutoStr_NewStd(), "urn:%s:%i", group == MG_PLANE? "Flats" : "Textures", uniqueId)), RC_NULL);
        Uri *uri = Materials_ComposeUri(P_ToIndex(DD_MaterialForTextureUri(textureUrn)));
        uriCString = Uri_Compose(uri);
        Uri_Delete(uri);
        Uri_Delete(textureUrn);
    }
    else
    {
        // In original DOOM, texture name references beginning with the
        // hypen '-' character are always treated as meaning "no reference"
        // or "invalid texture" and surfaces using them were not drawn.
        if(group != MG_PLANE && name[0] == '-')
        {
            return 0; // Not a valid id.
        }

        // Material paths must be encoded.
        AutoStr* path = Str_PercentEncode(AutoStr_FromText(name));
        Uri* uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
        Uri_SetScheme(uri, group == MG_PLANE? "Flats" : "Textures");
        uriCString = Uri_Compose(uri);
        Uri_Delete(uri);
    }

    // Intern this material URI in the dictionary.
    MaterialDictId internId = materials.intern(de::String(Str_Text(uriCString)));

    // We're done (phew!).
    return internId;
}

bool Id1Map::loadVertexes(Reader* reader, uint numElements)
{
    DENG2_ASSERT(reader);

    LOG_TRACE("Processing vertexes...");
    for(uint n = 0; n < numElements; ++n)
    {
        switch(mapFormat)
        {
        default:
        case MF_DOOM:
            vertexes[n * 2]     = coord_t( SHORT(Reader_ReadInt16(reader)) );
            vertexes[n * 2 + 1] = coord_t( SHORT(Reader_ReadInt16(reader)) );
            break;

        case MF_DOOM64:
            vertexes[n * 2]     = coord_t( FIX2FLT(LONG(Reader_ReadInt32(reader))) );
            vertexes[n * 2 + 1] = coord_t( FIX2FLT(LONG(Reader_ReadInt32(reader))) );
            break;
        }
    }

    return true;
}

bool Id1Map::loadLineDefs(Reader* reader, uint numElements)
{
    DENG2_ASSERT(reader);

    LOG_TRACE("Processing line definitions...");
    if(numElements)
    {
        lines.reserve(lines.size() + numElements);
        for(uint n = 0; n < numElements; ++n)
        {
            switch(mapFormat)
            {
            default:
            case MF_DOOM:
                lines.push_back(mline_t());
                MLine_Read(&lines.back(), reader);
                break;

            case MF_DOOM64:
                lines.push_back(mline_t());
                MLine64_Read(&lines.back(), reader);
                break;

            case MF_HEXEN:
                lines.push_back(mline_t());
                MLineHx_Read(&lines.back(), reader);
                break;
            }
        }
    }

    return true;
}

bool Id1Map::loadSideDefs(Reader* reader, uint numElements)
{
    DENG2_ASSERT(reader);

    LOG_TRACE("Processing side definitions...");
    if(numElements)
    {
        sides.reserve(sides.size() + numElements);
        for(uint n = 0; n < numElements; ++n)
        {
            switch(mapFormat)
            {
            default:
            case MF_DOOM:
                sides.push_back(mside_t());
                MSide_Read(&sides.back(), reader);
                break;

            case MF_DOOM64:
                sides.push_back(mside_t());
                MSide64_Read(&sides.back(), reader);
                break;
            }
        }
    }

    return true;
}

bool Id1Map::loadSectors(Reader* reader, uint numElements)
{
    DENG2_ASSERT(reader);

    LOG_TRACE("Processing sectors...");
    if(numElements)
    {
        sectors.reserve(sectors.size() + numElements);
        for(uint n = 0; n < numElements; ++n)
        {
            switch(mapFormat)
            {
            default:
                sectors.push_back(msector_t());
                MSector_Read(&sectors.back(), reader);
                break;

            case MF_DOOM64:
                sectors.push_back(msector_t());
                MSector64_Read(&sectors.back(), reader);
                break;
            }
        }
    }

    return true;
}

bool Id1Map::loadThings(Reader* reader, uint numElements)
{
    DENG2_ASSERT(reader);

    LOG_TRACE("Processing things...");
    if(numElements)
    {
        things.reserve(things.size() + numElements);
        for(uint n = 0; n < numElements; ++n)
        {
            switch(mapFormat)
            {
            default:
            case MF_DOOM:
                things.push_back(mthing_t());
                MThing_Read(&things.back(), reader);
                break;

            case MF_DOOM64:
                things.push_back(mthing_t());
                MThing64_Read(&things.back(), reader);
                break;

            case MF_HEXEN:
                things.push_back(mthing_t());
                MThingHx_Read(&things.back(), reader);
                break;
            }
        }
    }

    return true;
}

bool Id1Map::loadSurfaceTints(Reader* reader, uint numElements)
{
    DENG2_ASSERT(reader);

    LOG_TRACE("Processing surface tints...");
    if(numElements)
    {
        surfaceTints.reserve(surfaceTints.size() + numElements);
        for(uint n = 0; n < numElements; ++n)
        {
            surfaceTints.push_back(surfacetint_t());
            SurfaceTint_Read(&surfaceTints.back(), reader);
        }
    }

    return true;
}

void Id1Map::load(MapLumpInfos& lumpInfos)
{
    /**
     * Allocate the vertices first as a large contiguous array suitable for
     * passing directly to Doomsday's MPE interface.
     */
    size_t elementSize = ElementSizeForMapLumpType(mapFormat, ML_VERTEXES);
    uint numElements = lumpInfos[ML_VERTEXES]->length / elementSize;
    numVertexes = numElements;
    vertexes = (coord_t*)M_Malloc(numVertexes * 2 * sizeof(*vertexes));

    DENG2_FOR_EACH_CONST(MapLumpInfos, i, lumpInfos)
    {
        MapLumpInfo* info = i->second;

        if(!info || !info->length) continue;

        elementSize = ElementSizeForMapLumpType(mapFormat, info->type);
        if(!elementSize) continue;

        // Process this data lump.
        numElements = info->length / elementSize;
        Reader* reader = bufferLump(info);
        switch(info->type)
        {
        default: break;

        case ML_VERTEXES: loadVertexes(reader, numElements);        break;
        case ML_LINEDEFS: loadLineDefs(reader, numElements);        break;
        case ML_SIDEDEFS: loadSideDefs(reader, numElements);        break;
        case ML_SECTORS:  loadSectors(reader, numElements);         break;
        case ML_THINGS:   loadThings(reader, numElements);          break;
        case ML_LIGHTS:   loadSurfaceTints(reader, numElements);    break;
        }
        Reader_Delete(reader);
    }

    // We're done with the read buffer.
    clearReadBuffer();
}

void Id1Map::transferVertexes(void)
{
    LOG_TRACE("Transfering vertexes...");
    MPE_VertexCreatev(numVertexes, vertexes, NULL);
}

void Id1Map::transferSectors(void)
{
    LOG_TRACE("Transfering sectors...");

    DENG2_FOR_EACH(Sectors, i, sectors)
    {
        uint idx = MPE_SectorCreate(float((i)->lightLevel) / 255.0f, 1, 1, 1);

        MPE_PlaneCreate(idx, (i)->floorHeight, composeMaterialRef((i)->floorMaterial),
                        0, 0, 1, 1, 1, 1, 0, 0, 1);
        MPE_PlaneCreate(idx, (i)->ceilHeight, composeMaterialRef((i)->ceilMaterial),
                        0, 0, 1, 1, 1, 1, 0, 0, -1);

        MPE_GameObjProperty("XSector", idx-1, "Tag",    DDVT_SHORT, &(i)->tag);
        MPE_GameObjProperty("XSector", idx-1, "Type",   DDVT_SHORT, &(i)->type);

        if(mapFormat == MF_DOOM64)
        {
            MPE_GameObjProperty("XSector", idx-1, "Flags",          DDVT_SHORT, &(i)->d64flags);
            MPE_GameObjProperty("XSector", idx-1, "CeilingColor",   DDVT_SHORT, &(i)->d64ceilingColor);
            MPE_GameObjProperty("XSector", idx-1, "FloorColor",     DDVT_SHORT, &(i)->d64floorColor);
            MPE_GameObjProperty("XSector", idx-1, "UnknownColor",   DDVT_SHORT, &(i)->d64unknownColor);
            MPE_GameObjProperty("XSector", idx-1, "WallTopColor",   DDVT_SHORT, &(i)->d64wallTopColor);
            MPE_GameObjProperty("XSector", idx-1, "WallBottomColor", DDVT_SHORT, &(i)->d64wallBottomColor);
        }
    }
}

void Id1Map::transferLinesAndSides(void)
{
    LOG_TRACE("Transfering lines and sides...");
    DENG2_FOR_EACH(Lines, i, lines)
    {
        uint frontIdx = 0;
        mside_t* front = ((i)->sides[RIGHT] != 0? &sides[(i)->sides[RIGHT]-1] : NULL);
        if(front)
        {
            frontIdx =
                MPE_SidedefCreate((mapFormat == MF_DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  composeMaterialRef(front->topMaterial),
                                  front->offset[VX], front->offset[VY], 1, 1, 1,
                                  composeMaterialRef(front->middleMaterial),
                                  front->offset[VX], front->offset[VY], 1, 1, 1, 1,
                                  composeMaterialRef(front->bottomMaterial),
                                  front->offset[VX], front->offset[VY], 1, 1, 1);
        }

        uint backIdx = 0;
        mside_t* back = ((i)->sides[LEFT] != 0? &sides[(i)->sides[LEFT]-1] : NULL);
        if(back)
        {
            backIdx =
                MPE_SidedefCreate((mapFormat == MF_DOOM64? SDF_MIDDLE_STRETCH : 0),
                                  composeMaterialRef(back->topMaterial),
                                  back->offset[VX], back->offset[VY], 1, 1, 1,
                                  composeMaterialRef(back->middleMaterial),
                                  back->offset[VX], back->offset[VY], 1, 1, 1, 1,
                                  composeMaterialRef(back->bottomMaterial),
                                  back->offset[VX], back->offset[VY], 1, 1, 1);
        }

        uint lineIdx = MPE_LinedefCreate((i)->v[0], (i)->v[1], front? front->sector : 0,
                                         back? back->sector : 0, frontIdx, backIdx, (i)->ddFlags);

        MPE_GameObjProperty("XLinedef", lineIdx-1, "Flags", DDVT_SHORT, &(i)->flags);

        switch(mapFormat)
        {
        default:
        case MF_DOOM:
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Type",  DDVT_SHORT, &(i)->dType);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Tag",   DDVT_SHORT, &(i)->dTag);
            break;

        case MF_DOOM64:
            MPE_GameObjProperty("XLinedef", lineIdx-1, "DrawFlags", DDVT_BYTE,  &(i)->d64drawFlags);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "TexFlags",  DDVT_BYTE,  &(i)->d64texFlags);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Type",      DDVT_BYTE,  &(i)->d64type);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "UseType",   DDVT_BYTE,  &(i)->d64useType);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Tag",       DDVT_SHORT, &(i)->d64tag);
            break;

        case MF_HEXEN:
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Type", DDVT_BYTE, &(i)->xType);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Arg0", DDVT_BYTE, &(i)->xArgs[0]);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Arg1", DDVT_BYTE, &(i)->xArgs[1]);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Arg2", DDVT_BYTE, &(i)->xArgs[2]);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Arg3", DDVT_BYTE, &(i)->xArgs[3]);
            MPE_GameObjProperty("XLinedef", lineIdx-1, "Arg4", DDVT_BYTE, &(i)->xArgs[4]);
            break;
        }
    }
}

void Id1Map::transferSurfaceTints(void)
{
    if(surfaceTints.empty()) return;

    LOG_TRACE("Transfering surface tints...");
    DENG2_FOR_EACH(SurfaceTints, i, surfaceTints)
    {
        uint idx = i - surfaceTints.begin();

        MPE_GameObjProperty("Light", idx, "ColorR",   DDVT_FLOAT, &(i)->rgb[0]);
        MPE_GameObjProperty("Light", idx, "ColorG",   DDVT_FLOAT, &(i)->rgb[1]);
        MPE_GameObjProperty("Light", idx, "ColorB",   DDVT_FLOAT, &(i)->rgb[2]);
        MPE_GameObjProperty("Light", idx, "XX0",      DDVT_BYTE,  &(i)->xx[0]);
        MPE_GameObjProperty("Light", idx, "XX1",      DDVT_BYTE,  &(i)->xx[1]);
        MPE_GameObjProperty("Light", idx, "XX2",      DDVT_BYTE,  &(i)->xx[2]);
    }
}

void Id1Map::transferPolyobjs(void)
{
    if(polyobjs.empty()) return;

    LOG_TRACE("Transfering polyobjs...");
    DENG2_FOR_EACH(Polyobjs, i, polyobjs)
    {
        MPE_PolyobjCreate((i)->lineIndices, (i)->lineCount, (i)->tag, (i)->seqType,
                          coord_t((i)->anchor[VX]), coord_t((i)->anchor[VY]));
    }
}

void Id1Map::transferThings(void)
{
    if(things.empty()) return;

    LOG_TRACE("Transfering things...");
    DENG2_FOR_EACH(Things, i, things)
    {
        uint idx = i - things.begin();

        MPE_GameObjProperty("Thing", idx, "X",            DDVT_SHORT, &(i)->origin[VX]);
        MPE_GameObjProperty("Thing", idx, "Y",            DDVT_SHORT, &(i)->origin[VY]);
        MPE_GameObjProperty("Thing", idx, "Z",            DDVT_SHORT, &(i)->origin[VZ]);
        MPE_GameObjProperty("Thing", idx, "Angle",        DDVT_ANGLE, &(i)->angle);
        MPE_GameObjProperty("Thing", idx, "DoomEdNum",    DDVT_SHORT, &(i)->doomEdNum);
        MPE_GameObjProperty("Thing", idx, "SkillModes",   DDVT_INT,   &(i)->skillModes);
        MPE_GameObjProperty("Thing", idx, "Flags",        DDVT_INT,   &(i)->flags);

        if(mapFormat == MF_DOOM64)
        {
            MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &(i)->d64TID);
        }
        else if(mapFormat == MF_HEXEN)
        {
            MPE_GameObjProperty("Thing", idx, "Special",  DDVT_BYTE,  &(i)->xSpecial);
            MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &(i)->xTID);
            MPE_GameObjProperty("Thing", idx, "Arg0",     DDVT_BYTE,  &(i)->xArgs[0]);
            MPE_GameObjProperty("Thing", idx, "Arg1",     DDVT_BYTE,  &(i)->xArgs[1]);
            MPE_GameObjProperty("Thing", idx, "Arg2",     DDVT_BYTE,  &(i)->xArgs[2]);
            MPE_GameObjProperty("Thing", idx, "Arg3",     DDVT_BYTE,  &(i)->xArgs[3]);
            MPE_GameObjProperty("Thing", idx, "Arg4",     DDVT_BYTE,  &(i)->xArgs[4]);
        }
    }
}

int Id1Map::transfer(void)
{
    uint startTime = Timer_RealMilliseconds();

    LOG_AS("Id1Map");

    transferVertexes();
    transferSectors();
    transferLinesAndSides();
    transferSurfaceTints();
    transferPolyobjs();
    transferThings();

    LOG_VERBOSE("Transfer completed in %.2f seconds.") << ((Timer_RealMilliseconds() - startTime) / 1000.0f);

    return false; // Success.
}

static uint8_t* readPtr;
static uint8_t* readBuffer = NULL;
static size_t readBufferSize = 0;

static char readInt8(Reader* r)
{
    if(!r) return 0;
    char value = *((const int8_t*) (readPtr));
    readPtr += 1;
    return value;
}

static short readInt16(Reader* r)
{
    if(!r) return 0;
    short value = *((const int16_t*) (readPtr));
    readPtr += 2;
    return value;
}

static int readInt32(Reader* r)
{
    if(!r) return 0;
    int value = *((const int32_t*) (readPtr));
    readPtr += 4;
    return value;
}

static float readFloat(Reader* r)
{
    DENG2_ASSERT(sizeof(float) == 4);
    if(!r) return 0;
    int32_t val = *((const int32_t*) (readPtr));
    float returnValue = 0;
    memcpy(&returnValue, &val, 4);
    return returnValue;
}

static void readData(Reader* r, char* data, int len)
{
    if(!r) return;
    memcpy(data, readPtr, len);
    readPtr += len;
}

/// @todo It should not be necessary to buffer the lump data here.
static Reader* bufferLump(MapLumpInfo* info)
{
    // Need to enlarge our read buffer?
    if(info->length > readBufferSize)
    {
        readBuffer = (uint8_t*)M_Realloc(readBuffer, info->length);
        if(!readBuffer)
        {
            throw Id1Map::LumpBufferError("Id1Map::bufferLump",
                QString("Failed on (re)allocation of %1 bytes for the read buffer.")
                    .arg(info->length));
        }
        readBufferSize = info->length;
    }

    // Buffer the entire lump.
    W_ReadLump(info->lump, readBuffer);

    // Create the reader.
    readPtr = readBuffer;
    return Reader_NewWithCallbacks(readInt8, readInt16, readInt32, readFloat, readData);
}

static void clearReadBuffer(void)
{
    if(!readBuffer) return;
    M_Free(readBuffer);
    readBuffer = 0;
    readBufferSize = 0;
}
