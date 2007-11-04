/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * dam_loader.c: Doomsday Archived Map (DAM) reader. Loading.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_dam.h"
#include "de_bsp.h"
#include "de_play.h"
#include "de_defs.h"
#include "de_refresh.h"
#include "de_system.h"
#include "de_misc.h"

#include <math.h>

// MACROS ------------------------------------------------------------------

// Internal data types
#define MAPDATA_FORMATS 2

// MAXRADIUS is for precalculated sector block boxes
// the spider demon is larger,
// but we do not have any moving sectors nearby
#define MAXRADIUS       32*FRACUNIT

// TYPES -------------------------------------------------------------------

typedef struct mapdataformat_s {
    char       *vername;
    maplumpformat_t verInfo[5];
    boolean     supported;
} mapdataformat_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static boolean  readMapData(gamemap_t *map, listnode_t *nodes, int doClass,
                            selectprop_t *props,
                            uint numProps,
                            int (*callback)(int type, uint index, void* ctx));

static void     finishLineDefs(gamemap_t *map);

static boolean  loadReject(gamemap_t *map, maplumpinfo_t *maplump);
static boolean  loadBlockMap(gamemap_t *map, maplumpinfo_t *maplump);
static void     finalizeMapData(gamemap_t *map);

static void     allocateMapData(gamemap_t *map);
static void     countMapElements(gamemap_t *map, listnode_t *nodes);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The following is used in error fixing/detection/reporting:
// missing sidedefs
uint numMissingFronts;
uint *missingFronts;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean mustCreateBlockMap;
static vertex_t *rootVtx; // Used when sorting vertex line owners.

// Versions of map data structures
static mapdataformat_t mapDataFormats[] = {
    {"DOOM", {
                {1, "DOOM Things", ML_THINGS},
                {1, "DOOM Linedefs", ML_LINEDEFS},
                {1, "DOOM Sidedefs", ML_SIDEDEFS},
                {1, "DOOM Vertexes", ML_VERTEXES},
                {1, "DOOM Sectors", ML_SECTORS},
             },
     true},
    {"HEXEN", {
                {2, "Hexen Things", ML_THINGS},
                {2, "Hexen Linedefs", ML_LINEDEFS},
                {1, "DOOM Sidedefs", ML_SIDEDEFS},
                {1, "DOOM Vertexes", ML_VERTEXES},
                {1, "DOOM Sectors", ML_SECTORS},
              },
     true},
    {NULL}
};

// CODE --------------------------------------------------------------------

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
        { 0, NULL }
    };
    int i;

    for(i = 0; props[i].str; ++i)
        if(props[i].prop == prop)
            return props[i].str;

    sprintf(propStr, "(unnamed %i)", prop);
    return propStr;
}

#if 0
static boolean validateMapStructures(void)
{

    // Now confirm the data is valid.
    node = mapDataLumps;
    while(node)
    {
        mapLump = node->data;

        // Is the REJECT complete?
        if(mapLump->lumpClass == ML_REJECT)
        {   // Check the length of the lump
            size_t requiredLength = (((count[ML_SECTORS]*count[ML_SECTORS]) + 7) & ~7) /8;

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
        else if(mapLump->lumpClass == ML_BLOCKMAP)
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
}
#endif

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
    case DAM_SECTOR:
        if(id < map->numsectors)
            return &map->sectors[id];
        break;
    default:
        Con_Error("DAM_IndexToPtr: %i is not a valid type\n", objectType);
    }

    return NULL;
}

int DAM_DataTypeForLumpClass(int lumpClass)
{
    struct lumptype_s {
        int type;
        int lumpClass;
    } types[] =
    {
        { DAM_THING, ML_THINGS },
        { DAM_VERTEX, ML_VERTEXES },
        { DAM_LINE, ML_LINEDEFS },
        { DAM_SIDE, ML_SIDEDEFS },
        { DAM_SECTOR, ML_SECTORS },
        { DAM_MAPBLOCK, ML_BLOCKMAP },
        { DAM_SECREJECT, ML_REJECT },
        { DAM_UNKNOWN, 0 }
    };
    int         i;

    for(i = 0; types[i].type != DAM_UNKNOWN; ++i)
        if(types[i].lumpClass == lumpClass)
            return types[i].type;

    return DAM_UNKNOWN;
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
 * @param callback      Function to be called with the read data for each
 *                      element specified.
 *
 * @return:             If <code>true</code> all lumps of the requested
 *                      class were processed successfully.
 */
static boolean readMapData(gamemap_t *map, listnode_t *nodes, int doClass,
                           selectprop_t *props,
                           uint numProps,
                           int (*callback)(int type, uint index, void* ctx))
{
    uint        oldNum = 0;
    listnode_t *node;
    maplumpformat_t *lastUsedFormat = NULL;
    readprop_t *readProps = NULL;

    node = nodes;
    while(node)
    {
        maplumpinfo_t *mapLump = node->data;
        maplumpformat_t *lumpFormat;
        uint        startTime;

        // Only process lumps that match the class requested
        if(doClass == mapLump->lumpClass)
        {
            int         dataType;
            lumpFormat = mapLump->format;

            // Is this a "real" lump? (ie do we have to generate the data for it?)
            if(mapLump->lumpNum != -1)
            {
                VERBOSE(Con_Message("readMapData: Processing \"%s\" (#%d) ver %s...\n",
                                    (char*) W_CacheLumpNum(mapLump->lumpNum, PU_GETNAME),
                                    mapLump->elements,
                                    ((lumpFormat && lumpFormat->formatName)? lumpFormat->formatName :"Unknown")));
            }
            else
            {
                // Not a problem, we'll generate useable data automatically.
                VERBOSE(Con_Message("readMapData: Generating \"%s\"\n",
                                    DAM_Str(mapLump->lumpClass)));
            }

            // Read in the lump data
            startTime = Sys_GetRealTime();
            dataType = DAM_DataTypeForLumpClass(mapLump->lumpClass);
            if(dataType == DAM_MAPBLOCK)
            {
                if(!loadBlockMap(map, mapLump))
                {
                    if(readProps)
                        M_Free(readProps);
                    return false;
                }
            }
            else if(dataType == DAM_SECREJECT)
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
                uint        readNumProps = 0;
                uint        startIndex  = oldNum;

                /**
                * Create a NULL terminated array of the properties to be read.
                * \todo Improve property selection algorithm.
		        */

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
                            if(DAM_IDForProperty(dataType, def->properties[j].id) ==
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
                                if(DAM_IDForProperty(dataType, def->properties[j].id) ==
                                   props[i].id)
                                {
                                    // Property specific
                                    readProps[idx].id = props[i].id;
                                    readProps[idx].valueType = props[i].valueType;

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

                    // \todo sort the properties based on their byte offset, this should improve performance while reading.
                }
                else // Yes we can.
                    lumpFormat = lastUsedFormat;
                // END TODO improve property selection algorithm.

                lastUsedFormat = lumpFormat;

                if(readNumProps > 0)
                    if(!DAM_ReadMapDataFromLump(map, mapLump, startIndex, readProps,
                                                readNumProps,
                                                callback))
                    {
                        M_Free(readProps);
                        return false; // something went VERY horibly wrong...
                    }
            }
            // How much time did we spend?
            VERBOSE2(Con_Message("readMapData: Done in %.4f seconds.\n",
                                 (Sys_GetRealTime() - startTime) / 1000.0f));

            oldNum += mapLump->elements;
        }

        node = node->next;
    }

    if(readProps)
        M_Free(readProps);
    return true;
}

static boolean P_ReadMapData(gamemap_t* map, listnode_t *nodes, int doClass,
                             selectprop_t *props, uint numProps,
                             int (*callback)(int type, uint index, void* ctx))
{
    return readMapData(map, nodes, doClass, props, numProps, callback);
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
    }

    // Linedefs + missing fronts.
    map->lines = Z_Calloc(map->numlines * sizeof(line_t), PU_LEVELSTATIC, 0);
    for(k = 0; k < map->numlines; ++k)
    {
        line_t     *lin = &map->lines[k];

        lin->header.type = DMU_LINE;
        lin->L_vo1 = NULL;
        lin->L_vo2 = NULL;
    }

    // Sidedefs.
    map->sides = Z_Calloc(map->numsides * sizeof(side_t), PU_LEVEL, 0);
    for(k = 0; k < map->numsides; ++k)
    {
        side_t     *side = &map->sides[k];
        uint        c;

        side->header.type = DMU_SIDE;
        side->SW_topsurface.header.type = DMU_SURFACE;
        side->SW_middlesurface.header.type = DMU_SURFACE;
        side->SW_bottomsurface.header.type = DMU_SURFACE;
        side->SW_topflags = 0;
        side->SW_bottomflags = 0;
        side->SW_middleflags = 0;
        for(c = 0; c < 4; ++c)
        {
            side->SW_toprgba[c] = 1;
            side->SW_middlergba[c] = 1;
            side->SW_bottomrgba[c] = 1;
        }
        side->SW_middleblendmode = BM_NORMAL;
        side->SW_topsurface.material.isflat =
			side->SW_topsurface.oldmaterial.isflat = false;
        side->SW_middlesurface.material.isflat =
			side->SW_middlesurface.oldmaterial.isflat = false;
        side->SW_bottomsurface.material.isflat =
			side->SW_bottomsurface.oldmaterial.isflat = false;
    }

    // Sectors.
    map->sectors = Z_Calloc(map->numsectors * sizeof(sector_t),
                            PU_LEVELSTATIC, 0);
    for(k = 0; k < map->numsectors; ++k)
    {
        uint        j;
        sector_t   *sec = &map->sectors[k];

        sec->header.type = DMU_SECTOR;
        sec->thinglist = NULL;
        for(j = 0;  j < 3; ++j)
            sec->rgb[j] = 1;

        // Do the planes too.
        sec->planecount = 0;
        for(j = 0; j < 2; ++j)
            R_NewPlaneForSector(sec, j);
        sec->SP_ceilnormal[VZ] = -1;
    }

    // Call the game's setup routines
    if(gx.SetupForMapData)
    {
        gx.SetupForMapData(DAM_VERTEX, map->numvertexes);
        gx.SetupForMapData(DAM_THING, map->numthings);
        gx.SetupForMapData(DAM_LINE, map->numlines);
        gx.SetupForMapData(DAM_SIDE, map->numsides);
        gx.SetupForMapData(DAM_SECTOR, map->numsectors);
    }
}

