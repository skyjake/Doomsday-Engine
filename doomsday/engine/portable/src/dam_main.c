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
#include "de_dam.h"
#include "de_defs.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

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

/**
 * Registered by the game during init, these are used by DAM when reading
 * the map lump data as well as the DDay internal data types. The data is
 * read by DDay and any type/size/endian conversion is done before being
 * passed to the game to deal with.
 */
typedef struct {
    uint        id;
    int         type;       // DAM Object type e.g. DAM_SECTOR
    int         datatype;   // e.g. DDVT_INT
    char        name[DED_STRINGID_LEN + 1];
} mapproperty_t;

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

typedef struct mapdatalumpnode_s {
    mapdatalumpinfo_t *data;
    struct mapdatalumpnode_s *next;
} mapdatalumpnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean  readMapData(gamemap_t *map, int doClass, selectprop_t *props,
                            uint numProps);
static boolean  determineMapDataFormat(void);

static void     finishLineDefs(gamemap_t *map);
static void     processSegs(gamemap_t *map);

static boolean  loadReject(gamemap_t *map, mapdatalumpinfo_t *maplump);
static boolean  loadBlockMap(gamemap_t *map, mapdatalumpinfo_t *maplump);
static void     createBlockMap(gamemap_t *map);
static void     finalizeMapData(gamemap_t *map);

static void     allocateMapData(gamemap_t *map);
static void     countMapElements(gamemap_t *map);

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

static uint numTotalCustomProps;
static mapproperty_t *customProps[DAM_NODE];
static uint numCustomProps[DAM_NODE];

static boolean canRegisterProps = false;

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
    {"DOOM", {
                {1, NULL, NULL, true},
                {1, NULL, "DOOM Things", false},
                {1, NULL, "DOOM Linedefs", false},
                {1, NULL, "DOOM Sidedefs", false},
                {1, NULL, "DOOM Vertexes", false},
                {1, NULL, "DOOM Segs", false},
                {1, NULL, "DOOM Subsectors", false},
                {1, NULL, "DOOM Nodes", false},
                {1, NULL, "DOOM Sectors", false},
                {1, NULL, NULL, false},
                {1, NULL, NULL, false},
                {-1, NULL, NULL, true}},
     true},
    {"HEXEN",{
                {1, NULL, NULL, true},
                {2, NULL, "Hexen Things", false},
                {2, NULL, "Hexen Linedefs", false},
                {1, NULL, "DOOM Sidedefs", false},
                {1, NULL, "DOOM Vertexes", false},
                {1, NULL, "DOOM Segs", false},
                {1, NULL, "DOOM Subsectors", false},
                {1, NULL, "DOOM Nodes", false},
                {1, NULL, "DOOM Sectors", false},
                {1, NULL, NULL, false},
                {1, NULL, NULL, false},
                {1, NULL, NULL, true}},
     true},
    {NULL}
};

// Versions of GL node data structures
static glnodeformat_t glNodeFormats[] = {
    {"V1", {
                {1, NULL, NULL, true},
                {1, NULL, "DOOM Vertexes", false},
                {2, NULL, "GLv1 Segs", false},
                {1, NULL, "DOOM Subsectors", false},
                {1, NULL, "GLv1 Nodes", false}},
     true},
    {"V2", {
                {1, NULL, NULL, true},
                {2, "gNd2", "GLv2 Vertexes", false},
                {2, NULL, "GLv1 Segs", false},
                {1, NULL, "DOOM Subsectors", false},
                {1, NULL, "GLv1 Nodes", false}},
     true},
    {"V3", {
                {1, NULL, NULL, true},
                {2, "gNd2", "GLv2 Vertexes", false},
                {3, "gNd3", "GLv3 Segs", false},
                {3, "gNd3", "GLv3 Subsectors", false},
                {1, NULL, "GLv1 Nodes", false}},
     false},
    {"V4", {
                {1, NULL, NULL, true},
                {4, "gNd4", "GLv4 Vertexes", false},
                {4, NULL, "GLv4 Segs", false},
                {4, NULL, "GLv4 Subsectors", false},
                {4, NULL, "GLv4 Nodes", false}},
     false},
    {"V5", {
                {1, NULL, NULL, true},
                {5, "gNd5", "GLv5 Vertexes", false},
                {5, NULL, "GLv5 Segs", false},
                {3, NULL, "GLv3 Subsectors", false},
                {4, NULL, "GLv4 Nodes", false}},
     true},
    {NULL}
};

