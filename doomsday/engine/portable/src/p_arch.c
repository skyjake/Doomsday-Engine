/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * p_arch.c: Doomsday Archived Map (DAM) reader.
 *
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// map data type flags
#define DT_UNSIGNED   0x01
#define DT_FRACBITS   0x02
#define DT_FLAT       0x04
#define DT_TEXTURE    0x08
#define DT_NOINDEX    0x10
#define DT_MSBCONVERT 0x20

// number of map data lumps for a level
#define NUM_MAPLUMPS 12

// well, there is GL_PVIS too but we arn't interested in that
#define NUM_GLLUMPS 5

// Internal data types
#define MAPDATA_FORMATS 2

// GL Node versions
#define GLNODE_FORMATS  5

#define ML_SIDEDEFS     3 // TODO: read sidedefs using the generic data code

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32*FRACUNIT

// internal blockmap stuff
#define BLKSHIFT 7                // places to shift rel position for cell num
#define BLKMASK ((1<<BLKSHIFT)-1) // mask for rel position within cell
#define BLKMARGIN 0               // size guardband around map used

// TYPES -------------------------------------------------------------------

// Common map format properties
enum {
    DAM_UNKNOWN = -2,

    DAM_ALL = -1,
    DAM_NONE,

    // Object/Data types
    DAM_THING,
    DAM_VERTEX,
    DAM_LINE,
    DAM_SIDE,
    DAM_SECTOR,
    DAM_SEG,
    DAM_SUBSECTOR,
    DAM_NODE,
    DAM_MAPBLOCK,
    DAM_SECREJECT,
    DAM_ACSSCRIPT,

    // Object properties
    DAM_X,
    DAM_Y,
    DAM_DX,
    DAM_DY,

    DAM_VERTEX1,
    DAM_VERTEX2,
    DAM_FLAGS,
    DAM_SIDE0,
    DAM_SIDE1,

    DAM_TOP_TEXTURE_OFFSET_X,
    DAM_TOP_TEXTURE_OFFSET_Y,
    DAM_MIDDLE_TEXTURE_OFFSET_X,
    DAM_MIDDLE_TEXTURE_OFFSET_Y,
    DAM_BOTTOM_TEXTURE_OFFSET_X,
    DAM_BOTTOM_TEXTURE_OFFSET_Y,
    DAM_TOP_TEXTURE,
    DAM_MIDDLE_TEXTURE,
    DAM_BOTTOM_TEXTURE,
    DAM_FRONT_SECTOR,

    DAM_FLOOR_HEIGHT,
    DAM_FLOOR_TEXTURE,
    DAM_CEILING_HEIGHT,
    DAM_CEILING_TEXTURE,
    DAM_LIGHT_LEVEL,

    DAM_ANGLE,
    DAM_OFFSET,

    DAM_LINE_COUNT,
    DAM_LINE_FIRST,

    DAM_BBOX_RIGHT_TOP_Y,
    DAM_BBOX_RIGHT_LOW_Y,
    DAM_BBOX_RIGHT_LOW_X,
    DAM_BBOX_RIGHT_TOP_X,
    DAM_BBOX_LEFT_TOP_Y,
    DAM_BBOX_LEFT_LOW_Y,
    DAM_BBOX_LEFT_LOW_X,
    DAM_BBOX_LEFT_TOP_X,
    DAM_CHILD_RIGHT,
    DAM_CHILD_LEFT
};

// Game specific map format properties
// TODO: These should be registered by the game during preinit.
enum {
    DAM_LINE_TAG,
    DAM_LINE_SPECIAL,
    DAM_LINE_ARG1,
    DAM_LINE_ARG2,
    DAM_LINE_ARG3,
    DAM_LINE_ARG4,
    DAM_LINE_ARG5,
    DAM_SECTOR_SPECIAL,
    DAM_SECTOR_TAG,
    DAM_THING_TID,
    DAM_THING_X,
    DAM_THING_Y,
    DAM_THING_HEIGHT,
    DAM_THING_ANGLE,
    DAM_THING_TYPE,
    DAM_THING_OPTIONS,
    DAM_THING_SPECIAL,
    DAM_THING_ARG1,
    DAM_THING_ARG2,
    DAM_THING_ARG3,
    DAM_THING_ARG4,
    DAM_THING_ARG5,
    DAM_PROPERTY_COUNT
};

enum {
    LCM_LABEL,
    LCM_THINGS,
    LCM_LINEDEFS,
    LCM_SIDEDEFS,
    LCM_VERTEXES,
    LCM_SEGS,
    LCM_SUBSECTORS,
    LCM_NODES,
    LCM_SECTORS,
    LCM_REJECT,
    LCM_BLOCKMAP,
    LCM_BEHAVIOR,
    LCG_LABEL,
    LCG_VERTEXES,
    LCG_SEGS,
    LCG_SUBSECTORS,
    LCG_NODES,
    NUM_LUMPCLASSES
};

enum {
    NO,        // Not required.
    BSPBUILD,  // If we can build nodes we don't require it.
    YES        // MUST be present
};

typedef struct {
    char       *level;
    char       *builder;
    char       *time;
    char       *checksum;
} glbuildinfo_t;

// used to list lines in each block
typedef struct linelist_t {
    long        num;
    struct linelist_t *next;
} linelist_t;

// TODO: we still use this struct for texture byte offsets
typedef struct {
    short       textureoffset;
    short       rowoffset;
    char        toptexture[8];
    char        bottomtexture[8];
    char        midtexture[8];
    // Front sector, towards viewer.
    short       sector;
} mapsidedef_t;

typedef struct gamemap_s {
    char        levelid[9];
    uint        numvertexes;
    vertex_t   *vertexes;

    uint        numsegs;
    seg_t      *segs;

    uint        numsectors;
    sector_t   *sectors;

    uint        numsubsectors;
    subsector_t *subsectors;

    uint        numnodes;
    node_t     *nodes;

    uint        numlines;
    line_t     *lines;

    uint        numsides;
    side_t     *sides;

    uint        po_NumPolyobjs;
    polyobj_t  *polyobjs;

    int         numthings;

    long       *blockmaplump;           // offsets in blockmap are from here
    long       *blockmap;

    uint        bmapwidth, bmapheight;  // in mapblocks
    fixed_t     bmaporgx, bmaporgy;     // origin of block map
    linkmobj_t *blockrings;             // for thing rings

    byte       *rejectmatrix;
} gamemap_t;

//
// Types used in map data handling
//
typedef struct {
    int         id; // DAM property ID to map the data to
    boolean     gameprop; // if true is a game-specific property (passed to the game)
    int         flags;
    int         size;   // num of bytes
    int         offset;
} datatype_t;

typedef struct mapdatalumpformat_s {
    int         version;
    char       *magicid;
    boolean     isText; // True if the lump is a plain text lump.
    size_t      elmSize;
    uint        numProps;
    datatype_t *props;
} mapdatalumpformat_t;

typedef struct mapdataformat_s {
    char       *vername;
    mapdatalumpformat_t verInfo[NUM_MAPLUMPS];
    boolean     supported;
} mapdataformat_t;

typedef struct glnodeformat_s {
    char       *vername;
    mapdatalumpformat_t verInfo[NUM_GLLUMPS];
    boolean     supported;
} glnodeformat_t;

typedef struct {
    char       *lumpname;
    int         mdLump;
    int         glLump;
    int         dataType;
    int         lumpclass;
    int         required;
    boolean precache;
} maplumpinfo_t;

typedef struct mapdatalumpinfo_s {
    int         lumpNum;
    byte       *lumpp;      // ptr to the lump data
    mapdatalumpformat_t *format;
    int         lumpClass;
    int         startOffset;
    uint        elements;
    size_t      length;
} mapdatalumpinfo_t;

typedef struct mapdatalumpnode_s {
    mapdatalumpinfo_t *data;
    struct mapdatalumpnode_s *next;
} mapdatalumpnode_t;

typedef struct {
    size_t      elmsize;
    uint        elements;
    datatype_t **props;
    gamemap_t  *map;
} damargs_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

static void P_CreateBlockMap(gamemap_t* map);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean  ReadMapData(gamemap_t* map, int doClass, int *props,
                            uint numProps);
static boolean  DetermineMapDataFormat(void);

static int P_CallbackEX(int dataType, unsigned int startIndex, const byte *buffer,
                        void* context,
                        int (*callback)(gamemap_t *map, int dataType, void* ptr,
                                uint elmIdx, const datatype_t* prop,
                                const byte *buffer));

static int ReadMapProperty(gamemap_t *map, int dataType, void* ptr, uint elmIdx, 
                            const datatype_t* prop, const byte *buffer);

static void     finishLineDefs(gamemap_t *map);
static void     processSegs(gamemap_t *map);

static boolean  P_LoadReject(gamemap_t *map, mapdatalumpinfo_t* maplump);
static boolean  P_LoadBlockMap(gamemap_t *map, mapdatalumpinfo_t* maplump);
static void     finalizeMapData(gamemap_t *map);

static void     allocateMapData(gamemap_t *map);
static void     countMapElements(gamemap_t *map);

#if _DEBUG
//static void     printDebugMapData(gamemap_t* map);
#endif

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// BSP cvars.
int bspBuild = true;
static int bspCache = true;
static int bspFactor = 7;

// Should we generate new blockmap data if its invalid?
// 0: error out
// 1: generate new
// 2: Always generate new
int         createBMap = 1;

// Should we generate new reject data if its invalid?
// 0: error out
// 1: generate new
// 2: Always generate new
int         createReject = 1;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static mapdatalumpnode_t *mapDataLumps;
static uint numMapDataLumps;

static glbuildinfo_t *glBuilderInfo;

static gamemap_t* currentMap = NULL;
static uint    mapFormat;
static uint    glNodeFormat;
static uint    firstGLvertex = 0;

// Set to true if glNodeData exists for the level.
static boolean glNodeData = false;

// types of MAP data structure
// These arrays are temporary. Some of the data will be provided via DED definitions.
static maplumpinfo_t mapLumpInfo[] = {
//   lumpname    MD  GL  datatype      lumpclass     required?  precache?
    {NULL,       0, -1,  DAM_UNKNOWN,   LCM_LABEL,      NO,    false},
    {"THINGS",   1, -1,  DAM_THING,     LCM_THINGS,     YES,   false},
    {"LINEDEFS", 2, -1,  DAM_LINE,      LCM_LINEDEFS,   YES,   false},
    {"SIDEDEFS", 3, -1,  DAM_SIDE,      LCM_SIDEDEFS,   YES,   false},
    {"VERTEXES", 4, -1,  DAM_VERTEX,    LCM_VERTEXES,   YES,   false},
    {"SEGS",     5, -1,  DAM_SEG,       LCM_SEGS,       BSPBUILD, false},
    {"SSECTORS", 6, -1,  DAM_SUBSECTOR, LCM_SUBSECTORS, BSPBUILD, false},
    {"NODES",    7, -1,  DAM_NODE,      LCM_NODES,      BSPBUILD, false},
    {"SECTORS",  8, -1,  DAM_SECTOR,    LCM_SECTORS,    YES,   false},
    {"REJECT",   9, -1,  DAM_SECREJECT, LCM_REJECT,     NO,    false},
    {"BLOCKMAP", 10,-1,  DAM_MAPBLOCK,  LCM_BLOCKMAP,   NO,    false},
    {"BEHAVIOR", 11,-1,  DAM_ACSSCRIPT, LCM_BEHAVIOR,   NO,    false},
    {NULL,       -1, 0,  DAM_UNKNOWN,   LCG_LABEL,      NO,    false},
    {"GL_VERT",  -1, 1,  DAM_VERTEX,    LCG_VERTEXES,   NO,    false},
    {"GL_SEGS",  -1, 2,  DAM_SEG,       LCG_SEGS,       NO,    false},
    {"GL_SSECT", -1, 3,  DAM_SUBSECTOR, LCG_SUBSECTORS, NO,    false},
    {"GL_NODES", -1, 4,  DAM_NODE,      LCG_NODES,      NO,    false},
    {NULL}
};

