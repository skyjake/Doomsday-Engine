/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
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
 * dam_file.c: Doomsday Archived Map (DAM) reader/writer.
 */

// HEADER FILES ------------------------------------------------------------

#include <lzss.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_dam.h"
#include "de_defs.h"
#include "de_misc.h"
#include "de_refresh.h"

#include "p_mapdata.h"

// MACROS ------------------------------------------------------------------

// Global archived map format version identifier. Increment when making
// changes to the structure of the format.
#define DAM_VERSION             1

#define MAX_ARCHIVED_MATERIALS	2048
#define BADTEXNAME  "DD_BADTX"  // string that will be written in the texture
                                // archives to denote a missing texture.

// TYPES -------------------------------------------------------------------

// Segments of a doomsday archived map file.
typedef enum damsegment_e {
    DAMSEG_END = -1,                // Terminates a segment.
    DAMSEG_HEADER = 100,            // File-level meta.
    DAMSEG_RELOCATIONTABLES,        // Tables of offsets to file positions.
    DAMSEG_SYMBOLTABLES,            // Global symbol tables.

    DAMSEG_MAP = 200,               // Start of the map data.
    DAMSEG_THINGS,
    DAMSEG_POLYOBJS,
    DAMSEG_VERTEXES,
    DAMSEG_LINES,
    DAMSEG_SIDES,
    DAMSEG_SECTORS,
    DAMSEG_SSECTORS,
    DAMSEG_SEGS,
    DAMSEG_NODES,
    DAMSEG_BLOCKMAP,
    DAMSEG_REJECT
} damsegment_t;

typedef struct {
	char            name[9];
    materialtype_t  type;
} dictentry_t;

typedef struct {
    //// \todo Remove fixed limit.
	dictentry_t     table[MAX_ARCHIVED_MATERIALS];
	int             count;
} materialdict_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static LZFILE *mapFile;
static int mapFileVersion;

static materialdict_t *materialDict;

// CODE --------------------------------------------------------------------

/**
 * Called for every material in the map before saving by
 * initMaterialArchives.
 */
static void addMaterialToDict(materialdict_t *dict, material_t *mat)
{
    int                 c;
    char                name[9];
    dictentry_t *e;

    // Get the name of the material.
    if(R_MaterialNameForNum(mat->ofTypeID, mat->type))
        strncpy(name, R_MaterialNameForNum(mat->ofTypeID, mat->type), 8);
    else
        strncpy(name, BADTEXNAME, 8);

    name[8] = 0;

    // Has this already been registered?
    for(c = 0; c < dict->count; c++)
    {
        if(dict->table[c].type == mat->type &&
           !stricmp(dict->table[c].name, name))
        {   // Yes. skip it...
            return;
        }
    }

    e = &dict->table[dict->count];
    dict->count++;

    strncpy(e->name, name, 8);
    e->name[8] = '\0';
    e->type = mat->type;
}

/**
 * Initializes the material archives (translation tables).
 * Must be called before writing the tables!
 */
static void initMaterialDict(const gamemap_t *map, materialdict_t *dict)
{
    uint                i, j;

    for(i = 0; i < map->numsectors; ++i)
    {
        sector_t           *sec = &map->sectors[i];

        for(j = 0; j < sec->planecount; ++j)
            addMaterialToDict(dict, sec->SP_planematerial(j));
    }

    for(i = 0; i < map->numsides; ++i)
    {
        side_t             *side = &map->sides[i];

        addMaterialToDict(dict, side->SW_middlematerial);
        addMaterialToDict(dict, side->SW_topmaterial);
        addMaterialToDict(dict, side->SW_bottommaterial);
    }
}

static uint searchMaterialDict(materialdict_t *dict,
                               const char *name, materialtype_t type)
{
    int                 i;

    for(i = 0; i < dict->count; i++)
        if(dict->table[i].type == type &&
           !stricmp(dict->table[i].name, name))
            return i;

    // Not found?!!!
    return 0;
}

/**
 * @return              The archive number of the given texture.
 */
static uint getMaterialDictID(materialdict_t *dict, material_t *mat)
{
    char            name[9];

    if(R_MaterialNameForNum(mat->ofTypeID, mat->type))
        strncpy(name, R_MaterialNameForNum(mat->ofTypeID, mat->type), 8);
    else
        strncpy(name, BADTEXNAME, 8);
    name[8] = 0;

    return searchMaterialDict(dict, name, mat->type);
}

static material_t* lookupMaterialFromDict(materialdict_t *dict, int idx)
{
    dictentry_t        *e = &dict->table[idx];

    if(!strncmp(e->name, BADTEXNAME, 8))
        return NULL;

    return R_GetMaterial(R_MaterialNumForName(e->name, e->type), e->type);
}