static uint numProps = 44;
static mapproperty_t properties[] =
{
// Vertex
    {DAM_X, DAM_VERTEX, DMT_VERTEX_POS, "x"},
    {DAM_Y, DAM_VERTEX, DMT_VERTEX_POS, "y"},
// Line
    // TODO: should be DMT_LINE_V but we require special case logic
    {DAM_VERTEX1, DAM_LINE, DDVT_VERT_PTR, "vertex1"},
    // TODO: should be DMT_LINE_V but we require special case logic
    {DAM_VERTEX2, DAM_LINE, DDVT_VERT_PTR, "vertex2"},
    {DAM_FLAGS, DAM_LINE, DMT_LINE_FLAGS, "flags"},
    // TODO: should be DMT_LINE_SIDES but we require special case logic
    {DAM_SIDE0, DAM_LINE, DDVT_SIDE_PTR, "frontside"},
    // TODO: should be DMT_LINE_SIDES but we require special case logic
    {DAM_SIDE1, DAM_LINE, DDVT_SIDE_PTR, "backside"},
// Side
    {DAM_TOP_TEXTURE_OFFSET_X, DAM_SIDE, DMT_SURFACE_OFFX, "toptextureoffsetx"},
    {DAM_TOP_TEXTURE_OFFSET_Y, DAM_SIDE, DMT_SURFACE_OFFY, "toptextureoffsety"},
    {DAM_MIDDLE_TEXTURE_OFFSET_X, DAM_SIDE, DMT_SURFACE_OFFX, "middletextureoffsetx"},
    {DAM_MIDDLE_TEXTURE_OFFSET_Y, DAM_SIDE, DMT_SURFACE_OFFY, "middletextureoffsety"},
    {DAM_BOTTOM_TEXTURE_OFFSET_X, DAM_SIDE, DMT_SURFACE_OFFX, "bottomtextureoffsetx"},
    {DAM_BOTTOM_TEXTURE_OFFSET_Y, DAM_SIDE, DMT_SURFACE_OFFY, "bottomtextureoffsety"},
    {DAM_TOP_TEXTURE, DAM_SIDE, DMT_SURFACE_TEXTURE, "toptexture"},
    {DAM_MIDDLE_TEXTURE, DAM_SIDE, DMT_SURFACE_TEXTURE, "middletexture"},
    {DAM_BOTTOM_TEXTURE, DAM_SIDE, DMT_SURFACE_TEXTURE, "bottomtexture"},
    // TODO: should be DMT_SIDE_SECTOR but we require special case logic
    {DAM_FRONT_SECTOR, DAM_SIDE, DDVT_SECT_PTR, "frontsector"},
// Sector
    {DAM_FLOOR_HEIGHT, DAM_SECTOR, DMT_PLANE_HEIGHT, "floorheight"},
    {DAM_CEILING_HEIGHT, DAM_SECTOR, DMT_PLANE_HEIGHT, "ceilingheight"},
    {DAM_FLOOR_TEXTURE, DAM_SECTOR, DMT_SURFACE_TEXTURE, "floortexture"},
    {DAM_CEILING_TEXTURE, DAM_SECTOR, DMT_SURFACE_TEXTURE, "ceilingtexture"},
    {DAM_LIGHT_LEVEL, DAM_SECTOR, DMT_SECTOR_LIGHTLEVEL, "lightlevel"},
// Seg
    // TODO: should be DMT_SEG_V but we require special case logic
    {DAM_VERTEX1, DAM_SEG, DDVT_VERT_PTR, "vertex1"},
    // TODO: should be DMT_SEG_V but we require special case logic
    {DAM_VERTEX2, DAM_SEG, DDVT_VERT_PTR, "vertex2"},
    {DAM_ANGLE, DAM_SEG, DMT_SEG_ANGLE, "angle"},
    // TODO: should be DMT_SEG_LINE but we require special case logic
    {DAM_LINE, DAM_SEG, DDVT_LINE_PTR, "linedef"},
    // KLUDGE: Store the side id into the flags field
    {DAM_SIDE, DAM_SEG, DDVT_BYTE, "side"},
    {DAM_OFFSET, DAM_SEG, DMT_SEG_OFFSET, "offset"},
// Subsector
    {DAM_SEG_COUNT, DAM_SUBSECTOR, DMT_SUBSECTOR_SEGCOUNT, "segcount"},
    {DAM_SEG_FIRST, DAM_SUBSECTOR, DMT_SUBSECTOR_FIRSTSEG, "firstseg"},
// Node
    {DAM_X, DAM_NODE, DMT_NODE_X, "x"},
    {DAM_Y, DAM_NODE, DMT_NODE_Y, "y"},
    {DAM_DX, DAM_NODE, DMT_NODE_DX, "dx"},
    {DAM_DY, DAM_NODE, DMT_NODE_DY, "dy"},
    {DAM_BBOX_RIGHT_TOP_Y, DAM_NODE, DMT_NODE_BBOX, "bboxrighttopy"},
    {DAM_BBOX_RIGHT_LOW_Y, DAM_NODE, DMT_NODE_BBOX, "bboxrightlowy"},
    {DAM_BBOX_RIGHT_LOW_X, DAM_NODE, DMT_NODE_BBOX, "bboxrightlowx"},
    {DAM_BBOX_RIGHT_TOP_X, DAM_NODE, DMT_NODE_BBOX, "bboxrighttopx"},
    {DAM_BBOX_LEFT_TOP_Y, DAM_NODE, DMT_NODE_BBOX, "bboxlefttopy"},
    {DAM_BBOX_LEFT_LOW_Y, DAM_NODE, DMT_NODE_BBOX, "bboxleftlowy"},
    {DAM_BBOX_LEFT_LOW_X, DAM_NODE, DMT_NODE_BBOX, "bboxleftlowx"},
    {DAM_BBOX_LEFT_TOP_X, DAM_NODE, DMT_NODE_BBOX, "bboxlefttopx"},
    {DAM_CHILD_RIGHT, DAM_NODE, DMT_NODE_CHILDREN, "childright"},
    {DAM_CHILD_LEFT, DAM_NODE, DMT_NODE_CHILDREN, "childleft"}
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

maplumpinfo_t* DAM_MapLumpInfoForLumpClass(int lumpClass)
{
    return &mapLumpInfo[lumpClass];
}

/**
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
        { DAM_SEG_COUNT, "DAM_SEG_COUNT" },
        { DAM_SEG_FIRST, "DAM_SEG_FIRST" },
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

void DAM_Init(void)
{
    uint        i;
    canRegisterProps = true;

    for(i = 0; i < DAM_NODE; ++i)
    {
        numCustomProps[i] = 0;
        customProps[i] = NULL;
    }

    numTotalCustomProps = 0;
}

void DAM_LockCustomPropertys(void)
{
    // We're now closed for business.
    canRegisterProps = false;
}

/**
 * @param type          DAM object, type identifier e.g. <code>DAM_SIDE</code>
 *                      to check for custom property support.
 *
 * @return              <code>true</code> if the map object supports custom
 *                      properties in DAM.
 */
static boolean typeSupportsCustomProperty(int type)
{
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
        return true;

    default:
        break;
    }

    return false;
}

