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
 * load.c: Load and analyzation of the map data structures.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomsday.h"
#include "dd_api.h"

#include "wadmapconverter.h"

#include <stdlib.h>
#include <string.h>

// MACROS ------------------------------------------------------------------

// Size of the map data structures in bytes in the arrived WAD format.
#define SIZEOF_VERTEX           (2 * 2)
#define SIZEOF_XTHING           (2 * 7 + 1 * 6)
#define SIZEOF_THING            (2 * 5)
#define SIZEOF_XLINEDEF         (2 * 5 + 1 * 6)
#define SIZEOF_LINEDEF          (2 * 7)
#define SIZEOF_SIDEDEF          (2 * 3 + 8 * 3)
#define SIZEOF_SECTOR           (2 * 5 + 8 * 2)

#define PO_LINE_START           1 // polyobj line start special
#define PO_LINE_EXPLICIT        5

#define SEQTYPE_NUMSEQ          10

// TYPES -------------------------------------------------------------------

typedef enum lumptype_e {
    ML_INVALID = -1,
    FIRST_LUMP_TYPE,
    ML_LABEL = FIRST_LUMP_TYPE, // A separator, name, ExMx or MAPxx
    ML_THINGS,                  // Monsters, items..
    ML_LINEDEFS,                // LineDefs, from editing
    ML_SIDEDEFS,                // SideDefs, from editing
    ML_VERTEXES,                // Vertices, edited and BSP splits generated
    ML_SEGS,                    // LineSegs, from LineDefs split by BSP
    ML_SSECTORS,                // SubSectors, list of LineSegs
    ML_NODES,                   // BSP nodes
    ML_SECTORS,                 // Sectors, from editing
    ML_REJECT,                  // LUT, sector-sector visibility
    ML_BLOCKMAP,                // LUT, motion clipping, walls/grid element
    ML_BEHAVIOR,                // ACS Scripts (compiled).
    ML_SCRIPTS,                 // ACS Scripts (source).
    ML_GLVERT,                  // GL vertexes
    ML_GLSEGS,                  // GL segs
    ML_GLSSECT,                 // GL subsectors
    ML_GLNODES,                 // GL nodes
    ML_GLPVS,                   // GL PVS dataset
    NUM_LUMP_TYPES
} lumptype_t;

