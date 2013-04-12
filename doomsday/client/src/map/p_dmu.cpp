/** @file p_dmu.cpp Doomsday Map Update API
 *
 * @todo Throw a game-terminating exception if an illegal value is given
 * to a public API function.
 *
 * @authors Copyright &copy; 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#define DENG_NO_API_MACROS_MAP

#include <cstring>

#include "de_base.h"
#include "de_play.h"
#include "de_audio.h"

#include <de/LegacyCore>
#include <de/memoryzone.h>

#include "Materials"
#include "api_map.h"

// Converting a public void* pointer to an internal de::MapElement.
#define IN_ELEM(p)          reinterpret_cast<de::MapElement *>(p)
#define IN_ELEM_CONST(p)    reinterpret_cast<de::MapElement const *>(p)

using namespace de;

/**
 * Additional data for all dummy elements.
 */
struct DummyData
{
    void *extraData; /// Pointer to user data.

    DummyData() : extraData(0) {}
    virtual ~DummyData() {} // polymorphic
};

class DummyVertex  : public Vertex,  public DummyData {};
class DummySideDef : public SideDef, public DummyData {};
class DummySector  : public Sector,  public DummyData {};

class DummyLineDef : public LineDef, public DummyData
{
public:
    DummyLineDef(DummyVertex &v1, DummyVertex &v2) : LineDef(v1, v2) {}
};

typedef QSet<de::MapElement *> Dummies;
static Dummies dummies;
static DummyVertex dummyVertex; // The one dummy vertex.

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

