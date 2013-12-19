/** @file id1map.cpp  id Tech 1 map format reader.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

using namespace de;

static reader_s *bufferLump(MapLumpInfo *info);
static void clearReadBuffer();

static uint validCount = 0; // Used for Polyobj LineDef collection.

DENG2_PIMPL(Id1Map)
{
    MapFormatId mapFormat;

    uint numVertexes;
    coord_t *vertexes; ///< Array of vertex coords [v0:X, vo:Y, v1:X, v1:Y, ..]

    Lines lines;
    Sides sides;
    Sectors sectors;
    Things things;
    SurfaceTints surfaceTints;
    Polyobjs polyobjs;

    de::StringPool materials; ///< Material dictionary.

    Instance(Public *i)
        : Base(i)
        , mapFormat(MF_UNKNOWN)
        , numVertexes(0)
        , vertexes(0)
    {}

    ~Instance()
    {
        M_Free(vertexes);

        DENG2_FOR_EACH(Polyobjs, i, polyobjs)
        {
            M_Free(i->lineIndices);
        }
    }

    /**
     * Create a temporary polyobj.
     */
    mpolyobj_t *createPolyobj(LineList &lineList, int tag, int sequenceType,
        int16_t anchorX, int16_t anchorY)
    {
        // Allocate the new polyobj.
        polyobjs.push_back(mpolyobj_t());
        mpolyobj_t *po = &polyobjs.back();

        po->index      = polyobjs.size()-1;
        po->tag        = tag;
        po->seqType    = sequenceType;
        po->anchor[VX] = anchorX;
        po->anchor[VY] = anchorY;

        // Construct the line indices array we'll pass to the MPE interface.
        po->lineCount = lineList.size();
        po->lineIndices = (int *)M_Malloc(sizeof(int) * po->lineCount);
        int n = 0;
        for(LineList::iterator i = lineList.begin(); i != lineList.end(); ++i, ++n)
        {
            int lineIdx = *i;
            mline_t *line = &lines[lineIdx];

            // This line now belongs to a polyobj.
            line->aFlags |= LAF_POLYOBJ;

            /*
             * Due a logic error in hexen.exe, when the column drawer is presented
             * with polyobj segs built from two-sided linedefs; clipping is always
             * calculated using the pegging logic for single-sided linedefs.
             *
             * Here we emulate this behavior by automatically applying bottom unpegging
             * for two-sided linedefs.
             */
            if(line->sides[LEFT] >= 0)
                line->ddFlags |= DDLF_DONTPEGBOTTOM;

            po->lineIndices[n] = lineIdx;
        }

        return po;
    }

    /**
     * Find all linedefs marked as belonging to a polyobject with the given tag
     * and attempt to create a polyobject from them.
     *
     * @param tag  Line tag of linedefs to search for.
     *
     * @return  @c true = successfully created polyobj.
     */
    bool findAndCreatePolyobj(int16_t tag, int16_t anchorX, int16_t anchorY)
    {
        LineList polyLines;

        // First look for a PO_LINE_START linedef set with this tag.
        DENG2_FOR_EACH(Lines, i, lines)
        {
            // Already belongs to another polyobj?
            if(i->aFlags & LAF_POLYOBJ) continue;

            if(!(i->xType == PO_LINE_START && i->xArgs[0] == tag)) continue;

            collectPolyobjLines(polyLines, i);
            if(!polyLines.empty())
            {
                int8_t sequenceType = i->xArgs[2];
                if(sequenceType >= SEQTYPE_NUMSEQ) sequenceType = 0;

                createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
                return true;
            }
            return false;
        }

        // Perhaps a PO_LINE_EXPLICIT linedef set with this tag?
        for(int n = 0; ; ++n)
        {
            bool foundAnotherLine = false;

            DENG2_FOR_EACH(Lines, i, lines)
            {
                // Already belongs to another polyobj?
                if(i->aFlags & LAF_POLYOBJ) continue;

                if(i->xType == PO_LINE_EXPLICIT && i->xArgs[0] == tag)
                {
                    if(i->xArgs[1] <= 0)
                    {
                        LOG_WARNING("Linedef missing (probably #%d) in explicit polyobj (tag:%d).") << n + 1 << tag;
                        return false;
                    }

                    if(i->xArgs[1] == n + 1)
                    {
                        // Add this line to the list.
                        polyLines.push_back( i - lines.begin() );
                        foundAnotherLine = true;

                        // Clear any special.
                        i->xType = 0;
                        i->xArgs[0] = 0;
                    }
                }
            }

            if(foundAnotherLine)
            {
                // Check if an explicit line order has been skipped.
                // A line has been skipped if there are any more explicit lines with
                // the current tag value.
                DENG2_FOR_EACH(Lines, i, lines)
                {
                    if(i->xType == PO_LINE_EXPLICIT && i->xArgs[0] == tag)
                    {
                        LOG_WARNING("Linedef missing (#%d) in explicit polyobj (tag:%d).") << n << tag;
                        return false;
                    }
                }
            }
            else
            {
                // All lines have now been found.
                break;
            }
        }

        if(polyLines.empty())
        {
            LOG_WARNING("Failed to locate a single line for polyobj (tag:%d).") << tag;
            return false;
        }

        mline_t *line = &lines[ polyLines.front() ];
        int8_t const sequenceType = line->xArgs[3];

        // Setup the mirror if it exists.
        line->xArgs[1] = line->xArgs[2];

        createPolyobj(polyLines, tag, sequenceType, anchorX, anchorY);
        return true;
    }

    /**
     * @param lineList  @c NULL, will cause IterFindPolyLines to count the number
     *                  of lines in the polyobj.
     */
    void collectPolyobjLinesWorker(LineList &lineList, coord_t x, coord_t y)
    {
        DENG2_FOR_EACH(Lines, i, lines)
        {
            // Already belongs to another polyobj?
            if(i->aFlags & LAF_POLYOBJ) continue;

            // Have we already encounterd this?
            if(i->validCount == validCount) continue;

            coord_t v1[2] = { vertexes[i->v[0] * 2],
                              vertexes[i->v[0] * 2 + 1] };

            coord_t v2[2] = { vertexes[i->v[1] * 2],
                              vertexes[i->v[1] * 2 + 1] };

            if(de::fequal(v1[VX], x) && de::fequal(v1[VY], y))
            {
                i->validCount = validCount;
                lineList.push_back( i - lines.begin() );
                collectPolyobjLinesWorker(lineList, v2[VX], v2[VY]);
            }
        }
    }

    /**
     * @todo This terribly inefficent (naive) algorithm may need replacing
     *       (it is far outside an acceptable polynomial range!).
     */
    void collectPolyobjLines(LineList &lineList, Lines::iterator lineIt)
    {
        mline_t &line = *lineIt;
        line.xType    = 0;
        line.xArgs[0] = 0;

        coord_t v2[2] = { vertexes[line.v[1] * 2],
                          vertexes[line.v[1] * 2 + 1] };

        validCount++;
        // Insert the first line.
        lineList.push_back(lineIt - lines.begin());
        line.validCount = validCount;
        collectPolyobjLinesWorker(lineList, v2[VX], v2[VY]);
    }
};