enum {
    PO_ANCHOR_TYPE = 3000,
    PO_SPAWN_TYPE,
    PO_SPAWNCRUSH_TYPE
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static uint PolyLineCount;
static uint16_t PolyStart[2];

// CODE --------------------------------------------------------------------

static uint registerMaterial(const char *name, boolean isFlat)
{
    uint                i;
    materialref_t      *m;

    // Check if this material has already been registered.
    for(i = 0; i < map->nummaterials; ++i)
    {
        m = map->materials[i];

        if(m->isFlat == isFlat &&
           !strnicmp(name, map->materials[i]->name, 8))
        {   // Already registered.
            return i;
        }
    }

    /**
     * A new material.
     */
    m = malloc(sizeof(*m));
    memcpy(m->name, name, 8);
    m->name[8] = '\0';
    m->isFlat = isFlat;

    // Add it to the list of known materials.
    map->materials = realloc(map->materials, sizeof(m) * ++map->nummaterials);
    map->materials[map->nummaterials-1] = m;
    return map->nummaterials-1; // 0-based index.
}

int DataTypeForLumpName(const char *name)
{
    struct lumptype_s {
        lumptype_t      type;
        const char     *name;
    } knownLumps[] =
    {
        {ML_LABEL,      "*"},
        {ML_THINGS,     "THINGS"},
        {ML_LINEDEFS,   "LINEDEFS"},
        {ML_SIDEDEFS,   "SIDEDEFS"},
        {ML_VERTEXES,   "VERTEXES"},
        {ML_SEGS,       "SEGS"},
        {ML_SSECTORS,   "SSECTORS"},
        {ML_NODES,      "NODES"},
        {ML_SECTORS,    "SECTORS"},
        {ML_REJECT,     "REJECT"},
        {ML_BLOCKMAP,   "BLOCKMAP"},
        {ML_BEHAVIOR,   "BEHAVIOR"},
        {ML_SCRIPTS,    "SCRIPTS"},
        {ML_GLVERT,     "GL_VERT"},
        {ML_GLSEGS,     "GL_SEGS"},
        {ML_GLSSECT,    "GL_SSECT"},
        {ML_GLNODES,    "GL_NODES"},
        {ML_GLPVS,      "GL_PVS"},
        {ML_INVALID,    NULL},
    };
    lumptype_t          i;

    for(i = FIRST_LUMP_TYPE; knownLumps[i].type != ML_INVALID; ++i)
    {
        if(!strncmp(knownLumps[i].name, name, 8))
            return knownLumps[i].type;
    }

    return ML_INVALID;
}

/**
 * @return              @c  0 = Unclosed polygon.
 */
static int isClosedPolygon(mline_t **lineList, size_t num)
{
    uint            i;

    for(i = 0; i < num; ++i)
    {
        mline_t         *line = lineList[i];
        mline_t         *next = (i == num-1? lineList[0] : lineList[i+1]);

        if(!(line->v[1] == next->v[0]))
             return false;
    }

    // The polygon is closed.
    return true;
}

/**
 * Create a temporary polyobj (read from the original map data).
 */
static boolean createPolyobj(mline_t **lineList, uint num, uint *poIdx,
                           boolean crush, int tag, int sequenceType,
                           float startX, float startY)
{
    uint                i;
    mpolyobj_t          *po, **newList;

    if(!lineList || num == 0)
        return false;

    // Ensure that lineList is a closed polygon.
    if(isClosedPolygon(lineList, num) == 0)
    {   // Nope, perhaps it needs sorting?
        Con_Error("PO_CreatePolyobj: Linelist does not form a closed polygon.");
    }

    // Allocate the new polyobj.
    po = calloc(1, sizeof(*po));

    /**
     * Link the new polyobj into the global list.
     */
    newList = malloc(((++map->numpolyobjs) + 1) * sizeof(mpolyobj_t*));
    // Copy the existing list.
    for(i = 0; i < map->numpolyobjs - 1; ++i)
    {
        newList[i] = map->polyobjs[i];
    }
    newList[i++] = po; // Add the new polyobj.
    newList[i] = NULL; // Terminate.

    if(map->numpolyobjs-1 > 0)
        free(map->polyobjs);
    map->polyobjs = newList;

    po->idx = map->numpolyobjs-1;
    po->crush = crush;
    po->tag = tag;
    po->seqType = sequenceType;
    po->startSpot[VX] = startX;
    po->startSpot[VY] = startY;
    po->lineCount = num;
    po->lineIndices = malloc(sizeof(uint) * num);
    for(i = 0; i < num; ++i)
    {
        // This line is part of a polyobj.
        lineList[i]->aFlags |= LAF_POLYOBJ;

        po->lineIndices[i] = lineList[i] - map->lines;
    }

    if(poIdx)
        *poIdx = po->idx;

    return true; // Success!
}

/**
 * @param lineList      @c NULL, will cause IterFindPolyLines to count
 *                      the number of lines in the polyobj.
 */
static boolean iterFindPolyLines(int16_t x, int16_t y,
                                 mline_t **lineList)
{
    uint        i;

    if(x == PolyStart[VX] && y == PolyStart[VY])
    {
        return true;
    }

    for(i = 0; i < map->numlines; ++i)
    {
        mline_t        *line = &map->lines[i];
        mvertex_t      *v1 = &map->vertexes[line->v[0]-1];
        mvertex_t      *v2 = &map->vertexes[line->v[1]-1];

        if(v1->pos[VX] == x && v1->pos[VY] == y)
        {
            if(!lineList)
                PolyLineCount++;
            else
                *lineList++ = line;

            iterFindPolyLines(v2->pos[VX], v2->pos[VY], lineList);
            return true;
        }
    }

    return false;
}

/**
 * Find all linedefs marked as belonging to a polyobject with the given tag
 * and attempt to create a polyobject from them.
 *
 * @param tag               Line tag of linedefs to search for.
 * @param crush             Whether the polyobject should crush things.
 *
 * @return                  @c true = successfully created polyobj.
 */
static boolean findAndCreatePolyobj(int16_t tag, boolean crush,
                                    int16_t startX, int16_t startY)
{
#define PO_MAXPOLYLINES         32

    uint            i;

    for(i = 0; i < map->numlines; ++i)
    {
        mline_t        *line = &map->lines[i];

        if(line->xSpecial == PO_LINE_START && line->xArgs[0] == tag)
        {
            byte            seqType;
            mline_t       **lineList;
            mvertex_t      *v1, *v2;
            uint            poIdx;

            line->xSpecial = 0;
            line->xArgs[0] = 0;
            PolyLineCount = 1;

            v1 = &map->vertexes[line->v[0]-1];
            v2 = &map->vertexes[line->v[1]-1];
            PolyStart[VX] = v1->pos[VX];
            PolyStart[VY] = v1->pos[VY];
            if(!iterFindPolyLines(v2->pos[VX], v2->pos[VY], NULL))
            {
                Con_Error("mapConverter::spawnPolyobj: Found unclosed polyobj.\n");
            }

            lineList = malloc((PolyLineCount+1) * sizeof(mline_t*));

            lineList[0] = line; // Insert the first line.
            iterFindPolyLines(v2->pos[VX], v2->pos[VY], lineList + 1);
            lineList[PolyLineCount] = 0; // Terminate.

            seqType = line->xArgs[2];
            if(seqType < 0 || seqType >= SEQTYPE_NUMSEQ)
                seqType = 0;

            if(createPolyobj(lineList, PolyLineCount, &poIdx, crush,
                             tag, seqType, startX, startY))
            {
                free(lineList);
                return true;
            }

            free(lineList);
        }
    }

    /**
     * Didn't find a polyobj through PO_LINE_START.
     * We'll try another approach...
     */
    {
    mline_t         *polyLineList[PO_MAXPOLYLINES];
    uint            lineCount = 0;
    uint            j, psIndex, psIndexOld;

    psIndex = 0;
    for(j = 1; j < PO_MAXPOLYLINES; ++j)
    {
        psIndexOld = psIndex;
        for(i = 0; i < map->numlines; ++i)
        {
            mline_t         *line = &map->lines[i];

            if(line->xSpecial == PO_LINE_EXPLICIT &&
               line->xArgs[0] == tag)
            {
                if(!line->xArgs[1])
                {
                    Con_Error
                        ("spawnPolyobj:  Explicit line missing order number "
                         "(probably %d) in poly %d.\n",
                         j + 1, tag);
                }

                if(line->xArgs[1] == j)
                {
                    // Add this line to the list.
                    polyLineList[psIndex] = line;
                    lineCount++;
                    psIndex++;
                    if(psIndex > PO_MAXPOLYLINES)
                    {
                        Con_Error
                            ("spawnPolyobj:  psIndex > PO_MAXPOLYLINES\n");
                    }

                    // Clear out any special.
                    line->xSpecial = 0;
                    line->xArgs[0] = 0;
                }
            }
        }

        if(psIndex == psIndexOld)
        {   // Check if an explicit line order has been skipped
            // A line has been skipped if there are any more explicit
            // lines with the current tag value
            for(i = 0; i < map->numlines; ++i)
            {
                mline_t         *line = &map->lines[i];

                if(line->xSpecial == PO_LINE_EXPLICIT &&
                   line->xArgs[0] == tag)
                {
                    Con_Error
                        ("spawnPolyobj:  Missing explicit line %d for poly %d\n",
                         j, tag);
                }
            }
        }
    }

    if(lineCount)
    {
        uint            poIdx;
        int             seqType = polyLineList[0]->xArgs[3];

        if(createPolyobj(polyLineList, lineCount, &poIdx, crush, tag,
                         seqType, startX, startY))
        {
            mline_t        *line = polyLineList[0];

            // Next, change the polyobjs first line to point to a mirror
            // if it exists.
            line->xArgs[1] = line->xArgs[2];

            return true;
        }
    }
    }

    return false;

#undef PO_MAXPOLYLINES
}

static void findPolyobjs(void)
{
    uint            i;

    Con_Message("WadMapConverter::findPolyobjs: Processing...\n");

    for(i = 0; i < map->numthings; ++i)
    {
        mthing_t       *thing = &map->things[i];

        if(thing->type == PO_SPAWN_TYPE ||
           thing->type == PO_SPAWNCRUSH_TYPE)
        {   // A polyobj start spot.
            int             tag = thing->angle;
            boolean         crush = (thing->type == PO_SPAWNCRUSH_TYPE);

            findAndCreatePolyobj(tag, crush,
                                 thing->pos[VX], thing->pos[VY]);
        }
    }
}

void AnalyzeMap(void)
{
    if(map->hexenFormat)
        findPolyobjs();
}

boolean IsSupportedFormat(const int *lumpList, int numLumps)
{
    int                 i;
    boolean             supported = false;

    // Lets first check for a BEHAVIOR lump, as its prescense determines
    // the format of the map data; Hexen rather than DOOM format.
    for(i = 0; i < numLumps; ++i)
    {
        if(!strncmp(W_LumpName(lumpList[i]), "BEHAVIOR", 8))
        {
            map->hexenFormat = true;
            break;
        }
    }

    for(i = 0; i < numLumps; ++i)
    {
        uint               *ptr;
        size_t              elmSize; // Num of bytes.

        // Determine the number of map data objects of each data type.
        ptr = NULL;
        switch(DataTypeForLumpName(W_LumpName(lumpList[i])))
        {
        case ML_VERTEXES:
            ptr = &map->numvertexes;
            elmSize = SIZEOF_VERTEX;
            break;

        case ML_THINGS:
            ptr = &map->numthings;
            elmSize = (map->hexenFormat? SIZEOF_XTHING : SIZEOF_THING);
            break;

        case ML_LINEDEFS:
            ptr = &map->numlines;
            elmSize = (map->hexenFormat? SIZEOF_XLINEDEF : SIZEOF_LINEDEF);
            break;

        case ML_SIDEDEFS:
            ptr = &map->numsides;
            elmSize = SIZEOF_SIDEDEF;
            break;

        case ML_SECTORS:
            ptr = &map->numsectors;
            elmSize = SIZEOF_SECTOR;
            break;

        default:
            break;
        }

        if(ptr)
        {
            size_t              lumpLength = W_LumpLength(lumpList[i]);

            if(0 != lumpLength % elmSize)
                return false; // What is this??

            *ptr += lumpLength / elmSize;
        }
    }

    if(map->numvertexes > 0 && map->numlines > 0 && map->numsides > 0 &&
       map->numsectors > 0 && map->numthings > 0)
    {
        supported = true;
    }

    return supported;
}

static void freeMapData(void)
{
    if(map->vertexes)
        free(map->vertexes);
    map->vertexes = NULL;

    if(map->lines)
        free(map->lines);
    map->lines = NULL;

    if(map->sides)
        free(map->sides);
    map->sides = NULL;

    if(map->sectors)
        free(map->sectors);
    map->sectors = NULL;

    if(map->things)
        free(map->things);
    map->things = NULL;

    if(map->polyobjs)
    {
        uint                i;
        for(i = 0; i < map->numpolyobjs; ++i)
        {
            mpolyobj_t         *po = map->polyobjs[i];
            free(po->lineIndices);
            free(po);
        }
        free(map->polyobjs);
    }
    map->polyobjs = NULL;

    if(map->materials)
    {
        uint                i;
        for(i = 0; i < map->nummaterials; ++i)
        {
            materialref_t      *m = map->materials[i];
            free(m);
        }
        free(map->materials);
    }
    map->materials = NULL;
}

/**
 * Attempts to load the BLOCKMAP data resource.
 *
 * If the level is too large (would overflow the size limit of
 * the BLOCKMAP lump in a WAD therefore it will have been truncated),
 * it's zero length or we are forcing a rebuild - we'll have to
 * generate the blockmap data ourselves.
 */
#if 0 // Needs updating.
static boolean loadBlockmap(tempmap_t *map, maplumpinfo_t *maplump)
{
#define MAPBLOCKUNITS       128

    boolean     generateBMap = (createBMap == 2)? true : false;

    Con_Message("WadMapConverter::loadBlockmap: Processing...\n");

    // Do we have a lump to process?
    if(maplump->lumpNum == -1 || maplump->length == 0)
        generateBMap = true; // We'll HAVE to generate it.

    // Are we generating new blockmap data?
    if(generateBMap)
    {
        // Only announce if the user has choosen to always generate
        // new data (we will have already announced it if the lump
        // was missing).
        if(maplump->lumpNum != -1)
            VERBOSE(
            Con_Message("loadBlockMap: Generating NEW blockmap...\n"));
    }
    else
    {   // No, the existing data is valid - so load it in.
        uint        startTime;
        blockmap_t *blockmap;
        uint        x, y, width, height;
        float       v[2];
        vec2_t      bounds[2];
        long       *lineListOffsets, i, n, numBlocks, blockIdx;
        short      *blockmapLump;

        VERBOSE(
        Con_Message("loadBlockMap: Converting existing blockmap...\n"));

        startTime = Sys_GetRealTime();

        blockmapLump =
            (short *) W_CacheLumpNum(maplump->lumpNum, PU_STATIC);

        v[VX] = (float) SHORT(blockmapLump[0]);
        v[VY] = (float) SHORT(blockmapLump[1]);
        width  = ((SHORT(blockmapLump[2])) & 0xffff);
        height = ((SHORT(blockmapLump[3])) & 0xffff);

        numBlocks = (long) width * (long) height;

        /**
         * Expand WAD blockmap into a larger one, by treating all
         * offsets except -1 as unsigned and zero-extending them.
         * This potentially doubles the size of blockmaps allowed
         * because DOOM originally considered the offsets as always
         * signed.
	     */

        lineListOffsets = M_Malloc(sizeof(long) * numBlocks);
        n = 4;
        for(i = 0; i < numBlocks; ++i)
        {
            short t = SHORT(blockmapLump[n++]);
            lineListOffsets[i] = (t == -1? -1 : (long) t & 0xffff);
        }

        /**
         * Finally, convert the blockmap into our internal representation.
         * We'll ensure the blockmap is formed correctly as we go.
         *
         * \todo We could gracefully handle malformed blockmaps by
         * by cleaning up and then generating our own.
         */

        V2_Set(bounds[0], v[VX], v[VY]);
        v[VX] += (float) (width * MAPBLOCKUNITS);
        v[VY] += (float) (height * MAPBLOCKUNITS);
        V2_Set(bounds[1], v[VX], v[VY]);

        blockmap = P_BlockmapCreate(bounds[0], bounds[1],
                                    width, height);
        blockIdx = 0;
        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
            {
                long        offset = lineListOffsets[blockIdx];
                long        idx;
                uint        count;

#if _DEBUG
if(SHORT(blockmapLump[offset]) != 0)
{
    Con_Error("loadBlockMap: Offset (%li) for block %u [%u, %u] "
              "does not index the beginning of a line list!\n",
              offset, blockIdx, x, y);
}
#endif

                // Count the number of lines in this block.
                count = 0;
                while((idx = SHORT(blockmapLump[offset + 1 + count])) != -1)
                    count++;

                if(count > 0)
                {
                    line_t    **lines, **ptr;

                    // A NULL-terminated array of pointers to lines.
                    lines = Z_Malloc((count + 1) * sizeof(line_t *),
                                    PU_LEVELSTATIC, NULL);

                    // Copy pointers to the array, delete the nodes.
                    ptr = lines;
                    count = 0;
                    while((idx = SHORT(blockmapLump[offset + 1 + count])) != -1)
                    {
#if _DEBUG
if(idx < 0 || idx >= (long) map->numlines)
{
    Con_Error("loadBlockMap: Invalid linedef id %li\n!", idx);
}
#endif
                        *ptr++ = &map->lines[idx];
                        count++;
                    }
                    // Terminate.
                    *ptr = NULL;

                    // Link it into the BlockMap.
                    P_BlockmapSetBlock(blockmap, x, y, lines, NULL);
                }

                blockIdx++;
            }

        // Don't need this anymore.
        M_Free(lineListOffsets);

        map->blockMap = blockmap;

        // How much time did we spend?
        VERBOSE(Con_Message
                ("loadBlockMap: Done in %.2f seconds.\n",
                 (Sys_GetRealTime() - startTime) / 1000.0f));
    }

    return true;

#undef MAPBLOCKUNITS
}
#endif

static boolean loadVertexes(const byte *buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte         *ptr;

    Con_Message("WadMapConverter::loadVertexes: Processing...\n");

    elmSize = SIZEOF_VERTEX;
    num = len / elmSize;
    for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
    {
        mvertex_t          *v = &map->vertexes[n];

        v->pos[VX] = SHORT(*((const int16_t*) (ptr)));
        v->pos[VY] = SHORT(*((const int16_t*) (ptr+2)));
    }

    return true;
}

static boolean loadLinedefs(const byte *buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte         *ptr;

    Con_Message("WadMapConverter::loadLinedefs: Processing...\n");

    elmSize =
        (map->hexenFormat? SIZEOF_XLINEDEF : SIZEOF_LINEDEF);
    num = len / elmSize;

    if(map->hexenFormat)
    {
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int             idx;
            mline_t        *l = &map->lines[n];

            idx = USHORT(*((const uint16_t*) (ptr)));
            if(idx == 0xFFFF)
                l->v[0] = 0;
            else
                l->v[0] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+2)));
            if(idx == 0xFFFF)
                l->v[1] = 0;
            else
                l->v[1] = idx + 1;
            l->flags = SHORT(*((const int16_t*) (ptr+4)));
            l->xSpecial = *((const byte*) (ptr+6));
            l->xArgs[0] = *((const byte*) (ptr+7));
            l->xArgs[1] = *((const byte*) (ptr+8));
            l->xArgs[2] = *((const byte*) (ptr+9));
            l->xArgs[3] = *((const byte*) (ptr+10));
            l->xArgs[4] = *((const byte*) (ptr+11));
            idx = USHORT(*((const uint16_t*) (ptr+12)));
            if(idx == 0xFFFF)
                l->sides[RIGHT] = 0;
            else
                l->sides[RIGHT] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+14)));
            if(idx == 0xFFFF)
                l->sides[LEFT] = 0;
            else
                l->sides[LEFT] = idx + 1;
        }
    }
    else
    {   // DOOM format.
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            int             idx;
            mline_t        *l = &map->lines[n];

            idx = USHORT(*((const uint16_t*) (ptr)));
            if(idx == 0xFFFF)
                l->v[0] = 0;
            else
                l->v[0] = idx + 1;
            idx = USHORT(*((const uint16_t*) (ptr+2)));
            if(idx == 0xFFFF)
                l->v[1] = 0;
            else
                l->v[1] = idx + 1;
            l->flags = SHORT(*((const int16_t*) (ptr+4)));
            l->dSpecial = SHORT(*((const int16_t*) (ptr+6)));
            l->dTag = SHORT(*((const int16_t*) (ptr+8)));
            idx = USHORT(*((const uint16_t*) (ptr+10)));
            if(idx == 0xFFFF)
                l->sides[RIGHT] = 0;
            else
                l->sides[RIGHT] = idx + 1;
            idx = (short) (ptr+12);
            idx = USHORT(*((const uint16_t*) (ptr+12)));
            if(idx == 0xFFFF)
                l->sides[LEFT] = 0;
            else
                l->sides[LEFT] = idx + 1;
            l = l;
        }
    }

    return true;
}