#undef DMU_GetType
int DMU_GetType(void const *ptr)
{
    if(!ptr) return DMU_NONE;

    de::MapElement const *elem = IN_ELEM_CONST(ptr);

    // Make sure it's valid.
    switch(elem->type())
    {
    case DMU_VERTEX:
    case DMU_HEDGE:
    case DMU_LINEDEF:
    case DMU_SIDEDEF:
    case DMU_BSPLEAF:
    case DMU_SECTOR:
    case DMU_PLANE:
    case DMU_BSPNODE:
    case DMU_SURFACE:
    case DMU_MATERIAL:
        return elem->type();

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
    DENG_ASSERT(args && VALID_DMU_ELEMENT_TYPE_ID(type));

    std::memset(args, 0, sizeof(*args));
    args->type = type;
    args->prop = prop & ~DMU_FLAG_MASK;
    args->modifiers = prop & DMU_FLAG_MASK;
}

void P_InitMapUpdate()
{
    // TODO: free existing/old dummies here?

    dummies.clear();
}

#undef P_AllocDummy
void *P_AllocDummy(int type, void *extraData)
{
    switch(type)
    {
    case DMU_SIDEDEF: {
        DummySideDef *ds = new DummySideDef;
        dummies.insert(ds);
        ds->extraData = extraData;
        return ds; }

    case DMU_LINEDEF: {
        DummyLineDef *dl = new DummyLineDef(dummyVertex, dummyVertex);
        dummies.insert(dl);
        dl->extraData = extraData;
        return dl; }

    case DMU_SECTOR: {
        DummySector *ds = new DummySector;
        dummies.insert(ds);
        ds->extraData = extraData;
        return ds; }

    default: {
        /// @throw Throw exception.
        QByteArray msg = String("P_AllocDummy: Dummies of type %1 not supported.").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        break; }
    }

    return 0; // Unreachable.
}

#undef P_IsDummy
boolean P_IsDummy(void const *dummy)
{
    return P_DummyType(dummy) != DMU_NONE;
}

#undef P_FreeDummy
void P_FreeDummy(void *dummy)
{
    de::MapElement *elem = IN_ELEM(dummy);

    int type = P_DummyType(dummy);
    if(type == DMU_NONE)
    {
        /// @todo Throw exception.
        LegacyCore_FatalError("P_FreeDummy: Dummy is of unknown type.");
    }

    DENG2_ASSERT(dummies.contains(elem));

    dummies.remove(elem);
    delete elem;
}

/**
 * Determines the type of a dummy object.
 */
int P_DummyType(void const *dummy)
{
    de::MapElement const *elem = IN_ELEM_CONST(dummy);

    if(!dynamic_cast<DummyData const *>(elem))
    {
        // Not a dummy.
        return DMU_NONE;
    }

    DENG2_ASSERT(dummies.contains(const_cast<de::MapElement *>(elem)));

    return elem->type();
}

#undef P_DummyExtraData
void *P_DummyExtraData(void *dummy)
{
    if(P_IsDummy(dummy))
    {
        de::MapElement *elem = IN_ELEM(dummy);
        return elem->castTo<DummyData>()->extraData;
    }
    return 0;
}

#undef P_ToIndex
uint P_ToIndex(void const *ptr)
{
    if(!ptr) return 0;
    if(P_IsDummy(ptr)) return 0;

    de::MapElement const *elem = IN_ELEM_CONST(ptr);

    switch(elem->type())
    {
    case DMU_VERTEX:
        return theMap->vertexIndex(elem->castTo<Vertex>());

    case DMU_HEDGE:
        return theMap->hedgeIndex(elem->castTo<HEdge>());

    case DMU_LINEDEF:
        return theMap->lineIndex(elem->castTo<LineDef>());

    case DMU_SIDEDEF:
        return theMap->sideDefIndex(elem->castTo<SideDef>());

    case DMU_BSPLEAF:
        return theMap->bspLeafIndex(elem->castTo<BspLeaf>());

    case DMU_SECTOR:
        return theMap->sectorIndex(elem->castTo<Sector>());

    case DMU_BSPNODE:
        return theMap->bspNodeIndex(elem->castTo<BspNode>());

    case DMU_PLANE:
        return elem->castTo<Plane>()->inSectorIndex();

    case DMU_MATERIAL:
        return elem->castTo<Material>()->manifest().id();

    default:
        /// @todo Throw exception.
        DENG2_ASSERT(false); // Unknown/non-indexable DMU type.
        return 0;
    }
}

#undef P_ToPtr
void *P_ToPtr(int type, uint index)
{
    switch(type)
    {
    case DMU_VERTEX:
        return theMap->vertexes().at(index);

    case DMU_HEDGE:
        return theMap->hedges().at(index);

    case DMU_LINEDEF:
        return theMap->lines().at(index);

    case DMU_SIDEDEF:
        return theMap->sideDefs().at(index);

    case DMU_SECTOR:
        return theMap->sectors().at(index);

    case DMU_PLANE: {
        /// @todo Throw exception.
        QByteArray msg = String("P_ToPtr: Cannot convert %1 to a ptr (sector is unknown).").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable. */ }

    case DMU_BSPLEAF:
        return theMap->bspLeafs().at(index);

    case DMU_BSPNODE:
        return theMap->bspNodes().at(index);

    case DMU_MATERIAL:
        if(index == 0) return 0;
        return &App_Materials().toManifest(index).material();

    default: {
        /// @todo Throw exception.
        QByteArray msg = String("P_ToPtr: unknown type %1.").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable. */ }
    }
}

#undef P_Iteratep
int P_Iteratep(void *elPtr, uint prop, void *context, int (*callback) (void *p, void *ctx))
{
    de::MapElement *elem = IN_ELEM(elPtr);

    switch(elem->type())
    {
    case DMU_SECTOR:
        switch(prop)
        {
        case DMU_LINEDEF:
            foreach(LineDef *line, elem->castTo<Sector>()->lines())
            {
                int result = callback(line, context);
                if(result) return result;
            }
            return false; // Continue iteration

        case DMU_PLANE:
            foreach(Plane *plane, elem->castTo<Sector>()->planes())
            {
                int result = callback(plane, context);
                if(result) return result;
            }
            return false; // Continue iteration

        case DMU_BSPLEAF:
            foreach(BspLeaf *bspLeaf, elem->castTo<Sector>()->bspLeafs())
            {
                int result = callback(bspLeaf, context);
                if(result) return result;
            }
            return false; // Continue iteration.

        default:
            throw Error("P_Iteratep", QString("Property %1 unknown/not vector").arg(DMU_Str(prop)));
        }

    case DMU_BSPLEAF:
        switch(prop)
        {
        case DMU_HEDGE:
            if(HEdge *base = elem->castTo<BspLeaf>()->firstHEdge())
            {
                HEdge *hedge = base;
                do
                {
                    int result = callback(hedge, context);
                    if(result) return result;

                } while((hedge = &hedge->next()) != base);
            }
            return false; // Continue iteration.

        default:
            throw Error("P_Iteratep", QString("Property %1 unknown/not vector").arg(DMU_Str(prop)));
        }

    default:
        throw Error("P_Iteratep", QString("Type %1 unknown").arg(DMU_Str(elem->type())));
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
#undef P_Callback
int P_Callback(int type, uint index, void *context, int (*callback)(void *p, void *ctx))
{
    switch(type)
    {
    case DMU_VERTEX:
        if(index < theMap->vertexCount())
            return callback(theMap->vertexes().at(index), context);
        break;

    case DMU_HEDGE:
        if(index < theMap->hedgeCount())
            return callback(theMap->hedges().at(index), context);
        break;

    case DMU_LINEDEF:
        if(index < theMap->lineCount())
            return callback(theMap->lines().at(index), context);
        break;

    case DMU_SIDEDEF:
        if(index < theMap->sideDefCount())
            return callback(theMap->sideDefs().at(index), context);
        break;

    case DMU_BSPNODE:
        if(index < theMap->bspNodeCount())
            return callback(theMap->bspNodes().at(index), context);
        break;

    case DMU_BSPLEAF:
        if(index < theMap->bspLeafCount())
            return callback(theMap->bspLeafs().at(index), context);
        break;

    case DMU_SECTOR:
        if(index < theMap->sectorCount())
            return callback(theMap->sectors().at(index), context);
        break;

    case DMU_PLANE: {
        /// @todo Throw exception.
        QByteArray msg = String("P_Callback: %1 cannot be referenced by id alone (sector is unknown).").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }

    case DMU_MATERIAL:
        if(index != 0)
            return callback(&App_Materials().toManifest(materialid_t(index)).material(), context);
        break;

    case DMU_LINEDEF_BY_TAG:
    case DMU_SECTOR_BY_TAG:
    case DMU_LINEDEF_BY_ACT_TAG:
    case DMU_SECTOR_BY_ACT_TAG: {
        /// @todo Throw exception.
        QByteArray msg = String("P_Callback: Type %1 not implemented yet.").arg(DMU_Str(type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }

    default: {
        /// @todo Throw exception.
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
#undef P_Callbackp
int P_Callbackp(int type, void *elPtr, void *context, int (*callback)(void *p, void *ctx))
{
    de::MapElement *elem = IN_ELEM(elPtr);

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
        if(type == elem->type())
        {
            return callback(elem, context);
        }
#if _DEBUG
        else
        {
            LOG_DEBUG("Type mismatch %s != %s\n") << DMU_Str(type) << DMU_Str(elem->type());
            DENG2_ASSERT(false);
        }
#endif
        break;

    default: {
        /// @todo Throw exception.
        QByteArray msg = String("P_Callbackp: Type %1 unknown.").arg(DMU_Str(elem->type())).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }
    return true;
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
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
            /// @todo Throw exception.
            QByteArray msg = String("SetValue: DDVT_PTR incompatible with value type %1.").arg(value_Str(args->valueType)).toUtf8();
            LegacyCore_FatalError(msg.constData());
            }
        }
    }
    else
    {
        /// @todo Throw exception.
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
static int setProperty(void *ptr, void *context)
{
    de::MapElement *elem = IN_ELEM(ptr);
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
        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = elem->castTo<BspLeaf>()->sectorPtr();
            args->type = DMU_SECTOR;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = elem->castTo<BspLeaf>()->sectorPtr();
            args->type = DMU_SECTOR;
        }
    }

    if(args->type == DMU_SECTOR)
    {
        updateSector1 = elem->castTo<Sector>();

        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = &elem->castTo<Sector>()->floor();
            args->type = DMU_PLANE;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = &elem->castTo<Sector>()->ceiling();
            args->type = DMU_PLANE;
        }
    }

    if(args->type == DMU_LINEDEF)
    {
        updateLinedef = elem->castTo<LineDef>();

        if(args->modifiers & DMU_SIDEDEF0_OF_LINE)
        {
            elem = &elem->castTo<LineDef>()->frontSideDef();
            args->type = DMU_SIDEDEF;
        }
        else if(args->modifiers & DMU_SIDEDEF1_OF_LINE)
        {
            elem = &elem->castTo<LineDef>()->backSideDef();
            args->type = DMU_SIDEDEF;
        }
    }

    if(args->type == DMU_SIDEDEF)
    {
        updateSidedef = elem->castTo<SideDef>();

        if(args->modifiers & DMU_TOP_OF_SIDEDEF)
        {
            elem = &updateSidedef->top();
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_MIDDLE_OF_SIDEDEF)
        {
            elem = &updateSidedef->middle();
            args->type = DMU_SURFACE;
        }
        else if(args->modifiers & DMU_BOTTOM_OF_SIDEDEF)
        {
            elem = &updateSidedef->bottom();
            args->type = DMU_SURFACE;
        }
    }

    if(args->type == DMU_PLANE)
    {
        updatePlane = elem->castTo<Plane>();

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
            elem = &elem->castTo<Plane>()->surface();
            args->type = DMU_SURFACE;
            break;

        default:
            break;
        }
    }

    if(args->type == DMU_SURFACE)
    {
        updateSurface = elem->castTo<Surface>();
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
        elem->castTo<Surface>()->setProperty(*args);
        break;

    case DMU_PLANE:
        elem->castTo<Plane>()->setProperty(*args);
        break;

    case DMU_VERTEX:
        elem->castTo<Vertex>()->setProperty(*args);
        break;

    case DMU_HEDGE:
        elem->castTo<HEdge>()->setProperty(*args);
        break;

    case DMU_LINEDEF:
        elem->castTo<LineDef>()->setProperty(*args);
        break;

    case DMU_SIDEDEF:
        elem->castTo<SideDef>()->setProperty(*args);
        break;

    case DMU_BSPLEAF:
        elem->castTo<BspLeaf>()->setProperty(*args);
        break;

    case DMU_SECTOR:
        elem->castTo<Sector>()->setProperty(*args);
        break;

    case DMU_MATERIAL:
        elem->castTo<Material>()->setProperty(*args);
        break;

    case DMU_BSPNODE: {
        /// @todo Throw exception.
        QByteArray msg = String("SetProperty: Property %1 is not writable in DMU_BSPNODE.").arg(DMU_Str(args->prop)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        break; }

    default: {
        /// @todo Throw exception.
        QByteArray msg = String("SetProperty: Type %1 not writable.").arg(DMU_Str(args->type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }

    if(updatePlane)
    {
        updateSector1 = &updatePlane->sector();
    }

    if(updateSector1)
    {
        R_UpdateSector(*updateSector1);
    }

    if(updateSector2)
    {
        R_UpdateSector(*updateSector2);
    }

    return true; // Continue iteration.
}

void DMU_GetValue(valuetype_t valueType, void const *src, setargs_t *args, uint index)
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
            // works with map elements. Failure leads into a fatal error.
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

static int getProperty(void *ptr, void *context)
{
    de::MapElement const *elem = IN_ELEM_CONST(ptr);
    setargs_t *args = (setargs_t *) context;

    // Dereference where necessary. Note the order, these cascade.
    if(args->type == DMU_BSPLEAF)
    {
        if(args->modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = elem->castTo<BspLeaf>()->sectorPtr();
            args->type = DMU_SECTOR;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = elem->castTo<BspLeaf>()->sectorPtr();
            args->type = DMU_SECTOR;
        }
        else
        {
            switch(args->prop)
            {
            case DMU_LIGHT_LEVEL:
            case DMT_MOBJS:
                elem = elem->castTo<BspLeaf>()->sectorPtr();
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
            elem = &elem->castTo<Sector>()->floor();
            args->type = DMU_PLANE;
        }
        else if(args->modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = &elem->castTo<Sector>()->ceiling();
            args->type = DMU_PLANE;
        }
    }

    if(args->type == DMU_LINEDEF)
    {
        if(args->modifiers & DMU_SIDEDEF0_OF_LINE)
        {
            elem = &elem->castTo<LineDef>()->frontSideDef();
            args->type = DMU_SIDEDEF;
            DENG2_ASSERT(args->type == elem->type());
        }
        else if(args->modifiers & DMU_SIDEDEF1_OF_LINE)
        {
            elem = &elem->castTo<LineDef>()->backSideDef();
            args->type = DMU_SIDEDEF;
            DENG2_ASSERT(args->type == elem->type());
        }
    }

    if(args->type == DMU_SIDEDEF)
    {
        if(args->modifiers & DMU_TOP_OF_SIDEDEF)
        {
            elem = &elem->castTo<SideDef>()->top();
            args->type = DMU_SURFACE;
            DENG2_ASSERT(args->type == elem->type());
        }
        else if(args->modifiers & DMU_MIDDLE_OF_SIDEDEF)
        {
            elem = &elem->castTo<SideDef>()->middle();
            args->type = DMU_SURFACE;
            DENG2_ASSERT(args->type == elem->type());
        }
        else if(args->modifiers & DMU_BOTTOM_OF_SIDEDEF)
        {
            elem = &elem->castTo<SideDef>()->bottom();
            args->type = DMU_SURFACE;
            DENG2_ASSERT(args->type == elem->type());
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
            elem = &elem->castTo<Plane>()->surface();
            args->type = DMU_SURFACE;
            DENG2_ASSERT(elem->type() == args->type);
            break;

        default:
            break;
        }
    }

    switch(args->type)
    {
    case DMU_VERTEX:
        elem->castTo<Vertex>()->property(*args);
        break;

    case DMU_HEDGE:
        elem->castTo<HEdge>()->property(*args);
        break;

    case DMU_LINEDEF:
        elem->castTo<LineDef>()->property(*args);
        break;

    case DMU_SURFACE:
        elem->castTo<Surface>()->property(*args);
        break;

    case DMU_PLANE:
        elem->castTo<Plane>()->property(*args);
        break;

    case DMU_SECTOR:
        elem->castTo<Sector>()->property(*args);
        break;

    case DMU_SIDEDEF:
        elem->castTo<SideDef>()->property(*args);
        break;

    case DMU_BSPLEAF:
        elem->castTo<BspLeaf>()->property(*args);
        break;

    case DMU_MATERIAL:
        elem->castTo<Material>()->property(*args);
        break;

    default: {
        QByteArray msg = String("GetProperty: Type %1 not readable.").arg(DMU_Str(args->type)).toUtf8();
        LegacyCore_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }

    // Currently no aggregate values are collected.
    return false;
}

#undef P_SetBool
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

// p_data.cpp
DENG_EXTERN_C boolean P_MapExists(char const* uriCString);
DENG_EXTERN_C boolean P_MapIsCustom(char const* uriCString);
DENG_EXTERN_C AutoStr* P_MapSourceFile(char const* uriCString);
DENG_EXTERN_C boolean P_LoadMap(char const* uriCString);
DENG_EXTERN_C uint P_CountGameMapObjs(int entityId);

// p_mapdata.cpp
DENG_EXTERN_C byte P_GetGMOByte(int entityId, uint elementIndex, int propertyId);
DENG_EXTERN_C short P_GetGMOShort(int entityId, uint elementIndex, int propertyId);
DENG_EXTERN_C int P_GetGMOInt(int entityId, uint elementIndex, int propertyId);
DENG_EXTERN_C fixed_t P_GetGMOFixed(int entityId, uint elementIndex, int propertyId);
DENG_EXTERN_C angle_t P_GetGMOAngle(int entityId, uint elementIndex, int propertyId);
DENG_EXTERN_C float P_GetGMOFloat(int entityId, uint elementIndex, int propertyId);

// p_maputil.cpp
DENG_EXTERN_C void P_MobjLink(mobj_t* mo, byte flags);
DENG_EXTERN_C int P_MobjUnlink(mobj_t* mo);
DENG_EXTERN_C int P_MobjLinesIterator(mobj_t* mo, int (*callback) (LineDef*, void*), void* parameters);
DENG_EXTERN_C int P_MobjSectorsIterator(mobj_t* mo, int (*callback) (Sector*, void*), void* parameters);
DENG_EXTERN_C int P_LineMobjsIterator(LineDef *line, int (*callback) (mobj_t *, void *), void *parameters);
DENG_EXTERN_C int P_SectorTouchingMobjsIterator(Sector* sector, int (*callback) (mobj_t*, void*), void *parameters);
DENG_EXTERN_C BspLeaf* P_BspLeafAtPointXY(coord_t x, coord_t y);
DENG_EXTERN_C BspLeaf* P_BspLeafAtPoint(coord_t const point[2]);
DENG_EXTERN_C int P_MobjsBoxIterator(const AABoxd* box, int (*callback) (mobj_t*, void*), void* parameters);
DENG_EXTERN_C int P_LinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters);
DENG_EXTERN_C int P_PolyobjsBoxIterator(const AABoxd* box, int (*callback) (Polyobj*, void*), void* parameters);
DENG_EXTERN_C int P_PolyobjLinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters);
DENG_EXTERN_C int P_AllLinesBoxIterator(const AABoxd* box, int (*callback) (LineDef*, void*), void* parameters);
DENG_EXTERN_C int P_BspLeafsBoxIterator(const AABoxd* box, Sector* sector, int (*callback) (BspLeaf*, void*), void* parameters);
DENG_EXTERN_C int P_PathTraverse2(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback, void* parameters);
DENG_EXTERN_C int P_PathTraverse(coord_t const from[2], coord_t const to[2], int flags, traverser_t callback/*parameters=NULL*/);
DENG_EXTERN_C int P_PathXYTraverse2(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags, traverser_t callback, void* paramaters);
DENG_EXTERN_C int P_PathXYTraverse(coord_t fromX, coord_t fromY, coord_t toX, coord_t toY, int flags, traverser_t callback/*parameters=NULL*/);

#undef P_CheckLineSight
DENG_EXTERN_C boolean P_CheckLineSight(const_pvec3d_t from, const_pvec3d_t to, coord_t bottomSlope,
    coord_t topSlope, int flags)
{
    if(!theMap) return false; // I guess?
    return LineSightTest(Vector3d(from), Vector3d(to),
                         dfloat(bottomSlope), dfloat(topSlope), flags).trace(*theMap->bspRoot());
}

DENG_EXTERN_C const divline_t* P_TraceLOS(void);
DENG_EXTERN_C TraceOpening const *P_TraceOpening(void);
DENG_EXTERN_C void P_SetTraceOpening(LineDef* linedef);

// p_mobj.c
DENG_EXTERN_C mobj_t* P_MobjCreateXYZ(thinkfunc_t function, coord_t x, coord_t y, coord_t z, angle_t angle, coord_t radius, coord_t height, int ddflags);
DENG_EXTERN_C void P_MobjDestroy(mobj_t* mo);
DENG_EXTERN_C void P_MobjDestroy(mobj_t* mobj);
DENG_EXTERN_C void P_MobjSetState(mobj_t* mobj, int statenum);
DENG_EXTERN_C angle_t Mobj_AngleSmoothed(mobj_t* mo);
DENG_EXTERN_C void Mobj_OriginSmoothed(mobj_t* mo, coord_t origin[3]);

// p_particle.c
DENG_EXTERN_C void P_SpawnDamageParticleGen(struct mobj_s* mo, struct mobj_s* inflictor, int amount);

// p_think.c
DENG_EXTERN_C struct mobj_s* P_MobjForID(int id);

// polyobjs.c
DENG_EXTERN_C boolean P_PolyobjMoveXY(Polyobj* polyobj, coord_t x, coord_t y);
DENG_EXTERN_C boolean P_PolyobjRotate(Polyobj* polyobj, angle_t angle);
DENG_EXTERN_C void P_PolyobjLink(Polyobj* polyobj);
DENG_EXTERN_C void P_PolyobjUnlink(Polyobj* polyobj);
DENG_EXTERN_C LineDef *P_PolyobjFirstLine(Polyobj *polyobj);
DENG_EXTERN_C Polyobj* P_PolyobjByID(uint id);
DENG_EXTERN_C Polyobj* P_PolyobjByTag(int tag);
DENG_EXTERN_C void P_SetPolyobjCallback(void (*func) (struct mobj_s*, void*, void*));

#undef LineDef_PointDistance
DENG_EXTERN_C coord_t LineDef_PointDistance(LineDef *line, coord_t const point[2], coord_t *offset)
{
    DENG_ASSERT(line);
    return line->pointDistance(point, offset);
}

#undef LineDef_PointXYDistance
DENG_EXTERN_C coord_t LineDef_PointXYDistance(LineDef* line, coord_t x, coord_t y, coord_t* offset)
{
    DENG_ASSERT(line);
    return line->pointDistance(x, y, offset);
}

#undef LineDef_PointOnSide
DENG_EXTERN_C coord_t LineDef_PointOnSide(LineDef const *line, coord_t const point[2])
{
    DENG_ASSERT(line);
    if(!point)
    {
        LOG_AS("LineDef_PointOnSide");
        LOG_DEBUG("Invalid arguments, returning >0.");
        return 1;
    }
    return line->pointOnSide(point);
}

#undef LineDef_PointXYOnSide
DENG_EXTERN_C coord_t LineDef_PointXYOnSide(LineDef const *line, coord_t x, coord_t y)
{
    DENG_ASSERT(line);
    return line->pointOnSide(x, y);
}

#undef LineDef_BoxOnSide
DENG_EXTERN_C int LineDef_BoxOnSide(LineDef *line, AABoxd const *box)
{
    DENG_ASSERT(line && box);
    return line->boxOnSide(*box);
}

#undef LineDef_BoxOnSide_FixedPrecision
DENG_EXTERN_C int LineDef_BoxOnSide_FixedPrecision(LineDef *line, AABoxd const *box)
{
    DENG_ASSERT(line && box);
    return line->boxOnSide_FixedPrecision(*box);
}

DENG_DECLARE_API(Map) =
{
    { DE_API_MAP },
    P_MapExists,
    P_MapIsCustom,
    P_MapSourceFile,
    P_LoadMap,

    LineDef_BoxOnSide,
    LineDef_BoxOnSide_FixedPrecision,
    LineDef_PointDistance,
    LineDef_PointXYDistance,
    LineDef_PointOnSide,
    LineDef_PointXYOnSide,
    P_LineMobjsIterator,

    P_SectorTouchingMobjsIterator,

    P_MobjCreateXYZ,
    P_MobjDestroy,
    P_MobjForID,
    P_MobjSetState,
    P_MobjLink,
    P_MobjUnlink,
    P_SpawnDamageParticleGen,
    P_MobjLinesIterator,
    P_MobjSectorsIterator,
    Mobj_OriginSmoothed,
    Mobj_AngleSmoothed,

    P_PolyobjMoveXY,
    P_PolyobjRotate,
    P_PolyobjLink,
    P_PolyobjUnlink,
    P_PolyobjFirstLine,
    P_PolyobjByID,
    P_PolyobjByTag,
    P_SetPolyobjCallback,

    P_BspLeafAtPoint,
    P_BspLeafAtPointXY,

    P_MobjsBoxIterator,
    P_LinesBoxIterator,
    P_AllLinesBoxIterator,
    P_PolyobjLinesBoxIterator,
    P_BspLeafsBoxIterator,
    P_PolyobjsBoxIterator,
    P_PathTraverse2,
    P_PathTraverse,
    P_PathXYTraverse2,
    P_PathXYTraverse,
    P_CheckLineSight,
    P_TraceLOS,
    P_TraceOpening,
    P_SetTraceOpening,

    DMU_GetType,
    P_ToIndex,
    P_ToPtr,
    P_Callback,
    P_Callbackp,
    P_Iteratep,
    P_AllocDummy,
    P_FreeDummy,
    P_IsDummy,
    P_DummyExtraData,
    P_CountGameMapObjs,
    P_GetGMOByte,
    P_GetGMOShort,
    P_GetGMOInt,
    P_GetGMOFixed,
    P_GetGMOAngle,
    P_GetGMOFloat,
    P_SetBool,
    P_SetByte,
    P_SetInt,
    P_SetFixed,
    P_SetAngle,
    P_SetFloat,
    P_SetDouble,
    P_SetPtr,
    P_SetBoolv,
    P_SetBytev,
    P_SetIntv,
    P_SetFixedv,
    P_SetAnglev,
    P_SetFloatv,
    P_SetDoublev,
    P_SetPtrv,
    P_SetBoolp,
    P_SetBytep,
    P_SetIntp,
    P_SetFixedp,
    P_SetAnglep,
    P_SetFloatp,
    P_SetDoublep,
    P_SetPtrp,
    P_SetBoolpv,
    P_SetBytepv,
    P_SetIntpv,
    P_SetFixedpv,
    P_SetAnglepv,
    P_SetFloatpv,
    P_SetDoublepv,
    P_SetPtrpv,
    P_GetBool,
    P_GetByte,
    P_GetInt,
    P_GetFixed,
    P_GetAngle,
    P_GetFloat,
    P_GetDouble,
    P_GetPtr,
    P_GetBoolv,
    P_GetBytev,
    P_GetIntv,
    P_GetFixedv,
    P_GetAnglev,
    P_GetFloatv,
    P_GetDoublev,
    P_GetPtrv,
    P_GetBoolp,
    P_GetBytep,
    P_GetIntp,
    P_GetFixedp,
    P_GetAnglep,
    P_GetFloatp,
    P_GetDoublep,
    P_GetPtrp,
    P_GetBoolpv,
    P_GetBytepv,
    P_GetIntpv,
    P_GetFixedpv,
    P_GetAnglepv,
    P_GetFloatpv,
    P_GetDoublepv,
    P_GetPtrpv
};