static void countMapElements(gamemap_t *map, listnode_t *nodes)
{
    listnode_t *node;

    map->numvertexes = 0;
    map->numsubsectors = 0;
    map->numsectors = 0;
    map->numnodes = 0;
    map->numsides = 0;
    map->numlines = 0;
    map->numsegs = 0;
    map->numthings = 0;
    map->po_NumPolyobjs = 0;

    node = nodes;
    while(node)
    {
        maplumpinfo_t *mapLump = node->data;

        // Is this a "real" lump? (or do we have to generate the data for it?)
        if(mapLump->lumpNum != -1)
        {
            int         lumpClass = mapLump->lumpClass;
            boolean     inuse = true;

            if(inuse)
            {   // Determine the number of map data objects of each type we'll need.
                switch(DAM_DataTypeForLumpClass(lumpClass))
                {
                case DAM_VERTEX:    map->numvertexes   += mapLump->elements;    break;
                case DAM_THING:     map->numthings     += mapLump->elements;    break;
                case DAM_LINE:      map->numlines      += mapLump->elements;    break;
                case DAM_SIDE:      map->numsides      += mapLump->elements;    break;
                case DAM_SECTOR:    map->numsectors    += mapLump->elements;    break;
                default:
                    break;
                }
            }
        }
        node = node->next;
    }
}

static void copySideDef(side_t *dest, side_t *src)
{
    uint        i;

    if(!dest || !src)
        return; // wha?

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
    if(count > map->numsides)
    {
        uint        j, newIdx;
        side_t     *newSides, *side;
        ownerlist_t *sideOwnerLists;

        // Allocate memory for the side owner processing.
        newCount = count - map->numsides;
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
            uint        c;

            side->header.type = DMU_SIDE;
            side->SW_topsurface.header.type = DMU_SURFACE;
            side->SW_middlesurface.header.type = DMU_SURFACE;
            side->SW_bottomsurface.header.type = DMU_SURFACE;
            side->SW_topflags = 0;
            side->SW_bottomflags = 0;
            side->SW_middleflags = 0;
            for(c = 0; c < 4; ++c)
            {
                side->SW_toprgba[c] = 1;
                side->SW_middlergba[c] = 1;
                side->SW_bottomrgba[c] = 1;
            }
            side->SW_middleblendmode = BM_NORMAL;
            side->SW_topsurface.material.isflat =
				side->SW_topsurface.oldmaterial.isflat = false;
            side->SW_middlesurface.material.isflat =
				side->SW_middlesurface.oldmaterial.isflat = false;
            side->SW_bottomsurface.material.isflat =
				side->SW_bottomsurface.oldmaterial.isflat = false;
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
        }
        M_Free(sideOwnerLists);

        Z_Free(map->sides);
        map->sides = newSides;
        map->numsides = count;

        Con_Message("unpackSideDefs: Unpacked %i new sides\n", newCount);
        return newCount;
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("unpackSideDefs: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

    return map->numsides;
}

static boolean readAllTypeProperties(gamemap_t *map, listnode_t *nodes, int type,
                                     int (*callback)(int type, uint index, void* ctx))
{
    uint        pcount;
    boolean     result = true;
    selectprop_t *list;

    list = DAM_CollectProps(type, true, true, &pcount);
    if(list)
    {
        listnode_t *node;

        // Iterate our known lump classes array.
        node = nodes;
        while(node)
        {
            maplumpinfo_t *info = node->data;

            if(DAM_DataTypeForLumpClass(info->lumpClass) == type)
                result =
                    P_ReadMapData(map, nodes, info->lumpClass, list, pcount,
                                  callback);

            node = node->next;
        }

        M_Free(list);
    }

    return result;
}

static boolean loadMapData(gamemap_t *map, listnode_t *nodes)
{
    // Load all lumps of each class in this order.
    //
    // NOTE:
    // DJS 01/10/05 - revised load order to allow for cross-referencing
    //                data during loading (detect + fix trivial errors).
    readAllTypeProperties(map, nodes, DAM_VERTEX, DAM_SetProperty);
    readAllTypeProperties(map, nodes, DAM_SECTOR, DAM_SetProperty);
    {
        // Sidedefs (read all properties except textures).
        uint        ccount;
        boolean     result, freeList = false;
        selectprop_t *list, *cprops;
        selectprop_t props[] = {
            {DAM_TOP_TEXTURE_OFFSET_X, DMT_SURFACE_OFFX},
            {DAM_TOP_TEXTURE_OFFSET_Y, DMT_SURFACE_OFFY},
            {DAM_MIDDLE_TEXTURE_OFFSET_X, DMT_SURFACE_OFFX},
            {DAM_MIDDLE_TEXTURE_OFFSET_Y, DMT_SURFACE_OFFY},
            {DAM_BOTTOM_TEXTURE_OFFSET_X, DMT_SURFACE_OFFX},
            {DAM_BOTTOM_TEXTURE_OFFSET_Y, DMT_SURFACE_OFFY},
            // \todo should be DMT_SIDE_SECTOR but we require special case logic
            {DAM_FRONT_SECTOR, DDVT_SECT_IDX}
        };
        uint        pcount = 7;

        list = props;
        // Any custom properties?
        cprops = DAM_CollectProps(DAM_SIDE, false, true, &ccount);
        if(cprops)
        {   // Merge the property lists.
            list = DAM_MergePropLists(&(*props), pcount, cprops, ccount, &pcount);
            freeList = true;
            M_Free(cprops);
        }

        result = P_ReadMapData(map, nodes, ML_SIDEDEFS, list, pcount,
                                         DAM_SetProperty);
        if(freeList)
            M_Free(list);
        if(!result)
            return false;
    }
    readAllTypeProperties(map, nodes, DAM_LINE, DAM_SetProperty);
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
            {DAM_TOP_TEXTURE, DMT_MATERIAL_TEXTURE},
            {DAM_MIDDLE_TEXTURE, DMT_MATERIAL_TEXTURE},
            {DAM_BOTTOM_TEXTURE, DMT_MATERIAL_TEXTURE}
        };

        if(!P_ReadMapData(map, nodes, ML_SIDEDEFS, &(*props), 3,
                                    DAM_SetProperty))
            return false;
    }
    readAllTypeProperties(map, nodes, DAM_THING, DAM_SetProperty);

    unpackSideDefs(map);

    if(!P_ReadMapData(map, nodes, ML_BLOCKMAP, NULL, 0, NULL))
        return false;
    if(!P_ReadMapData(map, nodes, ML_REJECT, NULL, 0, NULL))
        return false;

    return true;
}