static boolean loadSidedefs(const byte *buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte         *ptr;

    Con_Message("WadMapConverter::loadSidedefs: Processing...\n");

    elmSize = SIZEOF_SIDEDEF;
    num = len / elmSize;
    for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
    {
        int                 idx;
        char                name[9];
        mside_t            *s = &map->sides[n];

        s->offset[VX] = SHORT(*((const int16_t*) (ptr)));
        s->offset[VY] = SHORT(*((const int16_t*) (ptr+2)));
        memcpy(name, ptr+4, 8);
        name[8] = '\0';
        s->topMaterial = registerMaterial(name, false);
        memcpy(name, ptr+12, 8);
        name[8] = '\0';
        s->bottomMaterial = registerMaterial(name, false);
        memcpy(name, ptr+20, 8);
        name[8] = '\0';
        s->middleMaterial = registerMaterial(name, false);
        idx = USHORT(*((const uint16_t*) (ptr+28)));
        if(idx == 0xFFFF)
            s->sector = 0;
        else
            s->sector = idx + 1;
    }

    return true;
}

static boolean loadSectors(const byte *buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte         *ptr;

    Con_Message("WadMapConverter::loadSectors: Processing...\n");

    elmSize = SIZEOF_SECTOR;
    num = len / elmSize;
    for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
    {
        char                name[9];
        msector_t          *s = &map->sectors[n];

        s->floorHeight = SHORT(*((const int16_t*) ptr));
        s->ceilHeight = SHORT(*((const int16_t*) (ptr+2)));
        memcpy(name, ptr+4, 8);
        name[8] = '\0';
        s->floorMaterial = registerMaterial(name, true);
        memcpy(name, ptr+12, 8);
        name[8] = '\0';
        s->ceilMaterial = registerMaterial(name, true);
        s->lightlevel = SHORT(*((const int16_t*) (ptr+20)));
        s->type = SHORT(*((const int16_t*) (ptr+22)));
        s->tag = SHORT(*((const int16_t*) (ptr+24)));
    }

    return true;
}

