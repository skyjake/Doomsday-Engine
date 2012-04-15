/**\file dam_file.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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
 * Doomsday Archived Map (DAM) reader/writer.
 */

#include <lzss.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_dam.h"
#include "de_defs.h"
#include "de_misc.h"
#include "de_refresh.h"
#include "de_filesys.h"

#include "p_mapdata.h"

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

static void writeNBytes(void* data, long len)
{
    lzWrite(data, len, mapFile);
}

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

static void readNBytes(void *ptr, long len)
{
    lzRead(ptr, len, mapFile);
}

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

static void writeVertex(const GameMap *map, uint idx)
{
    Vertex             *v = &map->vertexes[idx];

    writeFloat(v->pos[VX]);
    writeFloat(v->pos[VY]);
    writeLong((long) v->numLineOwners);

    if(v->numLineOwners > 0)
    {
        lineowner_t        *own, *base;

        own = base = (v->lineOwners)->LO_prev;
        do
        {
            writeLong((long) ((own->lineDef - map->lineDefs) + 1));
            writeLong((long) own->angle);
            own = own->LO_prev;
        } while(own != base);
    }
}

static void readVertex(const GameMap *map, uint idx)
{
    uint                i;
    Vertex             *v = &map->vertexes[idx];

    v->pos[VX] = readFloat();
    v->pos[VY] = readFloat();
    v->numLineOwners = (uint) readLong();

    if(v->numLineOwners > 0)
    {
        lineowner_t        *own;

        v->lineOwners = NULL;
        for(i = 0; i < v->numLineOwners; ++i)
        {
            own = Z_Malloc(sizeof(lineowner_t), PU_MAP, 0);
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
    uint                i;

    if(write)
        beginSegment(DAMSEG_VERTEXES);
    else
        assertSegment(DAMSEG_VERTEXES);

    if(write)
    {
        writeLong((long) map->numVertexes);
        for(i = 0; i < map->numVertexes; ++i)
            writeVertex(map, i);
    }
    else
    {
        map->numVertexes = (uint) readLong();
        for(i = 0; i < map->numVertexes; ++i)
            readVertex(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeLine(const GameMap *map, uint idx)
{
    int                 i;
    LineDef            *l = &map->lineDefs[idx];

    writeLong((long) ((l->v[0] - map->vertexes) + 1));
    writeLong((long) ((l->v[1] - map->vertexes) + 1));
    writeLong(l->flags);
    writeByte(l->inFlags);
    writeFloat(l->dX);
    writeFloat(l->dY);
    writeLong((long) l->slopeType);
    writeLong((long) (l->sideDefs[0]? ((l->sideDefs[0] - map->sideDefs) + 1) : 0));
    writeLong((long) (l->sideDefs[1]? ((l->sideDefs[1] - map->sideDefs) + 1) : 0));
    writeFloat(l->aaBox.minX);
    writeFloat(l->aaBox.minY);
    writeFloat(l->aaBox.maxX);
    writeFloat(l->aaBox.maxY);
    writeFloat(l->length);
    writeLong((long) l->angle);
    for(i = 0; i < DDMAXPLAYERS; ++i)
        writeByte(l->mapped[i]? 1 : 0);
}

static void readLine(const GameMap *map, uint idx)
{
    int                 i;
    long                sideIdx;
    LineDef            *l = &map->lineDefs[idx];

    l->v[0] = &map->vertexes[(unsigned) (readLong() - 1)];
    l->v[1] = &map->vertexes[(unsigned) (readLong() - 1)];
    l->flags = (int) readLong();
    l->inFlags = readByte();
    l->dX = readFloat();
    l->dY = readFloat();
    l->slopeType = (slopetype_t) readLong();
    sideIdx = readLong();
    l->sideDefs[0] = (sideIdx == 0? NULL : &map->sideDefs[sideIdx-1]);
    sideIdx = readLong();
    l->sideDefs[1] = (sideIdx == 0? NULL : &map->sideDefs[sideIdx-1]);
    l->aaBox.minX = readFloat();
    l->aaBox.minY = readFloat();
    l->aaBox.maxX = readFloat();
    l->aaBox.maxY = readFloat();
    l->length = readFloat();
    l->angle = (binangle_t) readLong();
    for(i = 0; i < DDMAXPLAYERS; ++i)
        l->mapped[i] = (readByte()? true : false);
}

static void archiveLines(GameMap *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_LINES);
    else
        assertSegment(DAMSEG_LINES);

    if(write)
    {
        writeLong(map->numLineDefs);
        for(i = 0; i < map->numLineDefs; ++i)
            writeLine(map, i);
    }
    else
    {
        map->numLineDefs = readLong();
        for(i = 0; i < map->numLineDefs; ++i)
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
        Surface* suf = &s->sections[3];

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
    writeLong(s->sector? ((s->sector - map->sectors) + 1) : 0);
    writeShort(s->flags);
    writeLong(GameMap_HEdgeIndex(map, s->hedgeLeft)  + 1);
    writeLong(GameMap_HEdgeIndex(map, s->hedgeRight) + 1);
}

static void readSide(GameMap* map, uint idx)
{
    uint i;
    long secIdx;
    float offset[2], rgba[4];
    SideDef* s = &map->sideDefs[idx];

    for(i = 0; i < 3; ++i)
    {
        Surface* suf = &s->sections[3];

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
    secIdx = readLong();
    s->sector = (secIdx == 0? NULL : &map->sectors[secIdx -1]);
    s->flags = readShort();
    s->hedgeLeft  = GameMap_HEdge(map, (unsigned) readLong() - 1);
    s->hedgeRight = GameMap_HEdge(map, (unsigned) readLong() - 1);
}

static void archiveSides(GameMap *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SIDES);
    else
        assertSegment(DAMSEG_SIDES);

    if(write)
    {
        writeLong(map->numSideDefs);
        for(i = 0; i < map->numSideDefs; ++i)
            writeSide(map, i);
    }
    else
    {
        map->numSideDefs = readLong();
        for(i = 0; i < map->numSideDefs; ++i)
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
        writeLong((s->lineDefs[i] - map->lineDefs) + 1);

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

    Sector_UpdateOrigin(s);

    for(i = 0; i < numPlanes; ++i)
    {
        Plane* p = s->planes[i];
        p->origin.pos[VX] = s->origin.pos[VX];
        p->origin.pos[VY] = s->origin.pos[VY];
        p->origin.pos[VZ] = p->height;
    }

    for(i = 0; i < NUM_REVERB_DATA; ++i)
        s->reverb[i] = readFloat();

    // Lightgrid block indices.
    s->changedBlockCount = (uint) readLong();
    s->blockCount = (uint) readLong();
    s->blocks = Z_Malloc(sizeof(short) * s->blockCount, PU_MAP, 0);
    for(i = 0; i < s->blockCount; ++i)
        s->blocks[i] = readShort();

    // Line list.
    s->lineDefCount = (uint) readLong();
    s->lineDefs = Z_Malloc(sizeof(LineDef*) * (s->lineDefCount + 1), PU_MAP, 0);
    for(i = 0; i < s->lineDefCount; ++i)
        s->lineDefs[i] = &map->lineDefs[(unsigned) readLong() - 1];
    s->lineDefs[i] = NULL; // Terminate.

    // BspLeaf list.
    s->bspLeafCount = (uint) readLong();
    s->bspLeafs =
        Z_Malloc(sizeof(BspLeaf*) * (s->bspLeafCount + 1), PU_MAP, 0);
    for(i = 0; i < s->bspLeafCount; ++i)
        s->bspLeafs[i] = GameMap_BspLeaf(map, (unsigned) readLong() - 1);
    s->bspLeafs[i] = NULL; // Terminate.

    // Reverb BSP leaf attributors.
    s->numReverbBspLeafAttributors = (uint) readLong();
    s->reverbBspLeafs =
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
        writeLong(map->numSectors);
        for(i = 0; i < map->numSectors; ++i)
            writeSector(map, i);
    }
    else
    {
        map->numSectors = readLong();
        for(i = 0; i < map->numSectors; ++i)
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
    writeLong(s->sector? ((s->sector - map->sectors) + 1) : 0);
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

    writeLong((s->v[0] - map->vertexes) + 1);
    writeLong((s->v[1] - map->vertexes) + 1);
    writeFloat(s->length);
    writeFloat(s->offset);
    writeLong(s->lineDef? ((s->lineDef - map->lineDefs) + 1) : 0);
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

static void writeBspReference(GameMap* map, runtime_mapdata_header_t* bspRef)
{
    assert(map);
    if(bspRef->type == DMU_BSPLEAF)
        writeLong((long)(GameMap_BspLeafIndex(map, (BspLeaf*)bspRef) | NF_LEAF));
    else
        writeLong((long)GameMap_BspNodeIndex(map, (BspNode*)bspRef));
}

static runtime_mapdata_header_t* readBspReference(GameMap* map)
{
    long idx;
    assert(map);
    idx = readLong();
    if(idx & NF_LEAF)
        return (runtime_mapdata_header_t*)GameMap_BspLeaf(map, idx & ~NF_LEAF);
    return (runtime_mapdata_header_t*)GameMap_BspNode(map, idx);
}

#undef NF_LEAF

static void writeNode(GameMap* map, BspNode* n)
{
    assert(n);
    writeFloat(n->partition.x);
    writeFloat(n->partition.y);
    writeFloat(n->partition.dX);
    writeFloat(n->partition.dY);
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
    n->partition.x = readFloat();
    n->partition.y = readFloat();
    n->partition.dX = readFloat();
    n->partition.dY = readFloat();
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
    writeFloat(p->pos[VX]);
    writeFloat(p->pos[VY]);
    writeFloat(p->pos[VZ]);
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
        HEdge* he = line->L_frontside->hedgeLeft;

        writeLong((he->v[0] - map->vertexes) + 1);
        writeLong((he->v[1] - map->vertexes) + 1);
        writeFloat(he->length);
        writeFloat(he->offset);
        writeLong(he->lineDef? ((he->lineDef - map->lineDefs) + 1) : 0);
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
    p->pos[VX] = readFloat();
    p->pos[VY] = readFloat();
    p->pos[VZ] = readFloat();
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

    hedges = Z_Calloc(sizeof(HEdge) * p->lineCount, PU_MAP, 0);
    p->lines = Z_Malloc(sizeof(HEdge*) * (p->lineCount + 1), PU_MAP, 0);
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
        line->L_frontside->hedgeLeft = line->L_frontside->hedgeRight = he;

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
            gx.SetupForMapData(DMU_VERTEX, map->numVertexes);
            gx.SetupForMapData(DMU_LINEDEF, map->numLineDefs);
            gx.SetupForMapData(DMU_SIDEDEF, map->numSideDefs);
            gx.SetupForMapData(DMU_SECTOR, map->numSectors);
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

boolean DAM_MapIsValid(const char* cachedMapPath, lumpnum_t markerLumpNum)
{
    if(NULL != cachedMapPath && !cachedMapPath[0] && markerLumpNum >= 0)
    {
        uint sourceTime = F_GetLastModified(F_LumpSourceFile(markerLumpNum));
        uint buildTime = F_GetLastModified(cachedMapPath);

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