/**
 * Validate the map data before loading the level.
 */
static boolean validateMapData(listnode_t *nodes)
{
    listnode_t *node;

    node = nodes;
    while(node)
    {
        maplumpinfo_t *info = node->data;

        // How many elements are in the lump?
        // Add the number of elements to the potential count for this class.
        if(info->lumpNum != -1 && info->format)
        {
            uint elmsize;
            // How many elements are in the lump?
            if(!info->format->formatName)
                elmsize = 1;
            else
                elmsize = Def_GetMapLumpFormat(info->format->formatName)->elmsize;
            info->elements = (info->length - info->startOffset) / elmsize;
        }
        node = node->next;
    }

    return true;
}

maplumpformat_t* DAM_GetMapDataLumpFormat(int mapFormat, int lumpClass)
{
    uint        i;

    for(i = 0; i < 5; ++i)
    {
        if(lumpClass == mapDataFormats[mapFormat].verInfo[i].lumpClass)
            return &mapDataFormats[mapFormat].verInfo[i];
    }

    return NULL;
}

/**
 * Determines the format of the map by comparing the (already determined)
 * lump formats against the known map formats.
 *
 * Map data lumps can be in any mixed format but GL Node data cannot so
 * we only check those atm.
 *
 * @return              The index of the map format.
 */
static int determineMapDataFormat(listnode_t *nodes)
{
    int         mapFormat;
    boolean     stop;
    listnode_t *node;

    VERBOSE(Con_Message(" Determining map format...\n"));

    // We'll assume we're loading a DOOM format map to begin with.
    mapFormat = 0;

    // If there is a BEHAVIOR lump, then this is a HEXEN format map.
    node = nodes;
    stop = false;
    while(node && !stop)
    {
        maplumpinfo_t *info = node->data;

        // If this is a BEHAVIOR lump, then this MUST be a HEXEN format map.
        if(info->lumpClass == ML_BEHAVIOR)
        {
            mapFormat = 1;
            stop = true; // We're done.
        }
        else
        {
            node = node->next;
        }
    }

    // Now that we know the data format of the lumps we need to update the
    // internal version number for any lumps that don't declare a version (-1).
    // Taken from the version stipulated in the map format.
    node = nodes;
    while(node)
    {
        maplumpinfo_t *info = node->data;

        // Set the lump version number for this format.
        info->format =
            DAM_GetMapDataLumpFormat(mapFormat, info->lumpClass);

        // Announce the format.
        VERBOSE2(
            Con_Message("  %s is %s.\n", W_CacheLumpNum(info->lumpNum, PU_GETNAME),
                        DAM_Str(DAM_DataTypeForLumpClass(info->lumpClass))));

        node = node->next;
    }

    // We support this map data format.
    return mapFormat;
}

/**
 * Completes the linedef loading by resolving the front/back
 * sector ptrs which we couldn't do earlier as the sidedefs
 * hadn't been loaded at the time.
 */
static void finishLineDefs(gamemap_t* map)
{
    uint        i;
    line_t     *ld;
    vertex_t   *v[2];
    seg_t      *startSeg, *endSeg;

    VERBOSE2(Con_Message("Finalizing Linedefs...\n"));

    for(i = 0; i < map->numlines; ++i)
    {
        ld = &map->lines[i];

        startSeg = ld->sides[0]->segs[0];
        endSeg = ld->sides[0]->segs[ld->sides[0]->segcount - 1];
        ld->v[0] = v[0] = startSeg->SG_v1;
        ld->v[1] = v[1] = endSeg->SG_v2;
        ld->dx = v[1]->V_pos[VX] - v[0]->V_pos[VX];
        ld->dy = v[1]->V_pos[VY] - v[0]->V_pos[VY];

        // Calculate the accurate length of each line.
        ld->length = P_AccurateDistancef(ld->dx, ld->dy);
        ld->angle = bamsAtan2((int) (ld->v[1]->V_pos[VY] - ld->v[0]->V_pos[VY]),
                      (int) (ld->v[1]->V_pos[VX] - ld->v[0]->V_pos[VX])) << FRACBITS;

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

        if(v[0]->V_pos[VX] < v[1]->V_pos[VX])
        {
            ld->bbox[BOXLEFT]   = FLT2FIX(v[0]->V_pos[VX]);
            ld->bbox[BOXRIGHT]  = FLT2FIX(v[1]->V_pos[VX]);
        }
        else
        {
            ld->bbox[BOXLEFT]   = FLT2FIX(v[1]->V_pos[VX]);
            ld->bbox[BOXRIGHT]  = FLT2FIX(v[0]->V_pos[VX]);
        }

        if(v[0]->V_pos[VY] < v[1]->V_pos[VY])
        {
            ld->bbox[BOXBOTTOM] = FLT2FIX(v[0]->V_pos[VY]);
            ld->bbox[BOXTOP]    = FLT2FIX(v[1]->V_pos[VY]);
        }
        else
        {
            ld->bbox[BOXBOTTOM] = FLT2FIX(v[1]->V_pos[VY]);
            ld->bbox[BOXTOP]    = FLT2FIX(v[0]->V_pos[VY]);
        }
    }
}

static void finishSides(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsides; ++i)
    {
        side_t *side = &map->sides[i];

        // Make sure the texture references are good.
        if(!side->SW_topisflat && side->SW_toptexture >= numtextures)
            side->SW_toptexture = 0;
        if(!side->SW_middleisflat && side->SW_middletexture >= numtextures)
            side->SW_middletexture = 0;
        if(!side->SW_bottomisflat && side->SW_bottomtexture >= numtextures)
            side->SW_bottomtexture = 0;
    }
}

/**
 * \pre Lines in sector must be setup before this is called!
 */
static void updateSectorBounds(sector_t *sec)
{
    uint        i;
    float      *bbox;
    vertex_t   *vtx;

    if(!sec)
        return;

    bbox = sec->bounds;

    if(!(sec->linecount > 0))
    {
        memset(sec->bounds, 0, sizeof(sec->bounds));
        return;
    }

    vtx = sec->Lines[0]->L_v1;
    bbox[BLEFT] = bbox[BRIGHT]  = vtx->V_pos[VX];
    bbox[BTOP]  = bbox[BBOTTOM] = vtx->V_pos[VY];

    for(i = 1; i < sec->linecount; ++i)
    {
        vtx = sec->Lines[i]->L_v1;

        if(vtx->V_pos[VX] < bbox[BLEFT])
            bbox[BLEFT]   = vtx->V_pos[VX];
        if(vtx->V_pos[VX] > bbox[BRIGHT])
            bbox[BRIGHT]  = vtx->V_pos[VX];
        if(vtx->V_pos[VY] < bbox[BTOP])
            bbox[BTOP]    = vtx->V_pos[VY];
        if(vtx->V_pos[VY] > bbox[BBOTTOM])
            bbox[BBOTTOM] = vtx->V_pos[VY];
    }
}