static boolean loadThings(const byte *buf, size_t len)
{
    uint                num, n;
    size_t              elmSize;
    const byte         *ptr;

    Con_Message("WadMapConverter::loadThings: Processing...\n");

    elmSize = (map->hexenFormat? SIZEOF_XTHING : SIZEOF_THING);
    num = len / elmSize;

    if(map->hexenFormat)
    {
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            mthing_t           *t = &map->things[n];

            t->xTID = SHORT(*((const int16_t*) ptr));
            t->pos[VX] = SHORT(*((const int16_t*) ptr+2));
            t->pos[VY] = SHORT(*((const int16_t*) ptr+4));
            t->xSpawnZ = SHORT(*((const int16_t*) ptr+6));
            t->angle = SHORT(*((const int16_t*) ptr+8));
            t->type = SHORT(*((const int16_t*) ptr+10));
            t->flags = SHORT(*((const int16_t*) ptr+12));
            t->xSpecial = *(ptr+14);
            t->xArgs[0] = *(ptr+15);
            t->xArgs[1] = *(ptr+16);
            t->xArgs[2] = *(ptr+17);
            t->xArgs[3] = *(ptr+18);
            t->xArgs[4] = *(ptr+19);
        }
    }
    else
    {   // DOOM format.
        for(n = 0, ptr = buf; n < num; ++n, ptr += elmSize)
        {
            mthing_t           *t = &map->things[n];

            t->pos[VX] = SHORT(*((const int16_t*) ptr));
            t->pos[VY] = SHORT(*((const int16_t*) ptr+2));
            t->angle = SHORT(*((const int16_t*) ptr+4));
            t->type = SHORT(*((const int16_t*) ptr+6));
            t->flags = SHORT(*((const int16_t*) ptr+8));
        }
    }

    return true;
}