static boolean openMapFile(char *path, boolean write)
{
    mapFile = NULL;
    mapFileVersion = 0;
    mapFile = lzOpen(path, (write? F_WRITE_PACKED : F_READ_PACKED));

    return ((mapFile)? true : false);
}

static boolean closeMapFile(void)
{
    return (lzClose(mapFile)? true : false);
}

static void writeNBytes(void *data, long len)
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

static void writeVertex(const gamemap_t *map, uint idx)
{
    vertex_t           *v = &map->vertexes[idx];

    writeFloat(v->V_pos[VX]);
    writeFloat(v->V_pos[VY]);
    writeByte((v->anchored? 1 : 0));
    writeLong((long) v->numlineowners);

    if(v->numlineowners > 0)
    {
        lineowner_t        *own, *base;

        own = base = (v->lineowners)->LO_prev;
        do
        {
            writeLong((long) ((own->line - map->lines) + 1));
            writeLong((long) own->angle);
            own = own->LO_prev;
        } while(own != base);
    }
}

static void readVertex(const gamemap_t *map, uint idx)
{
    uint                i;
    vertex_t           *v = &map->vertexes[idx];

    v->V_pos[VX] = readFloat();
    v->V_pos[VY] = readFloat();
    v->anchored = (readByte()? true : false);
    v->numlineowners = (uint) readLong();

    if(v->numlineowners > 0)
    {
        lineowner_t        *own;

        v->lineowners = NULL;
        for(i = 0; i < v->numlineowners; ++i)
        {
            own = Z_Malloc(sizeof(lineowner_t), PU_LEVEL, 0);
            own->line = &map->lines[(unsigned) (readLong() - 1)];
            own->angle = (binangle_t) readLong();

            own->LO_next = v->lineowners;
            v->lineowners = own;
        }

        own = v->lineowners;
        do
        {
            own->LO_next->LO_prev = own;
            own = own->LO_next;
        } while(own);
        own->LO_prev = v->lineowners;
    }
}