/**
 * \pre Sector bounds must be setup before this is called!
 */
void P_GetSectorBounds(sector_t *sec, float *min, float *max)
{
    min[VX] = sec->bounds[BLEFT];
    min[VY] = sec->bounds[BTOP];

    max[VX] = sec->bounds[BRIGHT];
    max[VY] = sec->bounds[BBOTTOM];
}

/**
 * \pre Sector bounds must be setup before this is called!
 */
static void updateSectorBlockBox(sector_t *sec, fixed_t bmapOrg[2],
                                 uint bmapSize[2])
{
    uint        block;
    fixed_t     bbox[4];

    if(!sec)
        return;

    bbox[BOXTOP] = FLT2FIX(sec->bounds[BOXTOP]);
    bbox[BOXBOTTOM] = FLT2FIX(sec->bounds[BOXBOTTOM]);
    bbox[BOXLEFT] = FLT2FIX(sec->bounds[BOXLEFT]);
    bbox[BOXRIGHT] = FLT2FIX(sec->bounds[BOXRIGHT]);

    // Determine sector blockmap blocks from the bounding box.
    block = (bbox[BOXTOP] - bmapOrg[VY] + MAXRADIUS) >> MAPBLOCKSHIFT;
    block = (block >= bmapSize[VY]? bmapSize[VY] - 1 : block);
    sec->blockbox[BOXTOP] = block;

    block = (bbox[BOXBOTTOM] - bmapOrg[VY] - MAXRADIUS) >> MAPBLOCKSHIFT;
    block = (block < 0? 0 : block);
    sec->blockbox[BOXBOTTOM] = block;

    block = (bbox[BOXRIGHT] - bmapOrg[VX] + MAXRADIUS) >> MAPBLOCKSHIFT;
    block = (block >= bmapSize[VX]? bmapSize[VX] - 1 : block);
    sec->blockbox[BOXRIGHT] = block;

    block = (bbox[BOXLEFT] - bmapOrg[VX] - MAXRADIUS) >> MAPBLOCKSHIFT;
    block = (block < 0? 0 : block);
    sec->blockbox[BOXLEFT] = block;
}

/**
 * \pre Sector planes must be initialized before this is called.
 * \todo Bad design: the subgroup is the same for all planes, only the
 * linked group ptrs need to be per-plane.
 */
static void findSectorSSecGroups(sector_t *sec)
{
    uint        i;

    if(!sec)
        return;

    sec->subsgroupcount = 1;
    sec->subsgroups =
        Z_Malloc(sizeof(ssecgroup_t) * sec->subsgroupcount, PU_LEVEL, 0);

    sec->subsgroups[0].linked =
        Z_Malloc(sizeof(sector_t*) * sec->planecount, PU_LEVEL, 0);
    for(i = 0; i < sec->planecount; ++i)
        sec->subsgroups[0].linked[i] = NULL;
}

static void initMapBlockRings(gamemap_t *map)
{
    uint        i;
    size_t      size;
    uint        bmapSize[2];

    P_GetBlockmapSize(map->blockmap, bmapSize);

    // Clear out mobj rings.
    size = sizeof(*map->blockrings) * bmapSize[VX] * bmapSize[VY];
    map->blockrings = Z_Calloc(size, PU_LEVEL, 0);

    for(i = 0; i < bmapSize[VX] * bmapSize[VY]; ++i)
        map->blockrings[i].next =
            map->blockrings[i].prev = (mobj_t *) &map->blockrings[i];
}

static void findMissingFrontSidedefs(gamemap_t *map)
{
    uint        i;

    numMissingFronts = 0;
    missingFronts = M_Calloc(map->numlines * sizeof(uint));

    for(i = 0; i < map->numlines; ++i)
    {
        line_t     *li = &map->lines[i];

        if(!li->L_frontside)
        {   // A missing front sidedef
            missingFronts[i] = 1;
            numMissingFronts++;
        }
    }
}

static void markSelfReferencingLinedefs(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numlines; ++i)
    {
        line_t     *li = &map->lines[i];

        // A self-referencing line?
        if(li->L_frontside && li->L_backside &&
           li->L_frontside->sector == li->L_backside->sector)
            li->flags |= LINEF_SELFREF;
    }
}

static void linkSSecsToSectors(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        seg_t      **segp;
        boolean     found;

        segp = ssec->segs;
        found = false;
        while(*segp)
        {
            seg_t      *seg = *segp;

            if(!found && seg->sidedef)
            {
                ssec->sector = seg->sidedef->sector;
                found = true;
            }

            seg->subsector = ssec;
            *segp++;
        }

        assert(ssec->sector);
    }
}

static void hardenSectorSSecList(gamemap_t *map, uint secIDX)
{
    uint        i, n, count;
    sector_t   *sec = &map->sectors[secIDX];

    count = 0;
    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        if(ssec->sector == sec)
            count++;
    }

    sec->subsectors =
        Z_Malloc((count + 1) * sizeof(subsector_t*), PU_LEVELSTATIC, NULL);

    n = 0;
    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];
        if(ssec->sector == sec)
            sec->subsectors[n++] = ssec;
    }
    sec->subsectors[n] = NULL; // Terminate.
}

/**
 * Build subsector tables for all sectors.
 */
static void buildSectorSSecLists(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsectors; ++i)
    {
        hardenSectorSSecList(map, i);
    }
}

static void buildSectorLineLists(gamemap_t *map)
{
    typedef struct linelink_s {
        line_t      *line;
        struct linelink_s *next;
    } linelink_t;

    uint        i, j;
    line_t     *li;
    sector_t   *sec;

    zblockset_t *lineLinksBlockSet;
    linelink_t  **sectorLineLinks;
    uint        totallinks;

    // build line tables for each sector.
    lineLinksBlockSet = Z_BlockCreate(sizeof(linelink_t), 512, PU_STATIC);
    sectorLineLinks = M_Calloc(sizeof(linelink_t*) * map->numsectors);
    totallinks = 0;
    for(i = 0, li = map->lines; i < map->numlines; ++i, li++)
    {
        uint        secIDX;
        linelink_t *link;

        if(li->L_frontside)
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = li->L_frontsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_frontsector->linecount++;
            totallinks++;
        }

        if(li->L_backside && li->L_backsector != li->L_frontsector)
        {
            link = Z_BlockNewElement(lineLinksBlockSet);

            secIDX = li->L_backsector - map->sectors;
            link->line = li;

            link->next = sectorLineLinks[secIDX];
            sectorLineLinks[secIDX] = link;
            li->L_backsector->linecount++;
            totallinks++;
        }
    }

    // Harden the sector line links into arrays.
    {
    line_t    **linebuffer;
    line_t    **linebptr;

    linebuffer = Z_Malloc((totallinks + map->numsectors) * sizeof(line_t*),
                          PU_LEVELSTATIC, 0);
    linebptr = linebuffer;

    for(i = 0, sec = map->sectors; i < map->numsectors; ++i, sec++)
    {
        if(sectorLineLinks[i])
        {
            linelink_t *link = sectorLineLinks[i];
            sec->Lines = linebptr;
            j = 0;
            while(link)
            {
                sec->Lines[j++] = link->line;
                link = link->next;
            }
            sec->Lines[j] = NULL; // terminate.
            sec->linecount = j;
            linebptr += j + 1;
        }
        else
        {
            sec->Lines = NULL;
            sec->linecount = 0;
        }
    }
    }

    // Free temporary storage.
    Z_BlockDestroy(lineLinksBlockSet);
    M_Free(sectorLineLinks);
}

