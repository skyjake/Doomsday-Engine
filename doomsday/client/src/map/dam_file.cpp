/** @file dam_file.cpp
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Doomsday Archived Map (DAM) reader/writer.
 */

#include <lzss.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_dam.h"
#include "de_defs.h"
#include "de_misc.h"
#include "de_filesys.h"

#include "map/p_mapdata.h"

// Global archived map format version identifier. Increment when making
// changes to the structure of the format.
#define DAM_VERSION             1

// Segments of a doomsday archived map file.
typedef enum damsegment_e {
    DAMSEG_END = -1,                // Terminates a segment.
    DAMSEG_HEADER = 100,            // File-level meta.
    DAMSEG_RELOCATIONTABLES,        // Tables of offsets to file positions.
    DAMSEG_SYMBOLTABLES,            // Global symbol tables.

    DAMSEG_MAP = 200,               // Start of the map data.
    DAMSEG_POLYOBJS,
    DAMSEG_VERTEXES,
    DAMSEG_LINES,
    DAMSEG_SIDES,
    DAMSEG_SECTORS,
    DAMSEG_BSPLEAFS,
    DAMSEG_HEDGES,
    DAMSEG_BSPNODES,
    DAMSEG_BLOCKMAP,
    DAMSEG_REJECT
} damsegment_t;

static LZFILE* mapFile;
static int mapFileVersion;

static boolean openMapFile(const char* path, boolean write)
{
    mapFile = NULL;
    mapFileVersion = 0;
    mapFile = lzOpen((char*)path, (write? F_WRITE_PACKED : F_READ_PACKED));

    return ((mapFile)? true : false);
}

static boolean closeMapFile(void)
{
    return (lzClose(mapFile)? true : false);
}

/*
static void writeNBytes(void* data, long len)
{
    lzWrite(data, len, mapFile);
}
*/

static void writeByte(byte val)
{
    lzPutC(val, mapFile);
}

static void writeShort(short val)
{
    lzPutW(val, mapFile);
}

static void writeLong(long val)
{
    lzPutL(val, mapFile);
}

static void writeFloat(float val)
{
    long temp = 0;
    memcpy(&temp, &val, 4);
    lzPutL(temp, mapFile);
}

/*
static void readNBytes(void *ptr, long len)
{
    lzRead(ptr, len, mapFile);
}
*/

static byte readByte(void)
{
    return lzGetC(mapFile);
}

static short readShort(void)
{
    return lzGetW(mapFile);
}

static long readLong(void)
{
    return lzGetL(mapFile);
}

static float readFloat(void)
{
    long    val = lzGetL(mapFile);
    float   returnValue = 0;

    memcpy(&returnValue, &val, 4);
    return returnValue;
}

/**
 * Exit with a fatal error if the value at the current location in the
 * map file does not match that associated with the specified segment.
 *
 * @param segType       Value by which to check for alignment.
 */
static void assertSegment(damsegment_t segment)
{
    if(readLong() != segment)
    {
        Con_Error("assertSegment: Segment [%d] failed alignment check",
                  (int) segment);
    }
}

static void beginSegment(damsegment_t segment)
{
    writeLong(segment);
}

static void endSegment(void)
{
    writeLong(DAMSEG_END);
}

static void writeVertex(const GameMap* map, uint idx)
{
    Vertex const *v = &map->vertexes[idx];

    writeFloat(v->origin[VX]);
    writeFloat(v->origin[VY]);
    writeLong((long) v->numLineOwners);

    if(v->numLineOwners > 0)
    {
        lineowner_t* own, *base;

        own = base = (v->lineOwners)->LO_prev;
        do
        {
            writeLong((long) (map->lineDefs.indexOf(own->lineDef) + 1));
            writeLong((long) own->angle);
            own = own->LO_prev;
        } while(own != base);
    }
}

static void readVertex(GameMap *map, uint idx)
{
    uint i;
    Vertex *v = &map->vertexes[idx];

    v->origin[VX] = readFloat();
    v->origin[VY] = readFloat();
    v->numLineOwners = (uint) readLong();

    if(v->numLineOwners > 0)
    {
        lineowner_t* own;

        v->lineOwners = NULL;
        for(i = 0; i < v->numLineOwners; ++i)
        {
            own = (lineowner_t *) Z_Malloc(sizeof(lineowner_t), PU_MAP, 0);
            own->lineDef = &map->lineDefs[(unsigned) (readLong() - 1)];
            own->angle = (binangle_t) readLong();

            own->LO_next = v->lineOwners;
            v->lineOwners = own;
        }

        own = v->lineOwners;
        do
        {
            own->LO_next->LO_prev = own;
            own = own->LO_next;
        } while(own);
        own->LO_prev = v->lineOwners;
    }
}