Id1Map::Id1Map(MapFormatId format) : d(new Instance(this))
{
    d->mapFormat = format;
}

MapFormatId Id1Map::format() const
{
    return d->mapFormat;
}

/// @todo fixme: A real performance killer...
AutoStr *Id1Map::composeMaterialRef(MaterialDictId id)
{
    AutoStr *ref = AutoStr_NewStd();
    String const &material = findMaterialInDictionary(id);
    QByteArray materialUtf8 = material.toUtf8();
    Str_Set(ref, materialUtf8.constData());
    return ref;
}

String const &Id1Map::findMaterialInDictionary(MaterialDictId id) const
{
    return d->materials.stringRef(id);
}

MaterialDictId Id1Map::addMaterialToDictionary(char const *name, MaterialDictGroup group)
{
    DENG2_ASSERT(name != 0);

    // Prepare the encoded URI for insertion into the dictionary.
    AutoStr *uriCString;
    if(d->mapFormat == MF_DOOM64)
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
        AutoStr *path = Str_PercentEncode(AutoStr_FromText(name));
        Uri *uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
        Uri_SetScheme(uri, group == MG_PLANE? "Flats" : "Textures");
        uriCString = Uri_Compose(uri);
        Uri_Delete(uri);
    }

    // Intern this material URI in the dictionary.
    MaterialDictId internId = d->materials.intern(de::String(Str_Text(uriCString)));

    // We're done (phew!).
    return internId;
}

