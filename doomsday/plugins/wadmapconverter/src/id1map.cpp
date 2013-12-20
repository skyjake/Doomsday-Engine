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
#include "id1map_datatypes.h"
#include "id1map_load.h"
#include "id1map_util.h"
#include <de/libdeng2.h>
#include <de/Error>
#include <de/Log>
#include <de/Time>
#include <de/memory.h>
#include <de/timer.h>

using namespace de;

static reader_s *bufferLump(MapLumpInfo *info);
static void clearReadBuffer();

static uint validCount = 0; // Used for Polyobj LineDef collection.

DENG2_PIMPL(Id1Map)
{
    Format format;

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
        , format(UnknownFormat)
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

    inline String const &findMaterialInDictionary(MaterialId id) const {
        return materials.stringRef(id);
    }

    /// @todo fixme: A real performance killer...
    AutoStr *composeMaterialRef(MaterialId id)
    {
        AutoStr *ref = AutoStr_NewStd();
        String const &material = findMaterialInDictionary(id);
        QByteArray materialUtf8 = material.toUtf8();
        Str_Set(ref, materialUtf8.constData());
        return ref;
    }

    bool loadVertexes(reader_s *reader, int numElements)
    {
        DENG2_ASSERT(reader != 0);

        LOG_TRACE("Processing vertexes...");
        for(int n = 0; n < numElements; ++n)
        {
            switch(format)
            {
            default:
            case DoomFormat:
                vertexes[n * 2]     = coord_t( SHORT(Reader_ReadInt16(reader)) );
                vertexes[n * 2 + 1] = coord_t( SHORT(Reader_ReadInt16(reader)) );
                break;

            case Doom64Format:
                vertexes[n * 2]     = coord_t( FIX2FLT(LONG(Reader_ReadInt32(reader))) );
                vertexes[n * 2 + 1] = coord_t( FIX2FLT(LONG(Reader_ReadInt32(reader))) );
                break;
            }
        }

        return true;
    }

    bool loadLineDefs(reader_s *reader, int numElements)
    {
        DENG2_ASSERT(reader != 0);

        LOG_TRACE("Processing line definitions...");
        if(numElements)
        {
            lines.reserve(lines.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                switch(format)
                {
                default:
                case DoomFormat:
                    lines.push_back(mline_t());
                    MLine_Read(&lines.back(), n, reader);
                    break;

                case Doom64Format:
                    lines.push_back(mline_t());
                    MLine64_Read(&lines.back(), n, reader);
                    break;

                case HexenFormat:
                    lines.push_back(mline_t());
                    MLineHx_Read(&lines.back(), n, reader);
                    break;
                }
            }
        }

        return true;
    }

    bool loadSideDefs(reader_s *reader, int numElements)
    {
        DENG2_ASSERT(reader != 0);

        LOG_TRACE("Processing side definitions...");
        if(numElements)
        {
            sides.reserve(sides.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                switch(format)
                {
                default:
                case DoomFormat:
                    sides.push_back(mside_t());
                    MSide_Read(&sides.back(), n, reader);
                    break;

                case Doom64Format:
                    sides.push_back(mside_t());
                    MSide64_Read(&sides.back(), n, reader);
                    break;
                }
            }
        }

        return true;
    }

    bool loadSectors(reader_s *reader, int numElements)
    {
        DENG2_ASSERT(reader != 0);

        LOG_TRACE("Processing sectors...");
        if(numElements)
        {
            sectors.reserve(sectors.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                switch(format)
                {
                default:
                    sectors.push_back(msector_t());
                    MSector_Read(&sectors.back(), n, reader);
                    break;

                case Doom64Format:
                    sectors.push_back(msector_t());
                    MSector64_Read(&sectors.back(), n, reader);
                    break;
                }
            }
        }

        return true;
    }

    bool loadThings(reader_s *reader, int numElements)
    {
        DENG2_ASSERT(reader != 0);

        LOG_TRACE("Processing things...");
        if(numElements)
        {
            things.reserve(things.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                switch(format)
                {
                default:
                case DoomFormat:
                    things.push_back(mthing_t());
                    MThing_Read(&things.back(), n, reader);
                    break;

                case Doom64Format:
                    things.push_back(mthing_t());
                    MThing64_Read(&things.back(), n, reader);
                    break;

                case HexenFormat:
                    things.push_back(mthing_t());
                    MThingHx_Read(&things.back(), n, reader);
                    break;
                }
            }
        }

        return true;
    }

    bool loadSurfaceTints(reader_s *reader, int numElements)
    {
        DENG2_ASSERT(reader != 0);

        LOG_TRACE("Processing surface tints...");
        if(numElements)
        {
            surfaceTints.reserve(surfaceTints.size() + numElements);
            for(int n = 0; n < numElements; ++n)
            {
                surfaceTints.push_back(surfacetint_t());
                SurfaceTint_Read(&surfaceTints.back(), n, reader);
            }
        }

        return true;
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

    void analyze()
    {
        Time begunAt;

        if(format == HexenFormat)
        {
            LOG_TRACE("Locating polyobjs...");
            DENG2_FOR_EACH(Things, i, things)
            {
                // A polyobj anchor?
                if(i->doomEdNum == PO_ANCHOR_DOOMEDNUM)
                {
                    int const tag = i->angle;
                    findAndCreatePolyobj(tag, i->origin[VX], i->origin[VY]);
                }
            }
        }

        LOG_INFO(String("Analyses completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));
    }

    void transferVertexes()
    {
        LOG_TRACE("Transfering vertexes...");
        int *indices = new int[numVertexes];
        for(uint i = 0; i < numVertexes; ++i)
        {
            indices[i] = i;
        }
        MPE_VertexCreatev(numVertexes, vertexes, indices, 0);
        delete[] indices;
    }

    void transferSectors()
    {
        LOG_TRACE("Transfering sectors...");

        DENG2_FOR_EACH(Sectors, i, sectors)
        {
            int idx = MPE_SectorCreate(float(i->lightLevel) / 255.0f, 1, 1, 1, i->index);

            MPE_PlaneCreate(idx, i->floorHeight, composeMaterialRef(i->floorMaterial),
                            0, 0, 1, 1, 1, 1, 0, 0, 1, -1);
            MPE_PlaneCreate(idx, i->ceilHeight, composeMaterialRef(i->ceilMaterial),
                            0, 0, 1, 1, 1, 1, 0, 0, -1, -1);

            MPE_GameObjProperty("XSector", idx, "Tag",    DDVT_SHORT, &i->tag);
            MPE_GameObjProperty("XSector", idx, "Type",   DDVT_SHORT, &i->type);

            if(format == Doom64Format)
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

    void transferLinesAndSides()
    {
        LOG_TRACE("Transfering lines and sides...");
        DENG2_FOR_EACH(Lines, i, lines)
        {
            mside_t *front = ((i)->sides[RIGHT] >= 0? &sides[(i)->sides[RIGHT]] : 0);
            mside_t *back  = ((i)->sides[LEFT]  >= 0? &sides[(i)->sides[LEFT]] : 0);

            int sideFlags = (format == Doom64Format? SDF_MIDDLE_STRETCH : 0);

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

            switch(format)
            {
            default:
            case DoomFormat:
                MPE_GameObjProperty("XLinedef", lineIdx, "Type",  DDVT_SHORT, &i->dType);
                MPE_GameObjProperty("XLinedef", lineIdx, "Tag",   DDVT_SHORT, &i->dTag);
                break;

            case Doom64Format:
                MPE_GameObjProperty("XLinedef", lineIdx, "DrawFlags", DDVT_BYTE,  &i->d64drawFlags);
                MPE_GameObjProperty("XLinedef", lineIdx, "TexFlags",  DDVT_BYTE,  &i->d64texFlags);
                MPE_GameObjProperty("XLinedef", lineIdx, "Type",      DDVT_BYTE,  &i->d64type);
                MPE_GameObjProperty("XLinedef", lineIdx, "UseType",   DDVT_BYTE,  &i->d64useType);
                MPE_GameObjProperty("XLinedef", lineIdx, "Tag",       DDVT_SHORT, &i->d64tag);
                break;

            case HexenFormat:
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

    void transferSurfaceTints()
    {
        if(surfaceTints.empty()) return;

        LOG_TRACE("Transfering surface tints...");
        DENG2_FOR_EACH(SurfaceTints, i, surfaceTints)
        {
            int idx = i - surfaceTints.begin();

            MPE_GameObjProperty("Light", idx, "ColorR",   DDVT_FLOAT, &i->rgb[0]);
            MPE_GameObjProperty("Light", idx, "ColorG",   DDVT_FLOAT, &i->rgb[1]);
            MPE_GameObjProperty("Light", idx, "ColorB",   DDVT_FLOAT, &i->rgb[2]);
            MPE_GameObjProperty("Light", idx, "XX0",      DDVT_BYTE,  &i->xx[0]);
            MPE_GameObjProperty("Light", idx, "XX1",      DDVT_BYTE,  &i->xx[1]);
            MPE_GameObjProperty("Light", idx, "XX2",      DDVT_BYTE,  &i->xx[2]);
        }
    }

    void transferPolyobjs()
    {
        if(polyobjs.empty()) return;

        LOG_TRACE("Transfering polyobjs...");
        DENG2_FOR_EACH(Polyobjs, i, polyobjs)
        {
            MPE_PolyobjCreate(i->lineIndices, i->lineCount, i->tag, i->seqType,
                              coord_t(i->anchor[VX]), coord_t(i->anchor[VY]),
                              i->index);
        }
    }

    void transferThings()
    {
        if(things.empty()) return;

        LOG_TRACE("Transfering things...");
        DENG2_FOR_EACH(Things, i, things)
        {
            int idx = i - things.begin();

            MPE_GameObjProperty("Thing", idx, "X",            DDVT_SHORT, &i->origin[VX]);
            MPE_GameObjProperty("Thing", idx, "Y",            DDVT_SHORT, &i->origin[VY]);
            MPE_GameObjProperty("Thing", idx, "Z",            DDVT_SHORT, &i->origin[VZ]);
            MPE_GameObjProperty("Thing", idx, "Angle",        DDVT_ANGLE, &i->angle);
            MPE_GameObjProperty("Thing", idx, "DoomEdNum",    DDVT_SHORT, &i->doomEdNum);
            MPE_GameObjProperty("Thing", idx, "SkillModes",   DDVT_INT,   &i->skillModes);
            MPE_GameObjProperty("Thing", idx, "Flags",        DDVT_INT,   &i->flags);

            if(format == Doom64Format)
            {
                MPE_GameObjProperty("Thing", idx, "ID",       DDVT_SHORT, &i->d64TID);
            }
            else if(format == HexenFormat)
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

Id1Map::Id1Map(Format format) : d(new Instance(this))
{
    d->format = format;
}

Id1Map::Format Id1Map::format() const
{
    return d->format;
}

Id1Map::MaterialId Id1Map::toMaterialId(String name, MaterialGroup group)
{
    // Prepare the encoded URI for insertion into the dictionary.
    AutoStr *uriCString;

    // In original DOOM, texture name references beginning with the
    // hypen '-' character are always treated as meaning "no reference"
    // or "invalid texture" and surfaces using them were not drawn.
    if(group != PlaneMaterials && name[0] == '-')
    {
        return 0; // Not a valid id.
    }

    // Material paths must be encoded.
    AutoStr *path = Str_PercentEncode(AutoStr_FromText(name.toUtf8().constData()));
    Uri *uri = Uri_NewWithPath2(Str_Text(path), RC_NULL);
    Uri_SetScheme(uri, group == PlaneMaterials? "Flats" : "Textures");
    uriCString = Uri_Compose(uri);
    Uri_Delete(uri);

    // Intern this material URI in the dictionary.
    return d->materials.intern(String(Str_Text(uriCString)));
}

Id1Map::MaterialId Id1Map::toMaterialId(int uniqueId, MaterialGroup group)
{
    // Prepare the encoded URI for insertion into the dictionary.
    AutoStr *uriCString;

    Uri *textureUrn = Uri_NewWithPath2(Str_Text(Str_Appendf(AutoStr_NewStd(), "urn:%s:%i", group == PlaneMaterials? "Flats" : "Textures", uniqueId)), RC_NULL);
    Uri *uri = Materials_ComposeUri(P_ToIndex(DD_MaterialForTextureUri(textureUrn)));
    uriCString = Uri_Compose(uri);
    Uri_Delete(uri);
    Uri_Delete(textureUrn);

    // Intern this material URI in the dictionary.
    return d->materials.intern(String(Str_Text(uriCString)));
}

void Id1Map::load(MapLumpInfos &lumpInfos)
{
    // Allocate the vertices first as a large contiguous array suitable for
    // passing directly to Doomsday's MapEdit interface.
    size_t elementSize = ElementSizeForMapLumpType(d->format, ML_VERTEXES);
    uint numElements = lumpInfos[ML_VERTEXES]->length / elementSize;

    d->numVertexes = numElements;
    d->vertexes = (coord_t *)M_Malloc(d->numVertexes * 2 * sizeof(*d->vertexes));

    DENG2_FOR_EACH_CONST(MapLumpInfos, i, lumpInfos)
    {
        MapLumpInfo *info = i->second;

        if(!info || !info->length) continue;

        elementSize = ElementSizeForMapLumpType(d->format, info->type);
        if(!elementSize) continue;

        // Process this data lump.
        numElements = info->length / elementSize;
        reader_s *reader = bufferLump(info);
        switch(info->type)
        {
        default: break;

        case ML_VERTEXES: d->loadVertexes(reader, numElements);        break;
        case ML_LINEDEFS: d->loadLineDefs(reader, numElements);        break;
        case ML_SIDEDEFS: d->loadSideDefs(reader, numElements);        break;
        case ML_SECTORS:  d->loadSectors(reader, numElements);         break;
        case ML_THINGS:   d->loadThings(reader, numElements);          break;
        case ML_LIGHTS:   d->loadSurfaceTints(reader, numElements);    break;
        }
        Reader_Delete(reader);
    }

    // We're done with the read buffer.
    clearReadBuffer();

    // Perform post load analyses.
    d->analyze();
}

int Id1Map::transfer()
{
    LOG_AS("Id1Map");

    Time begunAt;

    d->transferVertexes();
    d->transferSectors();
    d->transferLinesAndSides();
    d->transferSurfaceTints();
    d->transferPolyobjs();
    d->transferThings();

    LOG_INFO(String("Transfer completed in %1 seconds.").arg(begunAt.since(), 0, 'g', 2));

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