static uint DAM_IDForProperty(int type, char *name)
{
    uint        i, propID = 0;
    boolean     found = false;

    // Check built-in properties first.
    if(numProps > 0)
    {
        mapproperty_t *ptr = properties;
        i = 0;
        do
        {
            if(ptr->type == type && !stricmp(ptr->name, name))
            {
                propID = ptr->id;
                found = true;
            }
            else
            {
                i++;
                ptr++;
            }
        } while(!found && i < numProps);
    }

    // Check custom properties.
    if(!found && typeSupportsCustomProperty(type))
    {
        uint        id = type - 1;
        if(numCustomProps[id] > 0)
        {
            mapproperty_t *ptr = customProps[id];
            i = 0;
            do
            {
                if(!stricmp(name, ptr->name))
                {
                    propID = ptr->id;
                    found = true;
                }
                else
                {
                    i++;
                    ptr++;
                }
            } while(!found && i < numCustomProps[id]);
        }
    }

    if(!found)
        Con_Error("DAM_IDForProperty: %s has no property named \"%s\".",
                  DAM_Str(type), name);

    return propID;
}

/**
 * Called by the game via the Doomsday public API.
 * Registers a new custom map property which can then be read from a map
 * data lump and passed to the game for processing during load.
 *
 * @param type          DAM object, type identifier e.g. <code>DAM_SIDE</code>
 *                      which this property is to be a attributed to.
 * @param dataType      Data type of this property e.g. <code>DDVT_INT</code>.
 * @param name          Name which is used to associate this property. Used
 *                      when parsing map data, lump format definitions and
 *                      used as a token in text based formats.
 *
 * @return              The index number of this property. The game should
 *                      record this as it will be used to identify the custom
 *                      property when it is passed to it by Doomsday.
 */