static void finishSectors(gamemap_t *map)
{
    uint        i;
    fixed_t     bmapOrg[2];
    uint        bmapSize[2];

    P_GetBlockmapOrigin(map->blockmap, bmapOrg);
    P_GetBlockmapSize(map->blockmap, bmapSize);

    for(i = 0; i < map->numsectors; ++i)
    {
        uint        k;
        float       min[2], max[2];
        sector_t   *sec = &map->sectors[i];

        findSectorSSecGroups(sec);

        if(!(sec->linecount > 0))
        {   // Its a "benign" sector.
            // Send the game a status report (we don't need to do anything).
            if(gx.HandleMapObjectStatusReport)
                gx.HandleMapObjectStatusReport(DMUSC_SECTOR_ISBENIGN,
                                               sec - map->sectors,
                                               DMU_SECTOR, NULL);
        }

        updateSectorBounds(sec);
        updateSectorBlockBox(sec, bmapOrg, bmapSize);

        P_GetSectorBounds(sec, min, max);

        // Set the degenmobj_t to the middle of the bounding box
        sec->soundorg.pos[VX] = (min[VX] + max[VX]) / 2;
        sec->soundorg.pos[VY] = (min[VY] + max[VY]) / 2;

        // Set the z height of the sector sound origin
        sec->soundorg.pos[VZ] = (sec->SP_ceilheight - sec->SP_floorheight) / 2;

        // Set the position of the sound origin for all plane sound origins.
        // Set target heights for all planes.
        for(k = 0; k < sec->planecount; ++k)
        {
            sec->planes[k]->soundorg.pos[VX] = sec->soundorg.pos[VX];
            sec->planes[k]->soundorg.pos[VY] = sec->soundorg.pos[VY];
            sec->planes[k]->soundorg.pos[VZ] = sec->planes[k]->height;

            sec->planes[k]->target = sec->planes[k]->height;
        }
    }
}

/**
 * Initialize polyobject properties.
 */
static void initPolyObjects(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->po_NumPolyobjs; ++i)
        map->polyobjs[i].header.type = DMU_POLYOBJ;
}

/**
 * Builds sector line lists and subsector sector numbers.
 * Finds block bounding boxes for sectors.
 */
static void finalizeMapData(gamemap_t *map)
{
    uint        startTime = Sys_GetRealTime();

    Con_Message(" Sector look up...\n");
    linkSSecsToSectors(map);

    Con_Message(" Build subsector tables...\n");
    buildSectorSSecLists(map);

    Con_Message(" Build line tables...\n");
    buildSectorLineLists(map);

    finishLineDefs(map);
    finishSides(map);

    if(mustCreateBlockMap)
    {
        DAM_BuildBlockMap(map);
    }

    finishSectors(map);
    initMapBlockRings(map);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("finalizeMapData: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

static void initPlaneIllumination(subsector_t *ssec, uint planeID)
{
    uint        i, j, num;
    subplaneinfo_t *plane = ssec->planes[planeID];

    num = ssec->numvertices;

    plane->illumination =
        Z_Calloc(num * sizeof(vertexillum_t), PU_LEVELSTATIC, NULL);

    for(i = 0; i < num; ++i)
    {
        plane->illumination[i].flags |= VIF_STILL_UNSEEN;

        for(j = 0; j < MAX_BIAS_AFFECTED; ++j)
            plane->illumination[i].casted[j].source = -1;
    }
}

static void initSSecPlanes(subsector_t *ssec)
{
    uint        i;

    // Allocate the subsector plane info array.
    ssec->planes =
        Z_Malloc(ssec->sector->planecount * sizeof(subplaneinfo_t*),
                 PU_LEVEL, NULL);
    for(i = 0; i < ssec->sector->planecount; ++i)
    {
        ssec->planes[i] =
            Z_Calloc(sizeof(subplaneinfo_t), PU_LEVEL, NULL);

        // Initialize the illumination for the subsector.
        initPlaneIllumination(ssec, i);
    }

    // \fixme $nplanes
    // Initialize the plane types.
    ssec->planes[PLN_FLOOR]->type = PLN_FLOOR;
    ssec->planes[PLN_CEILING]->type = PLN_CEILING;
}

static void updateSSecMidPoint(subsector_t *sub)
{
    seg_t     **ptr;
    fvertex_t  *vtx;

    // Find the center point. First calculate the bounding box.
    ptr = sub->segs;
    vtx = &((*ptr)->SG_v1->v);
    sub->bbox[0].pos[VX] = sub->bbox[1].pos[VX] = sub->midpoint.pos[VX] = vtx->pos[VX];
    sub->bbox[0].pos[VY] = sub->bbox[1].pos[VY] = sub->midpoint.pos[VY] = vtx->pos[VY];

    *ptr++;
    while(*ptr)
    {
        vtx = &((*ptr)->SG_v1->v);
        if(vtx->pos[VX] < sub->bbox[0].pos[VX])
            sub->bbox[0].pos[VX] = vtx->pos[VX];
        if(vtx->pos[VY] < sub->bbox[0].pos[VY])
            sub->bbox[0].pos[VY] = vtx->pos[VY];
        if(vtx->pos[VX] > sub->bbox[1].pos[VX])
            sub->bbox[1].pos[VX] = vtx->pos[VX];
        if(vtx->pos[VY] > sub->bbox[1].pos[VY])
            sub->bbox[1].pos[VY] = vtx->pos[VY];

        sub->midpoint.pos[VX] += vtx->pos[VX];
        sub->midpoint.pos[VY] += vtx->pos[VY];
        *ptr++;
    }

    sub->midpoint.pos[VX] /= sub->segcount; // num vertices.
    sub->midpoint.pos[VY] /= sub->segcount;
}

static void prepareSubSectors(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];

        updateSSecMidPoint(ssec);
    }
}

static void prepareSubSectorsForBias(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *ssec = &map->subsectors[i];

        initSSecPlanes(ssec);
    }
}

static ownernode_t *unusedNodeList = NULL;