// Versions of map data structures
static mapdataformat_t mapDataFormats[] = {
    {"DOOM", {{1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL},
              {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {-1, NULL, true}}, true},
    {"HEXEN",{{1, NULL}, {2, NULL}, {2, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL},
              {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL}, {1, NULL, true}}, true},
    {NULL}
};

// Versions of GL node data structures
static glnodeformat_t glNodeFormats[] = {
// Ver String   Label       Verts       Segs        SSects       Nodes     Supported?
    {"V1", {{1, NULL, true},   {1, NULL},   {2, NULL},   {1, NULL},   {1, NULL}}, true},
    {"V2", {{1, NULL, true},   {2, "gNd2"}, {2, NULL},   {1, NULL},   {1, NULL}}, true},
    {"V3", {{1, NULL, true},   {2, "gNd2"}, {3, "gNd3"}, {3, "gNd3"}, {1, NULL}}, false},
    {"V4", {{1, NULL, true},   {4, "gNd4"}, {4, NULL},   {4, NULL},   {4, NULL}}, false},
    {"V5", {{1, NULL, true},   {5, "gNd5"}, {5, NULL},   {3, NULL},   {4, NULL}}, true},
    {NULL}
};

// CODE --------------------------------------------------------------------

void DAM_Register(void)
{
    C_VAR_INT("blockmap-build", &createBMap, 0, 0, 2);
    C_VAR_INT("bsp-build", &bspBuild, 0, 0, 1);
    // FIXME: bsp-cache and bsp-factor are not yet implemented.
    C_VAR_INT("bsp-cache", &bspCache, 0, 0, 1);
    C_VAR_INT("bsp-factor", &bspFactor, CVF_NO_MAX, 0, 0);
    C_VAR_INT("reject-build", &createReject, 0, 0, 2);
}

/*
 * Convert enum constant into a string for error/debug messages.
 */
const char* DAM_Str(int prop)
{
    static char propStr[40];
    struct prop_s {
        int prop;
        const char* str;
    } props[] =
    {
        { DAM_UNKNOWN, "(unknown)" },
        { DAM_ALL, "DAM_ALL" },
        { 0, "(invalid)" },
        { DAM_THING, "DAM_THING" },
        { DAM_VERTEX, "DAM_VERTEX" },
        { DAM_LINE, "DAM_LINE" },
        { DAM_SIDE, "DAM_SIDE" },
        { DAM_SECTOR, "DAM_SECTOR" },
        { DAM_SEG, "DAM_SEG" },
        { DAM_SUBSECTOR, "DAM_SUBSECTOR" },
        { DAM_NODE, "DAM_NODE" },
        { DAM_MAPBLOCK, "DAM_MAPBLOCK" },
        { DAM_SECREJECT, "DAM_SECREJECT" },
        { DAM_ACSSCRIPT, "DAM_ACSSCRIPT" },
        { DAM_X, "DAM_X" },
        { DAM_Y, "DAM_Y" },
        { DAM_DX, "DAM_DX" },
        { DAM_DY, "DAM_DY" },
        { DAM_VERTEX1, "DAM_VERTEX1" },
        { DAM_VERTEX2, "DAM_VERTEX2" },
        { DAM_FLAGS, "DAM_FLAGS" },
        { DAM_SIDE0, "DAM_SIDE0" },
        { DAM_SIDE1, "DAM_SIDE1" },
        { DAM_TOP_TEXTURE_OFFSET_X, "DAM_TOP_TEXTURE_OFFSET_X" },
        { DAM_TOP_TEXTURE_OFFSET_Y, "DAM_TOP_TEXTURE_OFFSET_Y" },
        { DAM_MIDDLE_TEXTURE_OFFSET_X, "DAM_MIDDLE_TEXTURE_OFFSET_X" },
        { DAM_MIDDLE_TEXTURE_OFFSET_Y, "DAM_MIDDLE_TEXTURE_OFFSET_Y" },
        { DAM_BOTTOM_TEXTURE_OFFSET_X, "DAM_BOTTOM_TEXTURE_OFFSET_X" },
        { DAM_BOTTOM_TEXTURE_OFFSET_Y, "DAM_BOTTOM_TEXTURE_OFFSET_Y" },
        { DAM_TOP_TEXTURE, "DAM_TOP_TEXTURE" },
        { DAM_MIDDLE_TEXTURE, "DAM_MIDDLE_TEXTURE" },
        { DAM_BOTTOM_TEXTURE, "DAM_BOTTOM_TEXTURE" },
        { DAM_FRONT_SECTOR, "DAM_FRONT_SECTOR" },
        { DAM_FLOOR_HEIGHT, "DAM_FLOOR_HEIGHT" },
        { DAM_FLOOR_TEXTURE, "DAM_FLOOR_TEXTURE" },
        { DAM_CEILING_HEIGHT, "DAM_CEILING_HEIGHT" },
        { DAM_CEILING_TEXTURE, "DAM_CEILING_TEXTURE" },
        { DAM_LIGHT_LEVEL, "DAM_LIGHT_LEVEL" },
        { DAM_ANGLE, "DAM_ANGLE" },
        { DAM_OFFSET, "DAM_OFFSET" },
        { DAM_LINE_COUNT, "DAM_LINE_COUNT" },
        { DAM_LINE_FIRST, "DAM_LINE_FIRST" },
        { DAM_BBOX_RIGHT_TOP_Y, "DAM_BBOX_RIGHT_TOP_Y" },
        { DAM_BBOX_RIGHT_LOW_Y, "DAM_BBOX_RIGHT_LOW_Y" },
        { DAM_BBOX_RIGHT_LOW_X, "DAM_BBOX_RIGHT_LOW_X" },
        { DAM_BBOX_RIGHT_TOP_X, "DAM_BBOX_RIGHT_TOP_X" },
        { DAM_BBOX_LEFT_TOP_Y, "DAM_BBOX_LEFT_TOP_Y" },
        { DAM_BBOX_LEFT_LOW_Y, "DAM_BBOX_LEFT_LOW_Y" },
        { DAM_BBOX_LEFT_LOW_X, "DAM_BBOX_LEFT_LOW_X" },
        { DAM_BBOX_LEFT_TOP_X, "DAM_BBOX_LEFT_TOP_X" },
        { DAM_CHILD_RIGHT, "DAM_CHILD_RIGHT" },
        { DAM_CHILD_LEFT, "DAM_CHILD_LEFT" },
        { 0, NULL }
    };
    int i;

    for(i = 0; props[i].str; ++i)
        if(props[i].prop == prop)
            return props[i].str;

    sprintf(propStr, "(unnamed %i)", prop);
    return propStr;
}

static void ParseGLBSPInf(mapdatalumpinfo_t* mapLump)
{
    int i, n, keylength = -1;
    char* ch;
    char line[250];

    glbuildinfo_t *newInfo = M_Malloc(sizeof(glbuildinfo_t));

    struct glbsp_keyword_s {
        const char* label;
        char** data;
    } keywords[] =
    {
        {"LEVEL", &newInfo->level},
        {"BUILDER", &newInfo->builder},
        {"TIME", &newInfo->time},
        {"CHECKSUM", &newInfo->checksum},
        {NULL}
    };

    newInfo->level = NULL;
    newInfo->builder = NULL;
    newInfo->time = NULL;
    newInfo->checksum = NULL;

    // Have we cached the lump yet?
    if(mapLump->lumpp == NULL)
        mapLump->lumpp = (byte *) W_CacheLumpNum(mapLump->lumpNum, PU_STATIC);

    ch = (char *) mapLump->lumpp;
    n = 0;
    for(;;)
    {
        // Read a line
        memset(line, 0, 250);
        for(i = 0; i < 250 - 1; ++n)    // Make the last null stay there.
        {
            if(n == mapLump->length || *ch == '\n')
                break;

            if(*ch == '=')
                keylength = i;

            line[i++] = *ch;
            ch++;
        }

        // Only one keyword per line. Is it known?
        for(i = 0; keywords[i].label; ++i)
            if(!strncmp(line, keywords[i].label, keylength))
            {
                *keywords[i].data = M_Malloc(strlen(line) - keylength + 1);
                strncpy(*keywords[i].data, line + keylength + 1,
                        strlen(line) - keylength);
            }

        n++;

        // End of lump;
        if(n == mapLump->length)
            break;
        ch++;
    }

    glBuilderInfo = newInfo;
}

static void FreeGLBSPInf(void)
{
    // Free the glBuilder info
    if(glBuilderInfo != NULL)
    {
        if(glBuilderInfo->level)
        {
            M_Free(glBuilderInfo->level);
            glBuilderInfo->level = NULL;
        }

        if(glBuilderInfo->builder)
        {
            M_Free(glBuilderInfo->builder);
            glBuilderInfo->builder = NULL;
        }

        if(glBuilderInfo->time)
        {
            M_Free(glBuilderInfo->time);
            glBuilderInfo->time = NULL;
        }

        if(glBuilderInfo->checksum)
        {
            M_Free(glBuilderInfo->checksum);
            glBuilderInfo->checksum = NULL;
        }

        M_Free(glBuilderInfo);
        glBuilderInfo = NULL;
    }
}

static void AddMapDataLump(int lumpNum, int lumpClass)
{
    mapdatalumpinfo_t *mdlumpInf;
    mapdatalumpnode_t *node;

    numMapDataLumps++;

    node = M_Malloc(sizeof(mapdatalumpnode_t));
    node->data = mdlumpInf = M_Malloc(sizeof(mapdatalumpinfo_t));

    mdlumpInf->lumpNum = lumpNum;
    mdlumpInf->lumpClass = lumpClass;
    mdlumpInf->lumpp = NULL;
    mdlumpInf->length = 0;
    mdlumpInf->format = NULL;
    mdlumpInf->startOffset = 0;

    node->next = mapDataLumps;
    mapDataLumps = node;
}

static void FreeMapDataLumps(void)
{
    // Free the map data lump array
    if(mapDataLumps != NULL)
    {
        mapdatalumpnode_t *node, *np;

        // Free any lumps we might have pre-cached.
        node = mapDataLumps;
        while(node)
        {
            np = node->next;
            if(node->data->lumpp)
                Z_Free(node->data->lumpp);

            M_Free(node->data);
            M_Free(node);
            node = np;
        }

        mapDataLumps = NULL;
        numMapDataLumps = 0;
    }
}

/*
 * Locate the lump indices where the data of the specified map
 * resides (both regular and GL Node data).
 *
 * @param levelID       Identifier of the map to be loaded (eg "E1M1").
 * @param lumpIndices   Ptr to the array to write the identifiers back to.
 *
 * @return boolean      (FALSE) If we cannot find the map data.
 */
static boolean P_LocateMapData(char *levelID, int *lumpIndices)
{
    char glLumpName[40];

    // Find map name.
    sprintf(glLumpName, "GL_%s", levelID);
    Con_Message("SetupLevel: %s\n", levelID);

    // Let's see if a plugin is available for loading the data.
    if(!Plug_DoHook(HOOK_LOAD_MAP_LUMPS, W_GetNumForName(levelID),
                    (void*) lumpIndices))
    {
        // The plugin failed.
        lumpIndices[0] = W_CheckNumForName(levelID);

        // FIXME: The latest GLBSP spec supports maps with non-standard
        // identifiers. To support these we must check the lump named
        // GL_LEVEL. In this lump will be a text string which identifies
        // the name of the lump the data is for.
        lumpIndices[1] = W_CheckNumForName(glLumpName);
    }

    if(lumpIndices[0] == -1)
        return false; // The map data cannot be found.

    // Do we have any GL Nodes?
    if(lumpIndices[1] > lumpIndices[0])
        glNodeData = true;
    else
    {
        glNodeData = false;
        glNodeFormat = -1;
    }

    return true;
}

/*
 * Find the lump offsets for this map dataset automatically.
 * Some obscure PWADs have these lumps in a non-standard order... tsk, tsk.
 *
 * @param startLump     The lump number to begin our search with.
 */
static void P_FindMapLumps(int startLump)
{
    unsigned int k;
    unsigned int i;
    boolean scan;
    maplumpinfo_t* mapLmpInf;
    boolean aux = false;

    // Add the marker lump to the list (there might be useful info in it)
    if(!strncmp(W_CacheLumpNum(startLump, PU_GETNAME), "GL_", 3))
    {
        AddMapDataLump(startLump, LCG_LABEL);
        // FIXME: This isn't right.
        aux = true; // We'll be checking the auxilary lump cache
    }
    else
        AddMapDataLump(startLump, LCM_LABEL);

    ++startLump;
    // Keep checking lumps to see if its a map data lump.
    for(i = (unsigned) startLump; ; ++i)
    {
        if(!aux && i > (unsigned) numlumps - 1) // No more lumps?
            break;

        scan = true;
        // Compare the name of this lump with our known map data lump names
        mapLmpInf = mapLumpInfo;
        for(k = NUM_LUMPCLASSES; k-- && scan; ++mapLmpInf)
        {
            if(mapLmpInf->lumpname)
            if(!strncmp(mapLmpInf->lumpname, W_CacheLumpNum(i, PU_GETNAME), 8))
            {
                // Lump name matches a known lump name.
                // Add it to the lumps we'll process for map data.
                AddMapDataLump(i, mapLmpInf->lumpclass);
                scan = false;
            }
        }
        // We didn't find a match for this name?
        if(scan)
            break; // Stop looking, we've found them all.
    }
}

/*
 * Attempt to determine the format of this map data lump.
 *
 * This is done by checking the start bytes of this lump (the lump "header")
 * to see if its of a known special format. Special formats include Doomsday
 * custom map data formats and GL Node formats.
 *
 * @param mapLump   Ptr to the map lump struct to work with.
 */
static void DetermineMapDataLumpFormat(mapdatalumpinfo_t* mapLump)
{
    byte lumpHeader[4];

    W_ReadLumpSection(mapLump->lumpNum, lumpHeader, 0, 4);

    // Check to see if this a Doomsday, custom map data - lump format.
    if(memcmp(lumpHeader, "DDAY", 4) == 0)
    {
        // It IS a custom Doomsday format.

        // TODO:
        // Determine the "named" format to use when processing this lump.

        // Imediatetly after "DDAY" is a block of text with various info
        // about this lump. This text block begins with "[" and ends at "]".
        // TODO: Decide on specifics for this text block.
        // (a simple name=value pair delimited by " " should suffice?)

        // Search this string for known keywords (eg the name of the format).

        // Store the TOTAL number of bytes (including the magic bytes "DDAY")
        // that the header uses, into the startOffset (the offset into the
        // byte stream where the data starts) for this lump.

        // Once we know the name of the format, the lump length and the Offset
        // we can check to make sure to the lump format definition is correct
        // for this lump thus:

        // sum = (lumplength - startOffset) / (number of bytes per element)
        // If sum is not a whole integer then something is wrong with either
        // the lump data or the lump format definition.
        return;
    }
    else if(glNodeData &&
            mapLump->lumpClass >= LCG_VERTEXES &&
            mapLump->lumpClass <= LCG_NODES)
    {
        unsigned int i;
        int lumpClass = mapLumpInfo[mapLump->lumpClass].glLump;
        mapdatalumpformat_t* mapDataLumpFormat;
        glnodeformat_t* nodeFormat = glNodeFormats;

        // Perhaps its a "named" GL Node format?

        // Find out which gl node version the data uses
        // loop backwards (check for latest version first)
        for(i = GLNODE_FORMATS; i--; ++nodeFormat)
        {
            mapDataLumpFormat = &(nodeFormat->verInfo[lumpClass]);

            // Check the header against each known name for this lump class.
            if(mapDataLumpFormat->magicid != NULL)
            {
                if(memcmp(lumpHeader, mapDataLumpFormat->magicid, 4) == 0)
                {
                    // Aha! It IS a "named" format.
                    // Record the version number.
                    mapLump->format = mapDataLumpFormat;

                    // Set the start offset into byte stream.
                    mapLump->startOffset = 4; // num of bytes

                    return;
                }
            }
        }

        // It's not a named format.
        // Most GL Node formats don't include magic bytes in each lump.
        // Because we don't KNOW the format of this lump we should
        // ignore it when determining the GL Node format.
        return;
    }
    else if(mapLump->lumpClass == LCG_LABEL)
    {
        // It's a GL NODE identifier lump.
        // Perhaps it can tell us something useful about this map data?
        // It is a text lump that contains a simple label=value pair list.
        if(mapLump->length > 0)
            ParseGLBSPInf(mapLump);
    }

    // It isn't a (known) named special format.
    // Use the default data format for this lump (map format specific).
}

/*
 * Make sure we have (at least) one lump of each lump class that we require.
 *
 * During the process we will attempt to determine the format of an individual
 * map data lump and record various info about the lumps that will be of use
 * further down the line.
 *
 * @param   levelID     The map identifer string, used only in error messages.
 *
 * @return  boolean     (True) If the map data provides us with enough data
 *                             to proceed with loading the map.
 */
static boolean verifyMapData(char *levelID)
{
    uint        i;
    boolean     found, required;
    mapdatalumpnode_t *node;
    maplumpinfo_t *mapLmpInf = mapLumpInfo;

    FreeGLBSPInf();

    // Itterate our known lump classes array.
    for(i = NUM_LUMPCLASSES; i--; ++mapLmpInf)
    {
        // Check all the registered map data lumps to make sure we have at least
        // one lump of each required lump class.
        found = false;
        node = mapDataLumps;
        while(node)
        {
            mapdatalumpinfo_t *mapDataLump = node->data;

            // Is this a lump of the class we are looking for?
            if(mapDataLump->lumpClass == mapLmpInf->lumpclass)
            {
                // Store the lump length.
                mapDataLump->length = W_LumpLength(mapDataLump->lumpNum);

                // If this is a BEHAVIOR lump, then this MUST be a HEXEN format map.
                if(mapDataLump->lumpClass == LCM_BEHAVIOR)
                    mapFormat = 1;

                // Are we precaching lumps of this class?
                if(mapLmpInf->precache && mapDataLump->lumpNum != -1)
                   mapDataLump->lumpp =
                        (byte *) W_CacheLumpNum(mapDataLump->lumpNum, PU_STATIC);

                // Attempt to determine the format of this map data lump.
                DetermineMapDataLumpFormat(mapDataLump);

                // Announce
                VERBOSE2(Con_Message("%s - %s is %d bytes.\n",
                                     W_CacheLumpNum(mapDataLump->lumpNum, PU_GETNAME),
                                     DAM_Str(mapLmpInf->dataType), mapDataLump->length));

                // We've found (at least) one lump of this class.
                found = true;
            }

            node = node->next;
        }

        // We arn't interested in indetifier lumps
        if(mapLmpInf->lumpclass == LCM_LABEL ||
           mapLmpInf->lumpclass == LCG_LABEL)
           continue;

        // We didn't find any lumps of this class?
        if(!found)
        {
            // Is it a required lump class?
            //   Is this lump that will be generated if a BSP builder is available?
            if(mapLmpInf->required == BSPBUILD &&
               Plug_CheckForHook(HOOK_LOAD_MAP_LUMPS) && bspBuild)
                required = false;
            else if(mapLmpInf->required)
                required = true;
            else
                required = false;

            if(required)
            {
                // Darn, the map data is incomplete. We arn't able to to load this map :`(
                // Inform the user.
                Con_Message("verifyMapData: %s for \"%s\" could not be found.\n" \
                            " This lump is required in order to play this map.\n",
                            mapLmpInf->lumpname, levelID);
                return false;
            }
            else
            {
                // It's not required (we can generate it/we don't need it)
                Con_Message("verifyMapData: %s for \"%s\" could not be found.\n" \
                            "Useable data will be generated automatically if needed.\n",
                            mapLmpInf->lumpname, levelID);
                // Add a dummy lump to the list
                AddMapDataLump(-1, mapLmpInf->lumpclass);
            }
        }
    }

    // All is well, we can attempt to determine the map format.
    return true;
}

/*
 * Determines the format of the map by comparing the (already determined)
 * lump formats against the known map formats.
 *
 * Map data lumps can be in any mixed format but GL Node data cannot so
 * we only check those atm.
 *
 * @return boolean     (True) If its a format we support.
 */
static boolean DetermineMapDataFormat(void)
{
    int         lumpClass;
    mapdatalumpnode_t *node;

    // Now that we know the data format of the lumps we need to update the
    // internal version number for any lumps that don't declare a version (-1).
    // Taken from the version stipulated in the map format.
    node = mapDataLumps;
    while(node)
    {
        mapdatalumpinfo_t *mapLump = node->data;

        // Is it a map data lump class?
        lumpClass = mapLumpInfo[mapLump->lumpClass].mdLump;
        if(mapLump->lumpClass >= LCM_THINGS &&
           mapLump->lumpClass <= LCM_BEHAVIOR)
        {
            // Set the lump version number for this format.
            if(mapLump->format == NULL)
                mapLump->format = &mapDataFormats[mapFormat].verInfo[lumpClass];
        }
        node = node->next;
    }

    // Do we have GL nodes?
    if(glNodeData)
    {
        uint        i;
        boolean     failed;
        glnodeformat_t* nodeFormat = &glNodeFormats[GLNODE_FORMATS];

        // Find out which GL Node version the data is in.
        // Loop backwards (check for latest version first).
        for(i = GLNODE_FORMATS; i--; --nodeFormat)
        {
            // Check the version number of each map data lump
            failed = false;
            node = mapDataLumps;
            while(node && !failed)
            {
                mapdatalumpinfo_t *mapLump = node->data;

                // Is it a GL Node data lump class?
                if(mapLump->lumpClass >= LCG_VERTEXES &&
                   mapLump->lumpClass <= LCG_NODES)
                {
                    lumpClass = mapLumpInfo[mapLump->lumpClass].glLump;

                    // SHOULD this lump format declare a version (magic bytes)?
                    if(mapLump->format == NULL)
                    {
                        if(nodeFormat->verInfo[lumpClass].magicid != NULL)
                            failed = true;
                    }
                    else
                    {
                        // Compare the versions.
                        if(mapLump->format->version != nodeFormat->verInfo[lumpClass].version)
                            failed = true;
                    }
                }

                node = node->next;
            }

            // Did all lumps match the required format for this version?
            if(!failed)
            {
                // We know the GL Node format
                glNodeFormat = i;

                Con_Message("DetermineMapDataFormat: (%s GL Node Data)\n",
                            nodeFormat->vername);

                // Did we find any glbuild info?
                if(glBuilderInfo)
                {
                    Con_Message("(");
                    if(glBuilderInfo->level)
                        Con_Message("%s | ", glBuilderInfo->level);

                    if(glBuilderInfo->builder)
                        Con_Message("%s | ", glBuilderInfo->builder);

                    if(glBuilderInfo->time)
                        Con_Message("%s | ", glBuilderInfo->time);

                    if(glBuilderInfo->checksum)
                        Con_Message("%s", glBuilderInfo->checksum);
                    Con_Message(")\n");
                }

                // Do we support this GL Node format?
                if(nodeFormat->supported)
                {
                    // Now that we know the GL Node format we need to update the internal
                    // version number for any lumps that don't declare a version (-1).
                    // Taken from the version stipulated in the Node format.
                    node = mapDataLumps;
                    while(node)
                    {
                        mapdatalumpinfo_t *mapLump = node->data;

                        // Is it a GL Node data lump class?
                        lumpClass = mapLumpInfo[mapLump->lumpClass].glLump;
                        if(mapLump->lumpClass >= LCG_VERTEXES &&
                           mapLump->lumpClass <= LCG_NODES)
                        {
                            // Set the lump version number for this format.
                            if(mapLump->format == NULL)
                                mapLump->format = &nodeFormat->verInfo[lumpClass];
                        }
                        node = node->next;
                    }
                    return true;
                }
                else
                {
                    // Unsupported GL Node format.
                    Con_Message("DetermineMapDataFormat: Sorry, %s GL Nodes arn't supported\n",
                                nodeFormat->vername);
                    return false;
                }
            }
        }
        Con_Message("DetermineMapDataFormat: Could not determine GL Node format\n");
        return false;
    }

    // We support this map data format.
    return true;
}

/*
 * Validate the map data before loading the level.
 * This entails:
 *
 * 1) Check the size of the REJECT data (we can generate if invalid).
 * 2) Check the size of the BLOCKMAP data (we can generate if invalid).
 */
static boolean validateMapData(void)
{
    uint        count[NUM_LUMPCLASSES];
    mapdatalumpnode_t *node;
    mapdatalumpinfo_t *mapLump;

    memset(count, 0, sizeof(uint)*NUM_LUMPCLASSES);

    node = mapDataLumps;
    while(node)
    {
        mapLump = node->data;

        // How many elements are in the lump?
        // Add the number of elements to the potential count for this class.
        if(mapLump->lumpNum != -1 && mapLump->format && !mapLump->format->isText)
        {
            // How many elements are in the lump?
            mapLump->elements = (mapLump->length - mapLump->startOffset) /
                                     mapLump->format->elmSize;

            count[mapLump->lumpClass] += mapLump->elements;
        }
        node = node->next;
    }

    // Now confirm the data is valid.
    node = mapDataLumps;
    while(node)
    {
        mapLump = node->data;

        // Is the REJECT complete?
        if(mapLump->lumpClass == LCM_REJECT)
        {   // Check the length of the lump
            size_t requiredLength = (((count[LCM_SECTORS]*count[LCM_SECTORS]) + 7) & ~7) /8;

            if(mapLump->length < requiredLength)
            {
                Con_Message("validateMapData: REJECT data is invalid.\n");

                // Are we allowed to generate new reject data?
                if(createReject == 0)
                {
                    Con_Message("validateMapData: Map has invalid REJECT resource.\n"
                                "You can circumvent this error by allowing Doomsday to\n"
                                "generate this resource when needed by setting the CVAR:\n"
                                "reject-build 1\n");

                    return false;
                }
                else
                    // Set the lump number to -1 so we generate it ourselves.
                    mapLump->lumpNum = -1;
            }
        }
        // Is the BLOCKMAP complete?
        else if(mapLump->lumpClass == LCM_BLOCKMAP)
        {
            uint count = mapLump->length / 2;

            // Is there valid BLOCKMAP data?
            if(count >= 0x10000)
            {
                Con_Message("validateMapData: Map exceeds limits of +/- 32767 map units.\n");

                // Are we allowed to generate new blockmap data?
                if(createBMap == 0)
                {
                    Con_Message("validateMapData: Map has invalid BLOCKMAP resource.\n"
                                "You can circumvent this error by allowing Doomsday to\n"
                                "generate this resource when needed by setting the CVAR:\n"
                                "blockmap-build 1");
                    return false;
                }
                else
                    // Set the lump number to -1 so we generate it ourselves.
                    mapLump->lumpNum = -1;
            }
        }

        node = node->next;
    }

    return true;
}

boolean P_GetMapFormat(void)
{
    if(DetermineMapDataFormat())
    {
        // We support the map data format.
        // Validate the map data.
        return validateMapData();
    }
    else
    {
        // Darn, we can't load this map.
        // Free any lumps we may have already precached in the process.
        FreeMapDataLumps();
        FreeGLBSPInf();
        return false;
    }
}

/**
 * Return <code>true</code> if gl node data is present for the CURRENT map.
 */
boolean P_GLNodeDataPresent(void)
{
    return glNodeData;
}

static boolean P_ReadMapData(gamemap_t* map, int doClass, int *props, uint numProps)
{
    // Cant load GL NODE data if we don't have it.
    if(!glNodeData && (doClass >= LCG_VERTEXES && doClass <= LCG_NODES))
        // Not having the data is considered a success.
        // This is due to us invoking the dpMapLoader plugin at an awkward
        // point in the map loading process (at the start).
        return true;

    if(!ReadMapData(map, doClass, props, numProps))
    {
        FreeMapDataLumps();
        FreeGLBSPInf();
        return false;
    }

    return true;
}

static void SetCurrentMap(gamemap_t* map)
{
    strncpy(levelid, map->levelid, sizeof(levelid));
    numvertexes = map->numvertexes;
    vertexes = map->vertexes;

    numsegs = map->numsegs;
    segs = map->segs;

    numsectors = map->numsectors;
    sectors = map->sectors;

    numsubsectors = map->numsubsectors;
    subsectors = map->subsectors;

    numnodes = map->numnodes;
    nodes = map->nodes;

    numlines = map->numlines;
    lines = map->lines;

    numsides = map->numsides;
    sides = map->sides;

    po_NumPolyobjs = map->po_NumPolyobjs;
    polyobjs = map->polyobjs;

    numthings = map->numthings;

    blockmaplump = map->blockmaplump;
    blockmap = map->blockmap;

    bmapwidth = map->bmapwidth;
    bmapheight = map->bmapheight;
    bmaporgx = map->bmaporgx;
    bmaporgy = map->bmaporgy;
    blockrings = map->blockrings;

    rejectmatrix = map->rejectmatrix;

    currentMap = map;
}

/**
 * Attempts to load the data structures for a map.
 *
 * @param levelId   Identifier of the map to be loaded (eg "E1M1").
 *
 * @return          <code>true</code> if the map was loaded successfully.
 */
boolean P_AttemptMapLoad(char *levelId)
{
    int         lumpNumbers[2];
    gamemap_t  *newmap;

    mapDataLumps = NULL;
    numMapDataLumps = 0;

    // We'll assume we're loading a DOOM format map to begin with.
    mapFormat = 0;

    // Attempt to find the map data for this level
    if(!P_LocateMapData(levelId, lumpNumbers))
    {
        // Well that was a non-starter...
        return false;
    }

    // Find the actual map data lumps and their offsets.
    // Add them to the list of lumps to be processed.
    P_FindMapLumps(lumpNumbers[0]);

    // If we have GL Node data, find those lumps too.
    if(glNodeData)
        P_FindMapLumps(lumpNumbers[1]);

    // Make sure we have all the data we need to load this level.
    if(!verifyMapData(levelId))
    {
        // Darn, the level data is incomplete.
        // Free any lumps we may have already precached in the process.
        FreeMapDataLumps();
        FreeGLBSPInf();

        // Sorry, but we can't continue.
        return false;
    }

    // Looking good so far.
    // Try to determine the format of this map.
    if(P_GetMapFormat())
    {
        newmap = M_Malloc(sizeof(gamemap_t));
        // Excellent, its a map we can read. Load it in!
        Con_Message("P_AttemptMapLoad: %s\n", levelId);

        if(glNodeData && !bspBuild)
            Con_Message(" : Ignoring GL Nodes\n");

        // Initialize the new map.
        strncpy(newmap->levelid, levelId, sizeof(newmap->levelid));

        countMapElements(newmap);
        allocateMapData(newmap);

        // Load all lumps of each class in this order.
        //
        // NOTE:
        // DJS 01/10/05 - revised load order to allow for cross-referencing
        //                data during loading (detect + fix trivial errors).
        {
        // Vertexes and GL vertexes (read all properties).
        int props[] = {
            DAM_X,
            DAM_Y};
        if(!P_ReadMapData(newmap, LCM_VERTEXES, &(*props), 2))
            return false;
        if(!P_ReadMapData(newmap, LCG_VERTEXES, &(*props), 2))
            return false;
        }
        {
        // Sectors (read all properties).
        int props[] = {
            DAM_FLOOR_HEIGHT,
            DAM_CEILING_HEIGHT,
            DAM_FLOOR_TEXTURE,
            DAM_CEILING_TEXTURE,
            DAM_LIGHT_LEVEL,
            // Game-specific properties:
            DAM_SECTOR_SPECIAL,
            DAM_SECTOR_TAG};
        if(!P_ReadMapData(newmap, LCM_SECTORS, &(*props), 7))
            return false;
        }
        {
        // Sidedefs (read all properties except textures).
        int props[] = {
            DAM_TOP_TEXTURE_OFFSET_X,
            DAM_TOP_TEXTURE_OFFSET_Y,
            DAM_MIDDLE_TEXTURE_OFFSET_X,
            DAM_MIDDLE_TEXTURE_OFFSET_Y,
            DAM_BOTTOM_TEXTURE_OFFSET_X,
            DAM_BOTTOM_TEXTURE_OFFSET_Y,
            DAM_FRONT_SECTOR};
        if(!P_ReadMapData(newmap, LCM_SIDEDEFS, &(*props), 7))
            return false;
        }
        {
        // Linedefs (read all properties).
        int props[] = {
            DAM_VERTEX1,
            DAM_VERTEX2,
            DAM_FLAGS,
            DAM_SIDE0,
            DAM_SIDE1,
            // Game-specific properties:
            DAM_LINE_TAG,
            DAM_LINE_SPECIAL,
            DAM_LINE_ARG1,
            DAM_LINE_ARG2,
            DAM_LINE_ARG3,
            DAM_LINE_ARG4,
            DAM_LINE_ARG5};
        if(!P_ReadMapData(newmap, LCM_LINEDEFS, &(*props), 12))
            return false;
        }
        {
        /* Sidedefs (read just textures).
         * MUST be called after Linedefs are loaded.
         *
         * Sidedef texture fields might be overloaded with all kinds of
         * different strings.
         *
         * In BOOM for example, these fields might contain strings that
         * influence what special is assigned to the line. The game will
         * then tell us what texture to use.
         */
        int props[] = {
            DAM_TOP_TEXTURE,
            DAM_MIDDLE_TEXTURE,
            DAM_BOTTOM_TEXTURE};
        if(!P_ReadMapData(newmap, LCM_SIDEDEFS, &(*props), 3))
            return false;
        }

        finishLineDefs(newmap);

        {
        // Things (read all properties).
        int props[] = {
            // Game-specific properties:
            DAM_THING_TID,
            DAM_THING_X,
            DAM_THING_Y,
            DAM_THING_HEIGHT,
            DAM_THING_ANGLE,
            DAM_THING_TYPE,
            DAM_THING_OPTIONS,
            DAM_THING_SPECIAL,
            DAM_THING_ARG1,
            DAM_THING_ARG2,
            DAM_THING_ARG3,
            DAM_THING_ARG4,
            DAM_THING_ARG5};
        if(!P_ReadMapData(newmap, LCM_THINGS, &(*props), 13))
            return false;
        }
        {
        // Segs (read all properties).
        int props[] = {
            DAM_VERTEX1,
            DAM_VERTEX2,
            DAM_ANGLE,
            DAM_LINE,
            DAM_SIDE,
            DAM_OFFSET};
        if(!P_ReadMapData(newmap, LCM_SEGS, &(*props), 6))
            return false;
        }

        processSegs(newmap);

        {
        // Subsectors (read all properties).
        int props[] = {
            DAM_LINE_COUNT,
            DAM_LINE_FIRST};
        if(!P_ReadMapData(newmap, LCM_SUBSECTORS, &(*props), 2))
            return false;
        }
        {
        // Nodes (read all properties).
        int props[] = {
            DAM_X,
            DAM_Y,
            DAM_DX,
            DAM_DY,
            DAM_BBOX_RIGHT_TOP_Y,
            DAM_BBOX_RIGHT_LOW_Y,
            DAM_BBOX_RIGHT_LOW_X,
            DAM_BBOX_RIGHT_TOP_X,
            DAM_BBOX_LEFT_TOP_Y,
            DAM_BBOX_LEFT_LOW_Y,
            DAM_BBOX_LEFT_LOW_X,
            DAM_BBOX_LEFT_TOP_X,
            DAM_CHILD_RIGHT,
            DAM_CHILD_LEFT};
        if(!P_ReadMapData(newmap, LCM_NODES, &(*props), 14))
            return false;
        }

        if(!P_ReadMapData(newmap, LCM_BLOCKMAP, NULL, 0))
            return false;
        if(!P_ReadMapData(newmap, LCM_REJECT, NULL, 0))
            return false;

        // We have complete level data but we're not out of the woods yet...
        FreeMapDataLumps();
        FreeGLBSPInf();

        //printDebugMapData(newmap);

        // Do any initialization/error checking work we need to do.
        // Must be called before we go any further
        finalizeMapData(newmap);

        SetCurrentMap(newmap);
        M_Free(newmap);

        // It's imperative that this is called!
        // - init map links.
        // - necessary GL data generated
        // - sky fix
        // - map info setup
        R_InitLevel(levelId);

        return true;
    }
    else
    {
        // Sorry, but we can't continue.
        return false;
    }
}

static void allocateMapData(gamemap_t *map)
{
    uint        k;

    // Vertexes.
    map->vertexes = Z_Calloc(map->numvertexes * sizeof(vertex_t), PU_LEVEL, 0);
    for(k = 0; k < map->numvertexes; ++k)
    {
        vertex_t   *vtx = &map->vertexes[k];

        vtx->header.type = DMU_VERTEX;
        vtx->numlineowners = 0;
        vtx->lineowners = NULL;
        vtx->anchored = false;
        vtx->numsecowners = 0;
        vtx->secowners = NULL;
    }

    // Linedefs + missing fronts.
    map->lines = Z_Calloc(map->numlines * sizeof(line_t), PU_LEVEL, 0);
    missingFronts = M_Calloc(map->numlines * sizeof(uint));
    for(k = 0; k < map->numlines; ++k)
    {
        line_t     *lin = &map->lines[k];

        lin->header.type = DMU_LINE;
        lin->L_vo1 = NULL;
        lin->L_vo2 = NULL;
        lin->selfrefhackroot = false;
    }

    // Sidedefs.
    map->sides = Z_Calloc(map->numsides * sizeof(side_t), PU_LEVEL, 0);
    for(k = 0; k < map->numsides; ++k)
    {
        side_t     *side = &map->sides[k];

        side->header.type = DMU_SIDE;
        side->SW_topsurface.header.type = DMU_SURFACE;
        side->SW_middlesurface.header.type = DMU_SURFACE;
        side->SW_bottomsurface.header.type = DMU_SURFACE;
        side->SW_topflags = 0;
        side->SW_bottomflags = 0;
        side->SW_middleflags = 0;
        memset(side->SW_toprgba, 0xff, 3);
        memset(side->SW_middlergba, 0xff, 4);
        memset(side->SW_bottomrgba, 0xff, 3);
        side->blendmode = BM_NORMAL;
        side->SW_topsurface.isflat = side->SW_topsurface.oldisflat = false;
        side->SW_middlesurface.isflat = side->SW_middlesurface.oldisflat = false;
        side->SW_bottomsurface.isflat = side->SW_bottomsurface.oldisflat = false;
    }

    // Segs.
    map->segs = Z_Calloc(map->numsegs * sizeof(seg_t), PU_LEVEL, 0);
    for(k = 0; k < map->numsegs; ++k)
    {
        seg_t      *seg = &map->segs[k];

        seg->header.type = DMU_SEG;
    }

    // Subsectors.
    map->subsectors = Z_Calloc(map->numsubsectors * sizeof(subsector_t), PU_LEVEL, 0);
    for(k = 0; k < map->numsubsectors; ++k)
    {
        subsector_t *ssec = &map->subsectors[k];

        ssec->header.type = DMU_SUBSECTOR;
    }

    // Nodes.
    map->nodes = Z_Calloc(map->numnodes * sizeof(node_t), PU_LEVEL, 0);
    for(k = 0; k < map->numnodes; ++k)
    {
        node_t     *node = &map->nodes[k];

        node->header.type = DMU_NODE;
    }

    // Sectors.
    map->sectors = Z_Calloc(map->numsectors * sizeof(sector_t), PU_LEVEL, 0);
    for(k = 0; k < map->numsectors; ++k)
    {
        uint        j;
        sector_t   *sec = &map->sectors[k];
        plane_t    *planes;

        sec->header.type = DMU_SECTOR;
        sec->subscount = 0;
        sec->thinglist = NULL;
        memset(sec->rgb, 0xff, 3);

        sec->planecount = 2;
        sec->planes = Z_Malloc(sizeof(plane_t*) * sec->planecount, PU_LEVEL, 0);
        planes = Z_Calloc(sizeof(plane_t) * sec->planecount, PU_LEVEL, 0);

        // Do the planes too.
        for(j = 0; j < sec->planecount; ++j, planes++)
        {          
            planes->header.type = DMU_PLANE;
            memset(planes->glowrgb, 0xff, 3);
            planes->glow = 0;
            planes->height = 0;

            // The back pointer (temporary)
            planes->sector = sec;

            planes->surface.header.type = DMU_SURFACE;
            planes->surface.isflat = true;
            planes->surface.oldisflat = true;
            memset(planes->surface.rgba, 0xff, 3);
            planes->surface.flags = 0;
            planes->surface.offx = planes->surface.offy = 0;

            sec->planes[j] = planes;
        }

        // Set plane normals
        sec->planes[PLN_FLOOR]->surface.normal[VX] = 0;
        sec->planes[PLN_FLOOR]->surface.normal[VY] = 0;
        sec->planes[PLN_FLOOR]->surface.normal[VZ] = 1;

        sec->planes[PLN_CEILING]->surface.normal[VX] = 0;
        sec->planes[PLN_CEILING]->surface.normal[VY] = 0;
        sec->planes[PLN_CEILING]->surface.normal[VZ] = -1;
    }

    // Call the game's setup routines
    if(gx.SetupForMapData)
    {
        gx.SetupForMapData(DAM_VERTEX, map->numvertexes);
        gx.SetupForMapData(DAM_THING, map->numthings);
        gx.SetupForMapData(DAM_LINE, map->numlines);
        gx.SetupForMapData(DAM_SIDE, map->numsides);
        gx.SetupForMapData(DAM_SEG, map->numsegs);
        gx.SetupForMapData(DAM_SUBSECTOR, map->numsubsectors);
        gx.SetupForMapData(DAM_NODE, map->numnodes);
        gx.SetupForMapData(DAM_SECTOR, map->numsectors);
    }
}

static void countMapElements(gamemap_t *map)
{
    mapdatalumpnode_t *node;

    map->numvertexes = 0;
    map->numsubsectors = 0;
    map->numsectors = 0;
    map->numnodes = 0;
    map->numsides = 0;
    map->numlines = 0;
    map->numsegs = 0;
    map->numthings = 0;
    map->po_NumPolyobjs = 0;

    node = mapDataLumps;
    while(node)
    {
        mapdatalumpinfo_t *mapLump = node->data;

        // Is this a "real" lump? (or do we have to generate the data for it?)
        if(mapLump->lumpNum != -1)
        {
            int         lumpClass = mapLump->lumpClass;
            boolean     inuse = true;

            // Are gl Nodes available?
            if(glNodeData)
            {
                // Are we using them?
                if(!bspBuild)
                {
                    if(mapLumpInfo[lumpClass].glLump >= 0)
                        inuse = false;
                }
                else
                {
                    // Use the gl versions of the following lumps:
                    if(lumpClass == LCM_SUBSECTORS ||
                       lumpClass == LCM_SEGS ||
                       lumpClass == LCM_NODES)
                        inuse = false;
                }
            }

            if(inuse)
            {   // Determine the number of map data objects of each type we'll need.
                switch(mapLumpInfo[lumpClass].dataType)
                {
                case DAM_VERTEX:    map->numvertexes   += mapLump->elements;    break;
                case DAM_THING:     map->numthings     += mapLump->elements;    break;
                case DAM_LINE:      map->numlines      += mapLump->elements;    break;
                case DAM_SIDE:      map->numsides      += mapLump->elements;    break;
                case DAM_SEG:       map->numsegs       += mapLump->elements;    break;
                case DAM_SUBSECTOR: map->numsubsectors += mapLump->elements;    break;
                case DAM_NODE:      map->numnodes      += mapLump->elements;    break;
                case DAM_SECTOR:    map->numsectors    += mapLump->elements;    break;
                default:
                    break;
                }
            }
        }
        node = node->next;
    }
}

static boolean ReadMapDataFromLump(gamemap_t *map, mapdatalumpinfo_t *mapLump,
                                   uint startIndex, datatype_t **props)
{
    int         type = mapLumpInfo[mapLump->lumpClass].dataType;
    damargs_t   args;

    // Is this a supported lump type?
    switch(type)
    {
    case DAM_THING:
    case DAM_VERTEX:
    case DAM_LINE:
    case DAM_SIDE:
    case DAM_SECTOR:
    case DAM_SEG:
    case DAM_SUBSECTOR:
    case DAM_NODE:
        break;

    default:
        return false; // Read from this lump type is not supported.
    }

     // Select the lump size, number of elements etc...
    args.map = map;
    args.elmsize = mapLump->format->elmSize;
    args.elements = mapLump->elements;
    args.props = props;

    // Have we cached the lump yet?
    if(mapLump->lumpp == NULL)
        mapLump->lumpp = (byte *) W_CacheLumpNum(mapLump->lumpNum, PU_STATIC);

    // Read in that data!
    // NOTE: We'll leave the lump cached, our caller probably knows better
    // than us whether it should be free'd.
    return P_CallbackEX(type, startIndex,
                        (mapLump->lumpp + mapLump->startOffset), &args,
                        ReadMapProperty);
}

/**
 * Works through the map data lump array, processing all the lumps of the
 * requested class.
 *
 * @param map           Ptr to the map we are reading the data into.
 * @param doClass       The class of map data to read.
 * @param props         Ptr to array of properties to be read (internal DAM
 *                      property identifiers e.g. DAM_FLOOR_HEIGHT).
 * @param numProps      Number of properties in @param props.
 *
 * @return: boolean     (True) All the lumps of the requested class
 *                      were processed successfully.
 */
static boolean ReadMapData(gamemap_t *map, int doClass, int *props,
                           uint numProps)
{
    uint        oldNum = 0;
    mapdatalumpnode_t *node;
    mapdatalumpformat_t *lastUsedFormat = NULL;
    datatype_t **readProps = NULL;

    // Are gl Nodes available?
    if(glNodeData)
    {
        // Are we using them?
        if(!bspBuild)
        {
            if(doClass == LCG_VERTEXES)
                return true;
        }
        else
        {
            // Use the gl versions of the following lumps:
            if(doClass == LCM_SUBSECTORS)
                doClass = LCG_SUBSECTORS;
            else if(doClass == LCM_SEGS)
                doClass = LCG_SEGS;
            else if(doClass == LCM_NODES)
                doClass = LCG_NODES;
        }
    }

    node = mapDataLumps;
    while(node)
    {
        mapdatalumpinfo_t *mapLump = node->data;
        mapdatalumpformat_t *lumpFormat;
        maplumpinfo_t *lumpInfo;
        uint        startTime;

        // Only process lumps that match the class requested
        if(doClass == mapLump->lumpClass)
        {
            lumpInfo = &mapLumpInfo[mapLump->lumpClass];
            lumpFormat = mapLump->format;

            // Is this a "real" lump? (ie do we have to generate the data for it?)
            if(mapLump->lumpNum != -1)
            {
                VERBOSE(Con_Message("P_ReadMapData: Processing \"%s\" (#%d) ver %d...\n",
                                    W_CacheLumpNum(mapLump->lumpNum, PU_GETNAME),
                                    mapLump->elements, lumpFormat->version));
            }
            else
            {
                // Not a problem, we'll generate useable data automatically.
                VERBOSE(Con_Message("P_ReadMapData: Generating \"%s\"\n",
                                    lumpInfo->lumpname));
            }

            // Read in the lump data
            startTime = Sys_GetRealTime();
            if(lumpInfo->dataType == DAM_MAPBLOCK)
            {
                if(!P_LoadBlockMap(map, mapLump))
                {
                    if(readProps)
                        M_Free(readProps);
                    return false;
                }
            }
            else if(lumpInfo->dataType == DAM_SECREJECT)
            {
                if(!P_LoadReject(map, mapLump))
                {
                    if(readProps)
                        M_Free(readProps);
                    return false;
                }
            }
            else
            {
                uint        startIndex;

                // KLUDGE: firstGLvertex. We should determine the start index for this
                // block of data depending on the map format.
                if(mapLump->lumpClass == LCM_VERTEXES)
                    firstGLvertex = mapLump->elements;
                
                startIndex = (mapLump->lumpClass == LCG_VERTEXES? firstGLvertex : oldNum);
                // < KLUDGE

                //
                // Create a NULL terminated array of the properties to be read.
                //
                // TODO:
                // Improve property selection algorithm.

                // Can we reuse a previously created array?
                if(lumpFormat != lastUsedFormat)
                {   // No we cannot.
                    uint        readNumProps;
                    uint        i, j, idx;
                    boolean     found;

                    // Count the number of requested properties that are present in
                    // the lump.
                    readNumProps = 0;
                    for(i = 0; i < numProps; ++i)
                    {
                        j = 0;
                        found = false;
                        do
                        {
                            if(lumpFormat->props[j].id == props[i])
                            {
                                readNumProps++;
                                found = true;
                            }
                            else
                                j++;
                        } while(!found && j < lumpFormat->numProps);
                    }

                    if(readProps)
                        M_Free(readProps);

                    readProps = M_Malloc(sizeof(datatype_t*) * (readNumProps + 1));
                    // Retrieve ptrs to the properties to be read from the lumpformat.

                    idx = 0;
                    for(i = 0; i < numProps; ++i)
                    {
                        j = 0;
                        found = false;
                        do
                        {
                            if(lumpFormat->props[j].id == props[i])
                            {
                                readProps[idx++] = &lumpFormat->props[j];
                                found = true;
                            }
                            else
                                j++;
                        } while(!found && j < lumpFormat->numProps);
                    }
                    readProps[idx] = NULL; // terminate
                }
                else // Yes we can.
                    lumpFormat = lastUsedFormat;
                // END TODO improve property selection algorithm.

                lastUsedFormat = lumpFormat;

                if(!ReadMapDataFromLump(map, mapLump, startIndex, readProps))
                {
                    M_Free(readProps);
                    return false; // something went VERY horibly wrong...
                }
            }
            // How much time did we spend?
            VERBOSE2(Con_Message("P_ReadMapData: Done in %.4f seconds.\n",
                                 (Sys_GetRealTime() - startTime) / 1000.0f));

            oldNum += mapLump->elements;

            // We're finished with this lump.
            if(mapLump->lumpp)
            {
                Z_Free(mapLump->lumpp);
                mapLump->lumpp = 0;
            }
        }

        node = node->next;
    }

    if(readProps)
        M_Free(readProps);
    return true;
}

static void *P_GetPtrToObject(gamemap_t* map, int objectType, uint id)
{
    switch(objectType)
    {
    case DAM_LINE:
        if(id < map->numlines)
            return &map->lines[id];
        break;
    case DAM_SIDE:
        if(id < map->numsides)
            return &map->sides[id];
        break;
    case DAM_VERTEX:
        if(id < map->numvertexes)
            return &map->vertexes[id];
        break;
    case DAM_SEG:
        if(id < map->numsegs)
            return &map->segs[id];
        break;
    case DAM_SUBSECTOR:
        if(id < map->numsubsectors)
            return &map->subsectors[id];
        break;
    case DAM_NODE:
        if(id < map->numnodes)
            return &map->nodes[id];
        break;
    case DAM_SECTOR:
        if(id < map->numsectors)
            return &map->sectors[id];
        break;
    default:
        Con_Error("P_GetPtrToObject: %i is not a valid type\n", objectType);
    }

    return NULL;
}

/**
 * Reads a value from the (little endian) source buffer. Does some basic
 * type checking so that incompatible types are not assigned.
 * Simple conversions are also done, e.g., float to fixed.
 */
static void ReadValue(gamemap_t* map, valuetype_t valueType, void* dst,
                      const byte *src, const datatype_t* prop, uint element)
{
    int flags = prop->flags;

    if(valueType == DDVT_BYTE)
    {
        byte* d = dst;
        switch(prop->size)
        {
        case 1:
            *d = *src;
            break;
        case 2:
            *d = *src;
            break;
        case 4:
            *d = *src;
            break;
        default:
            Con_Error("ReadValue: DDVT_BYTE incompatible with value type %s\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float* d = dst;
        switch(prop->size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = FIX2FLT(USHORT(*((short*)(src))) << FRACBITS);
                else
                    *d = FIX2FLT(USHORT(*((short*)(src))));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = FIX2FLT(SHORT(*((short*)(src))) << FRACBITS);
                else
                    *d = FIX2FLT(SHORT(*((short*)(src))));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = FIX2FLT(ULONG(*((long*)(src))) << FRACBITS);
                else
                    *d = FIX2FLT(ULONG(*((long*)(src))));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = FIX2FLT(LONG(*((long*)(src))) << FRACBITS);
                else
                    *d = FIX2FLT(LONG(*((long*)(src))));
            }
            break;

        default:
            Con_Error("ReadValue: DDVT_FLOAT incompatible with value type %s\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dst;
        switch(prop->size)
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;

        case 8:
            {
            if(flags & DT_TEXTURE)
            {
                *d = P_CheckTexture((char*)((long long*)(src)), false, valueType,
                                    element, prop->id);
            }
            else if(flags & DT_FLAT)
            {
                *d = P_CheckTexture((char*)((long long*)(src)), true, valueType,
                                    element, prop->id);
            }
            break;
            }
         default:
            Con_Error("ReadValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(prop->size));
         }
    }
    else if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_ULONG)
    {
        unsigned long* d = dst;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_FRACBITS)
                    *d = SHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = SHORT(*((short*)(src)));
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_ULONG incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_UINT)
    {
        unsigned int* d = dst;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    *d = NO_INDEX;

                    if(num != ((unsigned short)-1))
                        *d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        *d = SHORT(*((short*)(src))) << FRACBITS;
                    else
                        *d = USHORT(*((short*)(src)));
                }
            }
            if((flags & DT_MSBCONVERT) && (*d & 0x8000))
            {
                *d &= ~0x8000;
                *d |= 0x80000000;
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dst;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                if(flags & DT_FRACBITS)
                    *d = USHORT(*((short*)(src))) << FRACBITS;
                else
                    *d = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    *d = NO_INDEX;

                    if(num != ((unsigned short)-1))
                        *d = num;
                }
                else
                {
                    if(flags & DT_FRACBITS)
                        *d = SHORT(*((short*)(src))) << FRACBITS;
                    else
                        *d = SHORT(*((short*)(src)));
                }
            }
            if((flags & DT_MSBCONVERT) && (*d & 0x8000))
            {
                *d &= ~0x8000;
                *d |= 0x80000000;
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                *d = ULONG(*((long*)(src)));
            else
                *d = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t* d = dst;
        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_FRACBITS)
                *d = (angle_t) (SHORT(*((short*)(src))) << FRACBITS);
            else
                *d = (angle_t) SHORT(*((short*)(src)));
            break;

        default:
            Con_Error("ReadValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(prop->size));
        }
    }
    // TODO:
    // Once we have a way to convert internal member to property we should no
    // longer need these special case constants (eg DDVT_SECT_PTR).
    else if(valueType == DDVT_SECT_PTR || valueType == DDVT_VERT_PTR ||
            valueType == DDVT_LINE_PTR || valueType == DDVT_SIDE_PTR)
    {
        long idx = NO_INDEX;

        switch(prop->size) // Number of src bytes
        {
        case 2:
            if(flags & DT_UNSIGNED)
            {
                idx = USHORT(*((short*)(src)));
            }
            else
            {
                if(flags & DT_NOINDEX)
                {
                    unsigned short num = SHORT(*((short*)(src)));

                    if(num != ((unsigned short)-1))
                        idx = num;
                }
                else
                {
                    idx = SHORT(*((short*)(src)));
                }
            }
            break;

        case 4:
            if(flags & DT_UNSIGNED)
                idx = ULONG(*((long*)(src)));
            else
                idx = LONG(*((long*)(src)));
            break;

        default:
            Con_Error("ReadValue: %s incompatible with value type %s.\n",
                      valueType == DDVT_SECT_PTR? "DDVT_SECT_PTR" :
                      valueType == DDVT_VERT_PTR? "DDVT_VERT_PTR" :
                      valueType == DDVT_SIDE_PTR? "DDVT_SIDE_PTR" :
                      "DDVT_LINE_PTR", value_Str(prop->size));
        }

        switch(valueType)
        {
        case DDVT_LINE_PTR:
            *(line_t**) dst =
                P_GetPtrToObject(map, DAM_LINE, (unsigned) idx);
            break;

        case DDVT_SIDE_PTR:
            *(side_t**) dst =
                P_GetPtrToObject(map, DAM_SIDE, (unsigned) idx);
            break;

        case DDVT_SECT_PTR:
            *(sector_t**) dst =
                P_GetPtrToObject(map, DAM_SECTOR, (unsigned) idx);
            break;

        case DDVT_VERT_PTR:
            // FIXME:
            // The firstGLvertex offset should be handed down from the very
            // start of the read process, it should not be a global.
            // If GL NODES are available this might be an "extra" vertex.
            if(glNodeData && bspBuild)
            {
                switch(glNodeFormats[glNodeFormat].
                       verInfo[mapLumpInfo[LCG_SEGS].glLump].version)
                {
                case 2:
                    if(idx & 0x8000)
                    {
                        idx &= ~0x8000;
                        idx += firstGLvertex;
                    }
                    break;

                case 3:
                case 5:
                    if(idx & 0xc0000000)
                    {
                        idx &= ~0xc0000000;
                        idx += firstGLvertex;
                    }
                    break;

                default:
                    break;
                }
            }

            *(vertex_t**) dst =
                P_GetPtrToObject(map, DAM_VERTEX, (unsigned) idx);
            break;

        default:
            // TODO: Need to react?
            break;
        }
    }
    else
    {
        Con_Error("ReadValue: unknown value type %d.\n", valueType);
    }
}

