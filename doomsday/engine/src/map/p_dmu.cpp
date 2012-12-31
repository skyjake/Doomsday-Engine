/** @file p_dmu.cpp Doomsday Map Update API
 *
 * @author Copyright &copy; 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include <cstring>

#include "de_base.h"
#include "de_play.h"
#include "de_audio.h"

#include <de/LegacyCore>
#include <de/memoryzone.h>

#include "api/dd_world.h"
#include "resource/materials.h"

using namespace de;

typedef struct dummysidedef_s {
    SideDef sideDef; /// Side data.
    void *extraData; /// Pointer to user data.
    boolean inUse; /// true, if the dummy is being used.
} dummysidedef_t;

typedef struct dummyline_s {
    LineDef line; /// Line data.
    void *extraData; /// Pointer to user data.
    boolean inUse; /// true, if the dummy is being used.
} dummyline_t;

typedef struct dummysector_s {
    Sector sector; /// Sector data.
    void *extraData; /// Pointer to user data.
    boolean inUse; /// true, if the dummy is being used.
} dummysector_t;

static uint dummyCount = 8; // Number of dummies to allocate (per type).

static dummysidedef_t *dummySideDefs;
static dummyline_t *dummyLines;
static dummysector_t *dummySectors;

static int usingDMUAPIver; // Version of the DMU API the game expects.

char const *DMU_Str(uint prop)
{
    static char propStr[40];

    struct prop_s {
        uint prop;
        char const *str;
    } props[] =
    {
        { DMU_NONE, "(invalid)" },
        { DMU_VERTEX, "DMU_VERTEX" },
        { DMU_HEDGE, "DMU_HEDGE" },
        { DMU_LINEDEF, "DMU_LINEDEF" },
        { DMU_SIDEDEF, "DMU_SIDEDEF" },
        { DMU_BSPNODE, "DMU_BSPNODE" },
        { DMU_BSPLEAF, "DMU_BSPLEAF" },
        { DMU_SECTOR, "DMU_SECTOR" },
        { DMU_PLANE, "DMU_PLANE" },
        { DMU_MATERIAL, "DMU_MATERIAL" },
        { DMU_LINEDEF_BY_TAG, "DMU_LINEDEF_BY_TAG" },
        { DMU_SECTOR_BY_TAG, "DMU_SECTOR_BY_TAG" },
        { DMU_LINEDEF_BY_ACT_TAG, "DMU_LINEDEF_BY_ACT_TAG" },
        { DMU_SECTOR_BY_ACT_TAG, "DMU_SECTOR_BY_ACT_TAG" },
        { DMU_X, "DMU_X" },
        { DMU_Y, "DMU_Y" },
        { DMU_XY, "DMU_XY" },
        { DMU_TANGENT_X, "DMU_TANGENT_X" },
        { DMU_TANGENT_Y, "DMU_TANGENT_Y" },
        { DMU_TANGENT_Z, "DMU_TANGENT_Z" },
        { DMU_TANGENT_XYZ, "DMU_TANGENT_XYZ" },
        { DMU_BITANGENT_X, "DMU_BITANGENT_X" },
        { DMU_BITANGENT_Y, "DMU_BITANGENT_Y" },
        { DMU_BITANGENT_Z, "DMU_BITANGENT_Z" },
        { DMU_BITANGENT_XYZ, "DMU_BITANGENT_XYZ" },
        { DMU_NORMAL_X, "DMU_NORMAL_X" },
        { DMU_NORMAL_Y, "DMU_NORMAL_Y" },
        { DMU_NORMAL_Z, "DMU_NORMAL_Z" },
        { DMU_NORMAL_XYZ, "DMU_NORMAL_XYZ" },
        { DMU_VERTEX0, "DMU_VERTEX0" },
        { DMU_VERTEX1, "DMU_VERTEX1" },
        { DMU_FRONT_SECTOR, "DMU_FRONT_SECTOR" },
        { DMU_BACK_SECTOR, "DMU_BACK_SECTOR" },
        { DMU_SIDEDEF0, "DMU_SIDEDEF0" },
        { DMU_SIDEDEF1, "DMU_SIDEDEF1" },
        { DMU_FLAGS, "DMU_FLAGS" },
        { DMU_DX, "DMU_DX" },
        { DMU_DY, "DMU_DY" },
        { DMU_DXY, "DMU_DXY" },
        { DMU_LENGTH, "DMU_LENGTH" },
        { DMU_SLOPETYPE, "DMU_SLOPETYPE" },
        { DMU_ANGLE, "DMU_ANGLE" },
        { DMU_OFFSET, "DMU_OFFSET" },
        { DMU_OFFSET_X, "DMU_OFFSET_X" },
        { DMU_OFFSET_Y, "DMU_OFFSET_Y" },
        { DMU_OFFSET_XY, "DMU_OFFSET_XY" },
        { DMU_BLENDMODE, "DMU_BLENDMODE" },
        { DMU_VALID_COUNT, "DMU_VALID_COUNT" },
        { DMU_LINEDEF_COUNT, "DMU_LINEDEF_COUNT" },
        { DMU_COLOR, "DMU_COLOR" },
        { DMU_COLOR_RED, "DMU_COLOR_RED" },
        { DMU_COLOR_GREEN, "DMU_COLOR_GREEN" },
        { DMU_COLOR_BLUE, "DMU_COLOR_BLUE" },
        { DMU_ALPHA, "DMU_ALPHA" },
        { DMU_LIGHT_LEVEL, "DMU_LIGHT_LEVEL" },
        { DMT_MOBJS, "DMT_MOBJS" },
        { DMU_BOUNDING_BOX, "DMU_BOUNDING_BOX" },
        { DMU_BASE, "DMU_BASE" },
        { DMU_WIDTH, "DMU_WIDTH" },
        { DMU_HEIGHT, "DMU_HEIGHT" },
        { DMU_TARGET_HEIGHT, "DMU_TARGET_HEIGHT" },
        { DMU_HEDGE_COUNT, "DMU_HEDGE_COUNT" },
        { DMU_SPEED, "DMU_SPEED" },
        { DMU_FLOOR_PLANE, "DMU_FLOOR_PLANE" },
        { DMU_CEILING_PLANE, "DMU_CEILING_PLANE" },
        { 0, NULL }
    };

    for(uint i = 0; props[i].str; ++i)
    {
        if(props[i].prop == prop)
            return props[i].str;
    }

    dd_snprintf(propStr, 40, "(unnamed %i)", prop);
    return propStr;
}

/// @note Part of the Doomsday public API.
int DMU_GetType(void const *ptr)
{
    if(!ptr) return DMU_NONE;

    int type = P_DummyType((void *)ptr);
    if(type != DMU_NONE) return type;

    type = ((runtime_mapdata_header_t const *)ptr)->type;

    // Make sure it's valid.
    switch(type)
    {
    case DMU_VERTEX:
    case DMU_HEDGE:
    case DMU_LINEDEF:
    case DMU_SIDEDEF:
    case DMU_BSPLEAF:
    case DMU_SECTOR:
    case DMU_PLANE:
    case DMU_BSPNODE:
    case DMU_MATERIAL:
        return type;

    default: break; // Unknown.
    }
    return DMU_NONE;
}

/**
 * Initializes a setargs struct.
 *
 * @param type          Type of the map data object.
 * @param args          Ptr to setargs struct to be initialized.
 * @param prop          Property of the map data object.
 */
