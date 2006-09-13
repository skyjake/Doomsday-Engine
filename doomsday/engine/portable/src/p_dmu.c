/* DE1: $Id$
 * Copyright (C) 2006 Jaakko Keränen <jaakko.keranen@iki.fi>
 *                    Daniel Swanson <danij@users.sourceforge.net>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * p_dmu.c: Doomsday Map Update API
 *
 * The Map Update API is used for accessing and making changes to map data
 * during gameplay. From here, the relevant engine's subsystems will be
 * notified of changes in the map data they use, thus allowing them to
 * update their status whenever needed.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"

// MACROS ------------------------------------------------------------------

// Engine internal aliases for public DMU constants.
#define DMU_FLOOR_OF_SECTOR     0x01
#define DMU_CEILING_OF_SECTOR   0x02

// TYPES -------------------------------------------------------------------

typedef struct dummyline_s {
    line_t  line;               // Line data.
    void*   extraData;          // Pointer to user data.
    boolean inUse;              // true, if the dummy is being used.
} dummyline_t;

typedef struct dummysector_s {
    sector_t  sector;           // Sector data.
    void*   extraData;          // Pointer to user data.
    boolean inUse;              // true, if the dummy is being used.
} dummysector_t;

typedef struct setargs_s {
    int type;
    int prop;
    int modifiers;              // Property modifiers (e.g., line of sector)
    int aliases;                // Property aliases (non-public modifiers)
                                // (eg., floor of sector)
    valuetype_t valueType;
    boolean* booleanValues;
    byte* byteValues;
    int* intValues;
    fixed_t* fixedValues;
    float* floatValues;
    angle_t* angleValues;
    void** ptrValues;
} setargs_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     dummyCount = 8;         // Number of dummies to allocate (per type).

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static dummyline_t* dummyLines;
static dummysector_t* dummySectors;

static int  usingDMUAPIver;     // Version of the DMU API the game expects.

// CODE --------------------------------------------------------------------

/**
 * Convert DMU enum constant into a string for error/debug messages.
 */
const char* DMU_Str(int prop)
{
    static char propStr[40];
    struct prop_s {
        int prop;
        const char* str;
    } props[] =
    {
        { DMU_ALL, "DMU_ALL" },
        { 0, "(invalid)" },
        { DMU_VERTEX, "DMU_VERTEX" },
        { DMU_SEG, "DMU_SEG" },
        { DMU_LINE, "DMU_LINE" },
        { DMU_SIDE, "DMU_SIDE" },
        { DMU_NODE, "DMU_NODE" },
        { DMU_SUBSECTOR, "DMU_SUBSECTOR" },
        { DMU_SECTOR, "DMU_SECTOR" },
        { DMU_PLANE, "DMU_PLANE" },
        { DMU_BLOCKMAP, "DMU_BLOCKMAP" },
        { DMU_REJECT, "DMU_REJECT" },
        { DMU_POLYBLOCKMAP, "DMU_POLYBLOCKMAP" },
        { DMU_POLYOBJ, "DMU_POLYOBJ" },
        { DMU_LINE_BY_TAG, "DMU_LINE_BY_TAG" },
        { DMU_SECTOR_BY_TAG, "DMU_SECTOR_BY_TAG" },
        { DMU_LINE_BY_ACT_TAG, "DMU_LINE_BY_ACT_TAG" },
        { DMU_SECTOR_BY_ACT_TAG, "DMU_SECTOR_BY_ACT_TAG" },
        { DMU_X, "DMU_X" },
        { DMU_Y, "DMU_Y" },
        { DMU_XY, "DMU_XY" },
        { DMU_VERTEX1, "DMU_VERTEX1" },
        { DMU_VERTEX2, "DMU_VERTEX2" },
        { DMU_VERTEX1_X, "DMU_VERTEX1_X" },
        { DMU_VERTEX1_Y, "DMU_VERTEX1_Y" },
        { DMU_VERTEX1_XY, "DMU_VERTEX1_XY" },
        { DMU_VERTEX2_X, "DMU_VERTEX2_X" },
        { DMU_VERTEX2_Y, "DMU_VERTEX2_Y" },
        { DMU_VERTEX2_XY, "DMU_VERTEX2_XY" },
        { DMU_FRONT_SECTOR, "DMU_FRONT_SECTOR" },
        { DMU_BACK_SECTOR, "DMU_BACK_SECTOR" },
        { DMU_SIDE0, "DMU_SIDE0" },
        { DMU_SIDE1, "DMU_SIDE1" },
        { DMU_FLAGS, "DMU_FLAGS" },
        { DMU_DX, "DMU_DX" },
        { DMU_DY, "DMU_DY" },
        { DMU_LENGTH, "DMU_LENGTH" },
        { DMU_SLOPE_TYPE, "DMU_SLOPE_TYPE" },
        { DMU_ANGLE, "DMU_ANGLE" },
        { DMU_OFFSET, "DMU_OFFSET" },
        { DMU_TOP_TEXTURE, "DMU_TOP_TEXTURE" },
        { DMU_TOP_TEXTURE_OFFSET_X, "DMU_TOP_TEXTURE_OFFSET_X" },
        { DMU_TOP_TEXTURE_OFFSET_Y, "DMU_TOP_TEXTURE_OFFSET_Y" },
        { DMU_TOP_TEXTURE_OFFSET_XY, "DMU_TOP_TEXTURE_OFFSET_XY" },
        { DMU_TOP_COLOR, "DMU_TOP_COLOR" },
        { DMU_TOP_COLOR_RED, "DMU_TOP_COLOR_RED" },
        { DMU_TOP_COLOR_GREEN, "DMU_TOP_COLOR_GREEN" },
        { DMU_TOP_COLOR_BLUE, "DMU_TOP_COLOR_BLUE" },
        { DMU_MIDDLE_TEXTURE, "DMU_MIDDLE_TEXTURE" },
        { DMU_MIDDLE_TEXTURE_OFFSET_X, "DMU_MIDDLE_TEXTURE_OFFSET_X" },
        { DMU_MIDDLE_TEXTURE_OFFSET_Y, "DMU_MIDDLE_TEXTURE_OFFSET_Y" },
        { DMU_MIDDLE_TEXTURE_OFFSET_XY, "DMU_MIDDLE_TEXTURE_OFFSET_XY" },
        { DMU_MIDDLE_COLOR, "DMU_MIDDLE_COLOR" },
        { DMU_MIDDLE_COLOR_RED, "DMU_MIDDLE_COLOR_RED" },
        { DMU_MIDDLE_COLOR_GREEN, "DMU_MIDDLE_COLOR_GREEN" },
        { DMU_MIDDLE_COLOR_BLUE, "DMU_MIDDLE_COLOR_BLUE" },
        { DMU_MIDDLE_COLOR_ALPHA, "DMU_MIDDLE_COLOR_ALPHA" },
        { DMU_MIDDLE_BLENDMODE, "DMU_MIDDLE_BLENDMODE" },
        { DMU_BOTTOM_TEXTURE, "DMU_BOTTOM_TEXTURE" },
        { DMU_BOTTOM_TEXTURE_OFFSET_X, "DMU_BOTTOM_TEXTURE_OFFSET_X" },
        { DMU_BOTTOM_TEXTURE_OFFSET_Y, "DMU_BOTTOM_TEXTURE_OFFSET_Y" },
        { DMU_BOTTOM_TEXTURE_OFFSET_XY, "DMU_BOTTOM_TEXTURE_OFFSET_XY" },
        { DMU_BOTTOM_COLOR, "DMU_BOTTOM_COLOR" },
        { DMU_BOTTOM_COLOR_RED, "DMU_BOTTOM_COLOR_RED" },
        { DMU_BOTTOM_COLOR_GREEN, "DMU_BOTTOM_COLOR_GREEN" },
        { DMU_BOTTOM_COLOR_BLUE, "DMU_BOTTOM_COLOR_BLUE" },
        { DMU_VALID_COUNT, "DMU_VALID_COUNT" },
        { DMU_LINE_COUNT, "DMU_LINE_COUNT" },
        { DMU_COLOR, "DMU_COLOR" },
        { DMU_COLOR_RED, "DMU_COLOR_RED" },
        { DMU_COLOR_GREEN, "DMU_COLOR_GREEN" },
        { DMU_COLOR_BLUE, "DMU_COLOR_BLUE" },
        { DMU_LIGHT_LEVEL, "DMU_LIGHT_LEVEL" },
        { DMU_THINGS, "DMU_THINGS" },
        { DMU_BOUNDING_BOX, "DMU_BOUNDING_BOX" },
        { DMU_SOUND_ORIGIN, "DMU_SOUND_ORIGIN" },
        { DMU_SOUND_REVERB, "DMU_SOUND_REVERB" },
        { DMU_PLANE_HEIGHT, "DMU_PLANE_HEIGHT" },
        { DMU_PLANE_TEXTURE, "DMU_PLANE_TEXTURE" },
        { DMU_PLANE_OFFSET_X, "DMU_PLANE_OFFSET_X" },
        { DMU_PLANE_OFFSET_Y, "DMU_PLANE_OFFSET_Y" },
        { DMU_PLANE_OFFSET_XY, "DMU_PLANE_OFFSET_XY" },
        { DMU_PLANE_TARGET, "DMU_PLANE_TARGET" },
        { DMU_PLANE_SPEED, "DMU_PLANE_SPEED" },
        { DMU_PLANE_COLOR, "DMU_PLANE_COLOR" },
        { DMU_PLANE_COLOR_RED, "DMU_PLANE_COLOR_RED" },
        { DMU_PLANE_COLOR_GREEN, "DMU_PLANE_COLOR_GREEN" },
        { DMU_PLANE_COLOR_BLUE, "DMU_PLANE_COLOR_BLUE" },
        { DMU_PLANE_TEXTURE_MOVE_X, "DMU_PLANE_TEXTURE_MOVE_X" },
        { DMU_PLANE_TEXTURE_MOVE_Y, "DMU_PLANE_TEXTURE_MOVE_Y" },
        { DMU_PLANE_TEXTURE_MOVE_XY, "DMU_PLANE_TEXTURE_MOVE_XY" },
        { DMU_PLANE_SOUND_ORIGIN, "DMU_PLANE_SOUND_ORIGIN" },
        { DMU_FLOOR_HEIGHT, "DMU_FLOOR_HEIGHT" },
        { DMU_FLOOR_TEXTURE, "DMU_FLOOR_TEXTURE" },
        { DMU_FLOOR_OFFSET_X, "DMU_FLOOR_OFFSET_X" },
        { DMU_FLOOR_OFFSET_Y, "DMU_FLOOR_OFFSET_Y" },
        { DMU_FLOOR_OFFSET_XY, "DMU_FLOOR_OFFSET_XY" },
        { DMU_FLOOR_TARGET, "DMU_FLOOR_TARGET" },
        { DMU_FLOOR_SPEED, "DMU_FLOOR_SPEED" },
        { DMU_FLOOR_COLOR, "DMU_FLOOR_COLOR" },
        { DMU_FLOOR_COLOR_RED, "DMU_FLOOR_COLOR_RED" },
        { DMU_FLOOR_COLOR_GREEN, "DMU_FLOOR_COLOR_GREEN" },
        { DMU_FLOOR_COLOR_BLUE, "DMU_FLOOR_COLOR_BLUE" },
        { DMU_FLOOR_TEXTURE_MOVE_X, "DMU_FLOOR_TEXTURE_MOVE_X" },
        { DMU_FLOOR_TEXTURE_MOVE_Y, "DMU_FLOOR_TEXTURE_MOVE_Y" },
        { DMU_FLOOR_TEXTURE_MOVE_XY, "DMU_FLOOR_TEXTURE_MOVE_XY" },
        { DMU_FLOOR_SOUND_ORIGIN, "DMU_FLOOR_SOUND_ORIGIN" },
        { DMU_CEILING_HEIGHT, "DMU_CEILING_HEIGHT" },
        { DMU_CEILING_TEXTURE, "DMU_CEILING_TEXTURE" },
        { DMU_CEILING_OFFSET_X, "DMU_CEILING_OFFSET_X" },
        { DMU_CEILING_OFFSET_Y, "DMU_CEILING_OFFSET_Y" },
        { DMU_CEILING_OFFSET_XY, "DMU_CEILING_OFFSET_XY" },
        { DMU_CEILING_TARGET, "DMU_CEILING_TARGET" },
        { DMU_CEILING_SPEED, "DMU_CEILING_SPEED" },
        { DMU_CEILING_COLOR, "DMU_CEILING_COLOR" },
        { DMU_CEILING_COLOR_RED, "DMU_CEILING_COLOR_RED" },
        { DMU_CEILING_COLOR_GREEN, "DMU_CEILING_COLOR_GREEN" },
        { DMU_CEILING_COLOR_BLUE, "DMU_CEILING_COLOR_BLUE" },
        { DMU_CEILING_TEXTURE_MOVE_X, "DMU_CEILING_TEXTURE_MOVE_X" },
        { DMU_CEILING_TEXTURE_MOVE_Y, "DMU_CEILING_TEXTURE_MOVE_Y" },
        { DMU_CEILING_TEXTURE_MOVE_XY, "DMU_CEILING_TEXTURE_MOVE_XY" },
        { DMU_CEILING_SOUND_ORIGIN, "DMU_CEILING_SOUND_ORIGIN" },
        { DMU_SEG_LIST, "DMU_SEG_LIST" },
        { DMU_SEG_COUNT, "DMU_SEG_COUNT" },
        { DMU_TAG, "DMU_TAG" },
        { DMU_ORIGINAL_POINTS, "DMU_ORIGINAL_POINTS" },
        { DMU_PREVIOUS_POINTS, "DMU_PREVIOUS_POINTS" },
        { DMU_START_SPOT, "DMU_START_SPOT" },
        { DMU_START_SPOT_X, "DMU_START_SPOT_X" },
        { DMU_START_SPOT_Y, "DMU_START_SPOT_Y" },
        { DMU_START_SPOT_XY, "DMU_START_SPOT_XY" },
        { DMU_DESTINATION_X, "DMU_DESTINATION_X" },
        { DMU_DESTINATION_Y, "DMU_DESTINATION_Y" },
        { DMU_DESTINATION_XY, "DMU_DESTINATION_XY" },
        { DMU_DESTINATION_ANGLE, "DMU_DESTINATION_ANGLE" },
        { DMU_SPEED, "DMU_SPEED" },
        { DMU_ANGLE_SPEED, "DMU_ANGLE_SPEED" },
        { DMU_SEQUENCE_TYPE, "DMU_SEQUENCE_TYPE" },
        { DMU_CRUSH, "DMU_CRUSH" },
        { DMU_SPECIAL_DATA, "DMU_SPECIAL_DATA" },
        { 0, NULL }
    };
    int i;

    for(i = 0; props[i].str; ++i)
        if(props[i].prop == prop)
            return props[i].str;

    sprintf(propStr, "(unnamed %i)", prop);
    return propStr;
}