static int ReadCustomMapProperty(gamemap_t* map, int dataType, void *ptr, uint elmIdx,
                                 const datatype_t* prop, const byte *src)
{
    byte        tmpbyte = 0;
    short       tmpshort = 0;
    fixed_t     tmpfixed = 0;
    int         tmpint = 0;
    float       tmpfloat = 0;
    void       *dest = NULL;

    switch(dataType)
    {
    case DAM_THING:
        break;
    case DAM_LINE:
        break;
    case DAM_SIDE:
        break;
    case DAM_SECTOR:
        break;
    default:
        Con_Error("ReadCustomMapProperty: Type does not support custom properties\n");
    }

    switch(prop->size)
    {
    case DDVT_BYTE:
        dest = &tmpbyte;
        break;
    case DDVT_SHORT:
        dest = &tmpshort;
        break;
    case DDVT_FIXED:
        dest = &tmpfixed;
        break;
    case DDVT_INT:
        dest = &tmpint;
        break;
    case DDVT_FLOAT:
        dest = &tmpfloat;
        break;
    default:
        Con_Error("ReadCustomMapProperty: Unsupported data type id %i.\n", prop->size);
    };

    ReadValue(map, prop->size, dest, src, prop, elmIdx);
    gx.HandleMapDataProperty(elmIdx, dataType, prop->id, prop->size, dest);

    return true;
}