static void initArgs(setargs_t *args, int type, uint prop)
{
    std::memset(args, 0, sizeof(*args));
    args->type = type;
    args->prop = prop & ~DMU_FLAG_MASK;
    args->modifiers = prop & DMU_FLAG_MASK;
}

void P_InitMapUpdate()
{
    if(DD_GameLoaded())
    {
        // Request the DMU API version the game is expecting.
        usingDMUAPIver = gx.GetInteger(DD_DMU_VERSION);
        if(!usingDMUAPIver)
        {
            QByteArray msg = String("P_InitMapUpdate: Game library is not compatible with %1 %2.")
                                .arg(DOOMSDAY_NICENAME).arg(DOOMSDAY_VERSION_TEXT).toUtf8();
            LegacyCore_FatalError(msg.constData());
        }

        if(usingDMUAPIver > DMUAPI_VER)
        {
            QByteArray msg = String("P_InitMapUpdate: Game library expects a later version of the\n"
                                "DMU API then that defined by %1 %2 \n"
                                "This game is for a newer version of %1.")
                                .arg(DOOMSDAY_NICENAME).arg(DOOMSDAY_VERSION_TEXT).toUtf8();
            LegacyCore_FatalError(msg.constData());
        }
    }
    else
    {
        usingDMUAPIver = DMUAPI_VER;
    }

    // A fixed number of dummies is allocated because:
    // - The number of dummies is mostly dependent on recursive depth of
    //   game functions.
    // - To test whether a pointer refers to a dummy is based on pointer
    //   comparisons; if the array is reallocated, its address may change
    //   and all existing dummies are invalidated.
    dummyLines    = (dummyline_t *)    Z_Calloc(dummyCount * sizeof(dummyline_t),    PU_APPSTATIC, NULL);
    dummySideDefs = (dummysidedef_t *) Z_Calloc(dummyCount * sizeof(dummysidedef_t), PU_APPSTATIC, NULL);
    dummySectors  = (dummysector_t *)  Z_Calloc(dummyCount * sizeof(dummysector_t),  PU_APPSTATIC, NULL);
}

void *P_AllocDummy(int type, void *extraData)
{
    switch(type)
    {
    case DMU_SIDEDEF:
        for(uint i = 0; i < dummyCount; ++i)
        {
            if(!dummySideDefs[i].inUse)
            {
                dummySideDefs[i].inUse = true;
                dummySideDefs[i].extraData = extraData;
                dummySideDefs[i].sideDef.header.type = DMU_SIDEDEF;
                dummySideDefs[i].sideDef.line = 0;
                return &dummySideDefs[i];
            }
        }
        break;

    case DMU_LINEDEF:
        for(uint i = 0; i < dummyCount; ++i)
        {
            if(!dummyLines[i].inUse)
            {
                dummyLines[i].inUse = true;
                dummyLines[i].extraData = extraData;
                dummyLines[i].line.header.type = DMU_LINEDEF;
                dummyLines[i].line.L_frontsidedef =
                    dummyLines[i].line.L_backsidedef = 0;
                dummyLines[i].line.L_frontsector = 0;
                dummyLines[i].line.L_frontside.hedgeLeft =
                    dummyLines[i].line.L_frontside.hedgeRight =0;
                dummyLines[i].line.L_backsector = 0;
                dummyLines[i].line.L_backside.hedgeLeft =
                    dummyLines[i].line.L_backside.hedgeRight =0;
                return &dummyLines[i];
            }
        }
        break;

    case DMU_SECTOR:
        for(uint i = 0; i < dummyCount; ++i)
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

    default: {
        QByteArray msg = String("P_AllocDummy: Dummies of type %1 not supported.").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData()); }
    }

    QByteArray msg = String("P_AllocDummy: Out of dummies of type %1.").arg(DMU_Str(type)).toUtf8();
    LegacyCore_FatalError(msg.constData());
    return 0; // Unreachable.
}