static void bufferLump(int lumpNum,
                       byte **buf, size_t *len, size_t *oldLen)
{
    *len = W_LumpLength(lumpNum);

    // Need to enlarge our buffer?
    if(*len > *oldLen)
    {
        *buf = realloc(*buf, *len);
        *oldLen = *len;
    }

    // Buffer the entire lump.
    W_ReadLump(lumpNum, *buf);
}

boolean LoadMap(const int *lumpList, int numLumps)
{
    int                 i;
    byte               *buf = NULL;
    size_t              oldLen = 0;

    // Allocate the data structure arrays.
    map->vertexes = malloc(map->numvertexes * sizeof(mvertex_t));
    map->lines = malloc(map->numlines * sizeof(mline_t));
    map->sides = malloc(map->numsides * sizeof(mside_t));
    map->sectors = malloc(map->numsectors * sizeof(msector_t));
    map->things = malloc(map->numthings * sizeof(mthing_t));

    for(i = 0; i < numLumps; ++i)
    {
        size_t              len;
        lumptype_t          lumpType;

        lumpType = DataTypeForLumpName(W_LumpName(lumpList[i]));

        // Process it, transforming it into our local representation.
        switch(lumpType)
        {
        case ML_VERTEXES:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadVertexes(buf, len);
            break;

        case ML_LINEDEFS:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadLinedefs(buf, len);
            break;

        case ML_SIDEDEFS:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadSidedefs(buf, len);
            break;

        case ML_SECTORS:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadSectors(buf, len);
            break;

        case ML_THINGS:
            bufferLump(lumpList[i], &buf, &len, &oldLen);
            loadThings(buf, len);
            break;

        default:
            break;
        }
    }

    if(buf)
        free(buf);

    return true; // Read and converted successfully.
}