static void archiveVertexes(GameMap *map, boolean write)
{
    int i;

    if(write)
        beginSegment(DAMSEG_VERTEXES);
    else
        assertSegment(DAMSEG_VERTEXES);

    if(write)
    {
        writeLong(map->vertexes.size());
        for(i = 0; i < map->vertexes.size(); ++i)
            writeVertex(map, i);
    }
    else
    {
        map->vertexes.clearAndResize(readLong());
        for(i = 0; i < map->vertexes.size(); ++i)
            readVertex(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeLine(GameMap* map, uint idx)
{
    int i;
    LineDef* l = &map->lineDefs[idx];

    writeLong((long) (map->vertexes.indexOf(static_cast<Vertex const *>(l->v[0])) + 1));
    writeLong((long) (map->vertexes.indexOf(static_cast<Vertex const *>(l->v[1])) + 1));
    writeLong(l->flags);
    writeByte(l->inFlags);
    writeFloat(l->direction[VX]);
    writeFloat(l->direction[VY]);
    writeFloat(l->aaBox.minX);
    writeFloat(l->aaBox.minY);
    writeFloat(l->aaBox.maxX);
    writeFloat(l->aaBox.maxY);
    writeFloat(l->length);
    writeLong((long) l->angle);
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        writeByte(l->mapped[i]? 1 : 0);
    }

    for(i = 0; i < 2; ++i)
    {
        writeLong(l->L_sector(i)? (GameMap_SectorIndex(map, static_cast<Sector *>(l->L_sector(i))) + 1) : 0);
        writeLong(l->L_sidedef(i)? (GameMap_SideDefIndex(map, l->L_sidedef(i)) + 1) : 0);

        writeLong(l->L_side(i).hedgeLeft? (GameMap_HEdgeIndex(map, l->L_side(i).hedgeLeft)  + 1) : 0);
        writeLong(l->L_side(i).hedgeRight? (GameMap_HEdgeIndex(map, l->L_side(i).hedgeRight) + 1) : 0);
    }
}

static void readLine(GameMap* map, uint idx)
{
    int i;
    LineDef* l = &map->lineDefs[idx];

    l->v[0] = &map->vertexes[(unsigned) (readLong() - 1)];
    l->v[1] = &map->vertexes[(unsigned) (readLong() - 1)];
    l->flags = (int) readLong();
    l->inFlags = readByte();
    l->direction[VX] = readFloat();
    l->direction[VY] = readFloat();
    l->slopeType = M_SlopeType(l->direction);
    l->aaBox.minX = readFloat();
    l->aaBox.minY = readFloat();
    l->aaBox.maxX = readFloat();
    l->aaBox.maxY = readFloat();
    l->length = readFloat();
    l->angle = (binangle_t) readLong();
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        l->mapped[i] = (readByte()? true : false);
    }

    for(i = 0; i < 2; ++i)
    {
        long index;

        index= readLong();
        l->L_sector(i) = (index? GameMap_Sector(map, index-1) : NULL);

        index = readLong();
        l->L_sidedef(i) = (index? GameMap_SideDef(map, index-1) : NULL);

        index = readLong();
        l->L_side(i).hedgeLeft  = (index? GameMap_HEdge(map, index-1) : NULL);

        index = readLong();
        l->L_side(i).hedgeRight = (index? GameMap_HEdge(map, index-1) : NULL);
    }
}

static void archiveLines(GameMap* map, boolean write)
{
    uint i;

    if(write)
        beginSegment(DAMSEG_LINES);
    else
        assertSegment(DAMSEG_LINES);

    if(write)
    {
        writeLong(map->lineDefCount());
        for(i = 0; i < map->lineDefCount(); ++i)
            writeLine(map, i);
    }
    else
    {
        map->lineDefs.clearAndResize(readLong());
        for(i = 0; i < map->lineDefCount(); ++i)
            readLine(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSide(GameMap* map, uint idx)
{
    uint i;
    SideDef* s = &map->sideDefs[idx];

    for(i = 0; i < 3; ++i)
    {
        Surface* suf = &s->sections[i];

        writeLong(suf->flags);
        //writeLong(getMaterialDictID(materialDict, suf->material));
        writeLong((long) suf->blendMode);
        writeFloat(suf->normal[VX]);
        writeFloat(suf->normal[VY]);
        writeFloat(suf->normal[VZ]);
        writeFloat(suf->offset[VX]);
        writeFloat(suf->offset[VY]);
        writeFloat(suf->rgba[CR]);
        writeFloat(suf->rgba[CG]);
        writeFloat(suf->rgba[CB]);
        writeFloat(suf->rgba[CA]);
    }
    writeShort(s->flags);
}

static void readSide(GameMap* map, uint idx)
{
    uint i;
    float offset[2], rgba[4];
    SideDef* s = &map->sideDefs[idx];

    for(i = 0; i < 3; ++i)
    {
        Surface* suf = &s->sections[i];

        suf->flags = (int) readLong();
        //Surface_SetMaterial(suf, lookupMaterialFromDict(materialDict, readLong()));
        Surface_SetBlendMode(suf, (blendmode_t) readLong());
        suf->normal[VX] = readFloat();
        suf->normal[VY] = readFloat();
        suf->normal[VZ] = readFloat();
        offset[VX] = readFloat();
        offset[VY] = readFloat();
        Surface_SetMaterialOrigin(suf, offset[VX], offset[VY]);
        rgba[CR] = readFloat();
        rgba[CG] = readFloat();
        rgba[CB] = readFloat();
        rgba[CA] = readFloat();
        Surface_SetColorAndAlpha(suf, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);
        suf->decorations = NULL;
        suf->numDecorations = 0;
    }
    s->flags = readShort();

    SideDef_UpdateBaseOrigins(s);
}

static void archiveSides(GameMap *map, boolean write)
{
    int                i;

    if(write)
        beginSegment(DAMSEG_SIDES);
    else
        assertSegment(DAMSEG_SIDES);

    if(write)
    {
        writeLong(map->sideDefs.size());
        for(i = 0; i < map->sideDefs.size(); ++i)
            writeSide(map, i);
    }
    else
    {
        map->sideDefs.clearAndResize(readLong());
        for(i = 0; i < map->sideDefs.size(); ++i)
            readSide(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSector(GameMap* map, uint idx)
{
    uint                i;
    Sector             *s = &map->sectors[idx];

    writeFloat(s->lightLevel);
    writeFloat(s->rgb[CR]);
    writeFloat(s->rgb[CG]);
    writeFloat(s->rgb[CB]);
    writeLong(s->planeCount);
    for(i = 0; i < s->planeCount; ++i)
    {
        Plane              *p = s->planes[i];

        writeFloat(p->height);
        writeFloat(p->target);
        writeFloat(p->speed);
        writeFloat(p->visHeight);
        writeFloat(p->visHeightDelta);

        writeLong((long) p->surface.flags);
        //writeLong(getMaterialDictID(materialDict, p->surface.material));
        writeLong((long) p->surface.blendMode);
        writeFloat(p->surface.normal[VX]);
        writeFloat(p->surface.normal[VY]);
        writeFloat(p->surface.normal[VZ]);
        writeFloat(p->surface.offset[VX]);
        writeFloat(p->surface.offset[VY]);
        writeFloat(p->surface.rgba[CR]);
        writeFloat(p->surface.rgba[CG]);
        writeFloat(p->surface.rgba[CB]);
        writeFloat(p->surface.rgba[CA]);
    }

    writeFloat(s->aaBox.minX);
    writeFloat(s->aaBox.minY);
    writeFloat(s->aaBox.maxX);
    writeFloat(s->aaBox.maxY);

    for(i = 0; i < NUM_REVERB_DATA; ++i)
        writeFloat(s->reverb[i]);

    // Lightgrid block indices.
    writeLong((long) s->changedBlockCount);
    writeLong((long) s->blockCount);
    for(i = 0; i < s->blockCount; ++i)
        writeShort(s->blocks[i]);

    // Line list.
    writeLong((long) s->lineDefCount);
    for(i = 0; i < s->lineDefCount; ++i)
        writeLong(map->lineDefs.indexOf(s->lineDefs[i]) + 1);

    // BspLeaf list.
    writeLong((long) s->bspLeafCount);
    for(i = 0; i < s->bspLeafCount; ++i)
        writeLong(GameMap_BspLeafIndex(map, s->bspLeafs[i]) + 1);

    // Reverb BSP leaf attributors.
    writeLong((long) s->numReverbBspLeafAttributors);
    for(i = 0; i < s->numReverbBspLeafAttributors; ++i)
        writeLong(GameMap_BspLeafIndex(map, s->reverbBspLeafs[i]) + 1);
}

static void readSector(GameMap* map, uint idx)
{
    uint                i, numPlanes;
    float               offset[2], rgba[4];
    Sector             *s = &map->sectors[idx];

    s->lightLevel = readFloat();
    s->rgb[CR] = readFloat();
    s->rgb[CG] = readFloat();
    s->rgb[CB] = readFloat();
    numPlanes = (uint) readLong();
    for(i = 0; i < numPlanes; ++i)
    {
        Plane              *p = R_NewPlaneForSector(s);

        p->height = readFloat();
        p->target = readFloat();
        p->speed = readFloat();
        p->visHeight = readFloat();
        p->visHeightDelta = readFloat();

        p->surface.flags = (int) readLong();
        //Surface_SetMaterial(&p->surface, lookupMaterialFromDict(materialDict, readLong()));
        Surface_SetBlendMode(&p->surface, (blendmode_t) readLong());
        p->surface.normal[VX] = readFloat();
        p->surface.normal[VY] = readFloat();
        p->surface.normal[VZ] = readFloat();
        offset[VX] = readFloat();
        offset[VY] = readFloat();
        Surface_SetMaterialOrigin(&p->surface, offset[VX], offset[VY]);
        rgba[CR] = readFloat();
        rgba[CG] = readFloat();
        rgba[CB] = readFloat();
        rgba[CA] = readFloat();
        Surface_SetColorAndAlpha(&p->surface, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);

        p->surface.decorations = NULL;
        p->surface.numDecorations = 0;
    }

    s->aaBox.minX = readFloat();
    s->aaBox.minY = readFloat();
    s->aaBox.maxX = readFloat();
    s->aaBox.maxY = readFloat();

    Sector_UpdateBaseOrigin(s);
    for(i = 0; i < numPlanes; ++i)
    {
        Plane* pln = s->planes[i];
        Surface_UpdateBaseOrigin(&pln->surface);
    }

    for(i = 0; i < NUM_REVERB_DATA; ++i)
        s->reverb[i] = readFloat();

    // Lightgrid block indices.
    s->changedBlockCount = (uint) readLong();
    s->blockCount = (uint) readLong();
    s->blocks = (unsigned short *) Z_Malloc(sizeof(short) * s->blockCount, PU_MAP, 0);
    for(i = 0; i < s->blockCount; ++i)
        s->blocks[i] = readShort();

    // Line list.
    s->lineDefCount = (uint) readLong();
    s->lineDefs = (LineDef **) Z_Malloc(sizeof(LineDef*) * (s->lineDefCount + 1), PU_MAP, 0);
    for(i = 0; i < s->lineDefCount; ++i)
        s->lineDefs[i] = &map->lineDefs[(unsigned) readLong() - 1];
    s->lineDefs[i] = NULL; // Terminate.

    // BspLeaf list.
    s->bspLeafCount = (uint) readLong();
    s->bspLeafs = (BspLeaf**) Z_Malloc(sizeof(BspLeaf*) * (s->bspLeafCount + 1), PU_MAP, 0);
    for(i = 0; i < s->bspLeafCount; ++i)
        s->bspLeafs[i] = GameMap_BspLeaf(map, (unsigned) readLong() - 1);
    s->bspLeafs[i] = NULL; // Terminate.

    // Reverb BSP leaf attributors.
    s->numReverbBspLeafAttributors = (uint) readLong();
    s->reverbBspLeafs = (BspLeaf**)
        Z_Malloc(sizeof(BspLeaf*) * (s->numReverbBspLeafAttributors + 1), PU_MAP, 0);
    for(i = 0; i < s->numReverbBspLeafAttributors; ++i)
        s->reverbBspLeafs[i] = GameMap_BspLeaf(map, (unsigned) readLong() - 1);
    s->reverbBspLeafs[i] = NULL; // Terminate.
}

static void archiveSectors(GameMap *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SECTORS);
    else
        assertSegment(DAMSEG_SECTORS);

    if(write)
    {
        writeLong(map->sectorCount());
        for(i = 0; i < map->sectorCount(); ++i)
            writeSector(map, i);
    }
    else
    {
        map->sectors.clearAndResize(readLong());
        for(i = 0; i < map->sectorCount(); ++i)
            readSector(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeBspLeaf(GameMap* map, BspLeaf* s)
{
    HEdge* hedge;
    uint i;
    assert(s);

    writeFloat(s->aaBox.minX);
    writeFloat(s->aaBox.minY);
    writeFloat(s->aaBox.maxX);
    writeFloat(s->aaBox.maxY);
    writeFloat(s->midPoint[VX]);
    writeFloat(s->midPoint[VY]);
    writeLong(s->sector? ((map->sectors.indexOf(static_cast<Sector *>(s->sector))) + 1) : 0);
    writeLong(s->polyObj? (s->polyObj->idx + 1) : 0);

    // BspLeaf reverb.
    for(i = 0; i < NUM_REVERB_DATA; ++i)
        writeLong((long) s->reverb[i]);

    // BspLeaf hedges list.
    writeLong((long) s->hedgeCount);
    if(!s->hedge) return;

    hedge = s->hedge;
    do
    {
        writeLong(GameMap_HEdgeIndex(map, hedge) + 1);
    } while((hedge = hedge->next) != s->hedge);
}

static void readBspLeaf(GameMap* map, BspLeaf* s)
{
    uint i;
    HEdge* hedge;
    long obIdx;
    assert(s);

    s->aaBox.minX = readFloat();
    s->aaBox.minY = readFloat();
    s->aaBox.maxX = readFloat();
    s->aaBox.maxY = readFloat();
    s->midPoint[VX] = readFloat();
    s->midPoint[VY] = readFloat();
    obIdx = readLong();
    s->sector = (obIdx == 0? NULL : &map->sectors[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->polyObj = (obIdx == 0? NULL : map->polyObjs[(unsigned) obIdx - 1]);

    // BspLeaf reverb.
    for(i = 0; i < NUM_REVERB_DATA; ++i)
        s->reverb[i] = (uint) readLong();

    // BspLeaf hedges list.
    s->hedgeCount = (uint) readLong();
    if(!s->hedgeCount)
    {
        s->hedge = 0;
        return;
    }

    for(i = 0; i < s->hedgeCount; ++i)
    {
        HEdge* next = GameMap_HEdge(map, (unsigned) readLong() - 1);
        if(i == 0)
        {
            s->hedge = next;
            hedge = next;
        }
        else
        {
            hedge->next = next;
            next->prev = hedge;
            hedge = next;
        }
    }

    s->hedge->prev = hedge;
}

static void archiveBspLeafs(GameMap* map, boolean write)
{
    //uint i;
    DENG2_UNUSED(map);

    if(write)
        beginSegment(DAMSEG_BSPLEAFS);
    else
        assertSegment(DAMSEG_BSPLEAFS);

    /*if(write)
    {
        writeLong(map->numBspLeafs);
        for(i = 0; i < map->numBspLeafs; ++i)
            writeBspLeaf(map, i);
    }
    else
    {
        map->numBspLeafs = readLong();
        for(i = 0; i < map->numBspLeafs; ++i)
            readBspLeaf(map, i);
    }*/

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSeg(GameMap* map, HEdge* s)
{
    assert(map && s);

    writeLong(map->vertexes.indexOf(static_cast<Vertex const *>(s->v[0])) + 1);
    writeLong(map->vertexes.indexOf(static_cast<Vertex const *>(s->v[1])) + 1);
    writeFloat(s->length);
    writeFloat(s->offset);
    writeLong(s->lineDef? (map->lineDefs.indexOf(s->lineDef) + 1) : 0);
    writeLong(s->sector? (GameMap_SectorIndex(map, s->sector) + 1) : 0);
    writeLong(s->bspLeaf? (GameMap_BspLeafIndex(map, s->bspLeaf) + 1) : 0);
    writeLong(s->twin? (GameMap_HEdgeIndex(map, s->twin) + 1) : 0);
    writeLong((long) s->angle);
    writeByte(s->side);
    writeLong(s->next? (GameMap_HEdgeIndex(map, s->next) + 1) : 0);
    writeLong(s->twin? (GameMap_HEdgeIndex(map, s->prev) + 1) : 0);
}

static void readSeg(GameMap* map, HEdge* s)
{
    long obIdx;
    assert(map && s);

    s->v[0] = &map->vertexes[(unsigned) readLong() - 1];
    s->v[1] = &map->vertexes[(unsigned) readLong() - 1];
    s->length = readFloat();
    s->offset = readFloat();
    obIdx = readLong();
    s->lineDef = (obIdx == 0? NULL : &map->lineDefs[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->sector = (obIdx == 0? NULL : GameMap_Sector(map, (unsigned) obIdx - 1));
    obIdx = readLong();
    s->bspLeaf = (obIdx == 0? NULL : GameMap_BspLeaf(map, (unsigned) obIdx - 1));
    obIdx = readLong();
    s->twin = (obIdx == 0? NULL : GameMap_HEdge(map, (unsigned) obIdx - 1));
    s->angle = (angle_t) readLong();
    s->side = readByte();
    obIdx = readLong();
    s->next = (obIdx == 0? NULL : GameMap_HEdge(map, (unsigned) obIdx - 1));
    obIdx = readLong();
    s->prev = (obIdx == 0? NULL : GameMap_HEdge(map, (unsigned) obIdx - 1));
}

static void archiveSegs(GameMap *map, boolean write)
{
    DENG_UNUSED(map);
    //uint                i;

    if(write)
        beginSegment(DAMSEG_HEDGES);
    else
        assertSegment(DAMSEG_HEDGES);

    /*if(write)
    {
        writeLong(map->numHEdges);
        for(i = 0; i < map->numHEdges; ++i)
            writeSeg(map, i);
    }
    else
    {
        map->numHEdges = readLong();
        for(i = 0; i < map->numHEdges; ++i)
            readSeg(map, i);
    }*/

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

#define NF_LEAF            0x80000000

static void writeBspReference(GameMap* map, de::MapElement* bspRef)
{
    assert(map);
    if(bspRef->type() == DMU_BSPLEAF)
        writeLong((long)(GameMap_BspLeafIndex(map, bspRef->castTo<BspLeaf>()) | NF_LEAF));
    else
        writeLong((long)GameMap_BspNodeIndex(map, bspRef->castTo<BspNode>()));
}

static de::MapElement* readBspReference(GameMap* map)
{
    long idx;
    assert(map);
    idx = readLong();
    if(idx & NF_LEAF)
    {
        return GameMap_BspLeaf(map, idx & ~NF_LEAF);
    }
    return GameMap_BspNode(map, idx);
}

#undef NF_LEAF

static void writeNode(GameMap* map, BspNode* n)
{
    assert(n);
    writeFloat(n->partition.origin[VX]);
    writeFloat(n->partition.origin[VY]);
    writeFloat(n->partition.direction[VX]);
    writeFloat(n->partition.direction[VY]);
    writeFloat(n->aaBox[RIGHT].minX);
    writeFloat(n->aaBox[RIGHT].minY);
    writeFloat(n->aaBox[RIGHT].maxX);
    writeFloat(n->aaBox[RIGHT].maxY);
    writeFloat(n->aaBox[LEFT ].minX);
    writeFloat(n->aaBox[LEFT ].minY);
    writeFloat(n->aaBox[LEFT ].maxX);
    writeFloat(n->aaBox[LEFT ].maxY);
    writeBspReference(map, n->children[RIGHT]);
    writeBspReference(map, n->children[LEFT]);
}

static void readNode(GameMap* map, BspNode* n)
{
    assert(n);
    n->partition.origin[VX] = readFloat();
    n->partition.origin[VY] = readFloat();
    n->partition.direction[VX] = readFloat();
    n->partition.direction[VY] = readFloat();
    n->aaBox[RIGHT].minX = readFloat();
    n->aaBox[RIGHT].minY = readFloat();
    n->aaBox[RIGHT].maxX = readFloat();
    n->aaBox[RIGHT].maxY = readFloat();
    n->aaBox[LEFT ].minX = readFloat();
    n->aaBox[LEFT ].minY = readFloat();
    n->aaBox[LEFT ].maxX = readFloat();
    n->aaBox[LEFT ].maxY = readFloat();
    n->children[RIGHT] = readBspReference(map);
    n->children[LEFT]  = readBspReference(map);
}

static void archiveNodes(GameMap* map, boolean write)
{
    DENG_UNUSED(map);
    //uint i;

    if(write)
        beginSegment(DAMSEG_BSPNODES);
    else
        assertSegment(DAMSEG_BSPNODES);

    /*if(write)
    {
        writeLong(map->numBspNodes);
        for(i = 0; i < map->numBspNodes; ++i)
            writeNode(map, i);
    }
    else
    {
        map->numBspNodes = readLong();
        for(i = 0; i < map->numBspNodes; ++i)
            readNode(map, i);
    }*/

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveBlockmap(GameMap *map, boolean write)
{
    DENG_UNUSED(map);

    if(write)
        beginSegment(DAMSEG_BLOCKMAP);
    else
        assertSegment(DAMSEG_BLOCKMAP);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveReject(GameMap *map, boolean write)
{
    DENG_UNUSED(map);

    if(write)
        beginSegment(DAMSEG_REJECT);
    else
        assertSegment(DAMSEG_REJECT);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writePolyobj(GameMap* map, uint idx)
{
    Polyobj* p = map->polyObjs[idx];
    uint i;

    writeLong((long) p->idx);
    writeFloat(p->origin[VX]);
    writeFloat(p->origin[VY]);
    writeFloat(p->origin[VZ]);
    writeLong((long) p->angle);
    writeLong((long) p->tag);
    writeFloat(p->aaBox.minX);
    writeFloat(p->aaBox.minY);
    writeFloat(p->aaBox.maxX);
    writeFloat(p->aaBox.maxY);
    writeFloat(p->dest[VX]);
    writeFloat(p->dest[VY]);
    writeFloat(p->speed);
    writeLong((long) p->destAngle);
    writeLong((long) p->angleSpeed);
    writeByte(p->crush? 1 : 0);
    writeLong((long) p->seqType);

    writeLong((long) p->lineCount);
    for(i = 0; i < p->lineCount; ++i)
    {
        LineDef* line = p->lines[i];
        HEdge* he = line->L_frontside.hedgeLeft;

        writeLong(map->vertexes.indexOf(static_cast<Vertex const *>(he->v[0])) + 1);
        writeLong(map->vertexes.indexOf(static_cast<Vertex const *>(he->v[1])) + 1);
        writeFloat(he->length);
        writeFloat(he->offset);
        writeLong(he->lineDef? (map->lineDefs.indexOf(he->lineDef) + 1) : 0);
        writeLong(he->sector? (GameMap_SectorIndex(map, he->sector) + 1) : 0);
        writeLong((long) he->angle);
        writeByte(he->side);
    }
}

static void readPolyobj(GameMap* map, uint idx)
{
    Polyobj* p = map->polyObjs[idx];
    long obIdx;
    HEdge* hedges;
    uint i;

    p->idx = (uint) readLong();
    p->origin[VX] = readFloat();
    p->origin[VY] = readFloat();
    p->origin[VZ] = readFloat();
    p->angle = (angle_t) readLong();
    p->tag = (int) readLong();
    p->aaBox.minX = readFloat();
    p->aaBox.minY = readFloat();
    p->aaBox.maxX = readFloat();
    p->aaBox.maxY = readFloat();
    p->dest[VX] = readFloat();
    p->dest[VY] = readFloat();
    p->speed = readFloat();
    p->destAngle = (angle_t) readLong();
    p->angleSpeed = (angle_t) readLong();
    p->crush = (readByte()? true : false);
    p->seqType = (int) readLong();

    // Polyobj line list.
    p->lineCount = (uint) readLong();

    hedges = (HEdge *) Z_Calloc(sizeof(HEdge) * p->lineCount, PU_MAP, 0);
    p->lines = (LineDef **) Z_Malloc(sizeof(LineDef*) * (p->lineCount + 1), PU_MAP, 0);
    for(i = 0; i < p->lineCount; ++i)
    {
        HEdge* he = hedges + i;
        LineDef* line;

        he->v[0] = &map->vertexes[(unsigned) readLong() - 1];
        he->v[1] = &map->vertexes[(unsigned) readLong() - 1];
        he->length = readFloat();
        he->offset = readFloat();
        obIdx = readLong();
        he->lineDef = (obIdx == 0? NULL : &map->lineDefs[(unsigned) obIdx - 1]);
        obIdx = readLong();
        he->sector = (obIdx == 0? NULL : &map->sectors[(unsigned) obIdx - 1]);
        he->angle = (angle_t) readLong();
        he->side = (readByte()? 1 : 0);

        line = he->lineDef;
        line->L_frontside.hedgeLeft = line->L_frontside.hedgeRight = he;

        p->lines[i] = line;
    }
    p->lines[i] = NULL; // Terminate.
}

static void archivePolyobjs(GameMap* map, boolean write)
{
    uint i;

    if(write)
        beginSegment(DAMSEG_POLYOBJS);
    else
        assertSegment(DAMSEG_POLYOBJS);

    if(write)
    {
        writeLong(map->numPolyObjs);
        for(i = 0; i < map->numPolyObjs; ++i)
            writePolyobj(map, i);
    }
    else
    {
        map->numPolyObjs = readLong();
        for(i = 0; i < map->numPolyObjs; ++i)
            readPolyobj(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

/*
static void writeThing(const GameMap *map, uint idx)
{

}

static void readThing(const GameMap *map, uint idx)
{

}
*/

static void archiveMap(GameMap *map, boolean write)
{
    if(write)
        beginSegment(DAMSEG_MAP);
    else
    {
        assertSegment(DAMSEG_MAP);

        // Call the game's setup routines.
        if(gx.SetupForMapData)
        {
            gx.SetupForMapData(DMU_VERTEX, map->vertexCount());
            gx.SetupForMapData(DMU_LINEDEF, map->lineDefCount());
            gx.SetupForMapData(DMU_SIDEDEF, map->sideDefCount());
            gx.SetupForMapData(DMU_SECTOR, map->sectorCount());
        }
    }

    archivePolyobjs(map, write);
    archiveVertexes(map, write);
    archiveLines(map, write); // Must follow vertexes (lineowner nodes).
    archiveSides(map, write);
    archiveSectors(map, write);
    archiveBspLeafs(map, write);
    archiveSegs(map, write);
    archiveNodes(map, write);
    archiveBlockmap(map, write);
    archiveReject(map, write);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveSymbolTables(boolean write)
{
    if(write)
        beginSegment(DAMSEG_SYMBOLTABLES);
    else
        assertSegment(DAMSEG_SYMBOLTABLES);

    //archiveMaterialDict(materialDict, write);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveRelocationTables(boolean write)
{
    if(write)
        beginSegment(DAMSEG_RELOCATIONTABLES);
    else
        assertSegment(DAMSEG_RELOCATIONTABLES);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveHeader(boolean write)
{
    if(write)
        beginSegment(DAMSEG_HEADER);
    else
        assertSegment(DAMSEG_HEADER);

    if(write)
    {
        writeLong(DAM_VERSION);
    }
    else
    {
        mapFileVersion = readLong();
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static boolean doArchiveMap(GameMap* map, const char* path, boolean write)
{
    if(NULL == path || !path[0])
        return false;

    // Open the file.
    if(!openMapFile(path, write))
        return false; // Hmm, invalid path?

    Con_Message("DAM_MapRead: %s cached map %s.\n", write? "Saving" : "Loading", path);

    /*materialDict = M_Calloc(sizeof(*materialDict));
    if(write)
        initMaterialDict(map, materialDict);*/

    archiveHeader(write);
    archiveRelocationTables(write);
    archiveSymbolTables(write);
    archiveMap(map, write);

    // Close the file.
    closeMapFile();

    //M_Free(materialDict);

    return true;
}

boolean DAM_MapWrite(GameMap* map, const char* path)
{
    return doArchiveMap(map, path, true);
}

boolean DAM_MapRead(GameMap* map, const char* path)
{
    return doArchiveMap(map, path, false);
}

boolean DAM_MapIsValid(char const* cachedMapPath, lumpnum_t markerLumpNum)
{
    if(cachedMapPath && !cachedMapPath[0] && markerLumpNum >= 0)
    {
        uint const sourceTime = F_LumpLastModified(markerLumpNum);
        uint const buildTime  = F_GetLastModified(cachedMapPath);

        if(F_Access(cachedMapPath) && !(buildTime < sourceTime))
        {
            // Ok, lets check the header.
            if(openMapFile(cachedMapPath, false))
            {
                archiveHeader(false);
                closeMapFile();

                if(mapFileVersion == DAM_VERSION)
                    return true; // Its good.
            }
        }
    }
    return false;
}