void P_FreeDummy(void *dummy)
{
    switch(P_DummyType(dummy))
    {
    case DMU_SIDEDEF:
        ((dummysidedef_t *)dummy)->inUse = false;
        break;

    case DMU_LINEDEF:
        ((dummyline_t *)dummy)->inUse = false;
        break;

    case DMU_SECTOR:
        ((dummysector_t *)dummy)->inUse = false;
        break;

    default:
        LegacyCore_FatalError("P_FreeDummy: Dummy is of unknown type.");
    }
}

/**
 * Determines the type of a dummy object. For extra safety (in a debug build)
 * it would be possible to look through the dummy arrays and make sure the
 * pointer refers to a real dummy.
 */
int P_DummyType(void *dummy)
{
    // Is it a SideDef?
    if(dummy >= (void *) &dummySideDefs[0] &&
       dummy <= (void *) &dummySideDefs[dummyCount - 1])
    {
        return DMU_SIDEDEF;
    }

    // Is it a line?
    if(dummy >= (void *) &dummyLines[0] &&
       dummy <= (void *) &dummyLines[dummyCount - 1])
    {
        return DMU_LINEDEF;
    }

    // A sector?
    if(dummy >= (void *) &dummySectors[0] &&
       dummy <= (void *) &dummySectors[dummyCount - 1])
    {
        return DMU_SECTOR;
    }

    // Unknown.
    return DMU_NONE;
}

boolean P_IsDummy(void *dummy)
{
    return P_DummyType(dummy) != DMU_NONE;
}

void *P_DummyExtraData(void *dummy)
{
    switch(P_DummyType(dummy))
    {
    case DMU_SIDEDEF:
        return ((dummysidedef_t *)dummy)->extraData;

    case DMU_LINEDEF:
        return ((dummyline_t *)dummy)->extraData;

    case DMU_SECTOR:
        return ((dummysector_t *)dummy)->extraData;

    default:
        return 0;
    }
}

uint P_ToIndex(void const *ptr)
{
    if(!ptr) return 0;

    switch(DMU_GetType(ptr))
    {
    case DMU_VERTEX:
        return GET_VERTEX_IDX((Vertex *) ptr);

    case DMU_HEDGE:
        return GET_HEDGE_IDX((HEdge *) ptr);

    case DMU_LINEDEF:
        return GET_LINE_IDX((LineDef *) ptr);

    case DMU_SIDEDEF:
        return GET_SIDE_IDX((SideDef *) ptr);

    case DMU_BSPLEAF:
        return GET_BSPLEAF_IDX((BspLeaf *) ptr);

    case DMU_SECTOR:
        return GET_SECTOR_IDX((Sector *) ptr);

    case DMU_BSPNODE:
        return GET_BSPNODE_IDX((BspNode *) ptr);

    case DMU_PLANE:
        return GET_PLANE_IDX((Plane *) ptr);

    case DMU_MATERIAL:
        return Materials_Id((material_t *) ptr);

    default:
        QByteArray msg = QString("P_ToIndex: Unknown type %1.").arg(DMU_Str(DMU_GetType(ptr))).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; // Unreachable.
    }
}

/**
 * Convert index to pointer.
 */
void *P_ToPtr(int type, uint index)
{
    switch(type)
    {
    case DMU_VERTEX:
        return VERTEX_PTR(index);

    case DMU_HEDGE:
        return HEDGE_PTR(index);

    case DMU_LINEDEF:
        return LINE_PTR(index);

    case DMU_SIDEDEF:
        return SIDE_PTR(index);

    case DMU_BSPLEAF:
        return BSPLEAF_PTR(index);

    case DMU_SECTOR:
        return SECTOR_PTR(index);

    case DMU_BSPNODE:
        return BSPNODE_PTR(index);

    case DMU_PLANE: {
        QByteArray msg = String("P_ToPtr: Cannot convert %1 to a ptr (sector is unknown).").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable. */ }

    case DMU_MATERIAL:
        return Materials_ToMaterial(index);

    default: {
        QByteArray msg = String("P_ToPtr: unknown type %1.").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable. */ }
    }
}