static ownernode_t *newOwnerNode(void)
{
    ownernode_t *node;

    if(unusedNodeList)
    {   // An existing node is available for re-use.
        node = unusedNodeList;
        unusedNodeList = unusedNodeList->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {   // Need to allocate another.
        node = M_Malloc(sizeof(ownernode_t));
    }

    return node;
}

static void addVertexToSSecOwnerList(ownerlist_t *ownerList, fvertex_t *v)
{
    ownernode_t *node;

    if(!v)
        return; // Wha?

    // Add a new owner.
    // NOTE: No need to check for duplicates.
    ownerList->count++;

    node = newOwnerNode();
    node->data = v;
    node->next = ownerList->head;
    ownerList->head = node;
}

static void triangulateSubSector(subsector_t *ssec)
{
    uint        j;
    seg_t     **ptr;
    ownerlist_t subSecOwnerList;
    boolean     found = false;

    memset(&subSecOwnerList, 0, sizeof(subSecOwnerList));

    // Create one node for each vertex of the subsector.
    ptr = ssec->segs;
    while(*ptr)
    {
        fvertex_t *other = &((*ptr)->SG_v1->v);
        addVertexToSSecOwnerList(&subSecOwnerList, other);
        *ptr++;
    }

    // We need to find a good tri-fan base vertex, (one that doesn't
    // generate zero-area triangles).
    if(subSecOwnerList.count <= 3)
    {   // Always valid.
        found = true;
    }
    else
    {   // Higher vertex counts need checking, we'll test each one
        // and pick the first good one.
        ownernode_t *base = subSecOwnerList.head;

        while(base && !found)
        {
            ownernode_t *current;
            boolean     ok;

            current = base;
            ok = true;
            j = 0;
            while(j < subSecOwnerList.count - 2 && ok)
            {
#define TRIFAN_LIMIT    0.1

                ownernode_t *a, *b;

                if(current->next)
                    a = current->next;
                else
                    a = subSecOwnerList.head;
                if(a->next)
                    b = a->next;
                else
                    b = subSecOwnerList.head;

                if(TRIFAN_LIMIT >=
                   M_TriangleArea(((fvertex_t*) base->data)->pos,
                                  ((fvertex_t*) a->data)->pos,
                                  ((fvertex_t*) b->data)->pos))
                {
                    ok = false;
                }
                else
                {   // Keep checking...
                    if(current->next)
                        current = current->next;
                    else
                        current = subSecOwnerList.head;

                    j++;
                }

#undef TRIFAN_LIMIT
            }

            if(ok)
            {   // This will do nicely.
                // Must ensure that the vertices are ordered such that
                // base comes last (this is because when adding vertices
                // to the ownerlist; it is done backwards).
                ownernode_t *last;

                // Find the last.
                last = base;
                while(last->next) last = last->next;

                if(base != last)
                {   // Need to change the order.
                    last->next = subSecOwnerList.head;
                    subSecOwnerList.head = base->next;
                    base->next = NULL;
                }

                found = true;
            }
            else
            {
                base = base->next;
            }
        }
    }

    if(!found)
    {   // No suitable triangle fan base vertex found.
        ownernode_t *newNode, *last;

        // Use the subsector midpoint as the base since it will always
        // be valid.
        ssec->flags |= SUBF_MIDPOINT;

        // This entails adding the midpoint as a vertex at the start
        // and duplicating the first vertex at the end (so the fan
        // wraps around).

        // We'll have to add the end vertex manually...
        // Find the end.
        last = subSecOwnerList.head;
        while(last->next) last = last->next;

        newNode = newOwnerNode();
        newNode->data = &ssec->midpoint;
        newNode->next = NULL;

        last->next = newNode;
        subSecOwnerList.count++;

        addVertexToSSecOwnerList(&subSecOwnerList, last->data);
    }

    // We can now create the subsector vertex array by hardening the list.
    // NOTE: The same polygon is used for all planes of this subsector.
    ssec->numvertices = subSecOwnerList.count;
    ssec->vertices =
        Z_Malloc(sizeof(fvertex_t*) * (ssec->numvertices + 1),
                 PU_LEVELSTATIC, 0);

    {
    ownernode_t *node, *p;

    node = subSecOwnerList.head;
    j = ssec->numvertices - 1;
    while(node)
    {
        p = node->next;
        ssec->vertices[j--] = (fvertex_t*) node->data;

        // Move this node to the unused list for re-use.
        node->next = unusedNodeList;
        unusedNodeList = node;

        node = p;
    }
    }

    ssec->vertices[ssec->numvertices] = NULL; // terminate.
}

static void polygonize(gamemap_t *map)
{
    uint        i;
    ownernode_t *node, *p;
    uint        startTime;

    startTime = Sys_GetRealTime();

    // Init the unused ownernode list.
    unusedNodeList = NULL;

    // Polygonize each subsector.
    for(i = 0; i < map->numsubsectors; ++i)
    {
        subsector_t *sub = &map->subsectors[i];
        triangulateSubSector(sub);
    }

    // Free any nodes left in the unused list.
    node = unusedNodeList;
    while(node)
    {
        p = node->next;
        M_Free(node);
        node = p;
    }
    unusedNodeList = NULL;

    // How much time did we spend?
    VERBOSE(Con_Message
            ("polygonize: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

#ifdef _DEBUG
    Z_CheckHeap();
#endif
}

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int C_DECL lineAngleSorter(const void *a, const void *b)
{
    uint        i;
    fixed_t     dx, dy;
    binangle_t  angles[2];
    lineowner_t *own[2];
    line_t     *line;

    own[0] = (lineowner_t *)a;
    own[1] = (lineowner_t *)b;
    for(i = 0; i < 2; ++i)
    {
        if(own[i]->LO_prev) // We have a cached result.
        {
            angles[i] = own[i]->angle;
        }
        else
        {
            vertex_t    *otherVtx;

            line = own[i]->line;
            otherVtx = line->L_v(line->L_v1 == rootVtx? 1:0);

            dx = otherVtx->V_pos[VX] - rootVtx->V_pos[VX];
            dy = otherVtx->V_pos[VY] - rootVtx->V_pos[VY];

            own[i]->angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->LO_prev = (lineowner_t*) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return          Ptr to the newly merged list.
 */
static lineowner_t *mergeLineOwners(lineowner_t *left, lineowner_t *right,
                                    int (C_DECL *compare) (const void *a,
                                                           const void *b))
{
    lineowner_t tmp, *np;

    np = &tmp;
    tmp.LO_next = np;
    while(left != NULL && right != NULL)
    {
        if(compare(left, right) <= 0)
        {
            np->LO_next = left;
            np = left;

            left = left->LO_next;
        }
        else
        {
            np->LO_next = right;
            np = right;

            right = right->LO_next;
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->LO_next = left;
    if(right)
        np->LO_next = right;

    // Is the list empty?
    if(tmp.LO_next == &tmp)
        return NULL;

    return tmp.LO_next;
}

static lineowner_t *splitLineOwners(lineowner_t *list)
{
    lineowner_t *lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = listb->LO_next;
        lista = lista->LO_next;
        if(lista != NULL)
            lista = lista->LO_next;
    } while(lista);

    listc->LO_next = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static lineowner_t *sortLineOwners(lineowner_t *list,
                                   int (C_DECL *compare) (const void *a,
                                                          const void *b))
{
    lineowner_t *p;

    if(list && list->LO_next)
    {
        p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(vertex_t *vtx, line_t *lineptr,
                               lineowner_t **storage)
{
    lineowner_t *p, *newOwner;

    if(!lineptr)
        return;

    // If this is a one-sided line then this is an "anchored" vertex.
    if(!(lineptr->L_frontside && lineptr->L_backside))
        vtx->anchored = true;

    // Has this line already been registered with this vertex?
    if(vtx->numlineowners != 0)
    {
        p = vtx->lineowners;
        while(p)
        {
            if(p->line == lineptr)
                return;             // Yes, we can exit.

            p = p->LO_next;
        }
    }

    //Add a new owner.
    vtx->numlineowners++;

    newOwner = (*storage)++;
    newOwner->line = lineptr;
    newOwner->LO_prev = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->LO_next = vtx->lineowners;
    vtx->lineowners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == lineptr->L_v1)
        lineptr->L_vo1 = newOwner;
    else
        lineptr->L_vo2 = newOwner;
}

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings is
 * arranged in clockwise order, east = 0).
 */
static void buildVertexOwners(gamemap_t *map)
{
    uint        startTime = Sys_GetRealTime();

    uint        i;
    lineowner_t *lineOwners, *allocator;

    // We know how many vertex line owners we need (numlines * 2).
    lineOwners =
        Z_Malloc(sizeof(lineowner_t) * map->numlines * 2, PU_LEVELSTATIC, 0);
    allocator = lineOwners;

    for(i = 0; i < map->numlines; ++i)
    {
        uint        p;
        line_t     *line = &map->lines[i];

        for(p = 0; p < 2; ++p)
        {
            vertex_t    *vtx = line->L_v(p);

            setVertexLineOwner(vtx, line, &allocator);
        }
    }

    // Sort line owners and then finish the rings.
    for(i = 0; i < map->numvertexes; ++i)
    {
        vertex_t   *v = &map->vertexes[i];

        // Line owners:
        if(v->numlineowners != 0)
        {
            lineowner_t *p, *last;
            binangle_t  lastAngle = 0;

            // Sort them so that they are ordered clockwise based on angle.
            rootVtx = v;
            v->lineowners =
                sortLineOwners(v->lineowners, lineAngleSorter);

            // Finish the linking job and convert to relative angles.
            // They are only singly linked atm, we need them to be doubly
            // and circularly linked.
            last = v->lineowners;
            p = last->LO_next;
            while(p)
            {
                p->LO_prev = last;

                // Convert to a relative angle between last and this.
                last->angle = last->angle - p->angle;
                lastAngle += last->angle;

                last = p;
                p = p->LO_next;
            }
            last->LO_next = v->lineowners;
            v->lineowners->LO_prev = last;

            // Set the angle of the last owner.
            last->angle = BANG_360 - lastAngle;
/*
#if _DEBUG
{
// For checking the line owner link rings are formed correctly.
lineowner_t *base;
uint        idx;

if(verbose >= 2)
    Con_Message("Vertex #%i: line owners #%i\n", i, v->numlineowners);

p = base = v->lineowners;
idx = 0;
do
{
    if(verbose >= 2)
        Con_Message("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f\n",
                    idx, p->LO_prev->line - map->lines,
                    p->line - map->lines,
                    p->LO_next->line - map->lines, BANG2DEG(p->angle));

    if(p->LO_prev->LO_next != p || p->LO_next->LO_prev != p)
       Con_Error("Invalid line owner link ring!");

    p = p->LO_next;
    idx++;
} while(p != base);
}
#endif
*/
        }
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("buildVertexOwners: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

static void updateMapBounds(gamemap_t *map)
{
    uint        i;

    memset(map->bounds, 0, sizeof(map->bounds));
    for(i = 0; i < map->numsectors; ++i)
    {
        sector_t *sec = &map->sectors[i];

        if(i == 0)
        {
            // The first sector is used as is.
            memcpy(map->bounds, sec->bounds, sizeof(map->bounds));
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(map->bounds, sec->bounds);
        }
    }
}

static void markUnclosedSectors(gamemap_t *map)
{
    uint        i;

    for(i = 0; i < map->numsectors; ++i)
    {
        boolean     unclosed = false;
        sector_t   *sec = &map->sectors[i];

        if(sec->linecount < 3)
        {
            unclosed = true;
        }
        else
        {
            // \todo Add algorithm to check for unclosed sectors here.
            // Perhaps have a look at glBSP.
        }

        if(unclosed)
            sec->unclosed = true;
    }
}

/*boolean DD_SubContainTest(sector_t *innersec, sector_t *outersec)
   {
   uint i, k;
   boolean contained;
   float in[4], out[4];
   subsector_t *isub, *osub;

   // Test each subsector of innersec against all subsectors of outersec.
   for(i=0; i<numsubsectors; i++)
   {
   isub = SUBSECTOR_PTR(i);
   contained = false;
   // Only accept innersec's subsectors.
   if(isub->sector != innersec) continue;
   for(k=0; k<numsubsectors && !contained; k++)
   {
   osub = SUBSECTOR_PTR(i);
   // Only accept outersec's subsectors.
   if(osub->sector != outersec) continue;
   // Test if isub is inside osub.
   if(isub->bbox[BLEFT] >= osub->bbox[BLEFT]
   && isub->bbox[BRIGHT] <= osub->bbox[BRIGHT]
   && isub->bbox[BTOP] >= osub->bbox[BTOP]
   && isub->bbox[BBOTTOM] <= osub->bbox[BBOTTOM])
   {
   // This is contained.
   contained = true;
   }
   }
   if(!contained) return false;
   }
   // All tested subsectors were contained!
   return true;
   } */

/**
 * The test is done on subsectors.
 */
static sector_t *getContainingSectorOf(sector_t *sec)
{
    uint        i;
    float       cdiff = -1, diff;
    float       inner[4], outer[4];
    sector_t   *other, *closest = NULL;

    memcpy(inner, sec->bounds, sizeof(inner));

    // Try all sectors that fit in the bounding box.
    for(i = 0, other = sectors; i < numsectors; other++, ++i)
    {
        if(!other->linecount || other->unclosed)
            continue;
        if(other == sec)
            continue;           // Don't try on self!

        memcpy(outer, other->bounds, sizeof(outer));
        if(inner[BLEFT]  >= outer[BLEFT] &&
           inner[BRIGHT] <= outer[BRIGHT] &&
           inner[BTOP]   >= outer[BTOP] &&
           inner[BBOTTOM]<= outer[BBOTTOM])
        {
            // Inside! Now we must test each of the subsectors. Otherwise
            // we can't be sure...
            /*if(DD_SubContainTest(sec, other))
               { */
            // Sec is totally and completely inside other!
            diff = M_BoundingBoxDiff(inner, outer);
            if(cdiff < 0 || diff <= cdiff)
            {
                closest = other;
                cdiff = diff;
            }
            //}
        }
    }
    return closest;
}

static void buildSectorLinks(gamemap_t *map)
{
#define DOMINANT_SIZE 1000

    uint        i;

    for(i = 0; i < map->numsectors; ++i)
    {
        uint        k;
        sector_t   *other;
        line_t     *lin;
        sector_t   *sec = &map->sectors[i];
        boolean     dohack;

        if(!sec->linecount)
            continue;

        // Is this sector completely contained by another?
        sec->containsector = getContainingSectorOf(sec);

        dohack = true;
        for(k = 0; k < sec->linecount; ++k)
        {
            lin = sec->Lines[k];
            if(!lin->L_frontside || !lin->L_backside ||
                lin->L_frontsector != lin->L_backsector)
            {
                dohack = false;
                break;
            }
        }

        if(dohack)
        {   // Link all planes permanently.
            sec->permanentlink = true;
            // Only floor and ceiling can be linked, not all planes inbetween.
            for(k = 0; k < sec->subsgroupcount; ++k)
            {
                ssecgroup_t *ssgrp = &sec->subsgroups[k];
                uint        p;

                for(p = 0; p < sec->planecount; ++p)
                    ssgrp->linked[p] = sec->containsector;
            }

            Con_Printf("Linking S%i planes permanently to S%li\n", i,
                       (long) (sec->containsector - map->sectors));
        }

        // Is this sector large enough to be a dominant light source?
        if(sec->lightsource == NULL &&
           (R_IsSkySurface(&sec->SP_ceilsurface) ||
            R_IsSkySurface(&sec->SP_floorsurface)) &&
           sec->bounds[BRIGHT] - sec->bounds[BLEFT] > DOMINANT_SIZE &&
           sec->bounds[BBOTTOM] - sec->bounds[BTOP] > DOMINANT_SIZE)
        {
            // All sectors touching this one will be affected.
            for(k = 0; k < sec->linecount; ++k)
            {
                lin = sec->Lines[k];
                other = lin->L_frontsector;
                if(!other || other == sec)
                {
                    if(lin->L_backside)
                    {
                        other = lin->L_backsector;
                        if(!other || other == sec)
                            continue;
                    }
                }

                other->lightsource = sec;
            }
        }
    }

#undef DOMINANT_SIZE
}

static void doInitialSkyFix(void)
{
    uint        startTime = Sys_GetRealTime();

    R_InitSkyFix();
    R_SkyFix(true, true); // fix floors and ceilings.

    // How much time did we spend?
    VERBOSE(Con_Message
            ("doInitialSkyFix: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

static boolean loadMap(archivedmap_t *dam, gamemap_t *map)
{
    mustCreateBlockMap = false;

    countMapElements(map, dam->lumpNodes);
    allocateMapData(map);
    if(!loadMapData(map, dam->lumpNodes))
        return false; // something went horribly wrong...

    // Build the vertex line owner rings.
    buildVertexOwners(map);

    // Invoke glBSP.
    if(!BSP_Build(map))
    {
        Con_Message("P_AttemptMapLoad: Failed building BSP for map \"%s\"!\n",
                    map->levelid);
        return false;
    }

    // Do any initialization/error checking work we need to do.
    // Must be called before we go any further.
    findMissingFrontSidedefs(map);
    markSelfReferencingLinedefs(map);
    finalizeMapData(map);

    initPolyObjects(map);
    markUnclosedSectors(map);
    updateMapBounds(map);
    S_DetermineSubSecsAffectingSectorReverb(map);
    prepareSubSectors(map);

    // Polygonize.
    polygonize(map);

    // Must follow polygonize!
    prepareSubSectorsForBias(map);

    // Init polyobj blockmap.
    P_InitPolyBlockMap(map);

    // Must be called before any mobjs are spawned.
    R_InitLinks(map);

    buildSectorLinks(map);

    // Init blockmap for searching subsectors.
    P_BuildSubsectorBlockMap(map);

    return true;
}

/**
 * Attempts to load the data structures associated with the archived map.
 *
 * @param dam       Ptr to the archivedmap record of the map to load.
 * @return          @c true, if the map was loaded successfully.
 */
boolean DAM_LoadMap(archivedmap_t *dam)
{
    int         mapFormat;
    gamemap_t  *newmap;
    ded_mapinfo_t *mapInfo;

    if(!dam)
        return false;

    VERBOSE2(Con_Message("DAM_LoadMap: Loading \"%s\"...\n", dam->identifier));

    // Try to determine the format of this map.
    mapFormat = determineMapDataFormat(dam->lumpNodes);
    validateMapData(dam->lumpNodes);

    newmap = M_Malloc(sizeof(gamemap_t));
    // Initialize the new map.
    strncpy(newmap->levelid, dam->identifier, sizeof(newmap->levelid));
    strncpy(newmap->uniqueID, P_GenerateUniqueMapID(dam->identifier),
            sizeof(newmap->uniqueID));

    if(!loadMap(dam, newmap))
        return false;

    // See what mapinfo says about this level.
    mapInfo = Def_GetMapInfo(newmap->levelid);
    if(!mapInfo)
        mapInfo = Def_GetMapInfo("*");

    R_SetupSky(mapInfo);

    // Setup accordingly.
    if(mapInfo)
    {
        newmap->globalGravity = mapInfo->gravity * FRACUNIT;
        newmap->ambientLightLevel = mapInfo->ambient * 255;
    }
    else
    {
        // No map info found, so set some basic stuff.
        newmap->globalGravity = FRACUNIT;
        newmap->ambientLightLevel = 0;
    }

    // \todo should be called from P_LoadMap() but R_InitMap requires the
    // currentMap to be set first.
    P_SetCurrentMap(newmap);

    R_InitSectorShadows();

    doInitialSkyFix();

    // Announce any issues detected with the map.
    DAM_PrintMapErrors(dam, false);

    // \fixme: Should NOT be freeing this here!!
   // M_Free(newmap);
    return true;
}

/**
 * If we encountered any problems during setup - announce them to the user.
 *
 * \todo latter on this will be expanded to check for various
 * doom.exe renderer hacks and other stuff.
 *
 * @param silent        @c true = don't announce non-critical errors.
 *
 * @return              @c true = we can continue setting up the level.
 */
boolean DAM_PrintMapErrors(archivedmap_t *map, boolean silent)
{
    uint        i, printCount;
    boolean     canContinue = !numMissingFronts;

    Con_Message("P_CheckLevel: Checking %s for errors...\n",
                map->identifier);

    // If we are missing any front sidedefs announce them to the user.
    // Critical
    if(numMissingFronts)
    {
        Con_Message(" ![100] Error: Found %u linedef(s) missing front sidedefs:\n",
                    numMissingFronts);

        printCount = 0;
        for(i = 0; i < numlines; ++i)
        {
            if(missingFronts[i])
            {
                Con_Printf("%s%u,", printCount? " ": "   ", i);

                if((++printCount) > 9)
                {   // print 10 per line then wrap.
                    printCount = 0;
                    Con_Printf("\n ");
                }
            }
        }
        Con_Printf("\n");
    }

    // Announce any bad texture names we came across when loading the map.
    if(!silent)
        P_PrintMissingTextureList();

    // Dont need this stuff anymore
    if(missingFronts != NULL)
        M_Free(missingFronts);

    P_FreeBadTexList();

    if(!canContinue)
    {
        Con_Message("\nP_CheckLevel: Critical errors encountered "
                    "(marked with '!').\n  You will need to fix these errors in "
                    "order to play this map.\n");
        return false;
    }

    return true;
}

/**
 * Attempts to load the BLOCKMAP data resource.
 *
 * If the level is too large (would overflow the size limit of
 * the BLOCKMAP lump in a WAD therefore it will have been truncated),
 * it's zero length or we are forcing a rebuild - we'll have to
 * generate the blockmap data ourselves.
 */
static boolean loadBlockMap(gamemap_t* map, maplumpinfo_t* maplump)
{
    long        count = (maplump->length / 2);
    boolean     generateBMap = (createBMap == 2)? true : false;

    // \kludge: We should be able to patch up an older blockmap but for now
    // force a complete blockmap build.
    generateBMap = true;

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

        mustCreateBlockMap = true;
    }
    else
    {
        /**
         * No, the existing data is valid - so load it in.
         * Data in PWAD is little endian.
	     */
        blockmap_t *blockmap;
        uint        x, y, width, height;
        fixed_t     originX, originY;
        long       *blockmapExpanded;

        {
        uint        n;
        long        i;
        short      *blockmapLump;

        blockmapLump =
            (short *) W_CacheLumpNum(maplump->lumpNum, PU_STATIC);

        originX = ((fixed_t) SHORT(blockmapLump[0])) << FRACBITS;
        originY = ((fixed_t) SHORT(blockmapLump[1])) << FRACBITS;
        width  = ((SHORT(blockmapLump[2])) & 0xffff);
        height = ((SHORT(blockmapLump[3])) & 0xffff);

        /**
         * Expand WAD blockmap into a larger one, by treating all offsets
         * except -1 as unsigned and zero-extending them. This potentially
         * doubles the size of blockmaps allowed because DOOM originally
         * considered the offsets as always signed.
	     */
        blockmapExpanded = M_Malloc(sizeof(long) * (count - 4));

        n = 4;
        for(i = 0; i < count - 4; ++i)
        {
            short t = SHORT(blockmapLump[n]);
            blockmapExpanded[i] = (t == -1? -1 : (long) t & 0xffff);
        }
        }

        /**
         * Finally, convert the blockmap into our internal representation.
         */
        blockmap = P_BlockmapCreate(originX, originY, width, height);

        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
            {
                int         offset = *(blockmapExpanded + (y * width + x));
                long       *list;
                uint        count;

                // Count the number of lines in this block.
                count = 0;
                for(list = blockmapExpanded + offset; *list != -1; list++)
                    count++;

                if(count > 0)
                {
                    line_t    **lines, **ptr;

                    // A NULL-terminated array of pointers to lines.
                    lines = Z_Malloc((count + 1) * sizeof(line_t *),
                                    PU_LEVELSTATIC, NULL);

                    // Copy pointers to the array, delete the nodes.
                    ptr = lines;
                    for(list = blockmapExpanded + offset;
                        *list != -1; list++)
                    {
                        *ptr++ = &map->lines[*list];
                    }
                    // Terminate.
                    *ptr = NULL;

                    // Link it into the BlockMap.
                    P_BlockmapSetBlock(blockmap, x, y, lines);
                }
            }

        // Don't need this anymore.
        M_Free(blockmapExpanded);

        map->blockmap = blockmap;
    }

    return true;
}

/**
 * This is temporary. Ideally reject data should be loaded with
 * P_ReadBinaryMapData but not treated as an aggregate data type.
 * We should only need this func if we have to generate data.
 */
static boolean loadReject(gamemap_t* map, maplumpinfo_t* maplump)
{
    boolean     generateReject = (createReject == 2)? true : false;

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

        DAM_BuildReject(map);
    }
    else
    {
        byte       *lumpp =
            (byte *) W_CacheLumpNum(maplump->lumpNum, PU_STATIC);

        map->rejectmatrix = Z_Malloc(maplump->length, PU_LEVELSTATIC, 0);
        memcpy(map->rejectmatrix, lumpp, maplump->length);
        Z_Free(lumpp);
    }

    // Success!
    return true;
}