static int ReadMapProperty(gamemap_t* map, int dataType, void* ptr, uint elmIdx,
                            const datatype_t* prop, const byte *src)
{
    // Handle unknown (game specific) properties.
    if(prop->gameprop)
        return ReadCustomMapProperty(map, dataType, ptr, elmIdx, prop, src);

    // These are the exported map data properties that can be
    // assigned to when reading map data.
    switch(dataType)
    {
    case DAM_VERTEX:
        {
        vertex_t* p = ptr;

        switch(prop->id)
        {
        case DAM_X:
            ReadValue(map, DMT_VERTEX_POS, &p->pos[VX], src, prop, elmIdx);
            break;

        case DAM_Y:
            ReadValue(map, DMT_VERTEX_POS, &p->pos[VY], src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_VERTEX has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_LINE:
        {
        line_t* p = ptr;

        switch(prop->id)
        {
        case DAM_VERTEX1:
            // TODO: should be DMT_LINE_V1 but we require special case logic
            ReadValue(map, DDVT_VERT_PTR, &p->L_v1, src, prop, elmIdx);
            break;

        case DAM_VERTEX2:
            // TODO: should be DMT_LINE_V2 but we require special case logic
            ReadValue(map, DDVT_VERT_PTR, &p->L_v2, src, prop, elmIdx);
            break;

        case DAM_FLAGS:
            ReadValue(map, DMT_LINE_FLAGS, &p->flags, src, prop, elmIdx);
            break;

        case DAM_SIDE0:
            ReadValue(map, DDVT_SIDE_PTR, &p->L_frontside, src, prop, elmIdx);
            break;

        case DAM_SIDE1:
            ReadValue(map, DDVT_SIDE_PTR, &p->L_backside, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_LINE has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_SIDE:
        {
        side_t* p = ptr;

        switch(prop->id)
        {
        case DAM_TOP_TEXTURE_OFFSET_X:
            ReadValue(map, DMT_SURFACE_OFFX, &p->SW_topoffx, src, prop, elmIdx);
            break;

        case DAM_TOP_TEXTURE_OFFSET_Y:
            ReadValue(map, DMT_SURFACE_OFFY, &p->SW_topoffy, src, prop, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_X:
            ReadValue(map, DMT_SURFACE_OFFX, &p->SW_middleoffx, src, prop, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE_OFFSET_Y:
            ReadValue(map, DMT_SURFACE_OFFY, &p->SW_middleoffy, src, prop, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_X:
            ReadValue(map, DMT_SURFACE_OFFX, &p->SW_bottomoffx, src, prop, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE_OFFSET_Y:
            ReadValue(map, DMT_SURFACE_OFFY, &p->SW_bottomoffy, src, prop, elmIdx);
            break;

        case DAM_TOP_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SW_toppic, src, prop, elmIdx);
            break;

        case DAM_MIDDLE_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SW_middlepic, src, prop, elmIdx);
            break;

        case DAM_BOTTOM_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SW_bottompic, src, prop, elmIdx);
            break;

        case DAM_FRONT_SECTOR:
            // TODO: should be DMT_SIDE_SECTOR but we require special case logic
            ReadValue(map, DDVT_SECT_PTR, &p->sector, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_SIDE has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_SECTOR:
        {
        sector_t* p = ptr;

        switch(prop->id)
        {
        case DAM_FLOOR_HEIGHT:
            ReadValue(map, DMT_PLANE_HEIGHT, &p->SP_floorheight, src, prop, elmIdx);
            break;

        case DAM_CEILING_HEIGHT:
            ReadValue(map, DMT_PLANE_HEIGHT, &p->SP_ceilheight, src, prop, elmIdx);
            break;

        case DAM_FLOOR_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SP_floorpic, src, prop, elmIdx);
            break;

        case DAM_CEILING_TEXTURE:
            ReadValue(map, DMT_SURFACE_TEXTURE, &p->SP_ceilpic, src, prop, elmIdx);
            break;

        case DAM_LIGHT_LEVEL:
            ReadValue(map, DMT_SECTOR_LIGHTLEVEL, &p->lightlevel, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_SECTOR has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_SEG:
        {
        seg_t* p = ptr;

        switch(prop->id)
        {
        case DAM_VERTEX1:
            // TODO: should be DMT_SEG_V  but we require special case logic
            ReadValue(map, DDVT_VERT_PTR, &p->SG_v1, src, prop, elmIdx);
            break;

        case DAM_VERTEX2:
            // TODO: should be DMT_SEG_V  but we require special case logic
            ReadValue(map, DDVT_VERT_PTR, &p->SG_v2, src, prop, elmIdx);
            break;

        case DAM_ANGLE:
            ReadValue(map, DMT_SEG_ANGLE, &p->angle, src, prop, elmIdx);
            break;

        case DAM_LINE:
            // KLUDGE: Set the data type implicitly as DAM_LINE is DDVT_PTR
            ReadValue(map, DDVT_LINE_PTR, &p->linedef, src, prop, elmIdx);
            break;

        case DAM_SIDE:
            // KLUDGE: Store the side id into the flags field
            ReadValue(map, DDVT_BYTE, &p->flags, src, prop, elmIdx);
            break;

        case DAM_OFFSET:
            ReadValue(map, DMT_SEG_OFFSET, &p->offset, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_SEG has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_SUBSECTOR:
        {
        subsector_t* p = ptr;

        switch(prop->id)
        {
        case DAM_LINE_COUNT:
            ReadValue(map, DMT_SUBSECTOR_LINECOUNT, &p->linecount, src, prop, elmIdx);
            break;

        case DAM_LINE_FIRST:
            ReadValue(map, DMT_SUBSECTOR_FIRSTLINE, &p->firstline, src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_SUBSECTOR has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    case DAM_NODE:
        {
        node_t* p = ptr;

        switch(prop->id)
        {
        case DAM_X:
            ReadValue(map, DMT_NODE_X, &p->x, src, prop, elmIdx);
            break;

        case DAM_Y:
            ReadValue(map, DMT_NODE_Y, &p->y, src, prop, elmIdx);
            break;

        case DAM_DX:
            ReadValue(map, DMT_NODE_DX, &p->dx, src, prop, elmIdx);
            break;

        case DAM_DY:
            ReadValue(map, DMT_NODE_DY, &p->dy, src, prop, elmIdx);
            break;

        // TODO: the following should use DMT_NODE_?
        // Constants not defined as yet by the maptypes script.
        case DAM_BBOX_RIGHT_TOP_Y:
            ReadValue(map, DDVT_FLOAT, &p->bbox[0][0], src, prop, elmIdx);
            break;

        case DAM_BBOX_RIGHT_LOW_Y:
            ReadValue(map, DDVT_FLOAT, &p->bbox[0][1], src, prop, elmIdx);
            break;

        case DAM_BBOX_RIGHT_LOW_X:
            ReadValue(map, DDVT_FLOAT, &p->bbox[0][2], src, prop, elmIdx);
            break;

        case DAM_BBOX_RIGHT_TOP_X:
            ReadValue(map, DDVT_FLOAT, &p->bbox[0][3], src, prop, elmIdx);
            break;

        case DAM_BBOX_LEFT_TOP_Y:
            ReadValue(map, DDVT_FLOAT, &p->bbox[1][0], src, prop, elmIdx);
            break;

        case DAM_BBOX_LEFT_LOW_Y:
            ReadValue(map, DDVT_FLOAT, &p->bbox[1][1], src, prop, elmIdx);
            break;

        case DAM_BBOX_LEFT_LOW_X:
            ReadValue(map, DDVT_FLOAT, &p->bbox[1][2], src, prop, elmIdx);
            break;

        case DAM_BBOX_LEFT_TOP_X:
            ReadValue(map, DDVT_FLOAT, &p->bbox[1][3], src, prop, elmIdx);
            break;

        case DAM_CHILD_RIGHT:
            ReadValue(map, DDVT_UINT, &p->children[0], src, prop, elmIdx);
            break;

        case DAM_CHILD_LEFT:
            ReadValue(map, DDVT_UINT, &p->children[1], src, prop, elmIdx);
            break;

        default:
            Con_Error("ReadMapProperty: DAM_NODE has no property %s.\n",
                      DAM_Str(prop->id));
        }
        break;
        }
    default:
        Con_Error("ReadMapProperty: Type cannot be assigned to from a map format.\n");
    }

    return true; // Continue iteration
}

/*
 * Make multiple calls to a callback function on a selection of archived
 * map data objects.
 *
 * This function is essentially the same as P_Callback in p_dmu.c but with
 * the following key differences:
 *
 *  1  Multiple callbacks can be made for each object.
 *  2  Any number of properties (of different types) per object
 *     can be manipulated. To accomplish the same result using
 *     P_Callback would require numerous rounds of iteration.
 *  3  Optimised for bulk processing.
 *
 * Returns true if all the calls to the callback function return true. False
 * is returned when the callback function returns false; in this case, the
 * iteration is aborted immediately when the callback function returns false.
 *
 * NOTE: Not very pretty to look at but it IS pretty quick :-)
 *
 * NOTE2: I would suggest these manual optimizations be removed. The compiler
 *       is pretty good in unrolling loops, if need be. -jk
 *
 * TODO: Make required changes to make signature compatible with the other
 *       P_Callback function (may need to add a dummy parameter to P_Callback
 *       since (byte*) buffer needs to be accessed here.).
 *       Do NOT access the contents of (void*) context since we can't be
 *       sure what it is.
 *       DAM specific parameters could be passed to the callback function by
 *       using a (void*) context parameter. This would allow our callbacks to
 *       use the same signature as the DMU callbacks.
 *       Think of a way to combine index and startIndex.
 */
static int P_CallbackEX(int dataType, uint startIndex, const byte *buffer,
                        void* context,
                        int (*callback)(gamemap_t* map, int dataType, void* ptr,
                                 uint elmIdx, const datatype_t* prop,
                                 const byte *buffer))
{
#define NUMBLOCKS 8
#define BLOCK ptr = dataType == DAM_THING? &idx : P_GetPtrToObject(map, dataType, idx);  \
        for(prop = args->props[k = 0]; prop; prop = args->props[++k]) \
        { \
            if(!callback(map, dataType, ptr, idx, prop, buffer + prop->offset)) \
                return false; \
        } \
        buffer += args->elmsize; \
        ++idx;

    uint        idx;
    uint        i = 0, k;
    damargs_t  *args = (damargs_t*) context;
    gamemap_t  *map = args->map;
    uint        blockLimit = (args->elements / NUMBLOCKS) * NUMBLOCKS;
    void       *ptr;
    const datatype_t *prop;

    // Have we got enough to do some in blocks?
    if(args->elements >= blockLimit)
    {
        idx = startIndex + i;
        while(i < blockLimit)
        {
            BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK BLOCK

            i += NUMBLOCKS;
        }
    }

    // Have we got any left to do?
    if(i < args->elements)
    {
        // Yes, jump in at the number of elements remaining
        idx = startIndex + i;
        switch(args->elements - i)
        {
        case 7: BLOCK
        case 6: BLOCK
        case 5: BLOCK
        case 4: BLOCK
        case 3: BLOCK
        case 2: BLOCK
        case 1:
            ptr = dataType == DAM_THING? &idx : P_GetPtrToObject(map, dataType, idx);
            for(prop = args->props[k = 0]; prop; prop = args->props[++k])
            {
                if(!callback(map, dataType, ptr, idx, prop, buffer + prop->offset))
                    return false;
            }
        }
    }

    return true;
}

/*
 * Finalizes the segs by linking the various side & sector ptrs
 * and calculating the length of each segment.
 * If angle and offset information is not provided they are
 * calculated here.
 */
static void processSegs(gamemap_t *map)
{
    uint        i, j ,k, n;
    seg_t      *seg;
    line_t     *ldef;
    int         side;

    for(i = 0; i < map->numsegs; ++i)
    {
        seg = &map->segs[i];

        if(seg->angle == 0)
            seg->angle = -1;

        if(seg->offset == 0)
            seg->offset = -1;

        // Kludge: The flags member is used as a temporary holder
        // for the side value.
        side = (int) seg->flags;
        seg->flags = 0;

        if(seg->linedef)
        {
            ldef = seg->linedef;
            seg->sidedef = ldef->sides[side];
            seg->SG_frontsector = ldef->sides[side]->sector;

            if((ldef->flags & ML_TWOSIDED) && ldef->sides[side ^ 1])
            {
                seg->SG_backsector = ldef->sides[side ^ 1]->sector;
            }
            else
            {
                ldef->flags &= ~ML_TWOSIDED;
                seg->SG_backsector = 0;
            }

            if(seg->offset == -1)
            {
                if(side == 0)
                    seg->offset = P_AccurateDistancef(seg->v[0]->pos[VX] - ldef->v[0]->pos[VX],
                                                      seg->v[0]->pos[VY] - ldef->v[0]->pos[VY]);
                else
                    seg->offset = P_AccurateDistancef(seg->v[0]->pos[VX] - ldef->v[1]->pos[VX],
                                                      seg->v[0]->pos[VY] - ldef->v[1]->pos[VY]);
            }

            if(seg->angle == -1)
                seg->angle =
                    bamsAtan2((int) (seg->v[1]->pos[VY] - seg->v[0]->pos[VY]),
                              (int) (seg->v[1]->pos[VX] - seg->v[0]->pos[VX])) << FRACBITS;
        }
        else
        {
            seg->linedef = NULL;
            seg->sidedef = NULL;
            seg->SG_frontsector = NULL;
            seg->SG_backsector = NULL;
        }

        // Calculate the length of the segment. We need this for
        // the texture coordinates. -jk
        seg->length = P_AccurateDistancef(seg->v[1]->pos[VX] - seg->v[0]->pos[VX],
                                          seg->v[1]->pos[VY] - seg->v[0]->pos[VY]);

        if(seg->length == 0)
            seg->length = 0.01f; // Hmm...

        // Calculate the surface normals
        // Front first
        if(seg->sidedef)
        {
            surface_t *surface = &seg->sidedef->SW_topsurface;

            surface->normal[VY] = (seg->v[0]->pos[VX] - seg->v[1]->pos[VX]) / seg->length;
            surface->normal[VX] = (seg->v[1]->pos[VY] - seg->v[0]->pos[VY]) / seg->length;
            surface->normal[VZ] = 0;

            // All surfaces of a sidedef have the same normal.
            memcpy(seg->sidedef->SW_middlenormal, surface->normal, sizeof(surface->normal));
            memcpy(seg->sidedef->SW_bottomnormal, surface->normal, sizeof(surface->normal));
        }


        // Initialize the bias illumination data.
        for(k = 0; k < 4; ++k)
        {
            for(j = 0; j < 3; ++j)
            {
                seg->illum[j][k].flags = VIF_STILL_UNSEEN;

                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                {
                    seg->illum[j][k].casted[n].source = -1;
                }
            }
        }
    }
}

/*
 * Completes the linedef loading by resolving the front/back
 * sector ptrs which we couldn't do earlier as the sidedefs
 * hadn't been loaded at the time.
 *
 * Also increments the sector->linecount and tracks the
 * number of unqiue linedefs.
 *
 * Sidedefs MUST be loaded before this is called
 */
static void finishLineDefs(gamemap_t* map)
{
    uint        i;
    line_t     *ld;
    vertex_t   *v[2];

    VERBOSE2(Con_Message("Finalizing Linedefs...\n"));

    numUniqueLines = 0;
    for(i = 0; i < map->numlines; ++i)
    {
        ld = &map->lines[i];

        v[0] = ld->v[0];
        v[1] = ld->v[1];
        ld->dx = v[1]->pos[VX] - v[0]->pos[VX];
        ld->dy = v[1]->pos[VY] - v[0]->pos[VY];

        // Calculate the accurate length of each line.
        ld->length = P_AccurateDistancef(ld->dx, ld->dy);
        ld->angle = bamsAtan2(-(FLT2FIX(ld->dx) >> 13), FLT2FIX(ld->dy) >> 13);

        if(!ld->dx)
            ld->slopetype = ST_VERTICAL;
        else if(!ld->dy)
            ld->slopetype = ST_HORIZONTAL;
        else
        {
            if(ld->dy / ld->dx > 0)
                ld->slopetype = ST_POSITIVE;
            else
                ld->slopetype = ST_NEGATIVE;
        }

        if(v[0]->pos[VX] < v[1]->pos[VX])
        {
            ld->bbox[BOXLEFT]   = FLT2FIX(v[0]->pos[VX]);
            ld->bbox[BOXRIGHT]  = FLT2FIX(v[1]->pos[VX]);
        }
        else
        {
            ld->bbox[BOXLEFT]   = FLT2FIX(v[1]->pos[VX]);
            ld->bbox[BOXRIGHT]  = FLT2FIX(v[0]->pos[VX]);
        }

        if(v[0]->pos[VY] < v[1]->pos[VY])
        {
            ld->bbox[BOXBOTTOM] = FLT2FIX(v[0]->pos[VY]);
            ld->bbox[BOXTOP]    = FLT2FIX(v[1]->pos[VY]);
        }
        else
        {
            ld->bbox[BOXBOTTOM] = FLT2FIX(v[1]->pos[VY]);
            ld->bbox[BOXTOP]    = FLT2FIX(v[0]->pos[VY]);
        }

        if(ld->L_frontside)
            ld->L_frontsector = ld->L_frontside->sector;
        else
            ld->L_frontsector = 0;

        if(ld->L_backside)
            ld->L_backsector = ld->L_backside->sector;
        else
            ld->L_backsector = 0;

        // Increase the sector line count
        if(ld->L_frontsector)
        {
            ld->L_frontsector->linecount++;
            numUniqueLines++;
        }
        else
        {   // A missing front sidedef
            missingFronts[i] = 1;
            numMissingFronts++;
        }

        if(ld->L_backsector && ld->L_backsector != ld->L_frontsector)
        {
            ld->L_backsector->linecount++;
            numUniqueLines++;
        }
    }
}

/*
 * Builds sector line lists and subsector sector numbers.
 * Finds block bounding boxes for sectors.
 */
static void finalizeMapData(gamemap_t *map)
{
    uint        startTime = Sys_GetRealTime();

    uint       *linesInSector;
    uint       *ssecsInSector;
    int         block;
    uint        i, j, k, count;
    uint        secid;

    line_t    **linebuffer;
    line_t    **linebptr;
    line_t     *li;

    subsector_t **ssecbuffer;
    subsector_t **ssecbptr;
    subsector_t *ss;

    sector_t   *sec;
    seg_t      *seg;
    fixed_t     bbox[4];

    Con_Message("Group lines\n");
    Con_Message(" Sector look up\n");
    // look up sector number for each subsector
    for(i = 0, ss = map->subsectors; i < map->numsubsectors; ++i, ss++)
    {
        seg = &map->segs[ss->firstline];
        for(j = 0; j < ss->linecount; ++j, seg++)
            if(seg->sidedef)
            {
#if _DEBUG
ASSERT_DMU_TYPE(seg->sidedef->sector, DMU_SECTOR);
#endif
                ss->sector = seg->sidedef->sector;
                ss->sector->subscount++;
                break;
            }
    }

    Con_Message(" Build line and subsector tables\n");
    // build line tables for each sector
    linebuffer = Z_Malloc(numUniqueLines * sizeof(line_t*), PU_LEVELSTATIC, NULL);
    linebptr = linebuffer;
    linesInSector = M_Malloc(map->numsectors * sizeof(uint));
    memset(linesInSector, 0, map->numsectors * sizeof(uint));

    // build subsector tables for each sector
    ssecbuffer = Z_Malloc(map->numsubsectors * sizeof(subsector_t*), PU_LEVELSTATIC, NULL);
    ssecbptr = ssecbuffer;
    ssecsInSector = M_Malloc(map->numsectors * sizeof(uint));
    memset(ssecsInSector, 0, map->numsectors * sizeof(uint));

    for(i = 0, sec = map->sectors; i < map->numsectors; ++i, sec++)
    {
        if(sec->linecount > 0)
        {
            sec->Lines = linebptr;
            linebptr += sec->linecount;
        }

        if(sec->subscount > 0)
        {
            sec->subsectors = ssecbptr;
            ssecbptr += sec->subscount;
        }
    }

    for(k = 0, li = map->lines; k < map->numlines; ++k, li++)
    {
        if(li->L_frontsector != NULL)
        {
            secid = li->L_frontsector - map->sectors;
            li->L_frontsector->Lines[linesInSector[secid]++] = li;
        }

        if(li->L_backsector != NULL && li->L_backsector != li->L_frontsector)
        {
            secid = li->L_backsector - map->sectors;
            li->L_backsector->Lines[linesInSector[secid]++] = li;
        }
    }

    for(i = 0, ss = map->subsectors; i < map->numsubsectors; ++i, ss++)
    {
        if(ss->sector != NULL)
        {
            secid = ss->sector - map->sectors;
            ss->sector->subsectors[ssecsInSector[secid]++] = ss;
        }
    }

    for(i = 0, sec = map->sectors; i < map->numsectors; ++i, sec++)
    {
        if(linesInSector[i] != sec->linecount)
            Con_Error("finalizeMapData: miscounted lines"); // Hmm? Unusual...

        if(ssecsInSector[i] != sec->subscount)
            Con_Error("finalizeMapData: miscounted subsectors"); // Hmm? Unusual...

        if(sec->linecount != 0)
        {
            M_ClearBox(bbox);

            for(k = 0; k < sec->linecount; ++k)
            {
                li = sec->Lines[k];
                M_AddToBox(bbox, FLT2FIX(li->v[0]->pos[VX]), FLT2FIX(li->v[0]->pos[VY]));
                M_AddToBox(bbox, FLT2FIX(li->v[1]->pos[VX]), FLT2FIX(li->v[1]->pos[VY]));
            }
        }
        else // Its a "benign sector"
        {
            // Send the game a status report (we don't need to do anything).
            if(gx.HandleMapObjectStatusReport)
                gx.HandleMapObjectStatusReport(DMUSC_BENIGNSECTOR,
                                               sec - map->sectors,
                                               DMU_SECTOR, NULL);
        }

        // adjust bounding box to map blocks
        block = (bbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapheight ? bmapheight - 1 : block;
        sec->blockbox[BOXTOP] = block;

        block = (bbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sec->blockbox[BOXBOTTOM] = block;

        block = (bbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block >= bmapwidth ? bmapwidth - 1 : block;
        sec->blockbox[BOXRIGHT] = block;

        block = (bbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
        block = block < 0 ? 0 : block;
        sec->blockbox[BOXLEFT] = block;

        // Set the degenmobj_t to the middle of the bounding box
        sec->soundorg.pos[VX] = (bbox[BOXRIGHT] + bbox[BOXLEFT]) / 2;
        sec->soundorg.pos[VY] = (bbox[BOXTOP] + bbox[BOXBOTTOM]) / 2;

        // Set the z height of the sector sound origin
        sec->soundorg.pos[VZ] =
            FLT2FIX((sec->SP_ceilheight - sec->SP_floorheight) / 2);

        // Set the position of the sound origin for all plane sound origins.
        for(k = 0; k < sec->planecount; ++k)
        {
            sec->planes[k]->soundorg.pos[VX] = sec->soundorg.pos[VX];
            sec->planes[k]->soundorg.pos[VY] = sec->soundorg.pos[VY];
            sec->planes[k]->soundorg.pos[VZ] = FLT2FIX(sec->planes[k]->height);
        }

        // Set target heights of all planes.
        for(k = 0; k < sec->planecount; ++k)
            sec->planes[k]->target = sec->planes[k]->height;
    }

    M_Free(linesInSector);
    M_Free(ssecsInSector);

    // Finalize side properties
    for(i = 0; i < map->numsides; ++i)
    {
        side_t *side = &map->sides[i];

        // Make sure the texture references are good.
        if(!side->SW_topisflat && side->SW_toppic >= numtextures)
            side->SW_toppic = 0;
        if(!side->SW_middleisflat && side->SW_middlepic >= numtextures)
            side->SW_middlepic = 0;
        if(!side->SW_bottomisflat && side->SW_bottompic >= numtextures)
            side->SW_bottompic = 0;
    }

    // Initialize polyobject properties (here?)
    for(i = 0; i < map->po_NumPolyobjs; ++i)
        map->polyobjs[i].header.type = DMU_POLYOBJ;

    // Clear out mobj rings.
    count = sizeof(*map->blockrings) * map->bmapwidth * map->bmapheight;
    map->blockrings = Z_Malloc(count, PU_LEVEL, 0);
    memset(map->blockrings, 0, count);

    for(i = 0; i < map->bmapwidth * map->bmapheight; ++i)
        map->blockrings[i].next =
            map->blockrings[i].prev = (mobj_t *) &map->blockrings[i];

    // How much time did we spend?
    VERBOSE(Con_Message
            ("finalizeMapData: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

/*
 * Generate valid blockmap data from the already loaded level data.
 * Adapted from algorithm used in prBoom 2.2.6 -DJS
 *
 * Algorithm is order of nlines*(ncols+nrows) not nlines*ncols*nrows
 */

/*
 * Subroutine to add a line number to a block list
 * It simply returns if the line is already in the block
 */
static void AddBlockLine(linelist_t **lists, uint *count, uint *done,
                         uint blockno, long lineno)
{
    linelist_t *l;

    if(done[blockno])
        return;

    l = M_Malloc(sizeof(linelist_t));
    l->num = lineno;
    l->next = lists[blockno];

    lists[blockno] = l;

    count[blockno]++;

    done[blockno] = 1;
}

/*
 * Actually construct the blockmap lump from the level data
 *
 * This finds the intersection of each linedef with the column and
 * row lines at the left and bottom of each blockmap cell. It then
 * adds the line to all block lists touching the intersection.
 */
static void P_CreateBlockMap(gamemap_t* map)
{
    uint        i;
    int         j;
    int         bMapWidth, bMapHeight;  // blockmap dimensions
    static vec2_t bMapOrigin;           // blockmap origin (lower left)
    static vec2_t blockSize;            // size of the blocks
    uint       *blockcount = NULL;      // array of counters of line lists
    uint       *blockdone = NULL;       // array keeping track of blocks/line
    uint        numBlocks;              // number of cells = nrows*ncols

    linelist_t **blocklists = NULL;     // array of pointers to lists of lines
    long linetotal = 0;                 // total length of all blocklists

    vec2_t  bounds[2], point, dims;
    vertex_t *vtx;

    // scan for map limits, which the blockmap must enclose
    for(i = 0; i < map->numvertexes; ++i)
    {
        vtx = &map->vertexes[i];
        V2_Set(point, vtx->pos[VX], vtx->pos[VY]);
        if(!i)
            V2_InitBox(bounds, point);
        else
            V2_AddToBox(bounds, point);
    }

    // Setup the blockmap area to enclose the whole map,
    // plus a margin (margin is needed for a map that fits
    // entirely inside one blockmap cell).
    V2_Set(bounds[0], bounds[0][VX] - BLKMARGIN, bounds[0][VY] - BLKMARGIN);
    V2_Set(bounds[1], bounds[1][VX] + BLKMARGIN + 1, bounds[1][VY] + BLKMARGIN + 1);

    // Select a good size for the blocks.
    V2_Set(blockSize, 128, 128);
    V2_Copy(bMapOrigin, bounds[0]);   // min point
    V2_Subtract(dims, bounds[1], bounds[0]);

    // Calculate the dimensions of the blockmap.
    bMapWidth = ceil(dims[VX] / blockSize[VX]) + 1;
    bMapHeight = ceil(dims[VY] / blockSize[VY]) + 1;
    numBlocks = bMapWidth * bMapHeight;

    // Create the array of pointers on NBlocks to blocklists,
    // create an array of linelist counts on NBlocks, then finally,
    // make an array in which we can mark blocks done per line
    blocklists = M_Calloc(numBlocks * sizeof(linelist_t *));
    blockcount = M_Calloc(numBlocks * sizeof(uint));
    blockdone = M_Malloc(numBlocks * sizeof(uint));

    // Initialize each blocklist, and enter the trailing -1 in all blocklists.
    // NOTE: the linked list of lines grows backwards.
    for(i = 0; i < numBlocks; ++i)
    {
        blocklists[i] = M_Malloc(sizeof(linelist_t));
        blocklists[i]->num = -1;
        blocklists[i]->next = NULL;
        blockcount[i]++;
    }

    // For each linedef in the wad, determine all blockmap blocks it touches
    // and add the linedef number to the blocklists for those blocks.
    {
    int xorg = (int) bMapOrigin[VX];
    int yorg = (int) bMapOrigin[VY];

    for(i = 0; i < map->numlines; ++i)
    {
        line_t *line = &map->lines[i];
        int     v1[2] = {(int) line->v[0]->pos[VX],
                         (int) line->v[0]->pos[VY]};
        int     v2[2] = {(int) line->v[1]->pos[VX],
                         (int) line->v[1]->pos[VY]};
        int     dx = v2[VX] - v1[VX];
        int     dy = v2[VY] - v1[VY];
        int     vert = !dx;
        int     horiz = !dy;
        boolean slopePos = (dx ^ dy) > 0;
        boolean slopeNeg = (dx ^ dy) < 0;
        int     bx, by;
        // extremal lines[i] coords
        int     minx = (v1[VX] > v2[VX]? v2[VX] : v1[VX]);
        int     maxx = (v1[VX] > v2[VX]? v1[VX] : v2[VX]);
        int     miny = (v1[VY] > v2[VY]? v2[VY] : v1[VY]);
        int     maxy = (v1[VY] > v2[VY]? v1[VY] : v2[VY]);

        // no blocks done for this linedef yet
        memset(blockdone, 0, numBlocks * sizeof(uint));

        // The line always belongs to the blocks containing its endpoints
        bx = (v1[VX] - xorg) >> BLKSHIFT;
        by = (v1[VY] - yorg) >> BLKSHIFT;
        AddBlockLine(blocklists, blockcount, blockdone, by * bMapWidth + bx, i);

        bx = (v2[VX] - xorg) >> BLKSHIFT;
        by = (v2[VY] - yorg) >> BLKSHIFT;
        AddBlockLine(blocklists, blockcount, blockdone, by * bMapWidth + bx, i);

        // For each column, see where the line along its left edge, which
        // it contains, intersects the Linedef i. Add i to each corresponding
        // blocklist.
        // We don't want to interesect vertical lines with columns.
        if(!vert)
        {
            for(j = 0; j < bMapWidth; ++j)
            {
                // intersection of Linedef with x=xorg+(j<<BLKSHIFT)
                // (y-v1[VY])*dx = dy*(x-v1[VX])
                // y = dy*(x-v1[VX])+v1[VY]*dx;
                int     x = xorg + (j << BLKSHIFT);       // (x,y) is intersection
                int     y = (dy * (x - v1[VX])) / dx + v1[VY];
                int     yb = (y - yorg) >> BLKSHIFT;      // block row number
                int     yp = (y - yorg) & BLKMASK;        // y position within block

                // Already outside the blockmap?
                if(yb < 0 || yb > (bMapHeight - 1))
                    continue;

                // Does the line touch this column at all?
                if(x < minx || x > maxx)
                    continue;

                // The cell that contains the intersection point is always added
                AddBlockLine(blocklists, blockcount, blockdone, bMapWidth * yb + j, i);

                // If the intersection is at a corner it depends on the slope
                // (and whether the line extends past the intersection) which
                // blocks are hit.

                // Where does the intersection occur?
                if(yp == 0)
                {
                    // Intersection occured at a corner
                    if(slopeNeg) //   \ - blocks x,y-, x-,y
                    {
                        if(yb > 0 && miny < y)
                            AddBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * (yb - 1) + j, i);

                        if(j > 0 && minx < x)
                            AddBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * yb + j - 1, i);
                    }
                    else if(slopePos) //   / - block x-,y-
                    {
                        if(yb > 0 && j > 0 && minx < x)
                            AddBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * (yb - 1) + j - 1, i);
                    }
                    else if(horiz) //   - - block x-,y
                    {
                        if(j > 0 && minx < x)
                            AddBlockLine(blocklists, blockcount,
                                         blockdone, bMapWidth * yb + j - 1, i);
                    }
                }
                else if(j > 0 && minx < x)
                {
                    // Else not at corner: x-,y
                    AddBlockLine(blocklists, blockcount,
                                 blockdone, bMapWidth * yb + j - 1, i);
                }
            }
        }

        // For each row, see where the line along its bottom edge, which
        // it contains, intersects the Linedef i. Add i to all the corresponding
        // blocklists.
        if(!horiz)
        {
            for(j = 0; j < bMapHeight; ++j)
            {
                // intersection of Linedef with y=yorg+(j<<BLKSHIFT)
                // (x,y) on Linedef i satisfies: (y-v1[VY])*dx = dy*(x-v1[VX])
                // x = dx*(y-v1[VY])/dy+v1[VX];
                int     y = yorg + (j << BLKSHIFT);       // (x,y) is intersection
                int     x = (dx * (y - v1[VY])) / dy + v1[VX];
                int     xb = (x - xorg) >> BLKSHIFT;      // block column number
                int     xp = (x - xorg) & BLKMASK;        // x position within block

                // Outside the blockmap?
                if(xb < 0 || xb > bMapWidth - 1)
                    continue;

                // Touches this row?
                if(y < miny || y > maxy)
                    continue;

                // The cell that contains the intersection point is always added
                AddBlockLine(blocklists, blockcount, blockdone, bMapWidth * j + xb, i);

                // If the intersection is at a corner it depends on the slope
                // (and whether the line extends past the intersection) which
                // blocks are hit

                // Where does the intersection occur?
                if(xp == 0)
                {
                    // Intersection occured at a corner
                    if(slopeNeg) //   \ - blocks x,y-, x-,y
                    {
                        if(j > 0 && miny < y)
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb, i);
                        if(xb > 0 && minx < x)
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * j + xb - 1, i);
                    }
                    else if(vert) //   | - block x,y-
                    {
                        if(j > 0 && miny < y)
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb, i);
                    }
                    else if(slopePos) //   / - block x-,y-
                    {
                        if(xb > 0 && j > 0 && miny < y)
                            AddBlockLine(blocklists, blockcount, blockdone,
                                         bMapWidth * (j - 1) + xb -1, i);
                    }
                }
                else if(j > 0 && miny < y)
                {
                    // Else not on a corner: x, y-
                    AddBlockLine(blocklists, blockcount, blockdone,
                                 bMapWidth * (j - 1) + xb, i);
                }
            }
        }
    }
    }

    // Add initial 0 to all blocklists
    // count the total number of lines (and 0's and -1's)
    memset(blockdone, 0, numBlocks * sizeof(uint));
    for(i = 0, linetotal = 0; i < numBlocks; ++i)
    {
        AddBlockLine(blocklists, blockcount, blockdone, i, 0);
        linetotal += blockcount[i];
    }

    // Create the blockmap lump
    map->blockmaplump =
        Z_Malloc(sizeof(*map->blockmaplump) * (4 + numBlocks + linetotal),
                 PU_LEVELSTATIC, 0);
    // blockmap header
    map->blockmaplump[0] = map->bmaporgx = FLT2FIX(bMapOrigin[VX]);
    map->blockmaplump[1] = map->bmaporgy = FLT2FIX(bMapOrigin[VY]);
    map->blockmaplump[2] = map->bmapwidth  = bMapWidth;
    map->blockmaplump[3] = map->bmapheight = bMapHeight;

    // offsets to lists and block lists
    for(i = 0; i < numBlocks; ++i)
    {
        linelist_t *bl = blocklists[i];
        long    offs = map->blockmaplump[4 + i] =   // set offset to block's list
        (i? map->blockmaplump[4 + i - 1] : 4 + numBlocks) + (i? blockcount[i - 1] : 0);

        // add the lines in each block's list to the blockmaplump
        // delete each list node as we go
        while(bl)
        {
            linelist_t *tmp = bl->next;

            map->blockmaplump[offs++] = bl->num;
            M_Free(bl);
            bl = tmp;
        }
    }

    map->blockmap = map->blockmaplump + 4;

    // free all temporary storage
    M_Free(blocklists);
    M_Free(blockcount);
    M_Free(blockdone);
}

/*
 * Attempts to load the BLOCKMAP data resource.
 *
 * If the level is too large (would overflow the size limit of
 * the BLOCKMAP lump in a WAD therefore it will have been truncated),
 * it's zero length or we are forcing a rebuild - we'll have to
 * generate the blockmap data ourselves.
 */
static boolean P_LoadBlockMap(gamemap_t* map, mapdatalumpinfo_t* maplump)
{
    long i;
    long count = (maplump->length / 2);
    boolean generateBMap = (createBMap == 2)? true : false;

    // Do we have a lump to process?
    if(maplump->lumpNum == -1)
        generateBMap = true; // We'll HAVE to generate it.

    // Are we generating new blockmap data?
    if(generateBMap)
    {
        // Only announce if the user has choosen to always generate new data.
        // (As we will have already announced it if the lump was missing).
        if(maplump->lumpNum != -1)
            Con_Message("P_LoadBlockMap: Generating NEW blockmap...\n");

        P_CreateBlockMap(map);
    }
    else
    {
        // No, the existing data is valid - so load it in.
        // Data in PWAD is little endian.
        short *wadBlockMapLump;

        // Have we cached the lump yet?
        if(maplump->lumpp == NULL)
            maplump->lumpp = (byte *) W_CacheLumpNum(maplump->lumpNum, PU_STATIC);

        wadBlockMapLump = (short *) maplump->lumpp;

        map->blockmaplump =
            Z_Malloc(sizeof(*map->blockmaplump) * count, PU_LEVELSTATIC, 0);

        // Expand WAD blockmap into larger internal one, by treating all
        // offsets except -1 as unsigned and zero-extending them. This
        // potentially doubles the size of blockmaps allowed because DOOM
        // originally considered the offsets as always signed.
        map->blockmaplump[0] = SHORT(wadBlockMapLump[0]);
        map->blockmaplump[1] = SHORT(wadBlockMapLump[1]);
        map->blockmaplump[2] = (long)(SHORT(wadBlockMapLump[2])) & 0xffff;
        map->blockmaplump[3] = (long)(SHORT(wadBlockMapLump[3])) & 0xffff;

        for(i = 4; i < count; ++i)
        {
            short t = SHORT(wadBlockMapLump[i]);
            map->blockmaplump[i] = t == -1? -1 : (long) t & 0xffff;
        }

        map->bmaporgx = map->blockmaplump[0] << FRACBITS;
        map->bmaporgy = map->blockmaplump[1] << FRACBITS;
        map->bmapwidth = map->blockmaplump[2];
        map->bmapheight = map->blockmaplump[3];

        map->blockmap = map->blockmaplump + 4;
    }

    return true;
}

/*
 * Construct a REJECT LUT for the given map.
 *
 * TODO: We could generate a proper table if a suitable one is
 *       not made available to us, currently this simply creates
 *       an empty table (zero fill).
 */
static void P_CreateReject(gamemap_t* map)
{
    size_t requiredLength = (((map->numsectors*map->numsectors) + 7) & ~7) /8;

    if(createReject)
    {
        // Simply generate an empty REJECT LUT for now.
        map->rejectmatrix = Z_Malloc(requiredLength, PU_LEVELSTATIC, 0);
        memset(map->rejectmatrix, 0, requiredLength);
        // TODO: Generate valid REJECT for the map.
    }
    else
        map->rejectmatrix = NULL;
}

/*
 * Attempt to load the REJECT.
 *
 * If a lump is not found, we'll generate an empty REJECT LUT.
 *
 * The REJECT resource is a LUT that provides the results of
 * trivial line-of-sight tests between sectors. This is done
 * with a matrix of sector pairs ie if a monster in sector 4
 * can see the player in sector 2 - the inverse should be true.
 *
 * NOTE: Some PWADS have carefully constructed REJECT data to
 *       create special effects. For example it is possible to
 *       make a player completely invissible in certain sectors.
 *
 * The format of the table is a simple matrix of boolean values,
 * a (true) value indicates that it is impossible for mobjs in
 * sector A to see mobjs in sector B (and vice-versa). A (false)
 * value indicates that a line-of-sight MIGHT be possible and a
 * more accurate (thus more expensive) calculation will have to
 * be made.
 *
 * The table itself is constructed as follows:
 *
 *  X = sector num player is in
 *  Y = sector num monster is in
 *
 *         X
 *
 *       0 1 2 3 4 ->
 *     0 1 - 1 - -
 *  Y  1 - - 1 - -
 *     2 1 1 - - 1
 *     3 - - - 1 -
 *    \|/
 *
 * These results are read left-to-right, top-to-bottom and are
 * packed into bytes (each byte represents eight results).
 * As are all lumps in WAD the data is in little-endian order.
 *
 * Thus the size of a valid REJECT lump can be calculated as:
 *
 *   ceiling(numsectors^2)
 */

/*
 * This is temporary. Ideally reject data should be loaded with
 * P_ReadBinaryMapData but not treated as an aggregate data type.
 * We should only need this func if we have to generate data.
 */
static boolean P_LoadReject(gamemap_t* map, mapdatalumpinfo_t* maplump)
{
    boolean generateReject = (createReject == 2)? true : false;

    // Do we have a lump to process?
    if(maplump->lumpNum == -1)
         generateReject = true; // We'll HAVE to generate it.

    // Are we generating new reject data?
    if(generateReject)
    {
        // Only announce if the user has choosen to always generate new data.
        // (As we will have already announced it if the lump was missing).
        if(maplump->lumpNum != -1)
            Con_Message("P_LoadBlockMap: Generating NEW reject...\n");

        P_CreateReject(map);
    }
    else
    {
        // Have we cached the lump yet?
        if(maplump->lumpp == NULL)
            maplump->lumpp = (byte *) W_CacheLumpNum(maplump->lumpNum, PU_STATIC);

        map->rejectmatrix = Z_Malloc(maplump->length, PU_LEVELSTATIC, 0);
        memcpy(map->rejectmatrix, maplump->lumpp, maplump->length);
    }

    // Success!
    return true;
}

/*
 * The DED for the game.dll should tell doomsday what
 * data maps to which internal data value, what size
 * the data item is etc.
 *
 * TODO: ALL of this can be moved to a DED
 *
 * TEMP the initialization of internal data struct info
 * is currently done here. (FIXME!!!: It isn't free'd on exit!)
 */
void P_InitMapDataFormats(void)
{
    int i, j, index, lumpClass;
    int mlver, glver;
    size_t *mlptr = NULL;
    size_t *glptr = NULL;
    mapdataformat_t *stiptr = NULL;
    glnodeformat_t *glstiptr = NULL;

    // Setup the map data formats for the different
    // versions of the map data structs.

    // Calculate the size of the map data structs
    for(i = MAPDATA_FORMATS -1; i >= 0; --i)
    {
        for(j = 0; j < NUM_LUMPCLASSES; ++j)
        {
            lumpClass = mapLumpInfo[j].lumpclass;
            index = mapLumpInfo[j].mdLump;

            mlver = (mapDataFormats[i].verInfo[index].version);
            mlptr = &(mapDataFormats[i].verInfo[index].elmSize);
            stiptr = &(mapDataFormats[i]);

            if(lumpClass == LCM_THINGS)
            {
                if(mlver == 1)  // DOOM Format
                {
                    *mlptr = 10;
                    stiptr->verInfo[index].numProps = 5;
                    stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 5, PU_STATIC, 0);
                    // x
                    stiptr->verInfo[index].props[0].id = DAM_THING_X;
                    stiptr->verInfo[index].props[0].flags = 0;
                    stiptr->verInfo[index].props[0].size =  2;
                    stiptr->verInfo[index].props[0].offset = 0;
                    stiptr->verInfo[index].props[0].gameprop = 1;
                    // y
                    stiptr->verInfo[index].props[1].id = DAM_THING_Y;
                    stiptr->verInfo[index].props[1].flags = 0;
                    stiptr->verInfo[index].props[1].size =  2;
                    stiptr->verInfo[index].props[1].offset = 2;
                    stiptr->verInfo[index].props[1].gameprop = 1;
                    // angle
                    stiptr->verInfo[index].props[2].id = DAM_THING_ANGLE;
                    stiptr->verInfo[index].props[2].flags = 0;
                    stiptr->verInfo[index].props[2].size =  2;
                    stiptr->verInfo[index].props[2].offset = 4;
                    stiptr->verInfo[index].props[2].gameprop = 1;
                    // type
                    stiptr->verInfo[index].props[3].id = DAM_THING_TYPE;
                    stiptr->verInfo[index].props[3].flags = 0;
                    stiptr->verInfo[index].props[3].size =  2;
                    stiptr->verInfo[index].props[3].offset = 6;
                    stiptr->verInfo[index].props[3].gameprop = 1;
                    // options
                    stiptr->verInfo[index].props[4].id = DAM_THING_OPTIONS;
                    stiptr->verInfo[index].props[4].flags = 0;
                    stiptr->verInfo[index].props[4].size =  2;
                    stiptr->verInfo[index].props[4].offset = 8;
                    stiptr->verInfo[index].props[4].gameprop = 1;
                }
                else            // HEXEN format
                {
                    *mlptr = 20;
                    stiptr->verInfo[index].numProps = 13;
                    stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 13, PU_STATIC, 0);
                    // tid
                    stiptr->verInfo[index].props[0].id = DAM_THING_TID;
                    stiptr->verInfo[index].props[0].flags = 0;
                    stiptr->verInfo[index].props[0].size =  2;
                    stiptr->verInfo[index].props[0].offset = 0;
                    stiptr->verInfo[index].props[0].gameprop = 1;
                    // x
                    stiptr->verInfo[index].props[1].id = DAM_THING_X;
                    stiptr->verInfo[index].props[1].flags = 0;
                    stiptr->verInfo[index].props[1].size =  2;
                    stiptr->verInfo[index].props[1].offset = 2;
                    stiptr->verInfo[index].props[1].gameprop = 1;
                    // y
                    stiptr->verInfo[index].props[2].id = DAM_THING_Y;
                    stiptr->verInfo[index].props[2].flags = 0;
                    stiptr->verInfo[index].props[2].size =  2;
                    stiptr->verInfo[index].props[2].offset = 4;
                    stiptr->verInfo[index].props[2].gameprop = 1;
                    // height
                    stiptr->verInfo[index].props[3].id = DAM_THING_HEIGHT;
                    stiptr->verInfo[index].props[3].flags = 0;
                    stiptr->verInfo[index].props[3].size =  2;
                    stiptr->verInfo[index].props[3].offset = 6;
                    stiptr->verInfo[index].props[3].gameprop = 1;
                    // angle
                    stiptr->verInfo[index].props[4].id = DAM_THING_ANGLE;
                    stiptr->verInfo[index].props[4].flags = 0;
                    stiptr->verInfo[index].props[4].size =  2;
                    stiptr->verInfo[index].props[4].offset = 8;
                    stiptr->verInfo[index].props[4].gameprop = 1;
                    // type
                    stiptr->verInfo[index].props[5].id = DAM_THING_TYPE;
                    stiptr->verInfo[index].props[5].flags = 0;
                    stiptr->verInfo[index].props[5].size =  2;
                    stiptr->verInfo[index].props[5].offset = 10;
                    stiptr->verInfo[index].props[5].gameprop = 1;
                    // options
                    stiptr->verInfo[index].props[6].id = DAM_THING_OPTIONS;
                    stiptr->verInfo[index].props[6].flags = 0;
                    stiptr->verInfo[index].props[6].size =  2;
                    stiptr->verInfo[index].props[6].offset = 12;
                    stiptr->verInfo[index].props[6].gameprop = 1;
                    // special
                    stiptr->verInfo[index].props[7].id = DAM_THING_SPECIAL;
                    stiptr->verInfo[index].props[7].flags = 0;
                    stiptr->verInfo[index].props[7].size =  1;
                    stiptr->verInfo[index].props[7].offset = 14;
                    stiptr->verInfo[index].props[7].gameprop = 1;
                    // arg1
                    stiptr->verInfo[index].props[8].id = DAM_THING_ARG1;
                    stiptr->verInfo[index].props[8].flags = 0;
                    stiptr->verInfo[index].props[8].size =  1;
                    stiptr->verInfo[index].props[8].offset = 15;
                    stiptr->verInfo[index].props[8].gameprop = 1;
                    // arg2
                    stiptr->verInfo[index].props[9].id = DAM_THING_ARG2;
                    stiptr->verInfo[index].props[9].flags = 0;
                    stiptr->verInfo[index].props[9].size =  1;
                    stiptr->verInfo[index].props[9].offset = 16;
                    stiptr->verInfo[index].props[9].gameprop = 1;
                    // arg3
                    stiptr->verInfo[index].props[10].id = DAM_THING_ARG3;
                    stiptr->verInfo[index].props[10].flags = 0;
                    stiptr->verInfo[index].props[10].size =  1;
                    stiptr->verInfo[index].props[10].offset = 17;
                    stiptr->verInfo[index].props[10].gameprop = 1;
                    // arg4
                    stiptr->verInfo[index].props[11].id = DAM_THING_ARG4;
                    stiptr->verInfo[index].props[11].flags = 0;
                    stiptr->verInfo[index].props[11].size =  1;
                    stiptr->verInfo[index].props[11].offset = 18;
                    stiptr->verInfo[index].props[11].gameprop = 1;
                    // arg5
                    stiptr->verInfo[index].props[12].id = DAM_THING_ARG5;
                    stiptr->verInfo[index].props[12].flags = 0;
                    stiptr->verInfo[index].props[12].size =  1;
                    stiptr->verInfo[index].props[12].offset = 19;
                    stiptr->verInfo[index].props[12].gameprop = 1;
                }
            }
            else if(lumpClass == LCM_LINEDEFS)
            {
                if(mlver == 1)  // DOOM format
                {
                    *mlptr = 14;
                    stiptr->verInfo[index].numProps = 7;
                    stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 7, PU_STATIC, 0);
                    // v1
                    stiptr->verInfo[index].props[0].id = DAM_VERTEX1;
                    stiptr->verInfo[index].props[0].flags = DT_UNSIGNED;
                    stiptr->verInfo[index].props[0].size =  2;
                    stiptr->verInfo[index].props[0].offset = 0;
                    stiptr->verInfo[index].props[0].gameprop = 0;
                    // v2
                    stiptr->verInfo[index].props[1].id = DAM_VERTEX2;
                    stiptr->verInfo[index].props[1].flags = DT_UNSIGNED;
                    stiptr->verInfo[index].props[1].size =  2;
                    stiptr->verInfo[index].props[1].offset = 2;
                    stiptr->verInfo[index].props[1].gameprop = 0;
                    // flags
                    stiptr->verInfo[index].props[2].id = DAM_FLAGS;
                    stiptr->verInfo[index].props[2].flags = 0;
                    stiptr->verInfo[index].props[2].size =  2;
                    stiptr->verInfo[index].props[2].offset = 4;
                    stiptr->verInfo[index].props[2].gameprop = 0;
                    // special
                    stiptr->verInfo[index].props[3].id = DAM_LINE_SPECIAL;
                    stiptr->verInfo[index].props[3].flags = 0;
                    stiptr->verInfo[index].props[3].size =  2;
                    stiptr->verInfo[index].props[3].offset = 6;
                    stiptr->verInfo[index].props[3].gameprop = 1;
                    // tag
                    stiptr->verInfo[index].props[4].id = DAM_LINE_TAG;
                    stiptr->verInfo[index].props[4].flags = 0;
                    stiptr->verInfo[index].props[4].size =  2;
                    stiptr->verInfo[index].props[4].offset = 8;
                    stiptr->verInfo[index].props[4].gameprop = 1;
                    // front side
                    stiptr->verInfo[index].props[5].id = DAM_SIDE0;
                    stiptr->verInfo[index].props[5].flags = DT_NOINDEX;
                    stiptr->verInfo[index].props[5].size =  2;
                    stiptr->verInfo[index].props[5].offset = 10;
                    stiptr->verInfo[index].props[5].gameprop = 0;
                    // back side
                    stiptr->verInfo[index].props[6].id = DAM_SIDE1;
                    stiptr->verInfo[index].props[6].flags = DT_NOINDEX;
                    stiptr->verInfo[index].props[6].size =  2;
                    stiptr->verInfo[index].props[6].offset = 12;
                    stiptr->verInfo[index].props[6].gameprop = 0;
                }
                else            // HEXEN format
                {
                    *mlptr = 16;
                    stiptr->verInfo[index].numProps = 11;
                    stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 11, PU_STATIC, 0);
                    // v1
                    stiptr->verInfo[index].props[0].id = DAM_VERTEX1;
                    stiptr->verInfo[index].props[0].flags = DT_UNSIGNED;
                    stiptr->verInfo[index].props[0].size =  2;
                    stiptr->verInfo[index].props[0].offset = 0;
                    stiptr->verInfo[index].props[0].gameprop = 0;
                    // v2
                    stiptr->verInfo[index].props[1].id = DAM_VERTEX2;
                    stiptr->verInfo[index].props[1].flags = DT_UNSIGNED;
                    stiptr->verInfo[index].props[1].size =  2;
                    stiptr->verInfo[index].props[1].offset = 2;
                    stiptr->verInfo[index].props[1].gameprop = 0;
                    // flags
                    stiptr->verInfo[index].props[2].id = DAM_FLAGS;
                    stiptr->verInfo[index].props[2].flags = 0;
                    stiptr->verInfo[index].props[2].size =  2;
                    stiptr->verInfo[index].props[2].offset = 4;
                    stiptr->verInfo[index].props[2].gameprop = 0;
                    // special
                    stiptr->verInfo[index].props[3].id = DAM_LINE_SPECIAL;
                    stiptr->verInfo[index].props[3].flags = 0;
                    stiptr->verInfo[index].props[3].size =  1;
                    stiptr->verInfo[index].props[3].offset = 6;
                    stiptr->verInfo[index].props[3].gameprop = 1;
                    // arg1
                    stiptr->verInfo[index].props[4].id = DAM_LINE_ARG1;
                    stiptr->verInfo[index].props[4].flags = 0;
                    stiptr->verInfo[index].props[4].size =  1;
                    stiptr->verInfo[index].props[4].offset = 7;
                    stiptr->verInfo[index].props[4].gameprop = 1;
                    // arg2
                    stiptr->verInfo[index].props[5].id = DAM_LINE_ARG2;
                    stiptr->verInfo[index].props[5].flags = 0;
                    stiptr->verInfo[index].props[5].size =  1;
                    stiptr->verInfo[index].props[5].offset = 8;
                    stiptr->verInfo[index].props[5].gameprop = 1;
                    // arg3
                    stiptr->verInfo[index].props[6].id = DAM_LINE_ARG3;
                    stiptr->verInfo[index].props[6].flags = 0;
                    stiptr->verInfo[index].props[6].size =  1;
                    stiptr->verInfo[index].props[6].offset = 9;
                    stiptr->verInfo[index].props[6].gameprop = 1;
                    // arg4
                    stiptr->verInfo[index].props[7].id = DAM_LINE_ARG4;
                    stiptr->verInfo[index].props[7].flags = 0;
                    stiptr->verInfo[index].props[7].size =  1;
                    stiptr->verInfo[index].props[7].offset = 10;
                    stiptr->verInfo[index].props[7].gameprop = 1;
                    // arg5
                    stiptr->verInfo[index].props[8].id = DAM_LINE_ARG5;
                    stiptr->verInfo[index].props[8].flags = 0;
                    stiptr->verInfo[index].props[8].size =  1;
                    stiptr->verInfo[index].props[8].offset = 11;
                    stiptr->verInfo[index].props[8].gameprop = 1;
                    // front side
                    stiptr->verInfo[index].props[9].id = DAM_SIDE0;
                    stiptr->verInfo[index].props[9].flags = DT_NOINDEX;
                    stiptr->verInfo[index].props[9].size =  2;
                    stiptr->verInfo[index].props[9].offset = 12;
                    stiptr->verInfo[index].props[9].gameprop = 0;
                    // back side
                    stiptr->verInfo[index].props[10].id = DAM_SIDE1;
                    stiptr->verInfo[index].props[10].flags = DT_NOINDEX;
                    stiptr->verInfo[index].props[10].size =  2;
                    stiptr->verInfo[index].props[10].offset = 14;
                    stiptr->verInfo[index].props[10].gameprop = 0;
                }
            }
            else if(lumpClass == LCM_SIDEDEFS)
            {
                *mlptr = 30;
                stiptr->verInfo[index].numProps = 10;
                stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 10, PU_STATIC, 0);
                // DOOM format maps don't support per wall section offsets so, we'll
                // read the one set of x, y offsets into the internal top, middle and
                // bottom offsets.
                // top x offset
                stiptr->verInfo[index].props[0].id = DAM_TOP_TEXTURE_OFFSET_X;
                stiptr->verInfo[index].props[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[0].size =  2;
                stiptr->verInfo[index].props[0].offset = 0;
                stiptr->verInfo[index].props[0].gameprop = 0;
                // top y offset
                stiptr->verInfo[index].props[1].id = DAM_TOP_TEXTURE_OFFSET_Y;
                stiptr->verInfo[index].props[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[1].size =  2;
                stiptr->verInfo[index].props[1].offset = 2;
                stiptr->verInfo[index].props[1].gameprop = 0;
                // middle x offset
                stiptr->verInfo[index].props[2].id = DAM_MIDDLE_TEXTURE_OFFSET_X;
                stiptr->verInfo[index].props[2].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[2].size =  2;
                stiptr->verInfo[index].props[2].offset = 0;
                stiptr->verInfo[index].props[2].gameprop = 0;
                // middle y offset
                stiptr->verInfo[index].props[3].id = DAM_MIDDLE_TEXTURE_OFFSET_Y;
                stiptr->verInfo[index].props[3].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[3].size =  2;
                stiptr->verInfo[index].props[3].offset = 2;
                stiptr->verInfo[index].props[3].gameprop = 0;
                // bottom x offset
                stiptr->verInfo[index].props[4].id = DAM_BOTTOM_TEXTURE_OFFSET_X;
                stiptr->verInfo[index].props[4].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[4].size =  2;
                stiptr->verInfo[index].props[4].offset = 0;
                stiptr->verInfo[index].props[4].gameprop = 0;
                // bottom y offset
                stiptr->verInfo[index].props[5].id = DAM_BOTTOM_TEXTURE_OFFSET_Y;
                stiptr->verInfo[index].props[5].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[5].size =  2;
                stiptr->verInfo[index].props[5].offset = 2;
                stiptr->verInfo[index].props[5].gameprop = 0;
                // top pic
                stiptr->verInfo[index].props[6].id = DAM_TOP_TEXTURE;
                stiptr->verInfo[index].props[6].flags = DT_TEXTURE;
                stiptr->verInfo[index].props[6].size =  8;
                stiptr->verInfo[index].props[6].offset = 4;
                stiptr->verInfo[index].props[6].gameprop = 0;
                // bottom pic
                stiptr->verInfo[index].props[7].id = DAM_BOTTOM_TEXTURE;
                stiptr->verInfo[index].props[7].flags = DT_TEXTURE;
                stiptr->verInfo[index].props[7].size =  8;
                stiptr->verInfo[index].props[7].offset = 12;
                stiptr->verInfo[index].props[7].gameprop = 0;
                // middle pic
                stiptr->verInfo[index].props[8].id = DAM_MIDDLE_TEXTURE;
                stiptr->verInfo[index].props[8].flags = DT_TEXTURE;
                stiptr->verInfo[index].props[8].size =  8;
                stiptr->verInfo[index].props[8].offset = 20;
                stiptr->verInfo[index].props[8].gameprop = 0;
                // sector
                stiptr->verInfo[index].props[9].id = DAM_FRONT_SECTOR;
                stiptr->verInfo[index].props[9].flags = 0;
                stiptr->verInfo[index].props[9].size =  2;
                stiptr->verInfo[index].props[9].offset = 28;
                stiptr->verInfo[index].props[9].gameprop = 0;
            }
            else if(lumpClass == LCM_VERTEXES)
            {
                *mlptr = 4;
                stiptr->verInfo[index].numProps = 2;
                stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);
                // x
                stiptr->verInfo[index].props[0].id = DAM_X;
                stiptr->verInfo[index].props[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[0].size =  2;
                stiptr->verInfo[index].props[0].offset = 0;
                stiptr->verInfo[index].props[0].gameprop = 0;
                // y
                stiptr->verInfo[index].props[1].id = DAM_Y;
                stiptr->verInfo[index].props[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[1].size =  2;
                stiptr->verInfo[index].props[1].offset = 2;
                stiptr->verInfo[index].props[1].gameprop = 0;
            }
            else if(lumpClass == LCM_SEGS)
            {
                *mlptr = 12;
                stiptr->verInfo[index].numProps = 6;
                stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 6, PU_STATIC, 0);
                // v1
                stiptr->verInfo[index].props[0].id = DAM_VERTEX1;
                stiptr->verInfo[index].props[0].flags = DT_UNSIGNED;
                stiptr->verInfo[index].props[0].size =  2;
                stiptr->verInfo[index].props[0].offset = 0;
                stiptr->verInfo[index].props[0].gameprop = 0;
                // v2
                stiptr->verInfo[index].props[1].id = DAM_VERTEX2;
                stiptr->verInfo[index].props[1].flags = DT_UNSIGNED;
                stiptr->verInfo[index].props[1].size =  2;
                stiptr->verInfo[index].props[1].offset = 2;
                stiptr->verInfo[index].props[1].gameprop = 0;
                // angle
                stiptr->verInfo[index].props[2].id = DAM_ANGLE;
                stiptr->verInfo[index].props[2].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[2].size =  2;
                stiptr->verInfo[index].props[2].offset = 4;
                stiptr->verInfo[index].props[2].gameprop = 0;
                // linedef
                stiptr->verInfo[index].props[3].id = DAM_LINE;
                stiptr->verInfo[index].props[3].flags = DT_NOINDEX;
                stiptr->verInfo[index].props[3].size =  2;
                stiptr->verInfo[index].props[3].offset = 6;
                stiptr->verInfo[index].props[3].gameprop = 0;
                // side
                stiptr->verInfo[index].props[4].id = DAM_SIDE;
                stiptr->verInfo[index].props[4].flags = 0;
                stiptr->verInfo[index].props[4].size =  2;
                stiptr->verInfo[index].props[4].offset = 8;
                stiptr->verInfo[index].props[4].gameprop = 0;
                // offset
                stiptr->verInfo[index].props[5].id = DAM_OFFSET;
                stiptr->verInfo[index].props[5].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[5].size =  2;
                stiptr->verInfo[index].props[5].offset = 10;
                stiptr->verInfo[index].props[5].gameprop = 0;
            }
            else if(lumpClass == LCM_SUBSECTORS)
            {
                *mlptr = 4;
                stiptr->verInfo[index].numProps = 2;
                stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                stiptr->verInfo[index].props[0].id = DAM_LINE_COUNT;
                stiptr->verInfo[index].props[0].flags = DT_UNSIGNED;
                stiptr->verInfo[index].props[0].size =  2;
                stiptr->verInfo[index].props[0].offset = 0;
                stiptr->verInfo[index].props[0].gameprop = 0;

                stiptr->verInfo[index].props[1].id = DAM_LINE_FIRST;
                stiptr->verInfo[index].props[1].flags = DT_UNSIGNED;
                stiptr->verInfo[index].props[1].size =  2;
                stiptr->verInfo[index].props[1].offset = 2;
                stiptr->verInfo[index].props[1].gameprop = 0;
            }
            else if(lumpClass == LCM_NODES)
            {
                *mlptr = 28;
                stiptr->verInfo[index].numProps = 14;
                stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 14, PU_STATIC, 0);
                // x
                stiptr->verInfo[index].props[0].id = DAM_X;
                stiptr->verInfo[index].props[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[0].size =  2;
                stiptr->verInfo[index].props[0].offset = 0;
                stiptr->verInfo[index].props[0].gameprop = 0;
                // y
                stiptr->verInfo[index].props[1].id = DAM_Y;
                stiptr->verInfo[index].props[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[1].size =  2;
                stiptr->verInfo[index].props[1].offset = 2;
                stiptr->verInfo[index].props[1].gameprop = 0;
                // dx
                stiptr->verInfo[index].props[2].id = DAM_DX;
                stiptr->verInfo[index].props[2].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[2].size =  2;
                stiptr->verInfo[index].props[2].offset = 4;
                stiptr->verInfo[index].props[2].gameprop = 0;
                // dy
                stiptr->verInfo[index].props[3].id = DAM_DY;
                stiptr->verInfo[index].props[3].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[3].size =  2;
                stiptr->verInfo[index].props[3].offset = 6;
                stiptr->verInfo[index].props[3].gameprop = 0;
                // bbox[0][0]
                stiptr->verInfo[index].props[4].id = DAM_BBOX_RIGHT_TOP_Y;
                stiptr->verInfo[index].props[4].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[4].size =  2;
                stiptr->verInfo[index].props[4].offset = 8;
                stiptr->verInfo[index].props[4].gameprop = 0;
                // bbox[0][1]
                stiptr->verInfo[index].props[5].id = DAM_BBOX_RIGHT_LOW_Y;
                stiptr->verInfo[index].props[5].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[5].size =  2;
                stiptr->verInfo[index].props[5].offset = 10;
                stiptr->verInfo[index].props[5].gameprop = 0;
                // bbox[0][2]
                stiptr->verInfo[index].props[6].id = DAM_BBOX_RIGHT_LOW_X;
                stiptr->verInfo[index].props[6].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[6].size =  2;
                stiptr->verInfo[index].props[6].offset = 12;
                stiptr->verInfo[index].props[6].gameprop = 0;
                // bbox[0][3]
                stiptr->verInfo[index].props[7].id = DAM_BBOX_RIGHT_TOP_X;
                stiptr->verInfo[index].props[7].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[7].size =  2;
                stiptr->verInfo[index].props[7].offset = 14;
                stiptr->verInfo[index].props[7].gameprop = 0;
                // bbox[1][0]
                stiptr->verInfo[index].props[8].id = DAM_BBOX_LEFT_TOP_Y;
                stiptr->verInfo[index].props[8].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[8].size =  2;
                stiptr->verInfo[index].props[8].offset = 16;
                stiptr->verInfo[index].props[8].gameprop = 0;
                // bbox[1][1]
                stiptr->verInfo[index].props[9].id = DAM_BBOX_LEFT_LOW_Y;
                stiptr->verInfo[index].props[9].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[9].size =  2;
                stiptr->verInfo[index].props[9].offset = 18;
                stiptr->verInfo[index].props[9].gameprop = 0;
                // bbox[1][2]
                stiptr->verInfo[index].props[10].id = DAM_BBOX_LEFT_LOW_X;
                stiptr->verInfo[index].props[10].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[10].size =  2;
                stiptr->verInfo[index].props[10].offset = 20;
                stiptr->verInfo[index].props[10].gameprop = 0;
                // bbox[1][3]
                stiptr->verInfo[index].props[11].id = DAM_BBOX_LEFT_TOP_X;
                stiptr->verInfo[index].props[11].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[11].size =  2;
                stiptr->verInfo[index].props[11].offset = 22;
                stiptr->verInfo[index].props[11].gameprop = 0;
                // children[0]
                stiptr->verInfo[index].props[12].id = DAM_CHILD_RIGHT;
                stiptr->verInfo[index].props[12].flags = DT_MSBCONVERT;
                stiptr->verInfo[index].props[12].size =  2;
                stiptr->verInfo[index].props[12].offset = 24;
                stiptr->verInfo[index].props[12].gameprop = 0;
                // children[1]
                stiptr->verInfo[index].props[13].id = DAM_CHILD_LEFT;
                stiptr->verInfo[index].props[13].flags = DT_MSBCONVERT;
                stiptr->verInfo[index].props[13].size =  2;
                stiptr->verInfo[index].props[13].offset = 26;
                stiptr->verInfo[index].props[13].gameprop = 0;
            }
            else if(lumpClass == LCM_SECTORS)
            {
                *mlptr = 26;
                stiptr->verInfo[index].numProps = 7;
                stiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 7, PU_STATIC, 0);
                // floor height
                stiptr->verInfo[index].props[0].id = DAM_FLOOR_HEIGHT;
                stiptr->verInfo[index].props[0].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[0].size =  2;
                stiptr->verInfo[index].props[0].offset = 0;
                stiptr->verInfo[index].props[0].gameprop = 0;
                // ceiling height
                stiptr->verInfo[index].props[1].id = DAM_CEILING_HEIGHT;
                stiptr->verInfo[index].props[1].flags = DT_FRACBITS;
                stiptr->verInfo[index].props[1].size =  2;
                stiptr->verInfo[index].props[1].offset = 2;
                stiptr->verInfo[index].props[1].gameprop = 0;
                // floor pic
                stiptr->verInfo[index].props[2].id = DAM_FLOOR_TEXTURE;
                stiptr->verInfo[index].props[2].flags = DT_FLAT;
                stiptr->verInfo[index].props[2].size =  8;
                stiptr->verInfo[index].props[2].offset = 4;
                stiptr->verInfo[index].props[2].gameprop = 0;
                // ceiling pic
                stiptr->verInfo[index].props[3].id = DAM_CEILING_TEXTURE;
                stiptr->verInfo[index].props[3].flags = DT_FLAT;
                stiptr->verInfo[index].props[3].size =  8;
                stiptr->verInfo[index].props[3].offset = 12;
                stiptr->verInfo[index].props[3].gameprop = 0;
                // light level
                stiptr->verInfo[index].props[4].id = DAM_LIGHT_LEVEL;
                stiptr->verInfo[index].props[4].flags = 0;
                stiptr->verInfo[index].props[4].size =  2;
                stiptr->verInfo[index].props[4].offset = 20;
                stiptr->verInfo[index].props[4].gameprop = 0;
                // special
                stiptr->verInfo[index].props[5].id = DAM_SECTOR_SPECIAL;
                stiptr->verInfo[index].props[5].flags = 0;
                stiptr->verInfo[index].props[5].size =  2;
                stiptr->verInfo[index].props[5].offset = 22;
                stiptr->verInfo[index].props[5].gameprop = 1;
                // tag
                stiptr->verInfo[index].props[6].id = DAM_SECTOR_TAG;
                stiptr->verInfo[index].props[6].flags = 0;
                stiptr->verInfo[index].props[6].size =  2;
                stiptr->verInfo[index].props[6].offset = 24;
                stiptr->verInfo[index].props[6].gameprop = 1;
            }
            else if(lumpClass == LCM_REJECT)
            {
                *mlptr = 1;
            }
            else if(lumpClass == LCM_BLOCKMAP)
            {
                *mlptr = 1;
            }
        }
    }

    // Calculate the size of the gl node structs
    for(i = GLNODE_FORMATS -1; i >= 0; --i)
    {
        for(j = 0; j < NUM_LUMPCLASSES; ++j)
        {
            lumpClass = mapLumpInfo[j].lumpclass;
            index = mapLumpInfo[j].glLump;

            glver = (glNodeFormats[i].verInfo[index].version);
            glptr = &(glNodeFormats[i].verInfo[index].elmSize);
            glstiptr = &(glNodeFormats[i]);

            if(lumpClass == LCG_VERTEXES)
            {
                if(glver == 1)
                {
                    *glptr = 4;
                    glstiptr->verInfo[index].numProps = 2;
                    glstiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].props[0].id = DAM_X;
                    glstiptr->verInfo[index].props[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[0].size =  2;
                    glstiptr->verInfo[index].props[0].offset = 0;
                    glstiptr->verInfo[index].props[0].gameprop = 0;
                    // y
                    glstiptr->verInfo[index].props[1].id = DAM_Y;
                    glstiptr->verInfo[index].props[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[1].size =  2;
                    glstiptr->verInfo[index].props[1].offset = 2;
                    glstiptr->verInfo[index].props[1].gameprop = 0;
                }
                else
                {
                    *glptr = 8;
                    glstiptr->verInfo[index].numProps = 2;
                    glstiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].props[0].id = DAM_X;
                    glstiptr->verInfo[index].props[0].flags = 0;
                    glstiptr->verInfo[index].props[0].size =  4;
                    glstiptr->verInfo[index].props[0].offset = 0;
                    glstiptr->verInfo[index].props[0].gameprop = 0;
                    // y
                    glstiptr->verInfo[index].props[1].id = DAM_Y;
                    glstiptr->verInfo[index].props[1].flags = 0;
                    glstiptr->verInfo[index].props[1].size =  4;
                    glstiptr->verInfo[index].props[1].offset = 4;
                    glstiptr->verInfo[index].props[1].gameprop = 0;
                }
            }
            else if(lumpClass == LCG_SEGS)
            {
                if(glver == 2)
                {
                    *glptr = 10;
                    glstiptr->verInfo[index].numProps = 4;
                    glstiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 4, PU_STATIC, 0);
                    // v1
                    glstiptr->verInfo[index].props[0].id = DAM_VERTEX1;
                    glstiptr->verInfo[index].props[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[0].size =  2;
                    glstiptr->verInfo[index].props[0].offset = 0;
                    glstiptr->verInfo[index].props[0].gameprop = 0;
                    // v2
                    glstiptr->verInfo[index].props[1].id = DAM_VERTEX2;
                    glstiptr->verInfo[index].props[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[1].size =  2;
                    glstiptr->verInfo[index].props[1].offset = 2;
                    glstiptr->verInfo[index].props[1].gameprop = 0;
                    // linedef
                    glstiptr->verInfo[index].props[2].id = DAM_LINE;
                    glstiptr->verInfo[index].props[2].flags = DT_NOINDEX;
                    glstiptr->verInfo[index].props[2].size =  2;
                    glstiptr->verInfo[index].props[2].offset = 4;
                    glstiptr->verInfo[index].props[2].gameprop = 0;
                    // side
                    glstiptr->verInfo[index].props[3].id = DAM_SIDE;
                    glstiptr->verInfo[index].props[3].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[3].size =  2;
                    glstiptr->verInfo[index].props[3].offset = 6;
                    glstiptr->verInfo[index].props[3].gameprop = 0;
                }
                else if(glver == 4)
                {
                    *glptr = 0;        // Unsupported atm
                }
                else // Ver 3/5
                {
                    *glptr = 14;
                    glstiptr->verInfo[index].numProps = 4;
                    glstiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 4, PU_STATIC, 0);
                    // v1
                    glstiptr->verInfo[index].props[0].id = DAM_VERTEX1;
                    glstiptr->verInfo[index].props[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[0].size =  4;
                    glstiptr->verInfo[index].props[0].offset = 0;
                    glstiptr->verInfo[index].props[0].gameprop = 0;
                    // v2
                    glstiptr->verInfo[index].props[1].id = DAM_VERTEX2;
                    glstiptr->verInfo[index].props[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[1].size =  4;
                    glstiptr->verInfo[index].props[1].offset = 4;
                    glstiptr->verInfo[index].props[1].gameprop = 0;
                    // linedef
                    glstiptr->verInfo[index].props[2].id = DAM_LINE;
                    glstiptr->verInfo[index].props[2].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[2].size =  2;
                    glstiptr->verInfo[index].props[2].offset = 8;
                    glstiptr->verInfo[index].props[2].gameprop = 0;
                    // side
                    glstiptr->verInfo[index].props[3].id = DAM_SIDE;
                    glstiptr->verInfo[index].props[3].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[3].size =  2;
                    glstiptr->verInfo[index].props[3].offset = 10;
                    glstiptr->verInfo[index].props[3].gameprop = 0;
                }
            }
            else if(lumpClass == LCG_SUBSECTORS)
            {
                if(glver == 1)
                {
                    *glptr = 4;
                    glstiptr->verInfo[index].numProps = 2;
                    glstiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                    glstiptr->verInfo[index].props[0].id = DAM_LINE_COUNT;
                    glstiptr->verInfo[index].props[0].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[0].size =  2;
                    glstiptr->verInfo[index].props[0].offset = 0;
                    glstiptr->verInfo[index].props[0].gameprop = 0;

                    glstiptr->verInfo[index].props[1].id = DAM_LINE_FIRST;
                    glstiptr->verInfo[index].props[1].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[1].size =  2;
                    glstiptr->verInfo[index].props[1].offset = 2;
                    glstiptr->verInfo[index].props[1].gameprop = 0;
                }
                else
                {
                    *glptr = 8;
                    glstiptr->verInfo[index].numProps = 2;
                    glstiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 2, PU_STATIC, 0);

                    glstiptr->verInfo[index].props[0].id = DAM_LINE_COUNT;
                    glstiptr->verInfo[index].props[0].flags = 0;
                    glstiptr->verInfo[index].props[0].size =  4;
                    glstiptr->verInfo[index].props[0].offset = 0;
                    glstiptr->verInfo[index].props[0].gameprop = 0;

                    glstiptr->verInfo[index].props[1].id = DAM_LINE_FIRST;
                    glstiptr->verInfo[index].props[1].flags = 0;
                    glstiptr->verInfo[index].props[1].size =  4;
                    glstiptr->verInfo[index].props[1].offset = 4;
                    glstiptr->verInfo[index].props[1].gameprop = 0;
                }
            }
            else if(lumpClass == LCG_NODES)
            {
                if(glver == 1)
                {
                    *glptr = 28;
                    glstiptr->verInfo[index].numProps = 14;
                    glstiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 14, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].props[0].id = DAM_X;
                    glstiptr->verInfo[index].props[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[0].size =  2;
                    glstiptr->verInfo[index].props[0].offset = 0;
                    glstiptr->verInfo[index].props[0].gameprop = 0;
                    // y
                    glstiptr->verInfo[index].props[1].id = DAM_Y;
                    glstiptr->verInfo[index].props[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[1].size =  2;
                    glstiptr->verInfo[index].props[1].offset = 2;
                    glstiptr->verInfo[index].props[1].gameprop = 0;
                    // dx
                    glstiptr->verInfo[index].props[2].id = DAM_DX;
                    glstiptr->verInfo[index].props[2].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[2].size =  2;
                    glstiptr->verInfo[index].props[2].offset = 4;
                    glstiptr->verInfo[index].props[2].gameprop = 0;
                    // dy
                    glstiptr->verInfo[index].props[3].id = DAM_DY;
                    glstiptr->verInfo[index].props[3].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[3].size =  2;
                    glstiptr->verInfo[index].props[3].offset = 6;
                    glstiptr->verInfo[index].props[3].gameprop = 0;
                    // bbox[0][0]
                    glstiptr->verInfo[index].props[4].id = DAM_BBOX_RIGHT_TOP_Y;
                    glstiptr->verInfo[index].props[4].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[4].size =  2;
                    glstiptr->verInfo[index].props[4].offset = 8;
                    glstiptr->verInfo[index].props[4].gameprop = 0;
                    // bbox[0][1]
                    glstiptr->verInfo[index].props[5].id = DAM_BBOX_RIGHT_LOW_Y;
                    glstiptr->verInfo[index].props[5].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[5].size =  2;
                    glstiptr->verInfo[index].props[5].offset = 10;
                    glstiptr->verInfo[index].props[5].gameprop = 0;
                    // bbox[0][2]
                    glstiptr->verInfo[index].props[6].id = DAM_BBOX_RIGHT_LOW_X;
                    glstiptr->verInfo[index].props[6].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[6].size =  2;
                    glstiptr->verInfo[index].props[6].offset = 12;
                    glstiptr->verInfo[index].props[6].gameprop = 0;
                    // bbox[0][3]
                    glstiptr->verInfo[index].props[7].id = DAM_BBOX_RIGHT_TOP_X;
                    glstiptr->verInfo[index].props[7].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[7].size =  2;
                    glstiptr->verInfo[index].props[7].offset = 14;
                    glstiptr->verInfo[index].props[7].gameprop = 0;
                    // bbox[1][0]
                    glstiptr->verInfo[index].props[8].id = DAM_BBOX_LEFT_TOP_Y;
                    glstiptr->verInfo[index].props[8].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[8].size =  2;
                    glstiptr->verInfo[index].props[8].offset = 16;
                    glstiptr->verInfo[index].props[8].gameprop = 0;
                    // bbox[1][1]
                    glstiptr->verInfo[index].props[9].id = DAM_BBOX_LEFT_LOW_Y;
                    glstiptr->verInfo[index].props[9].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[9].size =  2;
                    glstiptr->verInfo[index].props[9].offset = 18;
                    glstiptr->verInfo[index].props[9].gameprop = 0;
                    // bbox[1][2]
                    glstiptr->verInfo[index].props[10].id = DAM_BBOX_LEFT_LOW_X;
                    glstiptr->verInfo[index].props[10].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[10].size =  2;
                    glstiptr->verInfo[index].props[10].offset = 20;
                    glstiptr->verInfo[index].props[10].gameprop = 0;
                    // bbox[1][3]
                    glstiptr->verInfo[index].props[11].id = DAM_BBOX_LEFT_TOP_X;
                    glstiptr->verInfo[index].props[11].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[11].size =  2;
                    glstiptr->verInfo[index].props[11].offset = 22;
                    glstiptr->verInfo[index].props[11].gameprop = 0;
                    // children[0]
                    glstiptr->verInfo[index].props[12].id = DAM_CHILD_RIGHT;
                    glstiptr->verInfo[index].props[12].flags = DT_UNSIGNED | DT_MSBCONVERT;
                    glstiptr->verInfo[index].props[12].size =  2;
                    glstiptr->verInfo[index].props[12].offset = 24;
                    glstiptr->verInfo[index].props[12].gameprop = 0;
                    // children[1]
                    glstiptr->verInfo[index].props[13].id = DAM_CHILD_LEFT;
                    glstiptr->verInfo[index].props[13].flags = DT_UNSIGNED | DT_MSBCONVERT;
                    glstiptr->verInfo[index].props[13].size =  2;
                    glstiptr->verInfo[index].props[13].offset = 26;
                    glstiptr->verInfo[index].props[13].gameprop = 0;
                }
                else
                {
                    *glptr = 32;
                    glstiptr->verInfo[index].numProps = 14;
                    glstiptr->verInfo[index].props = Z_Malloc(sizeof(datatype_t) * 14, PU_STATIC, 0);
                    // x
                    glstiptr->verInfo[index].props[0].id = DAM_X;
                    glstiptr->verInfo[index].props[0].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[0].size =  2;
                    glstiptr->verInfo[index].props[0].offset = 0;
                    glstiptr->verInfo[index].props[0].gameprop = 0;
                    // y
                    glstiptr->verInfo[index].props[1].id = DAM_Y;
                    glstiptr->verInfo[index].props[1].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[1].size =  2;
                    glstiptr->verInfo[index].props[1].offset = 2;
                    glstiptr->verInfo[index].props[1].gameprop = 0;
                    // dx
                    glstiptr->verInfo[index].props[2].id = DAM_DX;
                    glstiptr->verInfo[index].props[2].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[2].size =  2;
                    glstiptr->verInfo[index].props[2].offset = 4;
                    glstiptr->verInfo[index].props[2].gameprop = 0;
                    // dy
                    glstiptr->verInfo[index].props[3].id = DAM_DY;
                    glstiptr->verInfo[index].props[3].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[3].size =  2;
                    glstiptr->verInfo[index].props[3].offset = 6;
                    glstiptr->verInfo[index].props[3].gameprop = 0;
                    // bbox[0][0]
                    glstiptr->verInfo[index].props[4].id = DAM_BBOX_RIGHT_TOP_Y;
                    glstiptr->verInfo[index].props[4].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[4].size =  2;
                    glstiptr->verInfo[index].props[4].offset = 8;
                    glstiptr->verInfo[index].props[4].gameprop = 0;
                    // bbox[0][1]
                    glstiptr->verInfo[index].props[5].id = DAM_BBOX_RIGHT_LOW_Y;
                    glstiptr->verInfo[index].props[5].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[5].size =  2;
                    glstiptr->verInfo[index].props[5].offset = 10;
                    glstiptr->verInfo[index].props[5].gameprop = 0;
                    // bbox[0][2]
                    glstiptr->verInfo[index].props[6].id = DAM_BBOX_RIGHT_LOW_X;
                    glstiptr->verInfo[index].props[6].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[6].size =  2;
                    glstiptr->verInfo[index].props[6].offset = 12;
                    glstiptr->verInfo[index].props[6].gameprop = 0;
                    // bbox[0][3]
                    glstiptr->verInfo[index].props[7].id = DAM_BBOX_RIGHT_TOP_X;
                    glstiptr->verInfo[index].props[7].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[7].size =  2;
                    glstiptr->verInfo[index].props[7].offset = 14;
                    glstiptr->verInfo[index].props[7].gameprop = 0;
                    // bbox[1][0]
                    glstiptr->verInfo[index].props[8].id = DAM_BBOX_LEFT_TOP_Y;
                    glstiptr->verInfo[index].props[8].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[8].size =  2;
                    glstiptr->verInfo[index].props[8].offset = 16;
                    glstiptr->verInfo[index].props[8].gameprop = 0;
                    // bbox[1][1]
                    glstiptr->verInfo[index].props[9].id = DAM_BBOX_LEFT_LOW_Y;
                    glstiptr->verInfo[index].props[9].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[9].size =  2;
                    glstiptr->verInfo[index].props[9].offset = 18;
                    glstiptr->verInfo[index].props[9].gameprop = 0;
                    // bbox[1][2]
                    glstiptr->verInfo[index].props[10].id = DAM_BBOX_LEFT_LOW_X;
                    glstiptr->verInfo[index].props[10].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[10].size =  2;
                    glstiptr->verInfo[index].props[10].offset = 20;
                    glstiptr->verInfo[index].props[10].gameprop = 0;
                    // bbox[1][3]
                    glstiptr->verInfo[index].props[11].id = DAM_BBOX_LEFT_TOP_X;
                    glstiptr->verInfo[index].props[11].flags = DT_FRACBITS;
                    glstiptr->verInfo[index].props[11].size =  2;
                    glstiptr->verInfo[index].props[11].offset = 22;
                    glstiptr->verInfo[index].props[11].gameprop = 0;
                    // children[0]
                    glstiptr->verInfo[index].props[12].id = 12;
                    glstiptr->verInfo[index].props[12].flags = DAM_CHILD_RIGHT;
                    glstiptr->verInfo[index].props[12].size =  4;
                    glstiptr->verInfo[index].props[12].offset = 24;
                    glstiptr->verInfo[index].props[12].gameprop = 0;
                    // children[1]
                    glstiptr->verInfo[index].props[13].id = DAM_CHILD_LEFT;
                    glstiptr->verInfo[index].props[13].flags = DT_UNSIGNED;
                    glstiptr->verInfo[index].props[13].size =  4;
                    glstiptr->verInfo[index].props[13].offset = 28;
                    glstiptr->verInfo[index].props[13].gameprop = 0;
                }
            }
        }
    }
}

#if 0 // needs updating _DEBUG
static void printDebugMapData(gamemap_t* map)
{
    int i;
    vertex_t *vtx;
    sector_t *sec;
    subsector_t *ss;
    line_t  *li;
    seg_t   *seg;
    side_t  *si;
    node_t  *no;

    Con_Printf("VERTEXES:\n");
    for(i = 0; i < map->numvertexes; ++i)
    {
        vtx = &map->vertexes[i];
        Con_Printf("x=%i y=%i\n", vtx->x >> FRACBITS, vtx->y >> FRACBITS);
    }

    Con_Printf("SEGS:\n");
    for(i = 0; i < map->numsegs; ++i)
    {
        seg = &map->segs[i];
        Con_Printf("v1=%i v2=%i angle=%i line=%i side=%i offset=%i\n",
                    seg->v1 - map->vertexes, seg->v2 - map->vertexes,
                    seg->angle >> FRACBITS,
                    (seg->linedef != NULL)? seg->linedef - map->lines : -1,
                    (seg->sidedef != NULL)? seg->sidedef - map->sides : -1,
                    seg->offset >> FRACBITS);
    }

    Con_Printf("SECTORS:\n");
    for(i = 0; i < map->numsectors; ++i)
    {
        sec = &map->sectors[i];
        Con_Printf("floor=%i ceiling=%i floorpic(%i)=\"%s\" ",
                     sec->planes[PLN_FLOOR].height >> FRACBITS, sec->planes[PLN_CEILING].height >> FRACBITS,
                     sec->planes[PLN_FLOOR].pic, W_CacheLumpNum(sec->planes[PLN_FLOOR].pic, PU_GETNAME));
        Con_Printf("ceilingpic(%i)=\"%s\" light=%i\n",
                     sec->planes[PLN_CEILING].pic, W_CacheLumpNum(sec->planes[PLN_CEILING].pic, PU_GETNAME),
                     sec->lightlevel);
    }

    Con_Printf("SSECTORS:\n");
    for(i = 0; i < map->numsubsectors; ++i)
    {
        ss = &map->subsectors[i];
        Con_Printf("numlines=%i firstline=%i\n", ss->linecount, ss->firstline);
    }

    Con_Printf("NODES:\n");
    for(i = 0; i < map->numnodes; ++i)
    {
        no = &map->nodes[i];
        Con_Printf("x=%i y=%i dx=%i dy=%i bb[0][0]=%i bb[0][1]=%i bb[0][2]=%i bb[0][3]=%i " \
                   "bb[1][0]=%i bb[1][1]=%i bb[1][2]=%i bb[1][3]=%i child[0]=%i child[1]=%i\n",
                    no->x >> FRACBITS, no->y >> FRACBITS, no->dx >> FRACBITS, no->dy >>FRACBITS,
                    no->bbox[0][0] >> FRACBITS, no->bbox[0][1] >> FRACBITS,
                    no->bbox[0][2] >> FRACBITS, no->bbox[0][3] >> FRACBITS,
                    no->bbox[1][0] >> FRACBITS, no->bbox[1][1] >> FRACBITS,
                    no->bbox[1][2] >> FRACBITS, no->bbox[1][3] >> FRACBITS,
                    no->children[0], no->children[1]);
    }

    Con_Printf("LINEDEFS:\n");
    for(i = 0; i < map->numlines; ++i)
    {
        li = &map->lines[i];
        Con_Printf("v1=%i v2=%i flags=%i frontside=%i backside=%i\n",
                     li->v1 - map->vertexes, li->v2 - map->vertexes,
                     li->flags, li->sidenum[0],
                     (li->sidenum[1] == NO_INDEX)? -1 : li->sidenum[1]);
    }

    Con_Printf("SIDEDEFS:\n");
    for(i = 0; i < map->numsides; ++i)
    {
        si = &map->sides[i];
        Con_Printf("xoff=%i yoff=%i toptex\"%s\" bottomtex\"%s\" midtex=\"%s\" sec=%i\n",
                    si->textureoffset >> FRACBITS, si->rowoffset >> FRACBITS,
                    si->toptexture? R_TextureNameForNum(si->toptexture) : "-",
                    si->bottomtexture? R_TextureNameForNum(si->bottomtexture) : "-",
                    si->midtexture? R_TextureNameForNum(si->midtexture) : "-",
                    si->sector - map->sectors);
    }
}
#endif