bool Id1Map::loadVertexes(reader_s *reader, int numElements)
{
    DENG2_ASSERT(reader != 0);

    LOG_TRACE("Processing vertexes...");
    for(int n = 0; n < numElements; ++n)
    {
        switch(d->mapFormat)
        {
        default:
        case MF_DOOM:
            d->vertexes[n * 2]     = coord_t( SHORT(Reader_ReadInt16(reader)) );
            d->vertexes[n * 2 + 1] = coord_t( SHORT(Reader_ReadInt16(reader)) );
            break;

        case MF_DOOM64:
            d->vertexes[n * 2]     = coord_t( FIX2FLT(LONG(Reader_ReadInt32(reader))) );
            d->vertexes[n * 2 + 1] = coord_t( FIX2FLT(LONG(Reader_ReadInt32(reader))) );
            break;
        }
    }

    return true;
}

bool Id1Map::loadLineDefs(reader_s *reader, int numElements)
{
    DENG2_ASSERT(reader != 0);

    LOG_TRACE("Processing line definitions...");
    if(numElements)
    {
        d->lines.reserve(d->lines.size() + numElements);
        for(int n = 0; n < numElements; ++n)
        {
            switch(d->mapFormat)
            {
            default:
            case MF_DOOM:
                d->lines.push_back(mline_t());
                MLine_Read(&d->lines.back(), n, reader);
                break;

            case MF_DOOM64:
                d->lines.push_back(mline_t());
                MLine64_Read(&d->lines.back(), n, reader);
                break;

            case MF_HEXEN:
                d->lines.push_back(mline_t());
                MLineHx_Read(&d->lines.back(), n, reader);
                break;
            }
        }
    }

    return true;
}

bool Id1Map::loadSideDefs(reader_s *reader, int numElements)
{
    DENG2_ASSERT(reader != 0);

    LOG_TRACE("Processing side definitions...");
    if(numElements)
    {
        d->sides.reserve(d->sides.size() + numElements);
        for(int n = 0; n < numElements; ++n)
        {
            switch(d->mapFormat)
            {
            default:
            case MF_DOOM:
                d->sides.push_back(mside_t());
                MSide_Read(&d->sides.back(), n, reader);
                break;

            case MF_DOOM64:
                d->sides.push_back(mside_t());
                MSide64_Read(&d->sides.back(), n, reader);
                break;
            }
        }
    }

    return true;
}

bool Id1Map::loadSectors(reader_s *reader, int numElements)
{
    DENG2_ASSERT(reader != 0);

    LOG_TRACE("Processing sectors...");
    if(numElements)
    {
        d->sectors.reserve(d->sectors.size() + numElements);
        for(int n = 0; n < numElements; ++n)
        {
            switch(d->mapFormat)
            {
            default:
                d->sectors.push_back(msector_t());
                MSector_Read(&d->sectors.back(), n, reader);
                break;

            case MF_DOOM64:
                d->sectors.push_back(msector_t());
                MSector64_Read(&d->sectors.back(), n, reader);
                break;
            }
        }
    }

    return true;
}

bool Id1Map::loadThings(reader_s *reader, int numElements)
{
    DENG2_ASSERT(reader != 0);

    LOG_TRACE("Processing things...");
    if(numElements)
    {
        d->things.reserve(d->things.size() + numElements);
        for(int n = 0; n < numElements; ++n)
        {
            switch(d->mapFormat)
            {
            default:
            case MF_DOOM:
                d->things.push_back(mthing_t());
                MThing_Read(&d->things.back(), n, reader);
                break;

            case MF_DOOM64:
                d->things.push_back(mthing_t());
                MThing64_Read(&d->things.back(), n, reader);
                break;

            case MF_HEXEN:
                d->things.push_back(mthing_t());
                MThingHx_Read(&d->things.back(), n, reader);
                break;
            }
        }
    }

    return true;
}

bool Id1Map::loadSurfaceTints(reader_s *reader, int numElements)
{
    DENG2_ASSERT(reader != 0);

    LOG_TRACE("Processing surface tints...");
    if(numElements)
    {
        d->surfaceTints.reserve(d->surfaceTints.size() + numElements);
        for(int n = 0; n < numElements; ++n)
        {
            d->surfaceTints.push_back(surfacetint_t());
            SurfaceTint_Read(&d->surfaceTints.back(), n, reader);
        }
    }

    return true;
}