int P_Iteratep(void *ptr, uint prop, void *context, int (*callback) (void *p, void *ctx))
{
    int type = DMU_GetType(ptr);

    switch(type)
    {
    case DMU_SECTOR:
        switch(prop)
        {
        case DMU_LINEDEF: {
            Sector *sec = (Sector *) ptr;
            int result = false; // Continue iteration.

            if(sec->lineDefs)
            {
                LineDef **linePtr = sec->lineDefs;
                while(*linePtr && !(result = callback(*linePtr, context)))
                {
                    linePtr++;
                }
            }
            return result; }

        case DMU_PLANE: {
            Sector *sec = (Sector *) ptr;
            int result = false; // Continue iteration.

            if(sec->planes)
            {
                Plane **planePtr = sec->planes;
                while(*planePtr && !(result = callback(*planePtr, context)))
                {
                    planePtr++;
                }
            }
            return result; }

        case DMU_BSPLEAF: {
            Sector *sec = (Sector *) ptr;
            int result = false; // Continue iteration.

            if(sec->bspLeafs)
            {
                BspLeaf **ssecIter = sec->bspLeafs;
                while(*ssecIter && !(result = callback(*ssecIter, context)))
                {
                    ssecIter++;
                }
            }
            return result; }

        default: {
            QByteArray msg = String("P_Iteratep: Property %1 unknown/not vector.").arg(DMU_Str(prop)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            return 0; /* Unreachable */ }
        }

    case DMU_BSPLEAF:
        switch(prop)
        {
        case DMU_HEDGE: {
            BspLeaf *bspLeaf = (BspLeaf *) ptr;
            int result = false; // Continue iteration.
            if(bspLeaf->hedge)
            {
                HEdge *hedge = bspLeaf->hedge;
                do
                {
                    result = callback(hedge, context);
                    if(result) break;
                } while((hedge = hedge->next) != bspLeaf->hedge);
            }
            return result; }

        default: {
            QByteArray msg = String("P_Iteratep: Property %1 unknown/not vector.").arg(DMU_Str(prop)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            return 0; /* Unreachable */ }
        }

    default: {
        QByteArray msg = String("P_Iteratep: Type %1 unknown.").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }

    return false;
}

/**
 * Call a callback function on a selection of map data objects. The
 * selected objects will be specified by 'type' and 'index'.

 * @param context       Is passed to the callback function.
 *
 * @return              @c true if all the calls to the callback function
 *                      return @c true.
 *                      @c false is returned when the callback function
 *                      returns @c false; in this case, the iteration is
 *                      aborted immediately when the callback function
 *                      returns @c false.
 */
int P_Callback(int type, uint index, void *context, int (*callback)(void *p, void *ctx))
{
    switch(type)
    {
    case DMU_VERTEX:
        if(index < NUM_VERTEXES)
            return callback(VERTEX_PTR(index), context);
        break;

    case DMU_HEDGE:
        if(index < NUM_HEDGES)
            return callback(HEDGE_PTR(index), context);
        break;

    case DMU_LINEDEF:
        if(index < NUM_LINEDEFS)
            return callback(LINE_PTR(index), context);
        break;

    case DMU_SIDEDEF:
        if(index < NUM_SIDEDEFS)
            return callback(SIDE_PTR(index), context);
        break;

    case DMU_BSPNODE:
        if(index < NUM_BSPNODES)
            return callback(BSPNODE_PTR(index), context);
        break;

    case DMU_BSPLEAF:
        if(index < NUM_BSPLEAFS)
            return callback(BSPLEAF_PTR(index), context);
        break;

    case DMU_SECTOR:
        if(index < NUM_SECTORS)
            return callback(SECTOR_PTR(index), context);
        break;

    case DMU_PLANE: {
        QByteArray msg = String("P_Callback: %1 cannot be referenced by id alone (sector is unknown).").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }

    case DMU_MATERIAL:
        if(index < Materials_Size())
            return callback(Materials_ToMaterial(index), context);
        break;

    case DMU_LINEDEF_BY_TAG:
    case DMU_SECTOR_BY_TAG:
    case DMU_LINEDEF_BY_ACT_TAG:
    case DMU_SECTOR_BY_ACT_TAG: {
        QByteArray msg = String("P_Callback: Type %1 not implemented yet.").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }

    default: {
        QByteArray msg = String("P_Callback: Type %1 unknown (index %2).").arg(DMU_Str(type)).arg(index).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }

    // Successfully completed.
    return true;
}

/**
 * Another version of callback iteration. The set of selected objects is
 * determined by 'type' and 'ptr'. Otherwise works like P_Callback.
 */
int P_Callbackp(int type, void *ptr, void *context, int (*callback)(void *p, void *ctx))
{
    LOG_AS("P_Callbackp");

    switch(type)
    {
    case DMU_VERTEX:
    case DMU_HEDGE:
    case DMU_LINEDEF:
    case DMU_SIDEDEF:
    case DMU_BSPNODE:
    case DMU_BSPLEAF:
    case DMU_SECTOR:
    case DMU_PLANE:
    case DMU_MATERIAL:
        // Only do the callback if the type is the same as the object's.
        if(type == DMU_GetType(ptr))
        {
            return callback(ptr, context);
        }
#if _DEBUG
        else
        {
            LOG_DEBUG("Type mismatch %s != %s\n")
                << DMU_Str(type) << DMU_Str(DMU_GetType(ptr));
        }
#endif
        break;

    default: {
        QByteArray msg = String("P_Callbackp: Type %1 unknown.").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }
    return true;
}

int DMU_SetMaterialProperty(material_t *mat, setargs_t const *args)
{
    DENG_UNUSED(mat);
    QByteArray msg = String("DMU::SetMaterialProperty: Property '%1' is not writable.").arg(DMU_Str(args->prop)).toUtf8();
    LegacyCore_FatalError(msg.constData());
    return 0; // Unreachable.
}

void DMU_SetValue(valuetype_t valueType, void *dst, setargs_t const *args,
                  uint index)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t *d = (fixed_t *)dst;

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
        case DDVT_DOUBLE:
            *d = FLT2FIX(args->doubleValues[index]);
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_FIXED incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float *d = (float *)dst;

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
        case DDVT_DOUBLE:
            *d = (float)args->doubleValues[index];
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_FLOAT incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_DOUBLE)
    {
        double *d = (double *)dst;

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
        case DDVT_DOUBLE:
            *d = args->doubleValues[index];
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_DOUBLE incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean *d = (boolean *)dst;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            *d = args->booleanValues[index];
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_BOOL incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        byte *d = (byte *)dst;

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
        case DDVT_DOUBLE:
            *d = (byte) args->doubleValues[index];
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_BYTE incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_INT)
    {
        int *d = (int *)dst;

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
        case DDVT_DOUBLE:
            *d = args->doubleValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_INT incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_SHORT)
    {
        short *d = (short *)dst;

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
        case DDVT_DOUBLE:
            *d = args->doubleValues[index];
            break;
        case DDVT_FIXED:
            *d = (args->fixedValues[index] >> FRACBITS);
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_SHORT incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t *d = (angle_t *)dst;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            *d = args->angleValues[index];
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_ANGLE incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t *d = (blendmode_t *)dst;

        switch(args->valueType)
        {
        case DDVT_INT:
            if(args->intValues[index] > DDNUM_BLENDMODES || args->intValues[index] < 0)
            {
                QByteArray msg = String("SetValue: %1 is not a valid value for DDVT_BLENDMODE.").arg(args->intValues[index]).toUtf8();
                LegacyCore_FatalError(msg.constData());
            }

            *d = blendmode_t(args->intValues[index]);
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_BLENDMODE incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_PTR)
    {
        void **d = (void **)dst;

        switch(args->valueType)
        {
        case DDVT_PTR:
            *d = args->ptrValues[index];
            break;
        default: {
            QByteArray msg = String("SetValue: DDVT_PTR incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else
    {
        QByteArray msg = String("SetValue: unknown value type %1.").arg(valueType).toUtf8();
        LegacyCore_FatalError(msg.constData());
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
static int setProperty(void *obj, void *context)
{
    setargs_t *args = (setargs_t *) context;
    Sector *updateSector1 = NULL, *updateSector2 = NULL;
    Plane *updatePlane = NULL;
    LineDef *updateLinedef = NULL;
    SideDef *updateSidedef = NULL;
    Surface *updateSurface = NULL;
    // BspLeaf *updateBspLeaf = NULL;

    /**
     * @par Algorithm
     * When setting a property, reference resolution is done hierarchically so
     * that we can update all owner's of the objects being manipulated should
     * the DMU object's Set routine suggest that a change occured (which other
     * DMU objects may wish/need to respond to).
     * <ol>
     * <li> Collect references to all current owners of the object.
     * <li> Pass the change delta on to the object.
     * <li> Object responds: @c true = update owners, ELSE @c false.
     * <li> If num collected references > 0: recurse, Object = owners[n]
     * </ol>
     */

    // Dereference where necessary. Note the order, these cascade.
    if(args->type == DMU_BSPLEAF)
    {
        // updateBspLeaf = (BspLeaf *) obj;

        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            obj = ((BspLeaf *) obj)->sector;
            args->type = DMU_SECTOR;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            obj = ((BspLeaf *) obj)->sector;
            args->type = DMU_SECTOR;
        }
    }

    if(args->type == DMU_SECTOR)
    {
        updateSector1 = (Sector *) obj;

        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            Sector* sec = (Sector *) obj;
            obj = sec->SP_plane(PLN_FLOOR);
            args->type = DMU_PLANE;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            Sector* sec = (Sector *) obj;
            obj = sec->SP_plane(PLN_CEILING);
            args->type = DMU_PLANE;
        }
    }

    if(args->type == DMU_LINEDEF)
    {
        updateLinedef = (LineDef *) obj;

        if(args->modifiers & DMU_SIDEDEF0_OF_LINE)
        {
            obj = ((LineDef *) obj)->L_frontsidedef;
            args->type = DMU_SIDEDEF;
        }
        else if(args->modifiers & DMU_SIDEDEF1_OF_LINE)
        {
            LineDef *li = ((LineDef *) obj);
            if(!li->L_backsidedef)
            {
                QByteArray msg = String("DMU_setProperty: Linedef %1 has no back side.").arg(P_ToIndex(li)).toUtf8();
                LegacyCore_FatalError(msg.constData());
            }

            obj = li->L_backsidedef;
            args->type = DMU_SIDEDEF;
        }
    }

    if(args->type == DMU_SIDEDEF)
    {
        updateSidedef = (SideDef *) obj;

        if(args->modifiers & DMU_TOP_OF_SIDEDEF)
        {
            obj = &((SideDef *) obj)->SW_topsurface;
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_MIDDLE_OF_SIDEDEF)
        {
            obj = &((SideDef *) obj)->SW_middlesurface;
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_BOTTOM_OF_SIDEDEF)
        {
            obj = &((SideDef *) obj)->SW_bottomsurface;
            args->type = DMU_SURFACE;
        }
    }

    if(args->type == DMU_PLANE)
    {
        updatePlane = (Plane *) obj;

        switch(args->prop)
        {
        case DMU_MATERIAL:
        case DMU_OFFSET_X:
        case DMU_OFFSET_Y:
        case DMU_OFFSET_XY:
        case DMU_TANGENT_X:
        case DMU_TANGENT_Y:
        case DMU_TANGENT_Z:
        case DMU_TANGENT_XYZ:
        case DMU_BITANGENT_X:
        case DMU_BITANGENT_Y:
        case DMU_BITANGENT_Z:
        case DMU_BITANGENT_XYZ:
        case DMU_NORMAL_X:
        case DMU_NORMAL_Y:
        case DMU_NORMAL_Z:
        case DMU_NORMAL_XYZ:
        case DMU_COLOR:
        case DMU_COLOR_RED:
        case DMU_COLOR_GREEN:
        case DMU_COLOR_BLUE:
        case DMU_ALPHA:
        case DMU_BLENDMODE:
        case DMU_FLAGS:
            obj = &((Plane *) obj)->surface;
            args->type = DMU_SURFACE;
            break;

        default:
            break;
        }
    }

    if(args->type == DMU_SURFACE)
    {
        updateSurface = (Surface *) obj;
/*
        // Resolve implicit references to properties of the surface's material.
        switch(args->prop)
        {
        case UNKNOWN1:
            obj = &((Surface*) obj)->material;
            args->type = DMU_MATERIAL;
            break;

        default:
            break;
        }*/
    }

    switch(args->type)
    {
    case DMU_SURFACE:
        Surface_SetProperty((Surface *)obj, args);
        break;

    case DMU_PLANE:
        Plane_SetProperty((Plane *)obj, args);
        break;

    case DMU_VERTEX:
        Vertex_SetProperty((Vertex *)obj, args);
        break;

    case DMU_HEDGE:
        HEdge_SetProperty((HEdge *)obj, args);
        break;

    case DMU_LINEDEF:
        LineDef_SetProperty((LineDef *)obj, args);
        break;

    case DMU_SIDEDEF:
        SideDef_SetProperty((SideDef *)obj, args);
        break;

    case DMU_BSPLEAF:
        BspLeaf_SetProperty((BspLeaf *)obj, args);
        break;

    case DMU_SECTOR:
        Sector_SetProperty((Sector *)obj, args);
        break;

    case DMU_MATERIAL:
        DMU_SetMaterialProperty((material_t *)obj, args);
        break;

    case DMU_BSPNODE: {
        QByteArray msg = String("SetProperty: Property %1 is not writable in DMU_BSPNODE.").arg(DMU_Str(args->prop)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        break; }

    default: {
        QByteArray msg = String("SetProperty: Type %1 not writable.").arg(DMU_Str(args->type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }

    if(updateSurface)
    {
        if(R_UpdateSurface(updateSurface, false))
        {
            switch(DMU_GetType(updateSurface->owner))
            {
            case DMU_SIDEDEF:
                updateSidedef = (SideDef *)updateSurface->owner;
                break;

            case DMU_PLANE:
                updatePlane = (Plane *)updateSurface->owner;
                break;

            default:
                LegacyCore_FatalError("SetPropert: Internal error, surface owner unknown.");
            }
        }
    }

    if(updateSidedef)
    {
        if(R_UpdateSidedef(updateSidedef, false))
            updateLinedef = updateSidedef->line;
    }

    if(updateLinedef)
    {
        if(R_UpdateLinedef(updateLinedef, false))
        {
            updateSector1 = updateLinedef->L_frontsector;
            updateSector2 = updateLinedef->L_backsector;
        }
    }

    if(updatePlane)
    {
        if(R_UpdatePlane(updatePlane, false))
            updateSector1 = updatePlane->sector;
    }

    if(updateSector1)
    {
        R_UpdateSector(updateSector1, false);
    }

    if(updateSector2)
    {
        R_UpdateSector(updateSector2, false);
    }

/*  if(updateBspLeaf)
    {
        R_UpdateBspLeaf(updateBspLeaf, false);
    } */

    return true; // Continue iteration.
}

int DMU_GetMaterialProperty(material_t *mat, setargs_t *args)
{
    switch(args->prop)
    {
    case DMU_FLAGS: {
        short flags = Material_Flags(mat);
        DMU_GetValue(DMT_MATERIAL_FLAGS, &flags, args, 0);
        break; }

    case DMU_WIDTH: {
        int width = Material_Width(mat);
        DMU_GetValue(DMT_MATERIAL_WIDTH, &width, args, 0);
        break; }

    case DMU_HEIGHT: {
        int height = Material_Height(mat);
        DMU_GetValue(DMT_MATERIAL_HEIGHT, &height, args, 0);
        break; }

    default: {
        QByteArray msg = String("DMU::GetMaterialProperty: No property %1.").arg(DMU_Str(args->prop)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }
    return false; // Continue iteration.
}

void DMU_GetValue(valuetype_t valueType, void const *src, setargs_t *args,
                  uint index)
{
    if(valueType == DDVT_FIXED)
    {
        fixed_t const *s = (fixed_t const *)src;

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
        case DDVT_DOUBLE:
            args->doubleValues[index] = FIX2FLT(*s);
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_FIXED incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_FLOAT)
    {
        float const *s = (float const *)src;

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
        case DDVT_DOUBLE:
            args->doubleValues[index] = (double)*s;
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_FLOAT incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_DOUBLE)
    {
        double const *s = (double const *)src;

        switch(args->valueType)
        {
        case DDVT_BYTE:
            args->byteValues[index] = (byte)*s;
            break;
        case DDVT_INT:
            args->intValues[index] = (int) *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = FLT2FIX(*s);
            break;
        case DDVT_FLOAT:
            args->floatValues[index] = (float)*s;
            break;
        case DDVT_DOUBLE:
            args->doubleValues[index] = *s;
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_DOUBLE incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_BOOL)
    {
        boolean const *s = (boolean const *)src;

        switch(args->valueType)
        {
        case DDVT_BOOL:
            args->booleanValues[index] = *s;
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_BOOL incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_BYTE)
    {
        byte const *s = (byte const *)src;

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
        case DDVT_DOUBLE:
            args->doubleValues[index] = *s;
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_BYTE incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_INT)
    {
        int const *s = (int const *)src;

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
        case DDVT_DOUBLE:
            args->doubleValues[index] = *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = (*s << FRACBITS);
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_INT incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_SHORT)
    {
        short const *s = (short const *)src;

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
        case DDVT_DOUBLE:
            args->doubleValues[index] = *s;
            break;
        case DDVT_FIXED:
            args->fixedValues[index] = (*s << FRACBITS);
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_SHORT incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_ANGLE)
    {
        angle_t const *s = (angle_t const *)src;

        switch(args->valueType)
        {
        case DDVT_ANGLE:
            args->angleValues[index] = *s;
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_ANGLE incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_BLENDMODE)
    {
        blendmode_t const *s = (blendmode_t const *)src;

        switch(args->valueType)
        {
        case DDVT_INT:
            args->intValues[index] = *s;
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_BLENDMODE incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else if(valueType == DDVT_PTR)
    {
        void const *const *s = (void const *const *)src;

        switch(args->valueType)
        {
        case DDVT_INT:
            // Attempt automatic conversion using P_ToIndex(). Naturally only
            // works with map data objects. Failure leads into a fatal error.
            args->intValues[index] = P_ToIndex(*s);
            break;
        case DDVT_PTR:
            args->ptrValues[index] = (void *) *s;
            break;
        default: {
            QByteArray msg = String("GetValue: DDVT_PTR incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else
    {
        QByteArray msg = String("GetValue: unknown value type %1.").arg(valueType).toUtf8();
        LegacyCore_FatalError(msg.constData());
    }
}

static int getProperty(void *ob, void *context)
{
    setargs_t *args = (setargs_t *) context;

    // Dereference where necessary. Note the order, these cascade.
    if(args->type == DMU_BSPLEAF)
    {
        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            ob = ((BspLeaf *)ob)->sector;
            args->type = DMU_SECTOR;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            ob = ((BspLeaf *)ob)->sector;
            args->type = DMU_SECTOR;
        }
        else
        {
            switch(args->prop)
            {
            case DMU_LIGHT_LEVEL:
            case DMT_MOBJS:
                ob = ((BspLeaf *)ob)->sector;
                args->type = DMU_SECTOR;
                break;
            default: break;
            }
        }
    }

    if(args->type == DMU_SECTOR)
    {
        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            Sector *sec = (Sector *)ob;
            ob = sec->SP_plane(PLN_FLOOR);
            args->type = DMU_PLANE;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            Sector *sec = (Sector *)ob;
            ob = sec->SP_plane(PLN_CEILING);
            args->type = DMU_PLANE;
        }
    }

    if(args->type == DMU_LINEDEF)
    {
        if(args->modifiers & DMU_SIDEDEF0_OF_LINE)
        {
            LineDef *li = ((LineDef *)ob);
            if(!li->L_frontsidedef) // $degenleaf
            {
                QByteArray msg = String("DMU_setProperty: Linedef %1 has no front side.").arg(P_ToIndex(li)).toUtf8();
                LegacyCore_FatalError(msg.constData());
            }

            ob = li->L_frontsidedef;
            args->type = DMU_SIDEDEF;
        }
        else if(args->modifiers & DMU_SIDEDEF1_OF_LINE)
        {
            LineDef *li = ((LineDef *)ob);
            if(!li->L_backsidedef)
            {
                QByteArray msg = String("DMU_setProperty: Linedef %1 has no back side.").arg(P_ToIndex(li)).toUtf8();
                LegacyCore_FatalError(msg.constData());
            }

            ob = li->L_backsidedef;
            args->type = DMU_SIDEDEF;
        }
    }

    if(args->type == DMU_SIDEDEF)
    {
        if(args->modifiers & DMU_TOP_OF_SIDEDEF)
        {
            ob = &((SideDef *)ob)->SW_topsurface;
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_MIDDLE_OF_SIDEDEF)
        {
            ob = &((SideDef *)ob)->SW_middlesurface;
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_BOTTOM_OF_SIDEDEF)
        {
            ob = &((SideDef *)ob)->SW_bottomsurface;
            args->type = DMU_SURFACE;
        }
    }

    if(args->type == DMU_PLANE)
    {
        switch(args->prop)
        {
        case DMU_MATERIAL:
        case DMU_OFFSET_X:
        case DMU_OFFSET_Y:
        case DMU_OFFSET_XY:
        case DMU_TANGENT_X:
        case DMU_TANGENT_Y:
        case DMU_TANGENT_Z:
        case DMU_TANGENT_XYZ:
        case DMU_BITANGENT_X:
        case DMU_BITANGENT_Y:
        case DMU_BITANGENT_Z:
        case DMU_BITANGENT_XYZ:
        case DMU_NORMAL_X:
        case DMU_NORMAL_Y:
        case DMU_NORMAL_Z:
        case DMU_NORMAL_XYZ:
        case DMU_COLOR:
        case DMU_COLOR_RED:
        case DMU_COLOR_GREEN:
        case DMU_COLOR_BLUE:
        case DMU_ALPHA:
        case DMU_BLENDMODE:
        case DMU_FLAGS:
        case DMU_BASE:
            ob = &((Plane *)ob)->surface;
            args->type = DMU_SURFACE;
            break;

        default:
            break;
        }
    }

    switch(args->type)
    {
    case DMU_VERTEX:
        Vertex_GetProperty((Vertex *)ob, args);
        break;

    case DMU_HEDGE:
        HEdge_GetProperty((HEdge *)ob, args);
        break;

    case DMU_LINEDEF:
        LineDef_GetProperty((LineDef *)ob, args);
        break;

    case DMU_SURFACE:
        Surface_GetProperty((Surface *)ob, args);
        break;

    case DMU_PLANE:
        Plane_GetProperty((Plane *)ob, args);
        break;

    case DMU_SECTOR:
        Sector_GetProperty((Sector *)ob, args);
        break;

    case DMU_SIDEDEF:
        SideDef_GetProperty((SideDef *)ob, args);
        break;

    case DMU_BSPLEAF:
        BspLeaf_GetProperty((BspLeaf *)ob, args);
        break;

    case DMU_MATERIAL:
        DMU_GetMaterialProperty((material_t *)ob, args);
        break;

    default: {
        QByteArray msg = String("GetProperty: Type %1 not readable.").arg(DMU_Str(args->type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }

    // Currently no aggregate values are collected.
    return false;
}

void P_SetBool(int type, uint index, uint prop, boolean param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetByte(int type, uint index, uint prop, byte param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetInt(int type, uint index, uint prop, int param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetFixed(int type, uint index, uint prop, fixed_t param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetAngle(int type, uint index, uint prop, angle_t param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetFloat(int type, uint index, uint prop, float param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetDouble(int type, uint index, uint prop, double param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetPtr(int type, uint index, uint prop, void *param)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callback(type, index, &args, setProperty);
}

void P_SetBoolv(int type, uint index, uint prop, boolean *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetBytev(int type, uint index, uint prop, byte *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetIntv(int type, uint index, uint prop, int *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetFixedv(int type, uint index, uint prop, fixed_t *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetAnglev(int type, uint index, uint prop, angle_t *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetFloatv(int type, uint index, uint prop, float *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetDoublev(int type, uint index, uint prop, double *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callback(type, index, &args, setProperty);
}

void P_SetPtrv(int type, uint index, uint prop, void *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callback(type, index, &args, setProperty);
}

/* pointer-based write functions */

void P_SetBoolp(void *ptr, uint prop, boolean param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetBytep(void *ptr, uint prop, byte param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetIntp(void *ptr, uint prop, int param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetFixedp(void *ptr, uint prop, fixed_t param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetAnglep(void *ptr, uint prop, angle_t param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetFloatp(void *ptr, uint prop, float param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetDoublep(void *ptr, uint prop, double param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetPtrp(void *ptr, uint prop, void *param)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetBoolpv(void *ptr, uint prop, boolean *params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetBytepv(void *ptr, uint prop, byte *params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetIntpv(void *ptr, uint prop, int *params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetFixedpv(void *ptr, uint prop, fixed_t *params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetAnglepv(void *ptr, uint prop, angle_t *params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetFloatpv(void *ptr, uint prop, float *params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetDoublepv(void *ptr, uint prop, double *params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

void P_SetPtrpv(void *ptr, uint prop, void *params)
{
    setargs_t args;

    initArgs(&args, DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callbackp(args.type, ptr, &args, setProperty);
}

/* index-based read functions */

boolean P_GetBool(int type, uint index, uint prop)
{
    setargs_t args;
    boolean returnValue = false;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

byte P_GetByte(int type, uint index, uint prop)
{
    setargs_t args;
    byte returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

int P_GetInt(int type, uint index, uint prop)
{
    setargs_t args;
    int returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

fixed_t P_GetFixed(int type, uint index, uint prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

angle_t P_GetAngle(int type, uint index, uint prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

float P_GetFloat(int type, uint index, uint prop)
{
    setargs_t args;
    float returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

double P_GetDouble(int type, uint index, uint prop)
{
    setargs_t args;
    double returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

void *P_GetPtr(int type, uint index, uint prop)
{
    setargs_t args;
    void *returnValue = 0;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callback(type, index, &args, getProperty);
    return returnValue;
}

void P_GetBoolv(int type, uint index, uint prop, boolean *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetBytev(int type, uint index, uint prop, byte *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetIntv(int type, uint index, uint prop, int *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetFixedv(int type, uint index, uint prop, fixed_t *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetAnglev(int type, uint index, uint prop, angle_t *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetFloatv(int type, uint index, uint prop, float *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetDoublev(int type, uint index, uint prop, double *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callback(type, index, &args, getProperty);
}

void P_GetPtrv(int type, uint index, uint prop, void *params)
{
    setargs_t args;

    initArgs(&args, type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callback(type, index, &args, getProperty);
}

/* pointer-based read functions */

boolean P_GetBoolp(void *ptr, uint prop)
{
    setargs_t args;
    boolean returnValue = false;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

byte P_GetBytep(void *ptr, uint prop)
{
    setargs_t args;
    byte returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

int P_GetIntp(void *ptr, uint prop)
{
    setargs_t args;
    int returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

fixed_t P_GetFixedp(void *ptr, uint prop)
{
    setargs_t args;
    fixed_t returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

angle_t P_GetAnglep(void *ptr, uint prop)
{
    setargs_t args;
    angle_t returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

float P_GetFloatp(void *ptr, uint prop)
{
    setargs_t args;
    float returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

double P_GetDoublep(void *ptr, uint prop)
{
    setargs_t args;
    double returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_DOUBLE;
        args.doubleValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

void *P_GetPtrp(void *ptr, uint prop)
{
    setargs_t args;
    void *returnValue = 0;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }

    return returnValue;
}

void P_GetBoolpv(void *ptr, uint prop, boolean *params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetBytepv(void *ptr, uint prop, byte *params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetIntpv(void *ptr, uint prop, int *params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetFixedpv(void *ptr, uint prop, fixed_t *params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetAnglepv(void *ptr, uint prop, angle_t *params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetFloatpv(void *ptr, uint prop, float *params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetDoublepv(void *ptr, uint prop, double *params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_DOUBLE;
        args.doubleValues = params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}

void P_GetPtrpv(void *ptr, uint prop, void *params)
{
    setargs_t args;

    if(ptr)
    {
        initArgs(&args, DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = (void **)params;
        P_Callbackp(args.type, ptr, &args, getProperty);
    }
}