static void archiveVertexes(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_VERTEXES);
    else
        assertSegment(DAMSEG_VERTEXES);

    if(write)
    {
        writeLong((long) map->numvertexes);
        for(i = 0; i < map->numvertexes; ++i)
            writeVertex(map, i);
    }
    else
    {
        map->numvertexes = (uint) readLong();
        for(i = 0; i < map->numvertexes; ++i)
            readVertex(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeLine(const gamemap_t *map, uint idx)
{
    int                 i;
    line_t             *l = &map->lines[idx];

    writeLong((long) ((l->v[0] - map->vertexes) + 1));
    writeLong((long) ((l->v[1] - map->vertexes) + 1));
    writeLong(l->flags);
    writeShort(l->mapflags);
    writeFloat(l->dx);
    writeFloat(l->dy);
    writeLong((long) l->slopetype);
    writeLong((long) (l->sides[0]? ((l->sides[0] - map->sides) + 1) : 0));
    writeLong((long) (l->sides[1]? ((l->sides[1] - map->sides) + 1) : 0));
    writeFloat(l->bbox[BOXLEFT]);
    writeFloat(l->bbox[BOXRIGHT]);
    writeFloat(l->bbox[BOXBOTTOM]);
    writeFloat(l->bbox[BOXTOP]);
    writeFloat(l->length);
    writeLong((long) l->angle);
    for(i = 0; i < DDMAXPLAYERS; ++i)
        writeByte(l->mapped[DDMAXPLAYERS]? 1 : 0);
}

static void readLine(const gamemap_t *map, uint idx)
{
    int                 i;
    long                sideIdx;
    line_t             *l = &map->lines[idx];

    l->v[0] = &map->vertexes[(unsigned) (readLong() - 1)];
    l->v[1] = &map->vertexes[(unsigned) (readLong() - 1)];
    l->flags = (int) readLong();
    l->mapflags = readShort();
    l->dx = readFloat();
    l->dy = readFloat();
    l->slopetype = (slopetype_t) readLong();
    sideIdx = readLong();
    l->sides[0] = (sideIdx == 0? NULL : &map->sides[sideIdx-1]);
    sideIdx = readLong();
    l->sides[1] = (sideIdx == 0? NULL : &map->sides[sideIdx-1]);
    l->bbox[BOXLEFT] = readFloat();
    l->bbox[BOXRIGHT] = readFloat();
    l->bbox[BOXBOTTOM] = readFloat();
    l->bbox[BOXTOP] = readFloat();
    l->length = readFloat();
    l->angle = (binangle_t) readLong();
    for(i = 0; i < DDMAXPLAYERS; ++i)
        l->mapped[i] = (readByte()? true : false);
}

static void archiveLines(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_LINES);
    else
        assertSegment(DAMSEG_LINES);

    if(write)
    {
        writeLong(map->numlines);
        for(i = 0; i < map->numlines; ++i)
            writeLine(map, i);
    }
    else
    {
        map->numlines = readLong();
        for(i = 0; i < map->numlines; ++i)
            readLine(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSide(const gamemap_t *map, uint idx)
{
    uint                i;
    side_t             *s = &map->sides[idx];

    for(i = 0; i < 3; ++i)
    {
        surface_t          *suf = &s->sections[3];

        writeLong(suf->flags);
        writeLong(getMaterialDictID(materialDict, suf->material));
        writeLong((long) suf->blendmode);
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
    writeLong((long) s->segcount);
    for(i = 0; i < s->segcount; ++i)
        writeLong((s->segs[i] - map->segs) + 1);
}

static void readSide(const gamemap_t *map, uint idx)
{
    uint                i;
    long                secIdx;
    side_t             *s = &map->sides[idx];

    for(i = 0; i < 3; ++i)
    {
        surface_t          *suf = &s->sections[3];

        suf->flags = (int) readLong();
        suf->material = lookupMaterialFromDict(materialDict, readLong());
        suf->blendmode = (blendmode_t) readLong();
        suf->normal[VX] = readFloat();
        suf->normal[VY] = readFloat();
        suf->normal[VZ] = readFloat();
        suf->offset[VX] = readFloat();
        suf->offset[VY] = readFloat();
        suf->rgba[CR] = readFloat();
        suf->rgba[CG] = readFloat();
        suf->rgba[CB] = readFloat();
        suf->rgba[CA] = readFloat();
        suf->decorations = NULL;
        suf->numdecorations = 0;
    }
    secIdx = readLong();
    s->sector = (secIdx == 0? NULL : &map->sectors[secIdx -1]);
    s->flags = readShort();
    s->segcount = (uint) readLong();
    s->segs = Z_Malloc(sizeof(seg_t*) * (s->segcount + 1), PU_LEVEL, 0);
    for(i = 0; i < s->segcount; ++i)
        s->segs[i] = &map->segs[(unsigned) readLong() - 1];
    s->segs[i] = NULL; // Terminate.
}

static void archiveSides(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SIDES);
    else
        assertSegment(DAMSEG_SIDES);

    if(write)
    {
        writeLong(map->numsides);
        for(i = 0; i < map->numsides; ++i)
            writeSide(map, i);
    }
    else
    {
        map->numsides = readLong();
        for(i = 0; i < map->numsides; ++i)
            readSide(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSector(const gamemap_t *map, uint idx)
{
    uint                i, j;
    sector_t           *s = &map->sectors[idx];

    writeFloat(s->lightlevel);
    writeFloat(s->rgb[CR]);
    writeFloat(s->rgb[CG]);
    writeFloat(s->rgb[CB]);
    writeLong(s->planecount);
    for(i = 0; i < s->planecount; ++i)
    {
        plane_t            *p = s->planes[i];

        writeFloat(p->height);
        writeFloat(p->glow);
        writeFloat(p->glowrgb[CR]);
        writeFloat(p->glowrgb[CG]);
        writeFloat(p->glowrgb[CB]);
        writeFloat(p->target);
        writeFloat(p->speed);
        writeFloat(p->visheight);
        writeFloat(p->visoffset);

        writeLong((long) p->surface.flags);
        writeLong(getMaterialDictID(materialDict, p->surface.material));
        writeLong((long) p->surface.blendmode);
        writeFloat(p->surface.normal[VX]);
        writeFloat(p->surface.normal[VY]);
        writeFloat(p->surface.normal[VZ]);
        writeFloat(p->surface.offset[VX]);
        writeFloat(p->surface.offset[VY]);
        writeFloat(p->surface.rgba[CR]);
        writeFloat(p->surface.rgba[CG]);
        writeFloat(p->surface.rgba[CB]);
        writeFloat(p->surface.rgba[CA]);

        writeFloat(p->soundorg.pos[VX]);
        writeFloat(p->soundorg.pos[VY]);
        writeFloat(p->soundorg.pos[VZ]);
    }

    writeFloat(s->skyfix[PLN_FLOOR].offset);
    writeFloat(s->skyfix[PLN_CEILING].offset);
    writeLong(s->containsector? ((s->containsector - map->sectors) + 1) : 0);
    writeLong(s->flags);
    writeFloat(s->bbox[BOXLEFT]);
    writeFloat(s->bbox[BOXRIGHT]);
    writeFloat(s->bbox[BOXBOTTOM]);
    writeFloat(s->bbox[BOXTOP]);
    writeLong(s->lightsource? ((s->lightsource - map->sectors) + 1) : 0);
    writeFloat(s->soundorg.pos[VX]);
    writeFloat(s->soundorg.pos[VY]);
    writeFloat(s->soundorg.pos[VZ]);

    for(i = 0; i < NUM_REVERB_DATA; ++i)
        writeFloat(s->reverb[i]);

    // Lightgrid block indices.
    writeLong((long) s->changedblockcount);
    writeLong((long) s->blockcount);
    for(i = 0; i < s->blockcount; ++i)
        writeShort(s->blocks[i]);

    // Line list.
    writeLong((long) s->linecount);
    for(i = 0; i < s->linecount; ++i)
        writeLong((s->Lines[i] - map->lines) + 1);

    // Subsector list.
    writeLong((long) s->subscount);
    for(i = 0; i < s->subscount; ++i)
        writeLong((s->subsectors[i] - map->subsectors) + 1);

    // Reverb subsector attributors.
    writeLong((long) s->numReverbSSecAttributors);
    for(i = 0; i < s->numReverbSSecAttributors; ++i)
        writeLong((s->reverbSSecs[i] - map->subsectors) + 1);

    // Sector subsector groups.
    writeLong((long) s->subsgroupcount);
    for(i = 0; i < s->subsgroupcount; ++i)
    {
        ssecgroup_t        *g = &s->subsgroups[i];
        for(j = 0; j < s->planecount; ++j)
            writeLong((g->linked[j] - map->sectors) + 1);
    }
}

static void readSector(const gamemap_t *map, uint idx)
{
    uint                i, j, numPlanes;
    long                secIdx;
    sector_t           *s = &map->sectors[idx];

    s->lightlevel = readFloat();
    s->rgb[CR] = readFloat();
    s->rgb[CG] = readFloat();
    s->rgb[CB] = readFloat();
    numPlanes = (uint) readLong();
    for(i = 0; i < numPlanes; ++i)
    {
        plane_t            *p = R_NewPlaneForSector(s);

        p->height = readFloat();
        p->glow = readFloat();
        p->glowrgb[CR] = readFloat();
        p->glowrgb[CG] = readFloat();
        p->glowrgb[CB] = readFloat();
        p->target = readFloat();
        p->speed = readFloat();
        p->visheight = readFloat();
        p->visoffset = readFloat();

        p->surface.flags = (int) readLong();
        p->surface.material = lookupMaterialFromDict(materialDict, readLong());
        p->surface.blendmode = (blendmode_t) readLong();
        p->surface.normal[VX] = readFloat();
        p->surface.normal[VY] = readFloat();
        p->surface.normal[VZ] = readFloat();
        p->surface.offset[VX] = readFloat();
        p->surface.offset[VY] = readFloat();
        p->surface.rgba[CR] = readFloat();
        p->surface.rgba[CG] = readFloat();
        p->surface.rgba[CB] = readFloat();
        p->surface.rgba[CA] = readFloat();

        p->soundorg.pos[VX] = readFloat();
        p->soundorg.pos[VY] = readFloat();
        p->soundorg.pos[VZ] = readFloat();

        p->surface.decorations = NULL;
        p->surface.numdecorations = 0;
    }

    s->skyfix[PLN_FLOOR].offset = readFloat();
    s->skyfix[PLN_CEILING].offset = readFloat();
    secIdx = readLong();
    s->containsector = (secIdx == 0? NULL : &map->sectors[secIdx - 1]);
    s->flags = readLong();
    s->bbox[BOXLEFT] = readFloat();
    s->bbox[BOXRIGHT] = readFloat();
    s->bbox[BOXBOTTOM] = readFloat();
    s->bbox[BOXTOP] = readFloat();
    secIdx = readLong();
    s->lightsource = (secIdx == 0? NULL : &map->sectors[secIdx - 1]);
    s->soundorg.pos[VX] = readFloat();
    s->soundorg.pos[VY] = readFloat();
    s->soundorg.pos[VZ] = readFloat();

    for(i = 0; i < NUM_REVERB_DATA; ++i)
        s->reverb[i] = readFloat();

    // Lightgrid block indices.
    s->changedblockcount = (uint) readLong();
    s->blockcount = (uint) readLong();
    s->blocks = Z_Malloc(sizeof(short) * s->blockcount, PU_LEVEL, 0);
    for(i = 0; i < s->blockcount; ++i)
        s->blocks[i] = readShort();

    // Line list.
    s->linecount = (uint) readLong();
    s->Lines = Z_Malloc(sizeof(line_t*) * (s->linecount + 1), PU_LEVEL, 0);
    for(i = 0; i < s->linecount; ++i)
        s->Lines[i] = &map->lines[(unsigned) readLong() - 1];
    s->Lines[i] = NULL; // Terminate.

    // Subsector list.
    s->subscount = (uint) readLong();
    s->subsectors =
        Z_Malloc(sizeof(subsector_t*) * (s->subscount + 1), PU_LEVEL, 0);
    for(i = 0; i < s->subscount; ++i)
        s->subsectors[i] = &map->subsectors[(unsigned) readLong() - 1];
    s->subsectors[i] = NULL; // Terminate.

    // Reverb subsector attributors.
    s->numReverbSSecAttributors = (uint) readLong();
    s->reverbSSecs =
        Z_Malloc(sizeof(subsector_t*) * (s->numReverbSSecAttributors + 1), PU_LEVEL, 0);
    for(i = 0; i < s->numReverbSSecAttributors; ++i)
        s->reverbSSecs[i] = &map->subsectors[(unsigned) readLong() - 1];
    s->reverbSSecs[i] = NULL; // Terminate.

    // Subsector groups.
    s->subsgroupcount = (uint) readLong();
    s->subsgroups =
        Z_Malloc(sizeof(ssecgroup_t) * s->subsgroupcount, PU_LEVEL, 0);
    for(i = 0; i < s->subsgroupcount; ++i)
    {
        ssecgroup_t        *g = &s->subsgroups[i];

        g->linked =
            Z_Malloc(sizeof(sector_t*) * (s->planecount + 1), PU_LEVEL, 0);
        for(j = 0; j < s->planecount; ++j)
            g->linked[j] = &map->sectors[(unsigned) readLong() - 1];
        g->linked[j] = NULL; // Terminate.
    }
}

static void archiveSectors(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SECTORS);
    else
        assertSegment(DAMSEG_SECTORS);

    if(write)
    {
        writeLong(map->numsectors);
        for(i = 0; i < map->numsectors; ++i)
            writeSector(map, i);
    }
    else
    {
        map->numsectors = readLong();
        for(i = 0; i < map->numsectors; ++i)
            readSector(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSubsector(const gamemap_t *map, uint idx)
{
    uint                i;
    subsector_t        *s = &map->subsectors[idx];

    writeLong((long) s->flags);
    writeFloat(s->bbox[0].pos[VX]);
    writeFloat(s->bbox[0].pos[VY]);
    writeFloat(s->bbox[0].pos[VZ]);
    writeFloat(s->bbox[1].pos[VX]);
    writeFloat(s->bbox[1].pos[VY]);
    writeFloat(s->bbox[1].pos[VZ]);
    writeFloat(s->midpoint.pos[VX]);
    writeFloat(s->midpoint.pos[VY]);
    writeFloat(s->midpoint.pos[VZ]);
    writeLong(s->sector? ((s->sector - map->sectors) + 1) : 0);
    writeLong(s->poly? (s->poly->idx + 1) : 0);
    writeLong((long) s->group);

    // Subsector reverb.
    for(i = 0; i < NUM_REVERB_DATA; ++i)
        writeLong((long) s->reverb[i]);

    // Subsector segs list.
    writeLong((long) s->segcount);
    for(i = 0; i < s->segcount; ++i)
        writeLong((s->segs[i] - map->segs) + 1);
}

static void readSubsector(const gamemap_t *map, uint idx)
{
    uint                i;
    long                obIdx;
    subsector_t        *s = &map->subsectors[idx];

    s->flags = (int) readLong();
    s->bbox[0].pos[VX] = readFloat();
    s->bbox[0].pos[VY] = readFloat();
    s->bbox[0].pos[VZ] = readFloat();
    s->bbox[1].pos[VX] = readFloat();
    s->bbox[1].pos[VY] = readFloat();
    s->bbox[1].pos[VZ] = readFloat();
    s->midpoint.pos[VX] = readFloat();
    s->midpoint.pos[VY] = readFloat();
    s->midpoint.pos[VZ] = readFloat();
    obIdx = readLong();
    s->sector = (obIdx == 0? NULL : &map->sectors[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->poly = (obIdx == 0? NULL : map->polyobjs[(unsigned) obIdx - 1]);

    // Subsector reverb.
    for(i = 0; i < NUM_REVERB_DATA; ++i)
        s->reverb[i] = (uint) readLong();

    // Subsector segs list.
    s->segcount = (uint) readLong();
    s->segs = Z_Malloc(sizeof(seg_t*) * (s->segcount + 1), PU_LEVEL, 0);
    for(i = 0; i < s->segcount; ++i)
        s->segs[i] = &map->segs[(unsigned) readLong() - 1];
    s->segs[i] = NULL; // Terminate.
}

static void archiveSubsectors(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SSECTORS);
    else
        assertSegment(DAMSEG_SSECTORS);

    if(write)
    {
        writeLong(map->numsubsectors);
        for(i = 0; i < map->numsubsectors; ++i)
            writeSubsector(map, i);
    }
    else
    {
        map->numsubsectors = readLong();
        for(i = 0; i < map->numsubsectors; ++i)
            readSubsector(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeSeg(const gamemap_t *map, uint idx)
{
    seg_t              *s = &map->segs[idx];

    writeLong((s->v[0] - map->vertexes) + 1);
    writeLong((s->v[1] - map->vertexes) + 1);
    writeFloat(s->length);
    writeFloat(s->offset);
    writeLong(s->sidedef? ((s->sidedef - map->sides) + 1) : 0);
    writeLong(s->linedef? ((s->linedef - map->lines) + 1) : 0);
    writeLong(s->sec[FRONT]? ((s->sec[FRONT] - map->sectors) + 1) : 0);
    writeLong(s->sec[BACK]? ((s->sec[BACK] - map->sectors) + 1) : 0);
    writeLong(s->subsector? ((s->subsector - map->subsectors) + 1) : 0);
    writeLong(s->backseg? ((s->backseg - map->segs) + 1) : 0);
    writeLong((long) s->angle);
    writeByte(s->side);
    writeByte(s->flags);
}

static void readSeg(const gamemap_t *map, uint idx)
{
    long                obIdx;
    seg_t              *s = &map->segs[idx];

    s->v[0] = &map->vertexes[(unsigned) readLong() - 1];
    s->v[1] = &map->vertexes[(unsigned) readLong() - 1];
    s->length = readFloat();
    s->offset = readFloat();
    obIdx = readLong();
    s->sidedef = (obIdx == 0? NULL : &map->sides[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->linedef = (obIdx == 0? NULL : &map->lines[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->sec[FRONT] = (obIdx == 0? NULL : &map->sectors[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->sec[BACK] = (obIdx == 0? NULL : &map->sectors[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->subsector = (obIdx == 0? NULL : &map->subsectors[(unsigned) obIdx - 1]);
    obIdx = readLong();
    s->backseg = (obIdx == 0? NULL : &map->segs[(unsigned) obIdx - 1]);
    s->angle = (angle_t) readLong();
    s->side = readByte();
    s->flags = readByte();
}

static void archiveSegs(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_SEGS);
    else
        assertSegment(DAMSEG_SEGS);

    if(write)
    {
        writeLong(map->numsegs);
        for(i = 0; i < map->numsegs; ++i)
            writeSeg(map, i);
    }
    else
    {
        map->numsegs = readLong();
        for(i = 0; i < map->numsegs; ++i)
            readSeg(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeNode(const gamemap_t *map, uint idx)
{
    node_t             *n = &map->nodes[idx];

    writeFloat(n->x);
    writeFloat(n->y);
    writeFloat(n->dx);
    writeFloat(n->dy);
    writeFloat(n->bbox[RIGHT][BOXLEFT]);
    writeFloat(n->bbox[RIGHT][BOXRIGHT]);
    writeFloat(n->bbox[RIGHT][BOXBOTTOM]);
    writeFloat(n->bbox[RIGHT][BOXTOP]);
    writeFloat(n->bbox[LEFT][BOXLEFT]);
    writeFloat(n->bbox[LEFT][BOXRIGHT]);
    writeFloat(n->bbox[LEFT][BOXBOTTOM]);
    writeFloat(n->bbox[LEFT][BOXTOP]);
    writeLong((long) n->children[RIGHT]);
    writeLong((long) n->children[LEFT]);
}

static void readNode(const gamemap_t *map, uint idx)
{
    node_t             *n = &map->nodes[idx];

    n->x = readFloat();
    n->y = readFloat();
    n->dx = readFloat();
    n->dy = readFloat();
    n->bbox[RIGHT][BOXLEFT] = readFloat();
    n->bbox[RIGHT][BOXRIGHT] = readFloat();
    n->bbox[RIGHT][BOXBOTTOM] = readFloat();
    n->bbox[RIGHT][BOXTOP] = readFloat();
    n->bbox[LEFT][BOXLEFT] = readFloat();
    n->bbox[LEFT][BOXRIGHT] = readFloat();
    n->bbox[LEFT][BOXBOTTOM] = readFloat();
    n->bbox[LEFT][BOXTOP] = readFloat();
    n->children[RIGHT] = (uint) readLong();
    n->children[LEFT] = (uint) readLong();
}

static void archiveNodes(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_NODES);
    else
        assertSegment(DAMSEG_NODES);

    if(write)
    {
        writeLong(map->numnodes);
        for(i = 0; i < map->numnodes; ++i)
            writeNode(map, i);
    }
    else
    {
        map->numnodes = readLong();
        for(i = 0; i < map->numnodes; ++i)
            readNode(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveBlockmap(gamemap_t *map, boolean write)
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

static void archiveReject(gamemap_t *map, boolean write)
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

static void writePolyobj(const gamemap_t *map, uint idx)
{
    uint                i;
    polyobj_t          *p = map->polyobjs[idx];

    writeLong((long) p->idx);
    writeFloat(p->startSpot.pos[VX]);
    writeFloat(p->startSpot.pos[VY]);
    writeFloat(p->startSpot.pos[VZ]);
    writeLong((long) p->angle);
    writeLong((long) p->tag);
    writeFloat(p->box[0][VX]);
    writeFloat(p->box[0][VY]);
    writeFloat(p->box[1][VX]);
    writeFloat(p->box[1][VY]);
    writeFloat(p->dest.pos[VX]);
    writeFloat(p->dest.pos[VY]);
    writeFloat(p->speed);
    writeLong((long) p->destAngle);
    writeLong((long) p->angleSpeed);
    writeByte(p->crush? 1 : 0);
    writeLong((long) p->seqType);

    writeLong((long) p->numsegs);
    for(i = 0; i < p->numsegs; ++i)
    {
        seg_t              *s = p->segs[i];

        writeLong((s->v[0] - map->vertexes) + 1);
        writeLong((s->v[1] - map->vertexes) + 1);
        writeFloat(s->length);
        writeFloat(s->offset);
        writeLong(s->sidedef? ((s->sidedef - map->sides) + 1) : 0);
        writeLong(s->linedef? ((s->linedef - map->lines) + 1) : 0);
        writeLong(s->sec[FRONT]? ((s->sec[FRONT] - map->sectors) + 1) : 0);
        writeLong((long) s->angle);
        writeByte(s->side);
        writeByte(s->flags);
    }
}

static void readPolyobj(const gamemap_t *map, uint idx)
{
    uint                i;
    long                obIdx;
    polyobj_t          *p = map->polyobjs[idx];

    p->idx = (uint) readLong();
    p->startSpot.pos[VX] = readFloat();
    p->startSpot.pos[VY] = readFloat();
    p->startSpot.pos[VZ] = readFloat();
    p->angle = (angle_t) readLong();
    p->tag = (int) readLong();
    p->box[0][VX] = readFloat();
    p->box[0][VY] = readFloat();
    p->box[1][VX] = readFloat();
    p->box[1][VY] = readFloat();
    p->dest.pos[VX] = readFloat();
    p->dest.pos[VY] = readFloat();
    p->speed = readFloat();
    p->destAngle = (angle_t) readLong();
    p->angleSpeed = (angle_t) readLong();
    p->crush = (readByte()? true : false);
    p->seqType = (int) readLong();

    // Polyobj seg list.
    p->numsegs = (uint) readLong();
    p->segs = Z_Malloc(sizeof(seg_t*) * (p->numsegs + 1), PU_LEVEL, 0);
    for(i = 0; i < p->numsegs; ++i)
    {
        seg_t              *s =
            Z_Calloc(sizeof(*s), PU_LEVEL, 0);

        s->v[0] = &map->vertexes[(unsigned) readLong() - 1];
        s->v[1] = &map->vertexes[(unsigned) readLong() - 1];
        s->length = readFloat();
        s->offset = readFloat();
        obIdx = readLong();
        s->sidedef = (obIdx == 0? NULL : &map->sides[(unsigned) obIdx - 1]);
        obIdx = readLong();
        s->linedef = (obIdx == 0? NULL : &map->lines[(unsigned) obIdx - 1]);
        obIdx = readLong();
        s->sec[FRONT] = (obIdx == 0? NULL : &map->sectors[(unsigned) obIdx - 1]);
        s->angle = (angle_t) readLong();
        s->side = (readByte()? 1 : 0);
        s->flags = readByte();

        p->segs[i] = s;
    }
    p->segs[i] = NULL; // Terminate.
}

static void archivePolyobjs(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_POLYOBJS);
    else
        assertSegment(DAMSEG_POLYOBJS);

    if(write)
    {
        writeLong(map->numpolyobjs);
        for(i = 0; i < map->numpolyobjs; ++i)
            writePolyobj(map, i);
    }
    else
    {
        map->numpolyobjs = readLong();
        for(i = 0; i < map->numpolyobjs; ++i)
            readPolyobj(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void writeThing(const gamemap_t *map, uint idx)
{

}

static void readThing(const gamemap_t *map, uint idx)
{

}

static void archiveThings(gamemap_t *map, boolean write)
{
    uint                i;

    if(write)
        beginSegment(DAMSEG_THINGS);
    else
        assertSegment(DAMSEG_THINGS);

    if(write)
    {
        writeLong(map->numthings);
        for(i = 0; i < map->numthings; ++i)
            writeThing(map, i);
    }
    else
    {
        map->numthings = readLong();
        for(i = 0; i < map->numthings; ++i)
            readThing(map, i);
    }

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveMap(gamemap_t *map, boolean write)
{
    if(write)
        beginSegment(DAMSEG_MAP);
    else
    {
        assertSegment(DAMSEG_MAP);

        // Call the game's setup routines.
        if(gx.SetupForMapData)
        {
            gx.SetupForMapData(DAM_VERTEX, map->numvertexes);
            gx.SetupForMapData(DAM_THING, map->numthings);
            gx.SetupForMapData(DAM_LINE, map->numlines);
            gx.SetupForMapData(DAM_SIDE, map->numsides);
            gx.SetupForMapData(DAM_SECTOR, map->numsectors);
        }
    }

    archiveThings(map, write);
    archivePolyobjs(map, write);
    archiveVertexes(map, write);
    archiveLines(map, write); // Must follow vertexes (lineowner nodes).
    archiveSides(map, write);
    archiveSectors(map, write);
    archiveSubsectors(map, write);
    archiveSegs(map, write);
    archiveNodes(map, write);
    archiveBlockmap(map, write);
    archiveReject(map, write);

    if(write)
        endSegment();
    else
        assertSegment(DAMSEG_END);
}

static void archiveMaterialDict(materialdict_t *dict, boolean write)
{
    int                 i;

    if(write)
    {
        writeLong((long) dict->count);
        for(i = 0; i < dict->count; ++i)
        {
            writeNBytes(dict->table[i].name, 8);
        }
    }
    else
    {
        dict->count = readLong();
        for(i = 0; i < dict->count; ++i)
        {
            readNBytes(dict->table[i].name, 8);
            dict->table[i].name[8] = 0;
        }
    }
}
static void archiveSymbolTables(boolean write)
{
    if(write)
        beginSegment(DAMSEG_SYMBOLTABLES);
    else
        assertSegment(DAMSEG_SYMBOLTABLES);

    archiveMaterialDict(materialDict, write);

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

static boolean doArchiveMap(gamemap_t *map, filename_t path,
                            boolean write)
{
    if(!path)
        return false;

    // Open the file.
    if(!openMapFile(path, write))
        return false; // Hmm, invalid path?

    materialDict = M_Calloc(sizeof(*materialDict));

    if(write)
        initMaterialDict(map, materialDict);

    archiveHeader(write);
    archiveRelocationTables(write);
    archiveSymbolTables(write);
    archiveMap(map, write);

    // Close the file.
    closeMapFile();

    M_Free(materialDict);

    return true;
}

/**
 * Load data from a Doomsday archived map file.
 */
boolean DAM_MapWrite(gamemap_t *map, filename_t path)
{
    return doArchiveMap(map, path, true);
}

/**
 * Write the current state of a map into a Doomsday archived map file.
 */
boolean DAM_MapRead(gamemap_t *map, filename_t path)
{
    Con_Message("DAM_MapRead: Loading cached map. %s\n", path);
    return doArchiveMap(map, path, false);
}

/**
 * Check if archived map file is current.
 */
boolean DAM_MapIsValid(filename_t cachedMapDataFile, int markerLumpNum)
{
	uint                sourceTime, buildTime;

	// The source data must not be newer than the cached map data.
	sourceTime = F_LastModified(W_LumpSourceFile(markerLumpNum));
	buildTime = F_LastModified(cachedMapDataFile);

    if(F_Access(cachedMapDataFile) && !(buildTime < sourceTime))
    {   // Ok, lets check the header.
        if(openMapFile(cachedMapDataFile, false))
        {
            archiveHeader(false);
            closeMapFile();

            if(mapFileVersion == DAM_VERSION)
                return true; // Its good.
        }
    }

    return false;
}