void Id1Map::load(MapLumpInfos &lumpInfos)
{
    // Allocate the vertices first as a large contiguous array suitable for
    // passing directly to Doomsday's MapEdit interface.
    size_t elementSize = ElementSizeForMapLumpType(d->mapFormat, ML_VERTEXES);
    uint numElements = lumpInfos[ML_VERTEXES]->length / elementSize;

    d->numVertexes = numElements;
    d->vertexes = (coord_t *)M_Malloc(d->numVertexes * 2 * sizeof(*d->vertexes));

    DENG2_FOR_EACH_CONST(MapLumpInfos, i, lumpInfos)
    {
        MapLumpInfo *info = i->second;

        if(!info || !info->length) continue;

        elementSize = ElementSizeForMapLumpType(d->mapFormat, info->type);
        if(!elementSize) continue;

        // Process this data lump.
        numElements = info->length / elementSize;
        reader_s *reader = bufferLump(info);
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

void Id1Map::analyze()
{
    uint startTime = Timer_RealMilliseconds();

    LOG_AS("Id1Map");
    if(d->mapFormat == MF_HEXEN)
    {
        LOG_TRACE("Locating polyobjs...");
        DENG2_FOR_EACH(Things, i, d->things)
        {
            // A polyobj anchor?
            if(i->doomEdNum == PO_ANCHOR_DOOMEDNUM)
            {
                int const tag = i->angle;
                d->findAndCreatePolyobj(tag, i->origin[VX], i->origin[VY]);
            }
        }
    }

    LOG_VERBOSE("Analyses completed in %.2f seconds.") << ((Timer_RealMilliseconds() - startTime) / 1000.0f);
}

void Id1Map::transferVertexes()
{
    LOG_TRACE("Transfering vertexes...");
    int *indices = new int[d->numVertexes];
    for(uint i = 0; i < d->numVertexes; ++i)
    {
        indices[i] = i;
    }
    MPE_VertexCreatev(d->numVertexes, d->vertexes, indices, 0);
    delete[] indices;
}

void Id1Map::transferSectors()
{
    LOG_TRACE("Transfering sectors...");

    DENG2_FOR_EACH(Sectors, i, d->sectors)
    {
        int idx = MPE_SectorCreate(float(i->lightLevel) / 255.0f, 1, 1, 1, i->index);

        MPE_PlaneCreate(idx, i->floorHeight, composeMaterialRef(i->floorMaterial),
                        0, 0, 1, 1, 1, 1, 0, 0, 1, -1);
        MPE_PlaneCreate(idx, i->ceilHeight, composeMaterialRef(i->ceilMaterial),
                        0, 0, 1, 1, 1, 1, 0, 0, -1, -1);

        MPE_GameObjProperty("XSector", idx, "Tag",    DDVT_SHORT, &i->tag);
        MPE_GameObjProperty("XSector", idx, "Type",   DDVT_SHORT, &i->type);

        if(d->mapFormat == MF_DOOM64)
        {
            MPE_GameObjProperty("XSector", idx, "Flags",          DDVT_SHORT, &i->d64flags);
            MPE_GameObjProperty("XSector", idx, "CeilingColor",   DDVT_SHORT, &i->d64ceilingColor);
            MPE_GameObjProperty("XSector", idx, "FloorColor",     DDVT_SHORT, &i->d64floorColor);
            MPE_GameObjProperty("XSector", idx, "UnknownColor",   DDVT_SHORT, &i->d64unknownColor);
            MPE_GameObjProperty("XSector", idx, "WallTopColor",   DDVT_SHORT, &i->d64wallTopColor);
            MPE_GameObjProperty("XSector", idx, "WallBottomColor", DDVT_SHORT, &i->d64wallBottomColor);
        }
    }
}

void Id1Map::transferLinesAndSides()
{
    LOG_TRACE("Transfering lines and sides...");
    DENG2_FOR_EACH(Lines, i, d->lines)
    {
        mside_t *front = ((i)->sides[RIGHT] >= 0? &d->sides[(i)->sides[RIGHT]] : 0);
        mside_t *back  = ((i)->sides[LEFT]  >= 0? &d->sides[(i)->sides[LEFT]] : 0);

        int sideFlags = (d->mapFormat == MF_DOOM64? SDF_MIDDLE_STRETCH : 0);

        // Interpret the lack of a ML_TWOSIDED line flag to mean the
        // suppression of the side relative back sector.
        if(!(i->flags & 0x4 /*ML_TWOSIDED*/) && front && back)
            sideFlags |= SDF_SUPPRESS_BACK_SECTOR;

        int lineIdx = MPE_LineCreate(i->v[0], i->v[1], front? front->sector : -1,
                                     back? back->sector : -1, i->ddFlags, i->index);
        if(front)
        {
            MPE_LineAddSide(lineIdx, RIGHT, sideFlags,
                            composeMaterialRef(front->topMaterial),
                            front->offset[VX], front->offset[VY], 1, 1, 1,
                            composeMaterialRef(front->middleMaterial),
                            front->offset[VX], front->offset[VY], 1, 1, 1, 1,
                            composeMaterialRef(front->bottomMaterial),
                            front->offset[VX], front->offset[VY], 1, 1, 1,
                            front->index);
        }
        if(back)
        {
            MPE_LineAddSide(lineIdx, LEFT, sideFlags,
                            composeMaterialRef(back->topMaterial),
                            back->offset[VX], back->offset[VY], 1, 1, 1,
                            composeMaterialRef(back->middleMaterial),
                            back->offset[VX], back->offset[VY], 1, 1, 1, 1,
                            composeMaterialRef(back->bottomMaterial),
                            back->offset[VX], back->offset[VY], 1, 1, 1,
                            back->index);
        }

        MPE_GameObjProperty("XLinedef", lineIdx, "Flags", DDVT_SHORT, &i->flags);

        switch(d->mapFormat)
        {
        default:
        case MF_DOOM:
            MPE_GameObjProperty("XLinedef", lineIdx, "Type",  DDVT_SHORT, &i->dType);
            MPE_GameObjProperty("XLinedef", lineIdx, "Tag",   DDVT_SHORT, &i->dTag);
            break;

        case MF_DOOM64:
            MPE_GameObjProperty("XLinedef", lineIdx, "DrawFlags", DDVT_BYTE,  &i->d64drawFlags);
            MPE_GameObjProperty("XLinedef", lineIdx, "TexFlags",  DDVT_BYTE,  &i->d64texFlags);
            MPE_GameObjProperty("XLinedef", lineIdx, "Type",      DDVT_BYTE,  &i->d64type);
            MPE_GameObjProperty("XLinedef", lineIdx, "UseType",   DDVT_BYTE,  &i->d64useType);
            MPE_GameObjProperty("XLinedef", lineIdx, "Tag",       DDVT_SHORT, &i->d64tag);
            break;

        case MF_HEXEN:
            MPE_GameObjProperty("XLinedef", lineIdx, "Type", DDVT_BYTE, &i->xType);
            MPE_GameObjProperty("XLinedef", lineIdx, "Arg0", DDVT_BYTE, &i->xArgs[0]);
            MPE_GameObjProperty("XLinedef", lineIdx, "Arg1", DDVT_BYTE, &i->xArgs[1]);
            MPE_GameObjProperty("XLinedef", lineIdx, "Arg2", DDVT_BYTE, &i->xArgs[2]);
            MPE_GameObjProperty("XLinedef", lineIdx, "Arg3", DDVT_BYTE, &i->xArgs[3]);
            MPE_GameObjProperty("XLinedef", lineIdx, "Arg4", DDVT_BYTE, &i->xArgs[4]);
            break;
        }
    }
}

void Id1Map::transferSurfaceTints()
{
    if(d->surfaceTints.empty()) return;

    LOG_TRACE("Transfering surface tints...");
    DENG2_FOR_EACH(SurfaceTints, i, d->surfaceTints)
    {
        int idx = i - d->surfaceTints.begin();

        MPE_GameObjProperty("Light", idx, "ColorR",   DDVT_FLOAT, &i->rgb[0]);
        MPE_GameObjProperty("Light", idx, "ColorG",   DDVT_FLOAT, &i->rgb[1]);
        MPE_GameObjProperty("Light", idx, "ColorB",   DDVT_FLOAT, &i->rgb[2]);
        MPE_GameObjProperty("Light", idx, "XX0",      DDVT_BYTE,  &i->xx[0]);
        MPE_GameObjProperty("Light", idx, "XX1",      DDVT_BYTE,  &i->xx[1]);
        MPE_GameObjProperty("Light", idx, "XX2",      DDVT_BYTE,  &i->xx[2]);
    }
}

void Id1Map::transferPolyobjs()
{
    if(d->polyobjs.empty()) return;

    LOG_TRACE("Transfering polyobjs...");
    DENG2_FOR_EACH(Polyobjs, i, d->polyobjs)
    {
        MPE_PolyobjCreate(i->lineIndices, i->lineCount, i->tag, i->seqType,
                          coord_t(i->anchor[VX]), coord_t(i->anchor[VY]),
                          i->index);
    }
}

void Id1Map::transferThings()
{
    if(d->things.empty()) return;

    LOG_TRACE("Transfering things...");
    DENG2_FOR_EACH(Things, i, d->things)
    {
        int idx = i - d->things.begin();

        MPE_GameObjProperty("Thing", idx, "X",            DDVT_SHORT, &i->origin[VX]);
        MPE_GameObjProperty("Thing", idx, "Y",            DDVT_SHORT, &i->origin[VY]);
        MPE_GameObjProperty("Thing", idx, "Z",            DDVT_SHORT, &i->origin[VZ]);
        MPE_GameObjProperty("Thing", idx, "Angle",        DDVT_ANGLE, &i->angle);
        MPE_GameObjProperty("Thing", idx, "DoomEdNum",    DDVT_SHORT, &i->doomEdNum);
        MPE_GameObjProperty("Thing", idx, "SkillModes",   DDVT_INT,   &i->skillModes);
        MPE_GameObjProperty("Thing", idx, "Flags",        DDVT_INT,   &i->flags);

        if(d->mapFormat == MF_DOOM64)
        {
            MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &i->d64TID);
        }
        else if(d->mapFormat == MF_HEXEN)
        {
            MPE_GameObjProperty("Thing", idx, "Special",  DDVT_BYTE,  &i->xSpecial);
            MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &i->xTID);
            MPE_GameObjProperty("Thing", idx, "Arg0",     DDVT_BYTE,  &i->xArgs[0]);
            MPE_GameObjProperty("Thing", idx, "Arg1",     DDVT_BYTE,  &i->xArgs[1]);
            MPE_GameObjProperty("Thing", idx, "Arg2",     DDVT_BYTE,  &i->xArgs[2]);
            MPE_GameObjProperty("Thing", idx, "Arg3",     DDVT_BYTE,  &i->xArgs[3]);
            MPE_GameObjProperty("Thing", idx, "Arg4",     DDVT_BYTE,  &i->xArgs[4]);
        }
    }
}