/**
 * Determines the type of the map data object.
 *
 * @param ptr  Pointer to a map data object.
 */
static int DMU_GetType(const void* ptr)
{
    int type;

    type = P_DummyType((void*)ptr);
    if(type != DMU_NONE)
        return type;

    type = ((const runtime_mapdata_header_t*)ptr)->type;

    // Make sure it's valid.
    switch(type)
    {
        case DMU_VERTEX:
        case DMU_SEG:
        case DMU_LINE:
        case DMU_SIDE:
        case DMU_SUBSECTOR:
        case DMU_SECTOR:
        case DMU_PLANE:
        case DMU_POLYOBJ:
        case DMU_NODE:
            return type;

        default:
            // Unknown.
            break;
    }
    return DMU_NONE;
}

/**
 * Automatically detect and convert property constants that act as aliases.
 * Property constant aliases are "alternative names" for other constants
 * that relate to properties easily reached through indirection of the base
 * object but arn't actually properties of the base object itself.
 *
 * In otherwords - not modifiers:
 * Aliases are implicit, non-public and refer to one or more properties
 * of multiple objects.
 *
 * Side Effect: This routine may modify the content of "args".
 *
 * @param args      The setargs struct to work with.
 *
 * @return boolean  True if conversion took place.
 */
static boolean DMU_ConvertAliases(setargs_t* args)
{
    switch(args->type)
    {
    case DMU_SECTOR:
    case DMU_SUBSECTOR:
        switch(args->prop)
        {
        case DMU_FLOOR_HEIGHT:
        case DMU_FLOOR_TEXTURE:
        case DMU_FLOOR_OFFSET_X:
        case DMU_FLOOR_OFFSET_Y:
        case DMU_FLOOR_OFFSET_XY:
        case DMU_FLOOR_TARGET:
        case DMU_FLOOR_SPEED:
        case DMU_FLOOR_COLOR:
        case DMU_FLOOR_COLOR_RED:
        case DMU_FLOOR_COLOR_GREEN:
        case DMU_FLOOR_COLOR_BLUE:
        case DMU_FLOOR_TEXTURE_MOVE_X:
        case DMU_FLOOR_TEXTURE_MOVE_Y:
        case DMU_FLOOR_TEXTURE_MOVE_XY:
        case DMU_FLOOR_SOUND_ORIGIN:
            args->prop = DMU_PLANE_HEIGHT + (args->prop - DMU_FLOOR_HEIGHT);
            args->aliases |= DMU_FLOOR_OF_SECTOR;
            return true;

        case DMU_CEILING_HEIGHT:
        case DMU_CEILING_TEXTURE:
        case DMU_CEILING_OFFSET_X:
        case DMU_CEILING_OFFSET_Y:
        case DMU_CEILING_OFFSET_XY:
        case DMU_CEILING_TARGET:
        case DMU_CEILING_SPEED:
        case DMU_CEILING_COLOR:
        case DMU_CEILING_COLOR_RED:
        case DMU_CEILING_COLOR_GREEN:
        case DMU_CEILING_COLOR_BLUE:
        case DMU_CEILING_TEXTURE_MOVE_X:
        case DMU_CEILING_TEXTURE_MOVE_Y:
        case DMU_CEILING_TEXTURE_MOVE_XY:
        case DMU_CEILING_SOUND_ORIGIN:
            args->prop = DMU_PLANE_HEIGHT + (args->prop - DMU_CEILING_HEIGHT);
            args->aliases |= DMU_CEILING_OF_SECTOR;
            return true;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return false;
}

/**
 * Initializes a setargs struct.
 *
 * @param type  Type of the map data object (e.g., DMU_LINE).
 * @param prop  Property of the map data object.
 */
static void InitArgs(setargs_t* args, int type, int prop)
{
    memset(args, 0, sizeof(*args));
    args->type = type;
    args->prop = prop & ~DMU_FLAG_MASK;
    args->modifiers = prop & DMU_FLAG_MASK;
    args->aliases = 0;

    DMU_ConvertAliases(args);
}

/**
 * Initializes the dummy arrays with a fixed number of dummies.
 */
void P_InitMapUpdate(void)
{
    // Request the DMU API version the game is expecting.
    usingDMUAPIver = gx.GetInteger(DD_GAME_DMUAPI_VER);
    if(!usingDMUAPIver)
        Con_Error("P_InitMapUpdate: Game dll is not compatible with "
                  "Doomsday " DOOMSDAY_VERSION_TEXT ".");

    if(usingDMUAPIver > DMUAPI_VER)
        Con_Error("P_InitMapUpdate: Game dll expects a latter version of the\n"
                  "DMU API then that defined by Doomsday " DOOMSDAY_VERSION_TEXT ".\n"
                  "This game is for a newer version of Doomsday.");

    // A fixed number of dummies is allocated because:
    // - The number of dummies is mostly dependent on recursive depth of
    //   game functions.
    // - To test whether a pointer refers to a dummy is based on pointer
    //   comparisons; if the array is reallocated, its address may change
    //   and all existing dummies are invalidated.
    dummyLines = Z_Calloc(dummyCount * sizeof(dummyline_t), PU_STATIC, NULL);
    dummySectors = Z_Calloc(dummyCount * sizeof(dummysector_t), PU_STATIC, NULL);
}

/**
 * Allocates a new dummy object.
 *
 * @param type  DMU type of the dummy object.
 * @param extraData  Extra data pointer of the dummy. Points to caller-allocated
 *                   memory area of extra data for the dummy.
 */
void* P_AllocDummy(int type, void* extraData)
{
    int i;

    switch(type)
    {
    case DMU_LINE:
        for(i = 0; i < dummyCount; ++i)
        {
            if(!dummyLines[i].inUse)
            {
                dummyLines[i].inUse = true;
                dummyLines[i].extraData = extraData;
                dummyLines[i].line.header.type = DMU_LINE;
                dummyLines[i].line.sidenum[0] = -1;
                dummyLines[i].line.sidenum[1] = -1;
                return &dummyLines[i];
            }
        }
        break;

    case DMU_SECTOR:
        for(i = 0; i < dummyCount; ++i)
        {
            if(!dummySectors[i].inUse)
            {
                dummySectors[i].inUse = true;
                dummySectors[i].extraData = extraData;
                dummySectors[i].sector.header.type = DMU_SECTOR;
                return &dummySectors[i];
            }
        }
        break;

    default:
        Con_Error("P_AllocDummy: Dummies of type %s not supported.\n",
                  DMU_Str(type));
    }

    Con_Error("P_AllocDummy: Out of dummies of type %s.\n", DMU_Str(type));
    return 0;
}

/**
 * Frees a dummy object.
 */
void P_FreeDummy(void* dummy)
{
    int type = P_DummyType(dummy);

    switch(type)
    {
    case DMU_LINE:
        ((dummyline_t*)dummy)->inUse = false;
        break;

    case DMU_SECTOR:
        ((dummysector_t*)dummy)->inUse = false;
        break;

    default:
        Con_Error("P_FreeDummy: Dummy is of unknown type.\n");
        break;
    }
}

/**
 * Determines the type of a dummy object. For extra safety (in a debug build)
 * it would be possible to look through the dummy arrays and make sure the
 * pointer refers to a real dummy.
 */
int P_DummyType(void* dummy)
{
    // Is it a line?
    if(dummy >= (void*) &dummyLines[0] &&
       dummy <= (void*) &dummyLines[dummyCount - 1])
    {
        return DMU_LINE;
    }

    // A sector?
    if(dummy >= (void*) &dummySectors[0] &&
       dummy <= (void*) &dummySectors[dummyCount - 1])
    {
        return DMU_SECTOR;
    }

    // Unknown.
    return DMU_NONE;
}

/**
 * Determines if a map data object is a dummy.
 */
boolean P_IsDummy(void* dummy)
{
    return P_DummyType(dummy) != DMU_NONE;
}

/**
 * Returns the extra data pointer of the dummy, or NULL if the object is not
 * a dummy object.
 */
void* P_DummyExtraData(void* dummy)
{
    switch(P_DummyType(dummy))
    {
    case DMU_LINE:
        return ((dummyline_t*)dummy)->extraData;

    case DMU_SECTOR:
        return ((dummysector_t*)dummy)->extraData;

    default:
        break;
    }
    return NULL;
}

/**
 * Convert pointer to index.
 */
int P_ToIndex(const void* ptr)
{
    if(!ptr)
    {
        return -1;
    }

    switch(DMU_GetType(ptr))
    {
    case DMU_VERTEX:
        return GET_VERTEX_IDX((vertex_t*) ptr);

    case DMU_SEG:
        return GET_SEG_IDX((seg_t*) ptr);

    case DMU_LINE:
        return GET_LINE_IDX((line_t*) ptr);

    case DMU_SIDE:
        return GET_SIDE_IDX((side_t*) ptr);

    case DMU_SUBSECTOR:
        return GET_SUBSECTOR_IDX((subsector_t*) ptr);

    case DMU_SECTOR:
        return GET_SECTOR_IDX((sector_t*) ptr);

    case DMU_POLYOBJ:
        return GET_POLYOBJ_IDX((polyobj_t*) ptr);

    case DMU_NODE:
        return GET_NODE_IDX((node_t*) ptr);

    case DMU_PLANE:
        return GET_PLANE_IDX((plane_t*) ptr);

    default:
        Con_Error("P_ToIndex: Unknown type %s.\n", DMU_Str(DMU_GetType(ptr)));
    }
    return -1;
}

/**
 * Convert index to pointer.
 */
void* P_ToPtr(int type, int index)
{
    switch(type)
    {
    case DMU_VERTEX:
        return VERTEX_PTR(index);

    case DMU_SEG:
        return SEG_PTR(index);

    case DMU_LINE:
        return LINE_PTR(index);

    case DMU_SIDE:
        return SIDE_PTR(index);

    case DMU_SUBSECTOR:
        return SUBSECTOR_PTR(index);

    case DMU_SECTOR:
        return SECTOR_PTR(index);

    case DMU_POLYOBJ:
        return PO_PTR(index);

    case DMU_NODE:
        return NODE_PTR(index);

    case DMU_PLANE:
        Con_Error("P_ToPtr: Cannot convert %s to a ptr (sector is unknown).\n",
                  DMU_Str(type));
        break;

    default:
        Con_Error("P_ToPtr: unknown type %s.\n", DMU_Str(type));
    }
    return NULL;
}

/**
 * Call a callback function on a selection of map data objects. The selected
 * objects will be specified by 'type' and 'index'. 'context' is passed to the
 * callback function along with a pointer to the data object. P_Callback
 * returns true if all the calls to the callback function return true. False
 * is returned when the callback function returns false; in this case, the
 * iteration is aborted immediately when the callback function returns false.
 */
int P_Callback(int type, int index, void* context, int (*callback)(void* p, void* ctx))
{
    int i;

    switch(type)
    {
    case DMU_VERTEX:
        if(index >= 0 && index < numvertexes)
        {
            return callback(VERTEX_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numvertexes; ++i)
                if(!callback(VERTEX_PTR(i), context)) return false;
        }
        break;

    case DMU_SEG:
        if(index >= 0 && index < numsegs)
        {
            return callback(SEG_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsegs; ++i)
                if(!callback(SEG_PTR(i), context)) return false;
        }
        break;

    case DMU_LINE:
        if(index >= 0 && index < numlines)
        {
            return callback(LINE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numlines; ++i)
                if(!callback(LINE_PTR(i), context)) return false;
        }
        break;

    case DMU_SIDE:
        if(index >= 0 && index < numsides)
        {
            return callback(SIDE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsides; ++i)
                if(!callback(SIDE_PTR(i), context)) return false;
        }
        break;

    case DMU_NODE:
        if(index >= 0 && index < numnodes)
        {
            return callback(NODE_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numnodes; ++i)
                if(!callback(NODE_PTR(i), context)) return false;
        }
        break;

    case DMU_SUBSECTOR:
        if(index >= 0 && index < numsubsectors)
        {
            return callback(SUBSECTOR_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsubsectors; ++i)
                if(!callback(SUBSECTOR_PTR(i), context)) return false;
        }
        break;

    case DMU_SECTOR:
        if(index >= 0 && index < numsectors)
        {
            return callback(SECTOR_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < numsectors; ++i)
                if(!callback(SECTOR_PTR(i), context)) return false;
        }
        break;

    case DMU_POLYOBJ:
        if(index >= 0 && index < po_NumPolyobjs)
        {
            return callback(PO_PTR(index), context);
        }
        else if(index == DMU_ALL)
        {
            for(i = 0; i < po_NumPolyobjs; i++)
                if(!callback(PO_PTR(i), context)) return false;
        }
        break;

    case DMU_PLANE:
        Con_Error("P_Callback: %s cannot be referenced by id alone (sector is unknown).\n",
                  DMU_Str(type));
        break;

    case DMU_LINE_BY_TAG:
    case DMU_SECTOR_BY_TAG:
    case DMU_LINE_BY_ACT_TAG:
    case DMU_SECTOR_BY_ACT_TAG:
        Con_Error("P_Callback: Type %s not implemented yet.\n", DMU_Str(type));
        /*
        for(i = 0; i < numlines; ++i)
        {
            if(!callback(LINE_PTR(i), context)) return false;
        }
        */
        break;

    default:
        Con_Error("P_Callback: Type %s unknown (index %i).\n", DMU_Str(type), index);
    }

    // Successfully completed.
    return true;
}

/**
 * Another version of callback iteration. The set of selected objects is
 * determined by 'type' and 'ptr'. Otherwise works like P_Callback.
 */
int P_Callbackp(int type, void* ptr, void* context, int (*callback)(void* p, void* ctx))
{
    switch(type)
    {
    case DMU_VERTEX:
    case DMU_SEG:
    case DMU_LINE:
    case DMU_SIDE:
    case DMU_NODE:
    case DMU_SUBSECTOR:
    case DMU_SECTOR:
    case DMU_POLYOBJ:
    case DMU_PLANE:
        // Only do the callback if the type is the same as the object's.
        if(type == DMU_GetType(ptr))
        {
            return callback(ptr, context);
        }
#if _DEBUG
        else
        {
            Con_Message("P_Callbackp: Type mismatch %s != %s\n",
                        DMU_Str(type), DMU_Str(DMU_GetType(ptr)));
        }
#endif
        break;

    // TODO: If necessary, add special types for accessing multiple objects.

    default:
        Con_Error("P_Callbackp: Type %s unknown.\n", DMU_Str(type));
    }
    return true;
}

/**
 * Sets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
static void SetValue(valuetype_t valueType, void* dst, setargs_t* args, int index)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = (args->byteValues[index] << FRACBITS);
            break;
        case DDVT_INT:
            *d = (args->intValues[index] << FRACBITS);
            break;
        case DDVT_FIXED:
            *d = args->fixedValues[index];
            break;
        case DDVT_FLOAT:
            *d = FLT2FIX(args->floatValues[index]);
            break;
        default:
            Con_Error("SetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float* d = dst;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FIXED:
            *d = FIX2FLT(args->fixedValues[index]);
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BOOL incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        byte* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = (byte) args->floatValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_INT)
    {
        int* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short* d = dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        case DDVT_BYTE:
            *d = args->byteValues[index];
            break;
        case DDVT_INT:
            *d = args->intValues[index];
            break;
        case DDVT_FLOAT:
            *d = args->floatValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default:
            Con_Error("SetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            *d = args->angleValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t* d = dst;

        switch(args->valueType)
        {
        case DDVT_INT:
            if(args->intValues[index] > DDNUM_BLENDMODES || args->intValues[index] < 0)
                Con_Error("SetValue: %d is not a valid value for DDVT_BLENDMODE.\n",
                          args->intValues[index]);

            *d = args->intValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_BLENDMODE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_PTR)
    {
        void** d = dst;

        switch(args->valueType)
        {
        case DDVT_PTR:
            *d = args->ptrValues[index];
            break;
        default:
            Con_Error("SetValue: DDVT_PTR incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else
    {
        Con_Error("SetValue: unknown value type %d.\n", valueType);
    }
}

/**
 * Only those properties that are writable by outside parties (such as games)
 * are included here. Attempting to set a non-writable property causes a
 * fatal error.
 *
 * When a property changes, the relevant subsystems are notified of the change
 * so that they can update their state accordingly.
 */
static int SetProperty(void* ptr, void* context)
{
    setargs_t* args = (setargs_t*) context;

    // Check modified cases first.
    // Then aliases.
    if(args->type == DMU_PLANE ||
       (args->type == DMU_SECTOR &&
          ((args->aliases & DMU_FLOOR_OF_SECTOR) ||
           (args->aliases & DMU_CEILING_OF_SECTOR))) ||
       (args->type == DMU_SUBSECTOR &&
          ((args->aliases & DMU_FLOOR_OF_SECTOR) ||
           (args->aliases & DMU_CEILING_OF_SECTOR))))
    {
        plane_t* p = NULL;
        if(args->type == DMU_PLANE)
            p = ptr;
        else if(args->type == DMU_SECTOR)
        {
            sector_t* sec = (sector_t*)ptr;

            if(args->aliases & DMU_FLOOR_OF_SECTOR)
            {
                if(!sec->planes[PLN_FLOOR])
                    Con_Error("SetProperty: Sector %i does not have a "
                              "floor plane!", P_ToIndex(sec));

                p = sec->planes[PLN_FLOOR];
            }
            else
            {
                if(!sec->planes[PLN_FLOOR])
                    Con_Error("SetProperty: Sector %i does not have a "
                              "ceiling plane!", P_ToIndex(sec));

                p = sec->planes[PLN_CEILING];
            }
        }
        else if(args->type == DMU_SUBSECTOR)
        {
            sector_t* sec = ((subsector_t*)ptr)->sector;
            if(args->aliases & DMU_FLOOR_OF_SECTOR)
            {
                if(!sec->planes[PLN_FLOOR])
                    Con_Error("SetProperty: Sector %i does not have a "
                              "floor plane!", P_ToIndex(sec));

                p = sec->planes[PLN_FLOOR];
            }
            else
            {
                if(!sec->planes[PLN_FLOOR])
                    Con_Error("SetProperty: Sector %i does not have a "
                              "ceiling plane!", P_ToIndex(sec));

                p = sec->planes[PLN_CEILING];
            }
        }
        else
            Con_Error("SetProperty: Invalid args.\n");

        switch(args->prop)
        {
        case DMU_PLANE_COLOR:
            SetValue(DMT_SURFACE_RGBA, &p->surface.rgba[0], args, 0);
            SetValue(DMT_SURFACE_RGBA, &p->surface.rgba[1], args, 1);
            SetValue(DMT_SURFACE_RGBA, &p->surface.rgba[2], args, 2);
            break;
        case DMU_PLANE_COLOR_RED:
            SetValue(DMT_SURFACE_RGBA, &p->surface.rgba[0], args, 0);
            break;
        case DMU_PLANE_COLOR_GREEN:
            SetValue(DMT_SURFACE_RGBA, &p->surface.rgba[1], args, 0);
            break;
        case DMU_PLANE_COLOR_BLUE:
            SetValue(DMT_SURFACE_RGBA, &p->surface.rgba[2], args, 0);
            break;
        case DMU_PLANE_HEIGHT:
            SetValue(DMT_PLANE_HEIGHT, &p->height, args, 0);
            break;
        case DMU_PLANE_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->surface.texture, args, 0);
            p->surface.isflat = true; // Kludge
            break;
        case DMU_PLANE_OFFSET_X:
            SetValue(DMT_PLANE_OFFX, &p->surface.offx, args, 0);
            break;
        case DMU_PLANE_OFFSET_Y:
            SetValue(DMT_PLANE_OFFY, &p->surface.offy, args, 0);
            break;
        case DMU_PLANE_OFFSET_XY:
            SetValue(DMT_PLANE_OFFX, &p->surface.offx, args, 0);
            SetValue(DMT_PLANE_OFFY, &p->surface.offy, args, 1);
            break;
        case DMU_PLANE_TEXTURE_MOVE_X:
            SetValue(DMT_SURFACE_TEXMOVE, &p->surface.texmove[0], args, 0);
            break;
        case DMU_PLANE_TEXTURE_MOVE_Y:
            SetValue(DMT_SURFACE_TEXMOVE, &p->surface.texmove[1], args, 0);
            break;
        case DMU_PLANE_TEXTURE_MOVE_XY:
            SetValue(DMT_SURFACE_TEXMOVE, &p->surface.texmove[0], args, 0);
            SetValue(DMT_SURFACE_TEXMOVE, &p->surface.texmove[1], args, 1);
            break;
        case DMU_PLANE_TARGET:
            SetValue(DMT_PLANE_TARGET, &p->target, args, 0);
            break;
        case DMU_PLANE_SPEED:
            SetValue(DMT_PLANE_SPEED, &p->speed, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_PLANE.\n",
                      DMU_Str(args->prop));
        }

        // TODO: Notify relevant subsystems of any changes.
        R_UpdateSector(p->sector, false);

        // Continue iteration.
        return true;
    }

    switch(args->type)
    {
    case DMU_VERTEX:
        {
        // Vertices are not writable through DMU.
        Con_Error("SetProperty: DMU_VERTEX is not writable.\n");
        break;
        }

    case DMU_SEG:
        {
        seg_t* p = ptr;
        switch(args->prop)
        {
        case DMU_VERTEX1_X:
            SetValue(DMT_VERTEX_X, &p->v1->x, args, 0);
            break;
        case DMU_VERTEX1_Y:
            SetValue(DMT_VERTEX_Y, &p->v1->y, args, 0);
            break;
        case DMU_VERTEX1_XY:
            SetValue(DMT_VERTEX_X, &p->v1->x, args, 0);
            SetValue(DMT_VERTEX_Y, &p->v1->y, args, 1);
            break;
        case DMU_VERTEX2_X:
            SetValue(DMT_VERTEX_X, &p->v2->x, args, 0);
            break;
        case DMU_VERTEX2_Y:
            SetValue(DMT_VERTEX_Y, &p->v2->y, args, 0);
            break;
        case DMU_VERTEX2_XY:
            SetValue(DMT_VERTEX_X, &p->v2->x, args, 0);
            SetValue(DMT_VERTEX_Y, &p->v2->y, args, 1);
            break;
        case DMU_FLAGS:
            SetValue(DMT_SEG_FLAGS, &p->flags, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_SEG.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_LINE:
        {
        line_t* p = ptr;
        switch(args->prop)
        {
        case DMU_FRONT_SECTOR:
            SetValue(DMT_LINE_FRONTSECTOR, &p->frontsector, args, 0);
            break;
        case DMU_BACK_SECTOR:
            SetValue(DMT_LINE_BACKSECTOR, &p->backsector, args, 0);
            break;
        case DMU_SIDE0:
            SetValue(DMT_LINE_SIDENUM, &p->sidenum[0], args, 0);
            break;
        case DMU_SIDE1:
            SetValue(DMT_LINE_SIDENUM, &p->sidenum[1], args, 0);
            break;
        case DMU_VALID_COUNT:
            SetValue(DMT_LINE_VALIDCOUNT, &p->validcount, args, 0);
            break;
        case DMU_FLAGS:
            SetValue(DMT_LINE_FLAGS, &p->flags, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_LINE.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SIDE:
        {
        side_t* p = ptr;
        sideinfo_t *info = SIDE_INFO(p);

        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(DMT_SIDE_FLAGS, &p->flags, args, 0);
            break;
        case DMU_TOP_COLOR:
            SetValue(DMT_SURFACE_RGBA, &p->top.rgba[0], args, 0);
            SetValue(DMT_SURFACE_RGBA, &p->top.rgba[1], args, 1);
            SetValue(DMT_SURFACE_RGBA, &p->top.rgba[2], args, 2);
            break;
        case DMU_TOP_COLOR_RED:
            SetValue(DMT_SURFACE_RGBA, &p->top.rgba[0], args, 0);
            break;
        case DMU_TOP_COLOR_GREEN:
            SetValue(DMT_SURFACE_RGBA, &p->top.rgba[1], args, 0);
            break;
        case DMU_TOP_COLOR_BLUE:
            SetValue(DMT_SURFACE_RGBA, &p->top.rgba[2], args, 0);
            break;
        case DMU_TOP_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->top.texture, args, 0);
            p->top.isflat = false; // Kludge
           /* if(p->top.texture)
                p->flags &= ~SDF_MIDTEXUPPER;*/
            break;
        case DMU_TOP_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->top.offx, args, 0);
            break;
        case DMU_TOP_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->top.offy, args, 0);
            break;
        case DMU_TOP_TEXTURE_OFFSET_XY:
            SetValue(DMT_SURFACE_OFFX, &p->top.offx, args, 0);
            SetValue(DMT_SURFACE_OFFY, &p->top.offy, args, 1);
            break;
        case DMU_MIDDLE_COLOR:
            SetValue(DMT_SURFACE_RGBA, &p->middle.rgba[0], args, 0);
            SetValue(DMT_SURFACE_RGBA, &p->middle.rgba[1], args, 1);
            SetValue(DMT_SURFACE_RGBA, &p->middle.rgba[2], args, 2);
            SetValue(DMT_SURFACE_RGBA, &p->middle.rgba[3], args, 3);
            break;
        case DMU_MIDDLE_COLOR_RED:
            SetValue(DMT_SURFACE_RGBA, &p->middle.rgba[0], args, 0);
            break;
        case DMU_MIDDLE_COLOR_GREEN:
            SetValue(DMT_SURFACE_RGBA, &p->middle.rgba[1], args, 0);
            break;
        case DMU_MIDDLE_COLOR_BLUE:
            SetValue(DMT_SURFACE_RGBA, &p->middle.rgba[2], args, 0);
            break;
        case DMU_MIDDLE_COLOR_ALPHA:
            SetValue(DMT_SURFACE_RGBA, &p->middle.rgba[3], args, 0);
            break;
        case DMU_MIDDLE_BLENDMODE:
            SetValue(DMT_SIDE_BLENDMODE, &p->blendmode, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->middle.texture, args, 0);
            p->middle.isflat = false; // Kludge
            break;
        case DMU_MIDDLE_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->middle.offx, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->middle.offy, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE_OFFSET_XY:
            SetValue(DMT_SURFACE_OFFX, &p->middle.offx, args, 0);
            SetValue(DMT_SURFACE_OFFY, &p->middle.offy, args, 1);
            break;
        case DMU_BOTTOM_COLOR:
            SetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[0], args, 0);
            SetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[1], args, 1);
            SetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[2], args, 2);
            break;
        case DMU_BOTTOM_COLOR_RED:
            SetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[0], args, 0);
            break;
        case DMU_BOTTOM_COLOR_GREEN:
            SetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[1], args, 0);
            break;
        case DMU_BOTTOM_COLOR_BLUE:
            SetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[2], args, 0);
            break;
        case DMU_BOTTOM_TEXTURE:
            SetValue(DMT_SURFACE_TEXTURE, &p->bottom.texture, args, 0);
            p->bottom.isflat = false; // Kludge
            break;
        case DMU_BOTTOM_TEXTURE_OFFSET_X:
            SetValue(DMT_SURFACE_OFFX, &p->bottom.offx, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE_OFFSET_Y:
            SetValue(DMT_SURFACE_OFFY, &p->bottom.offy, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE_OFFSET_XY:
            SetValue(DMT_SURFACE_OFFX, &p->bottom.offx, args, 0);
            SetValue(DMT_SURFACE_OFFY, &p->bottom.offy, args, 1);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_SIDE.\n",
                      DMU_Str(args->prop));
        }

        R_UpdateSurface(&p->top, false);
        R_UpdateSurface(&p->middle, false);
        R_UpdateSurface(&p->bottom, false);
        break;
        }

    case DMU_SUBSECTOR:
        {
        subsector_t* p = ptr;
        switch(args->prop)
        {
        case DMU_FLAGS:
            SetValue(DMT_SUBSECTOR_FLAGS, &p->flags, args, 0);
            break;
        case DMU_POLYOBJ:
            SetValue(DMT_SUBSECTOR_POLY, &p->poly, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_SUBSECTOR.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SECTOR:
        {
        sector_t* p = ptr;
        switch(args->prop)
        {
        case DMU_COLOR:
            SetValue(DMT_SECTOR_RGB, &p->rgb[0], args, 0);
            SetValue(DMT_SECTOR_RGB, &p->rgb[1], args, 1);
            SetValue(DMT_SECTOR_RGB, &p->rgb[2], args, 2);
            break;
        case DMU_COLOR_RED:
            SetValue(DMT_SECTOR_RGB, &p->rgb[0], args, 0);
            break;
        case DMU_COLOR_GREEN:
            SetValue(DMT_SECTOR_RGB, &p->rgb[1], args, 0);
            break;
        case DMU_COLOR_BLUE:
            SetValue(DMT_SECTOR_RGB, &p->rgb[2], args, 0);
            break;
        case DMU_LIGHT_LEVEL:
            SetValue(DMT_SECTOR_LIGHTLEVEL, &p->lightlevel, args, 0);
            break;
        case DMU_VALID_COUNT:
            SetValue(DMT_SECTOR_VALIDCOUNT, &p->validcount, args, 0);
            break;
        case DMU_SOUND_REVERB:
        {
            int i;
            for(i = 0; i < NUM_REVERB_DATA; ++i)
            {
                SetValue(DMT_SECTOR_REVERB, &p->reverb[i], args, i);
            }
            break;
        }
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_SECTOR.\n",
                      DMU_Str(args->prop));
        }

        // TODO: Notify relevant subsystems of any changes.
        R_UpdateSector(p, false);

        break;
        }

    case DMU_POLYOBJ:
        {
        polyobj_t* p = ptr;
        if(args->modifiers & DMU_SEG_OF_POLYOBJ)
        {
            if(args->prop >= 0 && args->prop < p->numsegs)
            {
                SetValue(DDVT_PTR, &p->segs[args->prop], args, 0);
                break;
            }
            else
            {
                Con_Error("SetProperty: Polyobj seg out of range (%i out of %i).\n",
                          args->prop, p->numsegs);
            }
        }

        switch(args->prop)
        {
        case DMU_START_SPOT_X:
            SetValue(DDVT_FIXED, &p->startSpot.x, args, 0);
            break;
        case DMU_START_SPOT_Y:
            SetValue(DDVT_FIXED, &p->startSpot.y, args, 0);
            break;
        case DMU_START_SPOT_XY:
            SetValue(DDVT_FIXED, &p->startSpot.x, args, 0);
            SetValue(DDVT_FIXED, &p->startSpot.y, args, 1);
            break;
        case DMU_DESTINATION_X:
            SetValue(DDVT_FIXED, &p->dest.x, args, 0);
            break;
        case DMU_DESTINATION_Y:
            SetValue(DDVT_FIXED, &p->dest.y, args, 0);
            break;
        case DMU_DESTINATION_XY:
            SetValue(DDVT_FIXED, &p->dest.x, args, 0);
            SetValue(DDVT_FIXED, &p->dest.y, args, 1);
            break;
        case DMU_ORIGINAL_POINTS:
            SetValue(DDVT_PTR, &p->originalPts, args, 0);
            break;
        case DMU_PREVIOUS_POINTS:
            SetValue(DDVT_PTR, &p->prevPts, args, 0);
            break;
        case DMU_ANGLE:
            SetValue(DDVT_ANGLE, &p->angle, args, 0);
            break;
        case DMU_DESTINATION_ANGLE:
            SetValue(DDVT_ANGLE, &p->destAngle, args, 0);
            break;
        case DMU_SPEED:
            SetValue(DDVT_FIXED, &p->speed, args, 0);
            break;
        case DMU_ANGLE_SPEED:
            SetValue(DDVT_ANGLE, &p->angleSpeed, args, 0);
            break;
        case DMU_TAG:
            SetValue(DDVT_INT, &p->tag, args, 0);
            break;
        case DMU_CRUSH:
            SetValue(DDVT_BOOL, &p->crush, args, 0);
            break;
        case DMU_SEQUENCE_TYPE:
            SetValue(DDVT_INT, &p->seqType, args, 0);
            break;
        case DMU_SEG_COUNT:
            SetValue(DDVT_INT, &p->numsegs, args, 0);
            break;
        case DMU_SEG_LIST:
            SetValue(DDVT_PTR, &p->segs, args, 0);
            break;
        case DMU_SPECIAL_DATA:
            SetValue(DDVT_PTR, &p->specialdata, args, 0);
            break;
        default:
            Con_Error("SetProperty: Property %s is not writable in DMU_POLYOBJ.\n",
                      DMU_Str(args->prop));
            break;
        }
        break;
        }

    case DMU_NODE:
        Con_Error("SetProperty: Property %s is not writable in DMU_NODE.\n",
                  DMU_Str(args->prop));
        break;

    default:
        Con_Error("SetProperty: Type %s not writable.\n", DMU_Str(args->type));
    }
    // Continue iteration.
    return true;
}