boolean TransferMap(void)
{
    uint                i;

    MPE_Begin(map->name);

    // Create all the data structures.
    for(i = 0; i < map->numvertexes; ++i)
    {
        mvertex_t          *v = &map->vertexes[i];
        MPE_VertexCreate(v->pos[VX], v->pos[VY]);
    }

    for(i = 0; i < map->numsectors; ++i)
    {
        msector_t          *sec = &map->sectors[i];
        MPE_SectorCreate((float) sec->lightlevel / 255.0f, 1, 1, 1,
                         sec->floorHeight,
                         map->materials[sec->floorMaterial]->name,
                         (map->materials[sec->floorMaterial]->isFlat? MAT_FLAT : MAT_TEXTURE),
                         0, 0, 1, 1, 1,
                         sec->ceilHeight,
                         map->materials[sec->ceilMaterial]->name,
                         (map->materials[sec->ceilMaterial]->isFlat? MAT_FLAT : MAT_TEXTURE),
                         0, 0, 1, 1, 1);
    }

    for(i = 0; i < map->numlines; ++i)
    {
        mline_t            *l = &map->lines[i];
        mside_t            *front, *back;
        uint                frontIdx = 0, backIdx = 0;

        front = (l->sides[RIGHT] != 0? &map->sides[l->sides[RIGHT]-1] : NULL);
        if(front)
        {
            frontIdx =
                MPE_SidedefCreate(front->sector, 0,
                                  map->materials[front->topMaterial]->name,
                                  (map->materials[front->topMaterial]->isFlat? MAT_FLAT : MAT_TEXTURE),
                                  front->offset[VX], front->offset[VY], 1, 1, 1,
                                  map->materials[front->middleMaterial]->name,
                                  (map->materials[front->middleMaterial]->isFlat? MAT_FLAT : MAT_TEXTURE),
                                  front->offset[VX], front->offset[VY], 1, 1, 1, 1,
                                  map->materials[front->bottomMaterial]->name,
                                  (map->materials[front->bottomMaterial]->isFlat? MAT_FLAT : MAT_TEXTURE),
                                  front->offset[VX], front->offset[VY], 1, 1, 1);
        }

        back = (l->sides[LEFT] != 0? &map->sides[l->sides[LEFT]-1] : NULL);
        if(back)
        {
            backIdx =
                MPE_SidedefCreate(back->sector, 0,
                                  map->materials[back->topMaterial]->name,
                                  (map->materials[back->topMaterial]->isFlat? MAT_FLAT : MAT_TEXTURE),
                                  back->offset[VX], back->offset[VY], 1, 1, 1,
                                  map->materials[back->middleMaterial]->name,
                                  (map->materials[back->middleMaterial]->isFlat? MAT_FLAT : MAT_TEXTURE),
                                  back->offset[VX], back->offset[VY], 1, 1, 1, 1,
                                  map->materials[back->bottomMaterial]->name,
                                  (map->materials[back->bottomMaterial]->isFlat? MAT_FLAT : MAT_TEXTURE),
                                  back->offset[VX], back->offset[VY], 1, 1, 1);
        }

        MPE_LinedefCreate(l->v[0], l->v[1], frontIdx, backIdx, l->flags,
                          0);
    }

    for(i = 0; i < map->numpolyobjs; ++i)
    {
        mpolyobj_t         *po = map->polyobjs[i];
        uint                j, *lineList;

        lineList = malloc(sizeof(uint) * po->lineCount);
        for(j = 0; j < po->lineCount; ++j)
            lineList[j] = po->lineIndices[j] + 1;
        MPE_PolyobjCreate(lineList, po->lineCount, po->crush, po->tag,
                          po->seqType, po->startSpot[VX], po->startSpot[VY]);
        free(lineList);
    }

    // We've now finished with the original map data.
    freeMapData();

    // Let Doomsday know that we've finished with this map.
    return MPE_End();
}