int Id1Map::transfer()
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

static uint8_t *readPtr;
static uint8_t *readBuffer;
static size_t readBufferSize;

static char readInt8(reader_s *r)
{
    if(!r) return 0;
    char value = *((int8_t const *) (readPtr));
    readPtr += 1;
    return value;
}

static short readInt16(reader_s *r)
{
    if(!r) return 0;
    short value = *((int16_t const *) (readPtr));
    readPtr += 2;
    return value;
}

static int readInt32(reader_s *r)
{
    if(!r) return 0;
    int value = *((int32_t const *) (readPtr));
    readPtr += 4;
    return value;
}

static float readFloat(reader_s *r)
{
    DENG2_ASSERT(sizeof(float) == 4);
    if(!r) return 0;
    int32_t val = *((int32_t const *) (readPtr));
    float returnValue = 0;
    std::memcpy(&returnValue, &val, 4);
    return returnValue;
}

static void readData(reader_s *r, char *data, int len)
{
    if(!r) return;
    std::memcpy(data, readPtr, len);
    readPtr += len;
}

/// @todo It should not be necessary to buffer the lump data here.
static reader_s *bufferLump(MapLumpInfo *info)
{
    // Need to enlarge our read buffer?
    if(info->length > readBufferSize)
    {
        readBuffer = (uint8_t *)M_Realloc(readBuffer, info->length);
        readBufferSize = info->length;
    }

    // Buffer the entire lump.
    W_ReadLump(info->lump, readBuffer);

    // Create the reader.
    readPtr = readBuffer;
    return Reader_NewWithCallbacks(readInt8, readInt16, readInt32, readFloat, readData);
}

static void clearReadBuffer()
{
    if(!readBuffer) return;
    M_Free(readBuffer);
    readBuffer = 0;
    readBufferSize = 0;
}