/**
 * Gets a value. Does some basic type checking so that incompatible types are
 * not assigned. Simple conversions are also done, e.g., float to fixed.
 */
static void GetValue(valuetype_t valueType, const void* src, setargs_t* args, int index)
{
    if(valueType == DDVT_FIXED)
    {
        const fixed_t* s = src;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            args->byteValues[index] = (*s >> FRACBITS);
            break;
        case DDVT_INT:
            args->intValues[index] = (*s >> FRACBITS);
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = *s;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = FIX2FLT(*s);
            break;
        default:
            Con_Error("GetValue: DDVT_FIXED incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        const float* s = src;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            args->byteValues[index] = *s;
            break;
        case DDVT_INT:
            args->intValues[index] = (int) *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = FLT2FIX(*s);
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_FLOAT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        const boolean* s = src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_BOOL incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        const byte* s = src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *s;
            break;
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_BYTE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_INT)
    {
        const int* s = src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *s;
            break;
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = (*s << FRACBITS);
            break;
        default:
            Con_Error("GetValue: DDVT_INT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        const short* s = src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        case DDVT_BYTE:
            args->byteValues[index] = *s;
            break;
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        case DDVT_FLOAT:
            // TODO: Don't allow conversion from DDVT_FLATINDEX.
            args->floatValues[index] = *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = (*s << FRACBITS);
            break;
        default:
            Con_Error("GetValue: DDVT_SHORT incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        const angle_t* s = src;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            args->angleValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_ANGLE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        const blendmode_t* s = src;

        switch(args->valueType)
        {
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        default:
            Con_Error("GetValue: DDVT_BLENDMODE incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else if(valueType == DDVT_PTR)
    {
        const void* const* s = src;

        switch(args->valueType)
        {
        case DDVT_INT:
            // Attempt automatic conversion using P_ToIndex(). Naturally only
            // works with map data objects. Failure leads into a fatal error.
            args->intValues[index] = P_ToIndex(*s);
            break;
        case DDVT_PTR:
            args->ptrValues[index] = (void*) *s;
            break;
        default:
            Con_Error("GetValue: DDVT_PTR incompatible with value type %s.\n",
                      value_Str(args->valueType));
        }
    }
    else
    {
        Con_Error("GetValue: unknown value type %d.\n", valueType);
    }
}

static int GetProperty(void* ptr, void* context)
{
    setargs_t* args = (setargs_t*) context;

    // Check modified cases first.
    if(args->type == DMU_SECTOR &&
       (args->modifiers & DMU_LINE_OF_SECTOR))
    {
        sector_t* p = ptr;
        if(args->prop < 0 || args->prop >= p->linecount)
        {
            Con_Error("GetProperty: DMU_LINE_OF_SECTOR %i does not exist.\n",
                      args->prop);
        }
        GetValue(DDVT_PTR, &p->Lines[args->prop], args, 0);
        return false; // stop iteration
    }

    if(args->type == DMU_SUBSECTOR &&
       (args->modifiers & DMU_SEG_OF_SUBSECTOR))
    {
        subsector_t* p = ptr;
        seg_t* segptr;
        if(args->prop < 0 || args->prop >= p->linecount)
        {
            Con_Error("GetProperty: DMU_SEG_OF_SECTOR %i does not exist.\n",
                      args->prop);
        }
        segptr = SEG_PTR(p->firstline + args->prop);
        GetValue(DDVT_PTR, &segptr, args, 0);
        return false; // stop iteration
    }

    if(args->type == DMU_PLANE ||
       (args->type == DMU_SECTOR &&
          ((args->aliases & DMU_FLOOR_OF_SECTOR) ||
           (args->aliases & DMU_CEILING_OF_SECTOR))) ||
       (args->type == DMU_SUBSECTOR &&
          ((args->aliases & DMU_FLOOR_OF_SECTOR) ||
           (args->aliases & DMU_CEILING_OF_SECTOR))))
    {
        plane_t* p = NULL;
        if(args->type == DMU_PLANE)
            p = ptr;
        else if(args->type == DMU_SECTOR)
        {
            sector_t* sec = (sector_t*)ptr;
            if(args->aliases & DMU_FLOOR_OF_SECTOR)
            {
                if(!sec->planes[PLN_FLOOR])
                    Con_Error("SetProperty: Sector %i does not have a "
                              "floor plane!", P_ToIndex(sec));

                p = sec->planes[PLN_FLOOR];
            }
            else
            {
                if(!sec->planes[PLN_FLOOR])
                    Con_Error("SetProperty: Sector %i does not have a "
                              "ceiling plane!", P_ToIndex(sec));

                p = sec->planes[PLN_CEILING];
            }
        }
        else if(args->type == DMU_SUBSECTOR)
        {
            sector_t* sec = ((subsector_t*)ptr)->sector;
            if(args->aliases & DMU_FLOOR_OF_SECTOR)
                p = sec->planes[PLN_FLOOR];
            else
                p = sec->planes[PLN_CEILING];
        }
        else
            Con_Error("GetProperty: Invalid args.\n");

        switch(args->prop)
        {
        case DMU_SECTOR:
            GetValue(DMT_PLANE_SECTOR, &p->sector, args, 0);
            break;
        case DMU_PLANE_COLOR:
            GetValue(DMT_SURFACE_RGBA, &p->surface.rgba[0], args, 0);
            GetValue(DMT_SURFACE_RGBA, &p->surface.rgba[1], args, 1);
            GetValue(DMT_SURFACE_RGBA, &p->surface.rgba[2], args, 2);
            break;
        case DMU_PLANE_COLOR_RED:
            GetValue(DMT_SURFACE_RGBA, &p->surface.rgba[0], args, 0);
            break;
        case DMU_PLANE_COLOR_GREEN:
            GetValue(DMT_SURFACE_RGBA, &p->surface.rgba[1], args, 0);
            break;
        case DMU_PLANE_COLOR_BLUE:
            GetValue(DMT_SURFACE_RGBA, &p->surface.rgba[2], args, 0);
            break;
        case DMU_PLANE_HEIGHT:
            GetValue(DMT_PLANE_HEIGHT, &p->height, args, 0);
            break;
        case DMU_PLANE_TEXTURE:
            GetValue(DMT_SURFACE_TEXTURE, &p->surface.texture, args, 0);
            break;
        case DMU_PLANE_SOUND_ORIGIN:
        {
            degenmobj_t *dmo = &p->soundorg;
            GetValue(DMT_PLANE_SOUNDORG, &dmo, args, 0);
            break;
        }
        case DMU_PLANE_OFFSET_X:
            GetValue(DMT_PLANE_OFFX, &p->surface.offx, args, 0);
            break;
        case DMU_PLANE_OFFSET_Y:
            GetValue(DMT_PLANE_OFFY, &p->surface.offy, args, 0);
            break;
        case DMU_PLANE_OFFSET_XY:
            GetValue(DMT_PLANE_OFFX, &p->surface.offx, args, 0);
            GetValue(DMT_PLANE_OFFY, &p->surface.offy, args, 1);
            break;
        case DMU_PLANE_TEXTURE_MOVE_X:
            GetValue(DMT_SURFACE_TEXMOVE, &p->surface.texmove[0], args, 0);
            break;
        case DMU_PLANE_TEXTURE_MOVE_Y:
            GetValue(DMT_SURFACE_TEXMOVE, &p->surface.texmove[1], args, 0);
            break;
        case DMU_PLANE_TEXTURE_MOVE_XY:
            GetValue(DMT_SURFACE_TEXMOVE, &p->surface.texmove[0], args, 0);
            GetValue(DMT_SURFACE_TEXMOVE, &p->surface.texmove[1], args, 1);
            break;
        case DMU_PLANE_TARGET:
            GetValue(DMT_PLANE_TARGET, &p->target, args, 0);
            break;
        case DMU_PLANE_SPEED:
            GetValue(DMT_PLANE_SPEED, &p->speed, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_PLANE has no property %s.\n",
                      DMU_Str(args->prop));
        }
        return false; // stop iteration
    }

    if(args->type == DMU_SECTOR ||
       (args->type == DMU_SUBSECTOR &&
        (args->modifiers & DMU_SECTOR_OF_SUBSECTOR)))
    {
        sector_t* p = NULL;
        if(args->type == DMU_SECTOR)
            p = ptr;
        else if(args->type == DMU_SUBSECTOR)
            p = ((subsector_t*)ptr)->sector;
        else
            Con_Error("GetProperty: Invalid args.\n");

        switch(args->prop)
        {
        case DMU_LIGHT_LEVEL:
            GetValue(DMT_SECTOR_LIGHTLEVEL, &p->lightlevel, args, 0);
            break;
        case DMU_COLOR:
            GetValue(DMT_SECTOR_RGB, &p->rgb[0], args, 0);
            GetValue(DMT_SECTOR_RGB, &p->rgb[1], args, 1);
            GetValue(DMT_SECTOR_RGB, &p->rgb[2], args, 2);
            break;
        case DMU_COLOR_RED:
            GetValue(DMT_SECTOR_RGB, &p->rgb[0], args, 0);
            break;
        case DMU_COLOR_GREEN:
            GetValue(DMT_SECTOR_RGB, &p->rgb[1], args, 0);
            break;
        case DMU_COLOR_BLUE:
            GetValue(DMT_SECTOR_RGB, &p->rgb[2], args, 0);
            break;
        case DMU_SOUND_ORIGIN:
        {
            degenmobj_t *dmo = &p->soundorg;
            GetValue(DMT_SECTOR_SOUNDORG, &dmo, args, 0);
            break;
        }
        case DMU_SOUND_REVERB:
        {
            int i;
            for(i = 0; i < NUM_REVERB_DATA; ++i)
            {
                GetValue(DMT_SECTOR_REVERB, &p->reverb[i], args, i);
            }
            break;
        }
        case DMU_LINE_COUNT:
            GetValue(DMT_SECTOR_LINECOUNT, &p->linecount, args, 0);
            break;
        case DMU_THINGS:
            GetValue(DMT_SECTOR_THINGLIST, &p->thinglist, args, 0);
            break;
        case DMU_VALID_COUNT:
            GetValue(DMT_SECTOR_VALIDCOUNT, &p->validcount, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SECTOR has no property %s.\n",
                      DMU_Str(args->prop));
        }
        return false; // stop iteration
    }

    if(args->type == DMU_SIDE ||
       (args->type == DMU_LINE &&
        ((args->modifiers & DMU_SIDE0_OF_LINE) ||
         (args->modifiers & DMU_SIDE1_OF_LINE))))
    {
        side_t* p = NULL;
        if(args->type == DMU_SIDE)
            p = ptr;
        else if(args->type == DMU_LINE)
        {
            line_t* line = (line_t*)ptr;
            if(args->modifiers & DMU_SIDE0_OF_LINE)
                p = SIDE_PTR(line->sidenum[0]);
            else
            {
                if(line->sidenum[1] == NO_INDEX)
                    Con_Error("GetProperty: Line %d does not have a back side.\n",
                              GET_LINE_IDX(line));

                p = SIDE_PTR(line->sidenum[1]);
            }
        }
        else
            Con_Error("GetProperty: Invalid args.\n");

        switch(args->prop)
        {
        case DMU_SECTOR:
            GetValue(DMT_SIDE_SECTOR, &p->sector, args, 0);
            break;
        case DMU_TOP_TEXTURE:
            {
            short texture = p->top.texture;

            if(p->top.flags & SUF_TEXFIX)
                texture = 0;

           /*if(p->flags & SDF_MIDTEXUPPER)
                texture = 0;*/

            GetValue(DMT_SURFACE_TEXTURE, &texture, args, 0);
            break;
            }
        case DMU_TOP_TEXTURE_OFFSET_X:
            GetValue(DMT_SURFACE_OFFX, &p->top.offx, args, 0);
            break;
        case DMU_TOP_TEXTURE_OFFSET_Y:
            GetValue(DMT_SURFACE_OFFY, &p->top.offy, args, 0);
            break;
        case DMU_TOP_TEXTURE_OFFSET_XY:
            GetValue(DMT_SURFACE_OFFX, &p->top.offx, args, 0);
            GetValue(DMT_SURFACE_OFFY, &p->top.offy, args, 1);
            break;
        case DMU_TOP_COLOR:
            GetValue(DMT_SURFACE_RGBA, &p->top.rgba[0], args, 0);
            GetValue(DMT_SURFACE_RGBA, &p->top.rgba[1], args, 1);
            GetValue(DMT_SURFACE_RGBA, &p->top.rgba[2], args, 2);
            break;
        case DMU_TOP_COLOR_RED:
            GetValue(DMT_SURFACE_RGBA, &p->top.rgba[0], args, 0);
            break;
        case DMU_TOP_COLOR_GREEN:
            GetValue(DMT_SURFACE_RGBA, &p->top.rgba[1], args, 0);
            break;
        case DMU_TOP_COLOR_BLUE:
            GetValue(DMT_SURFACE_RGBA, &p->top.rgba[2], args, 0);
            break;
        case DMU_MIDDLE_TEXTURE:
            {
            short texture = p->middle.texture;

            if(p->middle.flags & SUF_TEXFIX)
                texture = 0;

            /*if(p->flags & SDF_MIDTEXUPPER)
                texture = p->top.texture;*/

            GetValue(DMT_SURFACE_TEXTURE, &texture, args, 0);
            break;
            }
        case DMU_MIDDLE_TEXTURE_OFFSET_X:
            GetValue(DMT_SURFACE_OFFX, &p->middle.offx, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE_OFFSET_Y:
            GetValue(DMT_SURFACE_OFFY, &p->middle.offy, args, 0);
            break;
        case DMU_MIDDLE_TEXTURE_OFFSET_XY:
            GetValue(DMT_SURFACE_OFFX, &p->middle.offx, args, 0);
            GetValue(DMT_SURFACE_OFFY, &p->middle.offy, args, 1);
            break;
        case DMU_MIDDLE_COLOR:
            GetValue(DMT_SURFACE_RGBA, &p->middle.rgba[0], args, 0);
            GetValue(DMT_SURFACE_RGBA, &p->middle.rgba[1], args, 1);
            GetValue(DMT_SURFACE_RGBA, &p->middle.rgba[2], args, 2);
            GetValue(DMT_SURFACE_RGBA, &p->middle.rgba[3], args, 3);
            break;
        case DMU_MIDDLE_COLOR_RED:
            GetValue(DMT_SURFACE_RGBA, &p->middle.rgba[0], args, 0);
            break;
        case DMU_MIDDLE_COLOR_GREEN:
            GetValue(DMT_SURFACE_RGBA, &p->middle.rgba[1], args, 0);
            break;
        case DMU_MIDDLE_COLOR_BLUE:
            GetValue(DMT_SURFACE_RGBA, &p->middle.rgba[2], args, 0);
            break;
        case DMU_MIDDLE_COLOR_ALPHA:
            GetValue(DMT_SURFACE_RGBA, &p->middle.rgba[3], args, 0);
            break;
        case DMU_MIDDLE_BLENDMODE:
            GetValue(DMT_SIDE_BLENDMODE, &p->blendmode, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE:
            {
            short texture = p->bottom.texture;

            if(p->bottom.flags & SUF_TEXFIX)
                texture = 0;

            GetValue(DMT_SURFACE_TEXTURE, &texture, args, 0);
            break;
            }
        case DMU_BOTTOM_TEXTURE_OFFSET_X:
            GetValue(DMT_SURFACE_OFFX, &p->bottom.offx, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE_OFFSET_Y:
            GetValue(DMT_SURFACE_OFFY, &p->bottom.offy, args, 0);
            break;
        case DMU_BOTTOM_TEXTURE_OFFSET_XY:
            GetValue(DMT_SURFACE_OFFX, &p->bottom.offx, args, 0);
            GetValue(DMT_SURFACE_OFFY, &p->bottom.offy, args, 1);
            break;
        case DMU_BOTTOM_COLOR:
            GetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[0], args, 0);
            GetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[1], args, 1);
            GetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[2], args, 2);
            break;
        case DMU_BOTTOM_COLOR_RED:
            GetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[0], args, 0);
            break;
        case DMU_BOTTOM_COLOR_GREEN:
            GetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[1], args, 0);
            break;
        case DMU_BOTTOM_COLOR_BLUE:
            GetValue(DMT_SURFACE_RGBA, &p->bottom.rgba[2], args, 0);
            break;
        case DMU_FLAGS:
            GetValue(DMT_SIDE_FLAGS, &p->flags, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SIDE has no property %s.\n", DMU_Str(args->prop));
        }
        return false; // stop iteration
    }

    switch(args->type)
    {
    case DMU_VERTEX:
        {
        vertex_t* p = ptr;
        switch(args->prop)
        {
        case DMU_X:
            GetValue(DMT_VERTEX_X, &p->x, args, 0);
            break;
        case DMU_Y:
            GetValue(DMT_VERTEX_Y, &p->y, args, 0);
            break;
        case DMU_XY:
            GetValue(DMT_VERTEX_X, &p->x, args, 0);
            GetValue(DMT_VERTEX_Y, &p->y, args, 1);
            break;
        default:
            Con_Error("GetProperty: DMU_VERTEX has no property %s.\n",
                      DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SEG:
        {
        seg_t* p = ptr;
        switch(args->prop)
        {
        case DMU_VERTEX1:
            GetValue(DMT_SEG_V1, &p->v1, args, 0);
            break;
        case DMU_VERTEX1_X:
            GetValue(DMT_VERTEX_X, &p->v1->x, args, 0);
            break;
        case DMU_VERTEX1_Y:
            GetValue(DMT_VERTEX_Y, &p->v1->y, args, 0);
            break;
        case DMU_VERTEX1_XY:
            GetValue(DMT_VERTEX_X, &p->v1->x, args, 0);
            GetValue(DMT_VERTEX_Y, &p->v1->y, args, 1);
            break;
        case DMU_VERTEX2:
            GetValue(DMT_SEG_V2, &p->v2, args, 0);
            break;
        case DMU_VERTEX2_X:
            GetValue(DMT_VERTEX_X, &p->v2->x, args, 0);
            break;
        case DMU_VERTEX2_Y:
            GetValue(DMT_VERTEX_Y, &p->v2->y, args, 0);
            break;
        case DMU_VERTEX2_XY:
            GetValue(DMT_VERTEX_X, &p->v2->x, args, 0);
            GetValue(DMT_VERTEX_Y, &p->v2->y, args, 1);
            break;
        case DMU_LENGTH:
            GetValue(DMT_SEG_LENGTH, &p->length, args, 0);
            break;
        case DMU_OFFSET:
            GetValue(DMT_SEG_OFFSET, &p->offset, args, 0);
            break;
        case DMU_SIDE:
            GetValue(DMT_SEG_SIDEDEF, &p->sidedef, args, 0);
            break;
        case DMU_LINE:
            GetValue(DMT_SEG_LINEDEF, &p->linedef, args, 0);
            break;
        case DMU_FRONT_SECTOR:
            GetValue(DMT_SEG_FRONTSECTOR, &p->frontsector, args, 0);
            break;
        case DMU_BACK_SECTOR:
            GetValue(DMT_SEG_BACKSECTOR, &p->backsector, args, 0);
            break;
        case DMU_FLAGS:
            GetValue(DMT_SEG_FLAGS, &p->flags, args, 0);
            break;
        case DMU_ANGLE:
            GetValue(DMT_SEG_ANGLE, &p->angle, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SEG has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_LINE:
        {
        line_t* p = ptr;
        switch(args->prop)
        {
        case DMU_VERTEX1:
            GetValue(DMT_LINE_V1, &p->v1, args, 0);
            break;
        case DMU_VERTEX1_X:
            GetValue(DMT_VERTEX_X, &p->v1->x, args, 0);
            break;
        case DMU_VERTEX1_Y:
            GetValue(DMT_VERTEX_Y, &p->v1->y, args, 0);
            break;
        case DMU_VERTEX1_XY:
            GetValue(DMT_VERTEX_X, &p->v1->x, args, 0);
            GetValue(DMT_VERTEX_Y, &p->v1->y, args, 1);
            break;
        case DMU_VERTEX2:
            GetValue(DMT_LINE_V2, &p->v2, args, 0);
            break;
        case DMU_VERTEX2_X:
            GetValue(DMT_VERTEX_X, &p->v2->x, args, 0);
            break;
        case DMU_VERTEX2_Y:
            GetValue(DMT_VERTEX_Y, &p->v2->y, args, 0);
            break;
        case DMU_VERTEX2_XY:
            GetValue(DMT_VERTEX_X, &p->v2->x, args, 0);
            GetValue(DMT_VERTEX_Y, &p->v2->y, args, 1);
            break;
        case DMU_DX:
            GetValue(DMT_LINE_DX, &p->dx, args, 0);
            break;
        case DMU_DY:
            GetValue(DMT_LINE_DY, &p->dy, args, 0);
            break;
        case DMU_LENGTH:
        {
            lineinfo_t* info = LINE_INFO(p);
            GetValue(DDVT_FLOAT, &info->length, args, 0);
            break;
        }
        /*
        case DMU_ANGLE:
        {
            // info->angle is a binangle_t, which is currently a 16-bit angle (ushort)
            // (see dd_types.h), whle DDVT_ANGLE is 32-bit unsigned long.
            // TODO: Use DDVT_ANGLE, but make the necessary conversion...

            lineinfo_t* info = LINE_INFO(p);
            GetValue(DDVT_ANGLE, &info->angle, args, 0);
            break;
        }*/
        case DMU_SLOPE_TYPE:
            GetValue(DMT_LINE_SLOPETYPE, &p->slopetype, args, 0);
            break;
        case DMU_FRONT_SECTOR:
            GetValue(DMT_LINE_FRONTSECTOR, &p->frontsector, args, 0);
            break;
        case DMU_BACK_SECTOR:
            GetValue(DMT_LINE_BACKSECTOR, &p->backsector, args, 0);
            break;
        case DMU_FLAGS:
            GetValue(DMT_LINE_FLAGS, &p->flags, args, 0);
            break;
        case DMU_SIDE0:
        {
            side_t* sidePtr = (p->sidenum[0] == NO_INDEX? NULL : SIDE_PTR(p->sidenum[0]));
            GetValue(DDVT_PTR, &sidePtr, args, 0);
            break;
        }
        case DMU_SIDE1:
        {
            side_t* sidePtr = (p->sidenum[1] == NO_INDEX? NULL : SIDE_PTR(p->sidenum[1]));
            GetValue(DDVT_PTR, &sidePtr, args, 0);
            break;
        }
        case DMU_BOUNDING_BOX:
            if(args->valueType == DDVT_PTR)
            {
                fixed_t* bbox = p->bbox;
                GetValue(DDVT_PTR, &bbox, args, 0);
            }
            else
            {
                GetValue(DMT_LINE_BBOX, &p->bbox[0], args, 0);
                GetValue(DMT_LINE_BBOX, &p->bbox[1], args, 1);
                GetValue(DMT_LINE_BBOX, &p->bbox[2], args, 2);
                GetValue(DMT_LINE_BBOX, &p->bbox[3], args, 3);
            }
            break;
        case DMU_VALID_COUNT:
            GetValue(DMT_LINE_VALIDCOUNT, &p->validcount, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_LINE has no property %s.\n", DMU_Str(args->prop));
        }
        break;
        }

    case DMU_SUBSECTOR:
    {
        subsector_t* p = ptr;
        switch(args->prop)
        {
        case DMU_SECTOR:
            GetValue(DMT_SUBSECTOR_SECTOR, &p->sector, args, 0);
            break;
        case DMU_LIGHT_LEVEL:
            GetValue(DMT_SECTOR_LIGHTLEVEL, &p->sector->lightlevel, args, 0);
            break;
        case DMU_THINGS:
            GetValue(DMT_SECTOR_THINGLIST, &p->sector->thinglist, args, 0);
            break;
        case DMU_POLYOBJ:
            GetValue(DMT_SUBSECTOR_POLY, &p->poly, args, 0);
            break;
        case DMU_LINE_COUNT:
            GetValue(DMT_SUBSECTOR_LINECOUNT, &p->linecount, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_SUBSECTOR has no property %s.\n",
                      DMU_Str(args->prop));
        }
        break;
    }

    case DMU_POLYOBJ:
    {
        polyobj_t* p = ptr;
        if(args->modifiers & DMU_SEG_OF_POLYOBJ)
        {
            if(args->prop >= 0 && args->prop < p->numsegs)
            {
                GetValue(DDVT_PTR, &p->segs[args->prop], args, 0);
                break;
            }
            else
            {
                Con_Error("GetProperty: Polyobj seg out of range (%i out of %i).\n",
                          args->prop, p->numsegs);
            }
        }

        switch(args->prop)
        {
        case DMU_START_SPOT:
        {
            void* spot = &p->startSpot;
            GetValue(DDVT_PTR, &spot, args, 0);
            break;
        }
        case DMU_START_SPOT_X:
            GetValue(DDVT_FIXED, &p->startSpot.x, args, 0);
            break;
        case DMU_START_SPOT_Y:
            GetValue(DDVT_FIXED, &p->startSpot.y, args, 0);
            break;
        case DMU_START_SPOT_XY:
            GetValue(DDVT_FIXED, &p->startSpot.x, args, 0);
            GetValue(DDVT_FIXED, &p->startSpot.y, args, 1);
            break;
        case DMU_ORIGINAL_POINTS:
            GetValue(DDVT_PTR, &p->originalPts, args, 0);
            break;
        case DMU_PREVIOUS_POINTS:
            GetValue(DDVT_PTR, &p->prevPts, args, 0);
            break;
        case DMU_ANGLE:
            GetValue(DDVT_ANGLE, &p->angle, args, 0);
            break;
        case DMU_DESTINATION_ANGLE:
            GetValue(DDVT_ANGLE, &p->destAngle, args, 0);
            break;
        case DMU_ANGLE_SPEED:
            GetValue(DDVT_ANGLE, &p->angleSpeed, args, 0);
            break;
        case DMU_TAG:
            GetValue(DDVT_INT, &p->tag, args, 0);
            break;
        case DMU_SEG_LIST:
            GetValue(DDVT_PTR, &p->segs, args, 0);
            break;
        case DMU_SEG_COUNT:
            GetValue(DDVT_INT, &p->numsegs, args, 0);
            break;
        case DMU_CRUSH:
            GetValue(DDVT_BOOL, &p->crush, args, 0);
            break;
        case DMU_SEQUENCE_TYPE:
            GetValue(DDVT_INT, &p->seqType, args, 0);
            break;
        case DMU_SPECIAL_DATA:
            GetValue(DDVT_PTR, &p->specialdata, args, 0);
            break;
        default:
            Con_Error("GetProperty: DMU_POLYOBJ has no property %s.\n",
                      DMU_Str(args->prop));
        }
        break;
    }

    default:
        Con_Error("GetProperty: Type %s not readable.\n", DMU_Str(args->type));
    }

    // Currently no aggregate values are collected.
    return false;
}


/**
 * Swaps two values. Does NOT do any type checking. Both values are
 * assumed to be of the correct (and same) type.
 */
static void SwapValue(valuetype_t valueType, void* src, void* dst)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t tmp = *(fixed_t*) dst;

        *(fixed_t*) dst = *(fixed_t*) src;
        *(fixed_t*) src = tmp;
    }
    else if(valueType == DDVT_FLOAT)
    {
        float tmp = *(float*) dst;

        *(float*) dst = *(float*) src;
        *(float*) src = tmp;
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean tmp = *(boolean*) dst;

        *(boolean*) dst = *(boolean*) src;
        *(boolean*) src = tmp;
    }
    else if(valueType == DDVT_BYTE)
    {
        byte tmp = *(byte*) dst;

        *(byte*) dst = *(byte*) src;
        *(byte*) src = tmp;
    }
    else if(valueType == DDVT_INT)
    {
        int tmp = *(int*) dst;

        *(int*) dst = *(int*) src;
        *(int*) src = tmp;
    }
    else if(valueType == DDVT_SHORT || valueType == DDVT_FLAT_INDEX)
    {
        short tmp = *(short*) dst;

        *(short*) dst = *(short*) src;
        *(short*) src = tmp;
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t tmp = *(angle_t*) dst;

        *(angle_t*) dst = *(angle_t*) src;
        *(angle_t*) src = tmp;
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t tmp = *(blendmode_t*) dst;

        *(blendmode_t*) dst = *(blendmode_t*) src;
        *(blendmode_t*) src = tmp;
    }
    else if(valueType == DDVT_PTR)
    {
        void* tmp = &dst;

        dst = &src;
        src = &tmp;
    }
    else
    {
        Con_Error("SwapValue: unknown value type %s.\n", DMU_Str(valueType));
    }
}

void P_SetBool(int type, int index, int prop, boolean param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetByte(int type, int index, int prop, byte param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetInt(int type, int index, int prop, int param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFixed(int type, int index, int prop, fixed_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetAngle(int type, int index, int prop, angle_t param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFloat(int type, int index, int prop, float param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetPtr(int type, int index, int prop, void* param)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetBoolv(int type, int index, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetBytev(int type, int index, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetIntv(int type, int index, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFixedv(int type, int index, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetAnglev(int type, int index, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetFloatv(int type, int index, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, SetProperty);
}

void P_SetPtrv(int type, int index, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, &args, SetProperty);
}

/* pointer-based write functions */

void P_SetBoolp(void* ptr, int prop, boolean param)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetBytep(void* ptr, int prop, byte param)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetIntp(void* ptr, int prop, int param)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetFixedp(void* ptr, int prop, fixed_t param)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetAnglep(void* ptr, int prop, angle_t param)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetFloatp(void* ptr, int prop, float param)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetPtrp(void* ptr, int prop, void* param)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetBoolpv(void* ptr, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetBytepv(void* ptr, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetIntpv(void* ptr, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetFixedpv(void* ptr, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetAnglepv(void* ptr, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetFloatpv(void* ptr, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

void P_SetPtrpv(void* ptr, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callbackp(args.type, ptr, &args, SetProperty);
}

/* index-based read functions */

boolean P_GetBool(int type, int index, int prop)
{
    setargs_t args;
    boolean returnValue = false;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

byte P_GetByte(int type, int index, int prop)
{
    setargs_t args;
    byte returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

int P_GetInt(int type, int index, int prop)
{
    setargs_t args;
    int returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

fixed_t P_GetFixed(int type, int index, int prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

angle_t P_GetAngle(int type, int index, int prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

float P_GetFloat(int type, int index, int prop)
{
    setargs_t args;
    float returnValue = 0;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

void* P_GetPtr(int type, int index, int prop)
{
    setargs_t args;
    void* returnValue = NULL;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callback(type, index, &args, GetProperty);
    return returnValue;
}

void P_GetBoolv(int type, int index, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetBytev(int type, int index, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetIntv(int type, int index, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetFixedv(int type, int index, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetAnglev(int type, int index, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetFloatv(int type, int index, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, GetProperty);
}

void P_GetPtrv(int type, int index, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callback(type, index, &args, GetProperty);
}

/* pointer-based read functions */

boolean P_GetBoolp(void* ptr, int prop)
{
    setargs_t args;
    boolean returnValue = false;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callbackp(args.type, ptr, &args, GetProperty);
    return returnValue;
}

byte P_GetBytep(void* ptr, int prop)
{
    setargs_t args;
    byte returnValue = 0;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callbackp(args.type, ptr, &args, GetProperty);
    return returnValue;
}

int P_GetIntp(void* ptr, int prop)
{
    setargs_t args;
    int returnValue = 0;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callbackp(args.type, ptr, &args, GetProperty);
    return returnValue;
}

fixed_t P_GetFixedp(void* ptr, int prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callbackp(args.type, ptr, &args, GetProperty);
    return returnValue;
}

angle_t P_GetAnglep(void* ptr, int prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callbackp(args.type, ptr, &args, GetProperty);
    return returnValue;
}

float P_GetFloatp(void* ptr, int prop)
{
    setargs_t args;
    float returnValue = 0;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callbackp(args.type, ptr, &args, GetProperty);
    return returnValue;
}

void* P_GetPtrp(void* ptr, int prop)
{
    setargs_t args;
    void* returnValue = NULL;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callbackp(args.type, ptr, &args, GetProperty);
    return returnValue;
}

void P_GetBoolpv(void* ptr, int prop, boolean* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(args.type, ptr, &args, GetProperty);
}

void P_GetBytepv(void* ptr, int prop, byte* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(args.type, ptr, &args, GetProperty);
}

void P_GetIntpv(void* ptr, int prop, int* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(args.type, ptr, &args, GetProperty);
}

void P_GetFixedpv(void* ptr, int prop, fixed_t* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(args.type, ptr, &args, GetProperty);
}

void P_GetAnglepv(void* ptr, int prop, angle_t* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(args.type, ptr, &args, GetProperty);
}

void P_GetFloatpv(void* ptr, int prop, float* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(args.type, ptr, &args, GetProperty);
}

void P_GetPtrpv(void* ptr, int prop, void* params)
{
    setargs_t args;

    InitArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = params;
    P_Callbackp(args.type, ptr, &args, GetProperty);
}

void P_Copy(int type, int prop, int fromIndex, int toIndex)
{
//    setargs_t args;
//    int ptype = propertyTypes[prop];

    Con_Error("P_Copy: Not implemented yet.");
#if 0
    InitArgs(&args, type, prop);

    switch(ptype)
    {
    case DDVT_BOOL:
        {
        boolean b = false;

        args.booleanValues = &b;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_BYTE:
        {
        byte b = 0;

        args.byteValues = &b;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_INT:
        {
        int i = 0;

        args.intValues = &i;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_FIXED:
        {
        fixed_t f = 0;

        args.fixedValues = &f;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_ANGLE:
        {
        angle_t a = 0;

        args.angleValues = &a;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_FLOAT:
        {
        float f = 0;

        args.floatValues = &f;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    case DDVT_PTR:
        {
        void *ptr = NULL;

        args.ptrValues = &ptr;
        P_Callback(type, fromIndex, &args, GetProperty);
        P_Callback(type, toIndex, &args, SetProperty);
        break;
        }

    default:
        Con_Error("P_Copy: properties of type %s cannot be copied\n",
                  DMU_Str(prop));
    }
#endif
}

void P_Copyp(int prop, void* from, void* to)
{
#if 0
    int type = DMU_GetType(from);
    setargs_t args;
//    int ptype = propertyTypes[prop];
#endif

    Con_Error("P_Copyp: Not implemented yet.");
#if 0
    if(DMU_GetType(to) != type)
    {
        Con_Error("P_Copyp: Type mismatch.\n");
    }

    InitArgs(&args, type, prop);

    switch(ptype)
    {
    case DDVT_BOOL:
        {
        boolean b = false;

        args.booleanValues = &b;
        P_Callbackp(args.type, from, &args, GetProperty);
        P_Callbackp(args.type, to, &args, SetProperty);
        break;
        }

    case DDVT_BYTE:
        {
        byte b = 0;

        args.byteValues = &b;
        P_Callbackp(args.type, from, &args, GetProperty);
        P_Callbackp(args.type, to, &args, SetProperty);
        break;
        }

    case DDVT_INT:
        {
        int i = 0;

        args.intValues = &i;
        P_Callbackp(args.type, from, &args, GetProperty);
        P_Callbackp(args.type, to, &args, SetProperty);
        break;
        }

    case DDVT_FIXED:
        {
        fixed_t f = 0;

        args.fixedValues = &f;
        P_Callbackp(args.type, from, &args, GetProperty);
        P_Callbackp(args.type, to, &args, SetProperty);
        break;
        }

    case DDVT_ANGLE:
        {
        angle_t a = 0;

        args.angleValues = &a;
        P_Callbackp(args.type, from, &args, GetProperty);
        P_Callbackp(args.type, to, &args, SetProperty);
        break;
        }

    case DDVT_FLOAT:
        {
        float f = 0;

        args.floatValues = &f;
        P_Callbackp(args.type, from, &args, GetProperty);
        P_Callbackp(args.type, to, &args, SetProperty);
        break;
        }

    case DDVT_PTR:
        {
        void *ptr = NULL;

        args.ptrValues = &ptr;
        P_Callbackp(args.type, from, &args, GetProperty);
        P_Callbackp(args.type, to, &args, SetProperty);
        break;
        }

    default:
        Con_Error("P_Copyp: properties of type %s cannot be copied\n",
                  DMU_Str(prop));
    }
#endif
}

void P_Swap(int type, int prop, int fromIndex, int toIndex)
{
//    setargs_t argsA, argsB;
//    int ptype = propertyTypes[prop];

    Con_Error("P_Swap: Not implemented yet.");
#if 0
    InitArgs(&argsA, type, prop);
    InitArgs(&argsB, type, prop);

    argsA.valueType = argsB.valueType = ptype;

    switch(ptype)
    {
    case DDVT_BOOL:
        {
        boolean a = false;
        boolean b = false;

        argsA.booleanValues = &a;
        argsB.booleanValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_BYTE:
        {
        byte a = 0;
        byte b = 0;

        argsA.byteValues = &a;
        argsB.byteValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_INT:
        {
        int a = 0;
        int b = 0;

        argsA.intValues = &a;
        argsB.intValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_FIXED:
        {
        fixed_t a = 0;
        fixed_t b = 0;

        argsA.fixedValues = &a;
        argsB.fixedValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_ANGLE:
        {
        angle_t a = 0;
        angle_t b = 0;

        argsA.angleValues = &a;
        argsB.angleValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_FLOAT:
        {
        float a = 0;
        float b = 0;

        argsA.floatValues = &a;
        argsB.floatValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_PTR:
        {
        void *a = NULL;
        void *b = NULL;

        argsA.ptrValues = &a;
        argsB.ptrValues = &b;

        P_Callback(type, fromIndex, &argsA, GetProperty);
        P_Callback(type, toIndex, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    default:
        Con_Error("P_Swap: properties of type %s cannot be swapped\n",
                  DMU_Str(prop));
    }
#endif
}

void P_Swapp(int prop, void* from, void* to)
{
#if 0
    int type = DMU_GetType(from);
    setargs_t argsA, argsB;
//    int ptype = propertyTypes[prop];
#endif

    Con_Error("P_Swapp: Not implemented yet.");
#if 0
    if(DMU_GetType(to) != type)
    {
        Con_Error("P_Swapp: Type mismatch.\n");
    }

    InitArgs(&argsA, type, prop);
    InitArgs(&argsB, type, prop);

    argsA.valueType = argsB.valueType = ptype;

    switch(ptype)
    {
    case DDVT_BOOL:
        {
        boolean a = false;
        boolean b = false;

        argsA.booleanValues = &a;
        argsB.booleanValues = &b;

        P_Callbackp(argsA.type, from, &argsA, GetProperty);
        P_Callbackp(argsB.type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_BYTE:
        {
        byte a = 0;
        byte b = 0;

        argsA.byteValues = &a;
        argsB.byteValues = &b;

        P_Callbackp(argsA.type, from, &argsA, GetProperty);
        P_Callbackp(argsB.type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_INT:
        {
        int a = 0;
        int b = 0;

        argsA.intValues = &a;
        argsB.intValues = &b;

        P_Callbackp(argsA.type, from, &argsA, GetProperty);
        P_Callbackp(argsB.type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_FIXED:
        {
        fixed_t a = 0;
        fixed_t b = 0;

        argsA.fixedValues = &a;
        argsB.fixedValues = &b;

        P_Callbackp(argsA.type, from, &argsA, GetProperty);
        P_Callbackp(argsA.type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_ANGLE:
        {
        angle_t a = 0;
        angle_t b = 0;

        argsA.angleValues = &a;
        argsB.angleValues = &b;

        P_Callbackp(argsA.type, from, &argsA, GetProperty);
        P_Callbackp(argsB.type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_FLOAT:
        {
        float a = 0;
        float b = 0;

        argsA.floatValues = &a;
        argsB.floatValues = &b;

        P_Callbackp(argsA.type, from, &argsA, GetProperty);
        P_Callbackp(argsB.type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    case DDVT_PTR:
        {
        void *a = NULL;
        void *b = NULL;

        argsA.ptrValues = &a;
        argsB.ptrValues = &b;

        P_Callbackp(argsA.type, from, &argsA, GetProperty);
        P_Callbackp(argsB.type, to, &argsB, GetProperty);

        SwapValue(type, &a, &b);
        break;
        }

    default:
        Con_Error("P_Swapp: properties of type %s cannot be swapped\n",
                  DMU_Str(prop));
    }
#endif
}