uint P_RegisterCustomMapProperty(int type, valuetype_t dataType, char *name)
{
    mapproperty_t *newProp;
    uint        id = type - 1; // DAM_THING == 1
    boolean     exists = false;

    // Are we able to register properties atm?
    if(!canRegisterProps)
        Con_Error("P_RegisterCustomMapProperty: Cannot register properties "
                  "at this time.");

    // Do we have a name?
    if(!name || name[0] == 0)
        Con_Error("P_RegisterCustomMapProperty: Cannot register property "
                  "of type %i to type %s - Custom properties must be named.",
                  dataType, DAM_Str(type));

    // Is the name too long?
    if(strlen(name) > DED_STRINGID_LEN)
        Con_Error("P_RegisterCustomMapProperty: Cannot register property "
                  "\"%s\" to type %s. Property names must be %i characters or less.",
                  name, DAM_Str(type), DED_STRINGID_LEN);

    // Does this type support custom properties?
    if(!typeSupportsCustomProperty(type))
        Con_Error("P_RegisterCustomMapProperty: Cannot register property "
                  "\"%s\" - Type %s does not support custom properties.",
                  name, DAM_Str(type));

    // Make sure the name is unique.
    if(numProps > 0)
    {   // Check built-in properties.
        uint        i = 0;
        mapproperty_t *ptr = properties;

        do
        {
            if(ptr->type == type && !stricmp(name, ptr->name))
            {
                exists = true;
            }
            else
            {
                i++;
                ptr++;
            }
        } while(!exists && i < numProps);
    }
    if(!exists && numCustomProps[id] > 0)
    {   // Check custom properties.
        uint        i = 0;
        mapproperty_t *ptr = customProps[id];

        do
        {
            if(!stricmp(name, ptr->name))
            {
                exists = true;
            }
            else
            {
                i++;
                ptr++;
            }
        } while(!exists && i < numCustomProps[id]);
    }

    if(exists)
        Con_Error("P_RegisterCustomMapProperty: Cannot register "
                  "property \"%s\" - A property by this name is "
                  "already attributed to object %s.", name,
                  DAM_Str(type));

    // Is it a known data type?
    switch(dataType)
    {
    case DDVT_BOOL:
    case DDVT_BYTE:
    case DDVT_SHORT:
    case DDVT_INT:
    case DDVT_UINT:
    case DDVT_FIXED:
    case DDVT_ANGLE:
    case DDVT_FLOAT:
    case DDVT_ULONG:
    case DDVT_BLENDMODE:
        break;

    default:
        Con_Error("P_RegisterCustomMapProperty: Cannot register property "
                  "\"%s\" - Invalid type %s.", name, value_Str(dataType));
        break;
    }

    // All is well, register the new property.
    numCustomProps[id]++;
    numTotalCustomProps++;

    customProps[id] =
        Z_Realloc(customProps[id], sizeof(mapproperty_t) * numCustomProps[id],
                  PU_STATIC);
    newProp = &customProps[id][numCustomProps[id] - 1];
    newProp->id = NUM_DAM_PROPERTIES + numTotalCustomProps - 1;
    newProp->type = type;
    newProp->datatype = dataType;
    memset(newProp->name, 0, sizeof(newProp->name));
    strcpy(newProp->name, name);

    VERBOSE2(Con_Message("P_RegisterCustomMapProperty: Added %s->%s (%s)\n",
                        DAM_Str(newProp->type), name, value_Str(newProp->datatype)));

    return newProp->id; // the idx for this property
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
static boolean determineMapDataFormat(void)
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
                    /*    Con_Message("Compare formats: %s (%i)| %s (%i)\n",
                                    mapLump->format->formatName,
                                    mapLump->format->hversion,
                                    nodeFormat->verInfo[lumpClass].formatName,
                                    nodeFormat->verInfo[lumpClass].hversion);*/

                        // Compare the lump formats.
                        if((!mapLump->format->formatName && nodeFormat->verInfo[lumpClass].formatName) ||
                           (mapLump->format->formatName && !nodeFormat->verInfo[lumpClass].formatName))
                        {
                           failed = true;
                        }
                        else
                        {
                            if(0 != stricmp(mapLump->format->formatName,
                                            nodeFormat->verInfo[lumpClass].formatName))
                                failed = true;
                        }
                    }
                }

                node = node->next;
            }

            // Did all lumps match the required format for this version?
            if(!failed)
            {
                // We know the GL Node format
                glNodeFormat = i;

                Con_Message("determineMapDataFormat: (%s GL Node Data)\n",
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
                    if(nodeFormat->vername)
                        Con_Message("determineMapDataFormat: Sorry, %s GL Nodes arn't supported.\n",
                                    nodeFormat->vername);
                    return false;
                }
            }
        }
        Con_Message("determineMapDataFormat: Could not determine GL Node format\n");
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

    memset(count, 0, sizeof(count));

    node = mapDataLumps;
    while(node)
    {
        mapLump = node->data;

        // How many elements are in the lump?
        // Add the number of elements to the potential count for this class.
        if(mapLump->lumpNum != -1 && mapLump->format && !mapLump->format->isText)
        {
            uint elmsize;
            // How many elements are in the lump?
            if(!mapLump->format->formatName)
                elmsize = 1;
            else
                elmsize = Def_GetMapLumpFormat(mapLump->format->formatName)->elmsize;
            mapLump->elements = (mapLump->length - mapLump->startOffset) / elmsize;

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
    if(determineMapDataFormat())
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

void *DAM_IndexToPtr(gamemap_t* map, int objectType, uint id)
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
        Con_Error("DAM_IndexToPtr: %i is not a valid type\n", objectType);
    }

    return NULL;
}

/**
 * This is a kludge. Need to revise the way object indices are resolved in DAM.
 */
long DAM_VertexIdx(long idx)
{
    // FIXME:
    // The firstGLvertex offset should be handed down from the very
    // start of the read process, it should not be a global.
    // If GL NODES are available this might be an "extra" vertex.
    if(glNodeData && bspBuild)
    {
        mapdatalumpformat_t *format = &glNodeFormats[glNodeFormat].
               verInfo[DAM_MapLumpInfoForLumpClass(LCG_SEGS)->glLump];

        // FIXME: Hard linked format logic.
        switch(format->hversion)
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

    return idx;
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
boolean readMapData(gamemap_t *map, int doClass, selectprop_t *props,
                    uint numProps)
{
    uint        oldNum = 0;
    mapdatalumpnode_t *node;
    mapdatalumpformat_t *lastUsedFormat = NULL;
    readprop_t *readProps = NULL;

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
                VERBOSE(Con_Message("P_ReadMapData: Processing \"%s\" (#%d) ver %s...\n",
                                    W_CacheLumpNum(mapLump->lumpNum, PU_GETNAME),
                                    mapLump->elements,
                                    (lumpFormat->formatName? lumpFormat->formatName :"Unknown")));
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
                if(!loadBlockMap(map, mapLump))
                {
                    if(readProps)
                        M_Free(readProps);
                    return false;
                }
            }
            else if(lumpInfo->dataType == DAM_SECREJECT)
            {
                if(!loadReject(map, mapLump))
                {
                    if(readProps)
                        M_Free(readProps);
                    return false;
                }
            }
            else
            {
                uint        readNumProps;
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
                    int         j;
                    uint        i, idx;
                    boolean     found;
                    ded_lumpformat_t *def = Def_GetMapLumpFormat(lumpFormat->formatName);

                    // Count the number of requested properties that are present
                    // in the lump.
                    readNumProps = 0;
                    for(i = 0; i < numProps; ++i)
                    {
                        j = 0;
                        found = false;
                        do
                        {
                            if(DAM_IDForProperty(lumpInfo->dataType, def->properties[j].id) ==
                               props[i].id)
                            {
                                readNumProps++;
                                found = true;
                            }
                            else
                                j++;
                        } while(!found && j < def->property_count.num);
                    }

                    if(readNumProps > 0)
                    {
                        if(readProps)
                            M_Free(readProps);

                        readProps = M_Malloc(sizeof(readprop_t) * readNumProps);
                        // Retrieve ptrs to the properties to be read from the lumpformat.

                        idx = 0;
                        for(i = 0; i < numProps; ++i)
                        {
                            j = 0;
                            found = false;
                            do
                            {
                                if(DAM_IDForProperty(lumpInfo->dataType, def->properties[j].id) ==
                                   props[i].id)
                                {
                                    // Property specific
                                    readProps[idx].id = props[i].id;
                                    readProps[idx].type = props[i].type;

                                    // Format specific
                                    readProps[idx].flags = def->properties[j].flags;
                                    readProps[idx].size = def->properties[j].size;
                                    readProps[idx].offset = def->properties[j].offset;

                                    idx++;
                                    found = true;
                                }
                                else
                                    j++;
                            } while(!found && j < def->property_count.num);
                        }
                    }

                    // Now lets sort the properties based on their byte offset,
                    // this should improve performance while reading.
                }
                else // Yes we can.
                    lumpFormat = lastUsedFormat;
                // END TODO improve property selection algorithm.

                lastUsedFormat = lumpFormat;

                if(readNumProps > 0)
                    if(!DAM_ReadMapDataFromLump(map, mapLump, startIndex, readProps,
                                                readNumProps))
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

static boolean P_ReadMapData(gamemap_t* map, int doClass, selectprop_t *props,
                             uint numProps)
{
    // Cant load GL NODE data if we don't have it.
    if(!glNodeData && (doClass >= LCG_VERTEXES && doClass <= LCG_NODES))
        // Not having the data is considered a success.
        // This is due to us invoking the dpMapLoader plugin at an awkward
        // point in the map loading process (at the start).
        return true;

    if(!readMapData(map, doClass, props, numProps))
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

static void allocateMapData(gamemap_t *map)
{
    uint        k;

    // Vertexes.
    map->vertexes = Z_Calloc(map->numvertexes * sizeof(vertex_t),
                             PU_LEVELSTATIC, 0);
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
    map->lines = Z_Calloc(map->numlines * sizeof(line_t), PU_LEVELSTATIC, 0);
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
    map->segs = Z_Calloc(map->numsegs * sizeof(seg_t), PU_LEVELSTATIC, 0);
    for(k = 0; k < map->numsegs; ++k)
    {
        seg_t      *seg = &map->segs[k];

        seg->header.type = DMU_SEG;
    }

    // Subsectors.
    map->subsectors = Z_Calloc(map->numsubsectors * sizeof(subsector_t),
                               PU_LEVELSTATIC, 0);
    for(k = 0; k < map->numsubsectors; ++k)
    {
        subsector_t *ssec = &map->subsectors[k];

        ssec->header.type = DMU_SUBSECTOR;
        ssec->group = 0;
    }

    // Nodes.
    map->nodes = Z_Calloc(map->numnodes * sizeof(node_t),
                          PU_LEVELSTATIC, 0);
    for(k = 0; k < map->numnodes; ++k)
    {
        node_t     *node = &map->nodes[k];

        node->header.type = DMU_NODE;
    }

    // Sectors.
    map->sectors = Z_Calloc(map->numsectors * sizeof(sector_t),
                            PU_LEVELSTATIC, 0);
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

/**
 * Creates an array of all the registered custom properties for the given DAM
 * object identifier.
 *
 * NOTE: The returned array must free'd with M_Free().
 *
 * @param type          DAM type to collect the custom props for e.g DAM_SIDE.
 * @param count         Num props will be written back to this address.
 *
 * @return              Ptr to the array of collected properties.
 */
static selectprop_t* collectCustomProps(int type, uint *count)
{
    uint        i, idx = type - 1;
    selectprop_t *props = NULL;

#if _DEBUG
if(!typeSupportsCustomProperty(type))
    Con_Error("collectCustomProps: type does not support custom properties.",
              DAM_Str(type));
#endif

    if(numCustomProps[idx] > 0)
    {
        props = M_Malloc(sizeof(selectprop_t) * numCustomProps[idx]);
        for(i = 0; i < numCustomProps[idx]; ++i)
        {
            props[i].id = customProps[idx][i].id;
            props[i].type = customProps[idx][i].datatype;
        }
    }

    if(count != NULL)
        *count = numCustomProps[idx];
    return props;
}

static selectprop_t* mergePropLists(selectprop_t *listA, uint numA,
                                    selectprop_t *listB, uint numB,
                                    uint *count)
{
    uint        i, idx, total = numA + numB;
    selectprop_t *newlist;

    newlist = M_Malloc(sizeof(selectprop_t) * total);
    idx = 0;
    for(i = 0; i < numA; ++i)
    {
        newlist[idx].id = listA[i].id;
        newlist[idx].type = listA[i].type;
        idx++;
    }
    for(i = 0; i < numB; ++i)
    {
        newlist[idx].id = listB[i].id;
        newlist[idx].type = listB[i].type;
        idx++;
    }

    if(count != NULL)
        *count = total;

    return newlist;
}

static void copySideDef(side_t *dest, side_t *src)
{
    uint        i;

    if(!dest || !src)
        return; // wha?

    dest->blendmode = src->blendmode;
    dest->flags = src->flags;
    dest->sector = src->sector;
    for(i = 0; i < 3; ++i)
        memcpy(&dest->sections[i], &src->sections[i], sizeof(surface_t));
}

static void setSideOwner(ownerlist_t *ownerList, void *data)
{
    ownernode_t *node;

    if(!data)
        return;

    // Add a new owner.
    ownerList->count++;

    node = M_Malloc(sizeof(ownernode_t));
    node->data = data;
    node->next = ownerList->head;
    ownerList->head = node;
}

static uint unpackSideDefs(gamemap_t *map)
{
    uint        startTime = Sys_GetRealTime();
    uint        i, newCount, count = 0;
    line_t     *line;

    // Count how many unique sides we SHOULD have.
    for(i = 0, count = 0; i < map->numlines; ++i)
    {
        line = &map->lines[i];

        if(line->L_frontside)
            count++;
        if(line->L_backside)
            count++;
    }

    // Check for packing.
    newCount = count - map->numsides;
    if(newCount > 0)
    {
        uint        j, newIdx;
        side_t     *newSides, *side;
        ownerlist_t *sideOwnerLists;

        // Allocate memory for the side owner processing.
        sideOwnerLists = M_Calloc(sizeof(ownerlist_t) * map->numsides);
        for(i = 0; i < map->numlines; ++i)
        {
            line = &map->lines[i];
            if(line->L_frontside)
                setSideOwner(&sideOwnerLists[line->L_frontside - map->sides],
                             line);
            if(line->L_backside)
                setSideOwner(&sideOwnerLists[line->L_backside - map->sides],
                             line);
        }

        newSides = Z_Calloc(count * sizeof(side_t), PU_LEVELSTATIC, 0);
        for(i = 0; i < count; ++i)
        {
            side_t     *side = &newSides[i];

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

        newIdx = map->numsides;
        for(i = 0; i < map->numsides; ++i)
        {
            ownernode_t *node, *p;

            side = &map->sides[i];
            node = sideOwnerLists[i].head;
            j = 0;
            while(node)
            {
                p = node->next;
                line = (line_t*) node->data;

                if(j == 0)
                {
                    copySideDef(&newSides[i], side);
                }
                else
                {
                    if(line->L_frontside == side)
                        line->L_frontside = &newSides[newIdx];
                    else
                        line->L_backside = &newSides[newIdx];

                    copySideDef(&newSides[newIdx], side);
                    newIdx++;
                }

                M_Free(node);
                node = p;
                j++;
            }

            for(j = 0; j < map->numsegs; ++j)
            {
                seg_t *seg = &map->segs[j];
                if(seg->sidedef == side)
                    seg->sidedef = seg->linedef->sides[seg->side];
            }
        }
        M_Free(sideOwnerLists);

        Z_Free(map->sides);
        map->sides = newSides;
        map->numsides = count;
    }

    if(newCount > 0)
        Con_Message("unpackSideDefs: Unpacked %i new sides\n", newCount);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("unpackSideDefs: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

    return newCount;
}

static boolean loadMapData(gamemap_t *map)
{
    uint        i;
    // Load all lumps of each class in this order.
    //
    // NOTE:
    // DJS 01/10/05 - revised load order to allow for cross-referencing
    //                data during loading (detect + fix trivial errors).
    {
        // Vertexes and GL vertexes (read all properties).
        uint        ccount;
        boolean     result, freeList = false;
        selectprop_t *list, *cprops;
        selectprop_t props[] = {
            {DAM_X, 0},
            {DAM_Y, 0}
        };
        uint        pcount = 2;

        list = props;
        // Any custom properties?
        cprops = collectCustomProps(DAM_VERTEX, &ccount);
        if(cprops)
        {   // Merge the property lists.
            list = mergePropLists(&(*props), pcount, cprops, ccount, &pcount);
            freeList = true;
            M_Free(cprops);
        }

        result = P_ReadMapData(map, LCM_VERTEXES, list, pcount);
        if(result)
            result = P_ReadMapData(map, LCG_VERTEXES, list, pcount);
        if(freeList)
            M_Free(list);
        if(!result)
            return false;
    }
    {
        // Sectors (read all properties).
        uint        ccount;
        boolean     result, freeList = false;
        selectprop_t *list, *cprops;
        selectprop_t props[] = {
            {DAM_FLOOR_HEIGHT, 0},
            {DAM_CEILING_HEIGHT, 0},
            {DAM_FLOOR_TEXTURE, 0},
            {DAM_CEILING_TEXTURE, 0},
            {DAM_LIGHT_LEVEL, 0}
        };
        uint        pcount = 5;

        list = props;
        // Any custom properties?
        cprops = collectCustomProps(DAM_SECTOR, &ccount);
        if(cprops)
        {   // Merge the property lists.
            list = mergePropLists(&(*props), pcount, cprops, ccount, &pcount);
            freeList = true;
            M_Free(cprops);
        }

        result = P_ReadMapData(map, LCM_SECTORS, list, pcount);
        if(freeList)
            M_Free(list);
        if(!result)
            return false;
    }
    {
        // Sidedefs (read all properties except textures).
        uint        ccount;
        boolean     result, freeList = false;
        selectprop_t *list, *cprops;
        selectprop_t props[] = {
            {DAM_TOP_TEXTURE_OFFSET_X, 0},
            {DAM_TOP_TEXTURE_OFFSET_Y, 0},
            {DAM_MIDDLE_TEXTURE_OFFSET_X, 0},
            {DAM_MIDDLE_TEXTURE_OFFSET_Y, 0},
            {DAM_BOTTOM_TEXTURE_OFFSET_X, 0},
            {DAM_BOTTOM_TEXTURE_OFFSET_Y, 0},
            {DAM_FRONT_SECTOR, 0}
        };
        uint        pcount = 7;

        list = props;
        // Any custom properties?
        cprops = collectCustomProps(DAM_SIDE, &ccount);
        if(cprops)
        {   // Merge the property lists.
            list = mergePropLists(&(*props), pcount, cprops, ccount, &pcount);
            freeList = true;
            M_Free(cprops);
        }

        result = P_ReadMapData(map, LCM_SIDEDEFS, list, pcount);
        if(freeList)
            M_Free(list);
        if(!result)
            return false;
    }
    {
        // Linedefs (read all properties).
        uint        ccount;
        boolean     result, freeList = false;
        selectprop_t *list, *cprops;
        selectprop_t props[] = {
            {DAM_VERTEX1, 0},
            {DAM_VERTEX2, 0},
            {DAM_FLAGS, 0},
            {DAM_SIDE0, 0},
            {DAM_SIDE1, 0}
        };
        uint        pcount = 5;

        list = props;
        // Any custom properties?
        cprops = collectCustomProps(DAM_LINE, &ccount);
        if(cprops)
        {   // Merge the property lists.
            list = mergePropLists(&(*props), pcount, cprops, ccount, &pcount);
            freeList = true;
            M_Free(cprops);
        }

        result = P_ReadMapData(map, LCM_LINEDEFS, list, pcount);
        if(freeList)
            M_Free(list);
        if(!result)
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
        selectprop_t props[] = {
            {DAM_TOP_TEXTURE, 0},
            {DAM_MIDDLE_TEXTURE, 0},
            {DAM_BOTTOM_TEXTURE, 0}
        };
        if(!P_ReadMapData(map, LCM_SIDEDEFS, &(*props), 3))
            return false;
    }

    // Things (read all custom properties).
    {
        uint        pcount;
        boolean     result;
        selectprop_t *cprops;

        // Any custom properties?
        cprops = collectCustomProps(DAM_THING, &pcount);
        if(cprops)
        {
            result = P_ReadMapData(map, LCM_THINGS, &(*cprops), pcount);
            M_Free(cprops);
            if(!result)
                return false;
        }
    }
    {
        // Segs (read all properties).
        uint        ccount;
        boolean     result, freeList = false;
        selectprop_t *list, *cprops;
        selectprop_t props[] = {
            {DAM_VERTEX1, 0},
            {DAM_VERTEX2, 0},
            {DAM_ANGLE, 0},
            {DAM_LINE, 0},
            {DAM_SIDE, 0},
            {DAM_OFFSET, 0}
        };
        uint        pcount = 6;

        list = props;
        // Any custom properties?
        cprops = collectCustomProps(DAM_SEG, &ccount);
        if(cprops)
        {   // Merge the property lists.
            list = mergePropLists(&(*props), pcount, cprops, ccount, &pcount);
            freeList = true;
            M_Free(cprops);
        }

        result = P_ReadMapData(map, LCM_SEGS, list, pcount);
        if(freeList)
            M_Free(list);
        if(!result)
            return false;
    }

    for(i = 0; i < map->numsegs; ++i)
    {
        seg_t *seg = &map->segs[i];
        if(seg->linedef)
            seg->sidedef = seg->linedef->sides[seg->side];
    }
    unpackSideDefs(map);
    finishLineDefs(map);
    processSegs(map);

    {
        // Subsectors (read all properties).
        uint        ccount;
        boolean     result, freeList = false;
        selectprop_t *list, *cprops;
        selectprop_t props[] = {
            {DAM_SEG_COUNT, 0},
            {DAM_SEG_FIRST, 0}
        };
        uint        pcount = 2;

        list = props;
        // Any custom properties?
        cprops = collectCustomProps(DAM_SUBSECTOR, &ccount);
        if(cprops)
        {   // Merge the property lists.
            list = mergePropLists(&(*props), pcount, cprops, ccount, &pcount);
            freeList = true;
            M_Free(cprops);
        }

        result = P_ReadMapData(map, LCM_SUBSECTORS, list, pcount);
        if(freeList)
            M_Free(list);
        if(!result)
            return false;
    }
    {
        // Nodes (read all properties).
        uint        ccount;
        boolean     result, freeList = false;
        selectprop_t *list, *cprops;
        selectprop_t props[] = {
            {DAM_X, 0},
            {DAM_Y, 0},
            {DAM_DX, 0},
            {DAM_DY, 0},
            {DAM_BBOX_RIGHT_TOP_Y, 0},
            {DAM_BBOX_RIGHT_LOW_Y, 0},
            {DAM_BBOX_RIGHT_LOW_X, 0},
            {DAM_BBOX_RIGHT_TOP_X, 0},
            {DAM_BBOX_LEFT_TOP_Y, 0},
            {DAM_BBOX_LEFT_LOW_Y, 0},
            {DAM_BBOX_LEFT_LOW_X, 0},
            {DAM_BBOX_LEFT_TOP_X, 0},
            {DAM_CHILD_RIGHT, 0},
            {DAM_CHILD_LEFT, 0}
        };
        uint        pcount = 14;

        list = props;
        // Any custom properties?
        cprops = collectCustomProps(DAM_NODE, &ccount);
        if(cprops)
        {   // Merge the property lists.
            list = mergePropLists(&(*props), pcount, cprops, ccount, &pcount);
            freeList = true;
            M_Free(cprops);
        }

        result = P_ReadMapData(map, LCM_NODES, list, pcount);
        if(freeList)
            M_Free(list);
        if(!result)
            return false;
    }

    if(!P_ReadMapData(map, LCM_BLOCKMAP, NULL, 0))
        return false;
    if(!P_ReadMapData(map, LCM_REJECT, NULL, 0))
        return false;

    return true;
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
        // Excellent, its a map we can read. Load it in!
        Con_Message("P_AttemptMapLoad: %s\n", levelId);
        if(glNodeData && !bspBuild)
            Con_Message(" : Ignoring GL Nodes\n");

        newmap = M_Malloc(sizeof(gamemap_t));
        // Initialize the new map.
        strncpy(newmap->levelid, levelId, sizeof(newmap->levelid));

        countMapElements(newmap);
        allocateMapData(newmap);
        if(!loadMapData(newmap))
            return false; // something went horribly wrong...

        // We have complete level data but we're not out of the woods yet...
        FreeMapDataLumps();
        FreeGLBSPInf();

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

    for(i = 0; i < map->numsegs; ++i)
    {
        seg = &map->segs[i];

        if(seg->angle == 0)
            seg->angle = -1;
        if(seg->offset == 0)
            seg->offset = -1;
        seg->flags = 0;

        if(seg->linedef)
        {
            ldef = seg->linedef;
            seg->SG_frontsector = ldef->sides[seg->side]->sector;

            if((ldef->flags & ML_TWOSIDED) && ldef->sides[seg->side ^ 1])
            {
                seg->SG_backsector = ldef->sides[seg->side ^ 1]->sector;
            }
            else
            {
                ldef->flags &= ~ML_TWOSIDED;
                seg->SG_backsector = 0;
            }

            if(seg->offset == -1)
            {
                if(seg->side == 0)
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
        for(j = 0, seg = ss->firstseg; j < ss->segcount; ++j, seg++)
            if(seg->sidedef)
            {
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

        sec->subsgroupcount = 1;
        sec->subsgroups = Z_Malloc(sizeof(ssecgroup_t) * sec->subsgroupcount, PU_LEVEL, 0);
        sec->subsgroups[0].linked = Z_Malloc(sizeof(sector_t*) * sec->planecount, PU_LEVEL, 0);
        for(k = 0; k < sec->planecount; ++k)
            sec->subsgroups[0].linked[k] = NULL;

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

        // Set the position of the sound origin for all plane sound origins
        // and target heights of all planes.
        for(k = 0; k < sec->planecount; ++k)
        {
            sec->planes[k]->soundorg.pos[VX] = sec->soundorg.pos[VX];
            sec->planes[k]->soundorg.pos[VY] = sec->soundorg.pos[VY];
            sec->planes[k]->soundorg.pos[VZ] = FLT2FIX(sec->planes[k]->height);

            sec->planes[k]->target = sec->planes[k]->height;
        }
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
static void createBlockMap(gamemap_t* map)
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
static boolean loadBlockMap(gamemap_t* map, mapdatalumpinfo_t* maplump)
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
            Con_Message("loadBlockMap: Generating NEW blockmap...\n");

        createBlockMap(map);
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
static boolean loadReject(gamemap_t* map, mapdatalumpinfo_t* maplump)
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
            Con_Message("loadBlockMap: Generating NEW reject...\n");

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
