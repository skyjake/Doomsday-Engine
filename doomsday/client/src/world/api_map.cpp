/** @file world/p_dmu.cpp Doomsday Map Update API.
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

#include <de/memoryzone.h>

#include "de_base.h"
#include "de_play.h"
#include "de_audio.h"

#include "Materials"
#include "api_map.h"

#include "world/dmuargs.h"

#ifdef __CLIENT__
#  include "render/lightgrid.h"
#endif

using namespace de;

// Converting a public void* pointer to an internal de::MapElement.
#define IN_ELEM(p)          reinterpret_cast<MapElement *>(p)
#define IN_ELEM_CONST(p)    reinterpret_cast<MapElement const *>(p)

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
class DummySector  : public Sector,  public DummyData {};

class DummyLine : public Line, public DummyData
{
public:
    DummyLine(DummyVertex &v1, DummyVertex &v2) : Line(v1, v2) {}
};

typedef QSet<MapElement *> Dummies;

static Dummies dummies;
static DummyVertex dummyVertex; // The one dummy vertex.

#undef DMU_Str
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
        { DMU_SEGMENT, "DMU_SEGMENT" },
        { DMU_LINE, "DMU_LINE" },
        { DMU_SIDE, "DMU_SIDE" },
        { DMU_BSPNODE, "DMU_BSPNODE" },
        { DMU_BSPLEAF, "DMU_BSPLEAF" },
        { DMU_SECTOR, "DMU_SECTOR" },
        { DMU_PLANE, "DMU_PLANE" },
        { DMU_SURFACE, "DMU_SURFACE" },
        { DMU_MATERIAL, "DMU_MATERIAL" },
        { DMU_LINE_BY_TAG, "DMU_LINE_BY_TAG" },
        { DMU_SECTOR_BY_TAG, "DMU_SECTOR_BY_TAG" },
        { DMU_LINE_BY_ACT_TAG, "DMU_LINE_BY_ACT_TAG" },
        { DMU_SECTOR_BY_ACT_TAG, "DMU_SECTOR_BY_ACT_TAG" },
        { DMU_ARCHIVE_INDEX, "DMU_ARCHIVE_INDEX" },
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
        { DMU_FRONT, "DMU_FRONT" },
        { DMU_BACK, "DMU_BACK" },
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
        { DMU_COLOR, "DMU_COLOR" },
        { DMU_COLOR_RED, "DMU_COLOR_RED" },
        { DMU_COLOR_GREEN, "DMU_COLOR_GREEN" },
        { DMU_COLOR_BLUE, "DMU_COLOR_BLUE" },
        { DMU_ALPHA, "DMU_ALPHA" },
        { DMU_LIGHT_LEVEL, "DMU_LIGHT_LEVEL" },
        { DMT_MOBJS, "DMT_MOBJS" },
        { DMU_BOUNDING_BOX, "DMU_BOUNDING_BOX" },
        { DMU_EMITTER, "DMU_EMITTER" },
        { DMU_WIDTH, "DMU_WIDTH" },
        { DMU_HEIGHT, "DMU_HEIGHT" },
        { DMU_TARGET_HEIGHT, "DMU_TARGET_HEIGHT" },
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

    MapElement const *elem = IN_ELEM_CONST(ptr);

    // Make sure it's valid.
    switch(elem->type())
    {
    case DMU_VERTEX:
    case DMU_SEGMENT:
    case DMU_LINE:
    case DMU_SIDE:
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

void Map::initDummies() // static
{
    // TODO: free existing/old dummies here?

    dummies.clear();
}

/**
 * Determines the type of a dummy object. For extra safety (in a debug build)
 * it would be possible to look through the dummy arrays and make sure the
 * pointer refers to a real dummy.
 */
static int dummyType(void const *dummy)
{
    MapElement const *elem = IN_ELEM_CONST(dummy);

    if(!dynamic_cast<DummyData const *>(elem))
    {
        // Not a dummy.
        return DMU_NONE;
    }

    DENG2_ASSERT(dummies.contains(const_cast<MapElement *>(elem)));

    return elem->type();
}

#undef P_AllocDummy
void *P_AllocDummy(int type, void *extraData)
{
    switch(type)
    {
    /*case DMU_SIDE: {
        DummySide *ds = new DummySide;
        dummies.insert(ds);
        ds->extraData = extraData;
        return ds; }*/

    case DMU_LINE: {
        DummyLine *dl = new DummyLine(dummyVertex, dummyVertex);
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
        App_FatalError(msg.constData());
        break; }
    }

    return 0; // Unreachable.
}

#undef P_IsDummy
boolean P_IsDummy(void const *dummy)
{
    return dummyType(dummy) != DMU_NONE;
}

#undef P_FreeDummy
void P_FreeDummy(void *dummy)
{
    MapElement *elem = IN_ELEM(dummy);

    int type = dummyType(dummy);
    if(type == DMU_NONE)
    {
        /// @todo Throw exception.
        App_FatalError("P_FreeDummy: Dummy is of unknown type.");
    }

    DENG2_ASSERT(dummies.contains(elem));

    dummies.remove(elem);
    delete elem;
}

#undef P_DummyExtraData
void *P_DummyExtraData(void *dummy)
{
    if(P_IsDummy(dummy))
    {
        MapElement *elem = IN_ELEM(dummy);
        return elem->castTo<DummyData>()->extraData;
    }
    return 0;
}

#undef P_ToIndex
int P_ToIndex(void const *ptr)
{
    if(!ptr) return -1;
    if(P_IsDummy(ptr)) return -1;

    MapElement const *elem = IN_ELEM_CONST(ptr);

    switch(elem->type())
    {
    case DMU_VERTEX:
    case DMU_SEGMENT:
    case DMU_LINE:
    case DMU_SIDE:
    case DMU_BSPLEAF:
    case DMU_SECTOR:
    case DMU_BSPNODE:
        return elem->indexInMap();

    case DMU_PLANE:
        return elem->castTo<Plane>()->inSectorIndex();

    case DMU_MATERIAL:
        return elem->castTo<Material>()->manifest().id(); // 1-based

    default:
        /// @todo Throw exception.
        DENG2_ASSERT(false); // Unknown/non-indexable DMU type.
        return -1;
    }
}

#undef P_ToPtr
void *P_ToPtr(int type, int index)
{
    switch(type)
    {
    case DMU_VERTEX:
        return theMap->vertexes().at(index);

    case DMU_SEGMENT:
        return theMap->segments().at(index);

    case DMU_LINE:
        return theMap->lines().at(index);

    case DMU_SIDE:
        return theMap->sideByIndex(index);

    case DMU_SECTOR:
        return theMap->sectors().at(index);

    case DMU_PLANE: {
        /// @todo Throw exception.
        QByteArray msg = String("P_ToPtr: Cannot convert %1 to a ptr (sector is unknown).").arg(DMU_Str(type)).toUtf8();
        App_FatalError(msg.constData());
        return 0; /* Unreachable. */ }

    case DMU_BSPLEAF:
        return theMap->bspLeafs().at(index);

    case DMU_BSPNODE:
        return theMap->bspNodes().at(index);

    case DMU_MATERIAL:
        /// @note @a index is 1-based.
        if(index > 0)
            return &App_Materials().toManifest(index).material();
        return 0;

    default: {
        /// @todo Throw exception.
        QByteArray msg = String("P_ToPtr: unknown type %1.").arg(DMU_Str(type)).toUtf8();
        App_FatalError(msg.constData());
        return 0; /* Unreachable. */ }
    }
}

#undef P_Count
int P_Count(int type)
{
    switch(type)
    {
    case DMU_VERTEX:    return theMap? theMap->vertexCount()  : 0;
    case DMU_SEGMENT:   return theMap? theMap->segmentCount() : 0;
    case DMU_LINE:      return theMap? theMap->lineCount()    : 0;
    case DMU_SIDE:      return theMap? theMap->sideCount()    : 0;
    case DMU_BSPNODE:   return theMap? theMap->bspNodeCount() : 0;
    case DMU_BSPLEAF:   return theMap? theMap->bspLeafCount() : 0;
    case DMU_SECTOR:    return theMap? theMap->sectorCount()  : 0;

    case DMU_MATERIAL:  return (int)App_Materials().count();

    default:
        /// @throw Invalid/unknown DMU element type.
        throw Error("P_Count", String("Unknown type %1").arg(DMU_Str(type)));
    }
}

#undef P_Iteratep
int P_Iteratep(void *elPtr, uint prop, void *context, int (*callback) (void *p, void *ctx))
{
    MapElement *elem = IN_ELEM(elPtr);

    switch(elem->type())
    {
    case DMU_SECTOR:
        switch(prop)
        {
        case DMU_LINE:
            foreach(Line::Side *side, elem->castTo<Sector>()->sides())
            {
                int result = callback(&side->line(), context);
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
        case DMU_SEGMENT:
            foreach(Segment *seg, elem->castTo<BspLeaf>()->allSegments())
            {
                int result = callback(seg, context);
                if(result) return result;
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

#undef P_Callback
int P_Callback(int type, int index, void *context, int (*callback)(void *p, void *ctx))
{
    switch(type)
    {
    case DMU_VERTEX:
        if(index >= 0 && index < theMap->vertexCount())
            return callback(theMap->vertexes().at(index), context);
        break;

    case DMU_SEGMENT:
        if(index >= 0 && index < theMap->segmentCount())
            return callback(theMap->segments().at(index), context);
        break;

    case DMU_LINE:
        if(index >= 0 && index < theMap->lineCount())
            return callback(theMap->lines().at(index), context);
        break;

    case DMU_SIDE: {
        Line::Side *side = theMap->sideByIndex(index);
        if(side)
            return callback(side, context);
        break; }

    case DMU_BSPNODE:
        if(index >= 0 && index < theMap->bspNodeCount())
            return callback(theMap->bspNodes().at(index), context);
        break;

    case DMU_BSPLEAF:
        if(index >= 0 && index < theMap->bspLeafCount())
            return callback(theMap->bspLeafs().at(index), context);
        break;

    case DMU_SECTOR:
        if(index >= 0 && index < theMap->sectorCount())
            return callback(theMap->sectors().at(index), context);
        break;

    case DMU_PLANE: {
        /// @todo Throw exception.
        QByteArray msg = String("P_Callback: %1 cannot be referenced by id alone (sector is unknown).").arg(DMU_Str(type)).toUtf8();
        App_FatalError(msg.constData());
        return 0; /* Unreachable */ }

    case DMU_MATERIAL:
        if(index > 0)
            return callback(&App_Materials().toManifest(materialid_t(index)).material(), context);
        break;

    case DMU_LINE_BY_TAG:
    case DMU_SECTOR_BY_TAG:
    case DMU_LINE_BY_ACT_TAG:
    case DMU_SECTOR_BY_ACT_TAG: {
        /// @todo Throw exception.
        QByteArray msg = String("P_Callback: Type %1 not implemented yet.").arg(DMU_Str(type)).toUtf8();
        App_FatalError(msg.constData());
        return 0; /* Unreachable */ }

    default: {
        /// @todo Throw exception.
        QByteArray msg = String("P_Callback: Type %1 unknown (index %2).").arg(DMU_Str(type)).arg(index).toUtf8();
        App_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }

    return false; // Continue iteration.
}

#undef P_Callbackp
int P_Callbackp(int type, void *elPtr, void *context, int (*callback)(void *p, void *ctx))
{
    MapElement *elem = IN_ELEM(elPtr);

    LOG_AS("P_Callbackp");

    switch(type)
    {
    case DMU_VERTEX:
    case DMU_SEGMENT:
    case DMU_LINE:
    case DMU_SIDE:
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
#ifdef DENG_DEBUG
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
        App_FatalError(msg.constData());
        return 0; /* Unreachable */ }
    }
    return false; // Continue iteration.
}

/**
 * Only those properties that are writable by outside parties (such as games)
 * are included here. Attempting to set a non-writable property causes a
 * fatal error.
 *
 * When a property changes, the relevant subsystems are notified of the change
 * so that they can update their state accordingly.
 */
static void setProperty(MapElement *elem, DmuArgs &args)
{
    DENG_ASSERT(elem != 0);

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
    if(args.type == DMU_BSPLEAF)
    {
        if(args.modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = elem->castTo<BspLeaf>()->sectorPtr();
            args.type = DMU_SECTOR;
        }
        else if(args.modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = elem->castTo<BspLeaf>()->sectorPtr();
            args.type = DMU_SECTOR;
        }
    }

    if(args.type == DMU_SECTOR)
    {
        if(args.modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = &elem->castTo<Sector>()->floor();
            args.type = DMU_PLANE;
        }
        else if(args.modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = &elem->castTo<Sector>()->ceiling();
            args.type = DMU_PLANE;
        }
    }

    if(args.type == DMU_LINE)
    {
        if(args.modifiers & DMU_FRONT_OF_LINE)
        {
            elem = &elem->castTo<Line>()->front();
            args.type = DMU_SIDE;
        }
        else if(args.modifiers & DMU_BACK_OF_LINE)
        {
            elem = &elem->castTo<Line>()->back();
            args.type = DMU_SIDE;
        }
    }

    if(args.type == DMU_SIDE)
    {
        if(args.modifiers & DMU_TOP_OF_SIDE)
        {
            elem = &elem->castTo<Line::Side>()->top();
            args.type = DMU_SURFACE;
        }
        else if(args.modifiers & DMU_MIDDLE_OF_SIDE)
        {
            elem = &elem->castTo<Line::Side>()->middle();
            args.type = DMU_SURFACE;
        }
        else if(args.modifiers & DMU_BOTTOM_OF_SIDE)
        {
            elem = &elem->castTo<Line::Side>()->bottom();
            args.type = DMU_SURFACE;
        }
    }

    if(args.type == DMU_PLANE)
    {
        switch(args.prop)
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
            args.type = DMU_SURFACE;
            break;

        default: break;
        }
    }

    // Write the property value(s).
    /// @throws MapElement::WritePropertyError  If the requested property is not writable.
    elem->setProperty(args);
}

static void getProperty(MapElement const *elem, DmuArgs &args)
{
    DENG_ASSERT(elem != 0);

    // Dereference where necessary. Note the order, these cascade.
    if(args.type == DMU_BSPLEAF)
    {
        if(args.modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = elem->castTo<BspLeaf>()->sectorPtr();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = elem->castTo<BspLeaf>()->sectorPtr();
            args.type = elem->type();
        }
        else
        {
            switch(args.prop)
            {
            case DMU_LIGHT_LEVEL:
            case DMT_MOBJS:
                elem = elem->castTo<BspLeaf>()->sectorPtr();
                args.type = elem->type();
                break;

            default: break;
            }
        }
    }

    if(args.type == DMU_SECTOR)
    {
        if(args.modifiers & DMU_FLOOR_OF_SECTOR)
        {
            elem = &elem->castTo<Sector>()->floor();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_CEILING_OF_SECTOR)
        {
            elem = &elem->castTo<Sector>()->ceiling();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_LINE)
    {
        if(args.modifiers & DMU_FRONT_OF_LINE)
        {
            elem = &elem->castTo<Line>()->front();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_BACK_OF_LINE)
        {
            elem = &elem->castTo<Line>()->back();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_SIDE)
    {
        if(args.modifiers & DMU_TOP_OF_SIDE)
        {
            elem = &elem->castTo<Line::Side>()->top();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_MIDDLE_OF_SIDE)
        {
            elem = &elem->castTo<Line::Side>()->middle();
            args.type = elem->type();
        }
        else if(args.modifiers & DMU_BOTTOM_OF_SIDE)
        {
            elem = &elem->castTo<Line::Side>()->bottom();
            args.type = elem->type();
        }
    }

    if(args.type == DMU_PLANE)
    {
        switch(args.prop)
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
            args.type = elem->type();
            break;

        default: break;
        }
    }

    // Read the property value(s).
    /// @throws MapElement::UnknownPropertyError  If the requested property is not readable.
    elem->property(args);

    // Currently no aggregate values are collected.
}

static int setPropertyWorker(void *elPtr, void *context)
{
    setProperty(IN_ELEM(elPtr), *reinterpret_cast<DmuArgs *>(context));
    return false; // Continue iteration.
}

#undef P_SetBool
void P_SetBool(int type, int index, uint prop, boolean param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetByte
void P_SetByte(int type, int index, uint prop, byte param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetInt
void P_SetInt(int type, int index, uint prop, int param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetFixed
void P_SetFixed(int type, int index, uint prop, fixed_t param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetAngle
void P_SetAngle(int type, int index, uint prop, angle_t param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetFloat
void P_SetFloat(int type, int index, uint prop, float param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetDouble
void P_SetDouble(int type, int index, uint prop, double param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &param;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetPtr
void P_SetPtr(int type, int index, uint prop, void *param)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetBoolv
void P_SetBoolv(int type, int index, uint prop, boolean *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetBytev
void P_SetBytev(int type, int index, uint prop, byte *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetIntv
void P_SetIntv(int type, int index, uint prop, int *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetFixedv
void P_SetFixedv(int type, int index, uint prop, fixed_t *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetAnglev
void P_SetAnglev(int type, int index, uint prop, angle_t *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetFloatv
void P_SetFloatv(int type, int index, uint prop, float *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetDoublev
void P_SetDoublev(int type, int index, uint prop, double *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callback(type, index, &args, setPropertyWorker);
}

#undef P_SetPtrv
void P_SetPtrv(int type, int index, uint prop, void *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callback(type, index, &args, setPropertyWorker);
}

/* pointer-based write functions */

#undef P_SetBoolp
void P_SetBoolp(void *ptr, uint prop, boolean param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    // Make sure invalid values are not allowed.
    param = (param? true : false);
    args.booleanValues = &param;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetBytep
void P_SetBytep(void *ptr, uint prop, byte param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &param;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetIntp
void P_SetIntp(void *ptr, uint prop, int param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = &param;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetFixedp
void P_SetFixedp(void *ptr, uint prop, fixed_t param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &param;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetAnglep
void P_SetAnglep(void *ptr, uint prop, angle_t param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &param;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetFloatp
void P_SetFloatp(void *ptr, uint prop, float param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &param;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetDoublep
void P_SetDoublep(void *ptr, uint prop, double param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &param;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetPtrp
void P_SetPtrp(void *ptr, uint prop, void *param)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &param;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetBoolpv
void P_SetBoolpv(void *ptr, uint prop, boolean *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetBytepv
void P_SetBytepv(void *ptr, uint prop, byte *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetIntpv
void P_SetIntpv(void *ptr, uint prop, int *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetFixedpv
void P_SetFixedpv(void *ptr, uint prop, fixed_t *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetAnglepv
void P_SetAnglepv(void *ptr, uint prop, angle_t *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetFloatpv
void P_SetFloatpv(void *ptr, uint prop, float *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetDoublepv
void P_SetDoublepv(void *ptr, uint prop, double *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

#undef P_SetPtrpv
void P_SetPtrpv(void *ptr, uint prop, void *params)
{
    DmuArgs args(DMU_GetType(ptr), prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callbackp(args.type, ptr, &args, setPropertyWorker);
}

static int getPropertyWorker(void *elPtr, void *context)
{
    getProperty(IN_ELEM_CONST(elPtr), *reinterpret_cast<DmuArgs *>(context));
    return false; // Continue iteration.
}

/* index-based read functions */

#undef P_GetBool
boolean P_GetBool(int type, int index, uint prop)
{
    boolean returnValue = false;
    DmuArgs args(type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = &returnValue;
    P_Callback(type, index, &args, getPropertyWorker);
    return returnValue;
}

#undef P_GetByte
byte P_GetByte(int type, int index, uint prop)
{
    byte returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = &returnValue;
    P_Callback(type, index, &args, getPropertyWorker);
    return returnValue;
}

#undef P_GetInt
int P_GetInt(int type, int index, uint prop)
{
    int returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_INT;
    args.intValues = &returnValue;
    P_Callback(type, index, &args, getPropertyWorker);
    return returnValue;
}

#undef P_GetFixed
fixed_t P_GetFixed(int type, int index, uint prop)
{
    fixed_t returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = &returnValue;
    P_Callback(type, index, &args, getPropertyWorker);
    return returnValue;
}

#undef P_GetAngle
angle_t P_GetAngle(int type, int index, uint prop)
{
    angle_t returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = &returnValue;
    P_Callback(type, index, &args, getPropertyWorker);
    return returnValue;
}

#undef P_GetFloat
float P_GetFloat(int type, int index, uint prop)
{
    float returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = &returnValue;
    P_Callback(type, index, &args, getPropertyWorker);
    return returnValue;
}

#undef P_GetDouble
double P_GetDouble(int type, int index, uint prop)
{
    double returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = &returnValue;
    P_Callback(type, index, &args, getPropertyWorker);
    return returnValue;
}

#undef P_GetPtr
void *P_GetPtr(int type, int index, uint prop)
{
    void *returnValue = 0;
    DmuArgs args(type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = &returnValue;
    P_Callback(type, index, &args, getPropertyWorker);
    return returnValue;
}

#undef P_GetBoolv
void P_GetBoolv(int type, int index, uint prop, boolean *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BOOL;
    args.booleanValues = params;
    P_Callback(type, index, &args, getPropertyWorker);
}

#undef P_GetBytev
void P_GetBytev(int type, int index, uint prop, byte *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_BYTE;
    args.byteValues = params;
    P_Callback(type, index, &args, getPropertyWorker);
}

#undef P_GetIntv
void P_GetIntv(int type, int index, uint prop, int *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_INT;
    args.intValues = params;
    P_Callback(type, index, &args, getPropertyWorker);
}

#undef P_GetFixedv
void P_GetFixedv(int type, int index, uint prop, fixed_t *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FIXED;
    args.fixedValues = params;
    P_Callback(type, index, &args, getPropertyWorker);
}

#undef P_GetAnglev
void P_GetAnglev(int type, int index, uint prop, angle_t *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_ANGLE;
    args.angleValues = params;
    P_Callback(type, index, &args, getPropertyWorker);
}

#undef P_GetFloatv
void P_GetFloatv(int type, int index, uint prop, float *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_FLOAT;
    args.floatValues = params;
    P_Callback(type, index, &args, getPropertyWorker);
}

#undef P_GetDoublev
void P_GetDoublev(int type, int index, uint prop, double *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_DOUBLE;
    args.doubleValues = params;
    P_Callback(type, index, &args, getPropertyWorker);
}

#undef P_GetPtrv
void P_GetPtrv(int type, int index, uint prop, void *params)
{
    DmuArgs args(type, prop);
    args.valueType = DDVT_PTR;
    args.ptrValues = (void **)params;
    P_Callback(type, index, &args, getPropertyWorker);
}

/* pointer-based read functions */

#undef P_GetBoolp
boolean P_GetBoolp(void *ptr, uint prop)
{
    boolean returnValue = false;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }

    return returnValue;
}

#undef P_GetBytep
byte P_GetBytep(void *ptr, uint prop)
{
    byte returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }

    return returnValue;
}

#undef P_GetIntp
int P_GetIntp(void *ptr, uint prop)
{
    int returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }

    return returnValue;
}

#undef P_GetFixedp
fixed_t P_GetFixedp(void *ptr, uint prop)
{
    fixed_t returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }

    return returnValue;
}

#undef P_GetAnglep
angle_t P_GetAnglep(void *ptr, uint prop)
{
    angle_t returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }

    return returnValue;
}

#undef P_GetFloatp
float P_GetFloatp(void *ptr, uint prop)
{
    float returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }

    return returnValue;
}

#undef P_GetDoublep
double P_GetDoublep(void *ptr, uint prop)
{
    double returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_DOUBLE;
        args.doubleValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }

    return returnValue;
}

#undef P_GetPtrp
void *P_GetPtrp(void *ptr, uint prop)
{
    void *returnValue = 0;

    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = &returnValue;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }

    return returnValue;
}

#undef P_GetBoolpv
void P_GetBoolpv(void *ptr, uint prop, boolean *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_BOOL;
        args.booleanValues = params;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }
}

#undef P_GetBytepv
void P_GetBytepv(void *ptr, uint prop, byte *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_BYTE;
        args.byteValues = params;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }
}

#undef P_GetIntpv
void P_GetIntpv(void *ptr, uint prop, int *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_INT;
        args.intValues = params;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }
}

#undef P_GetFixedpv
void P_GetFixedpv(void *ptr, uint prop, fixed_t *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_FIXED;
        args.fixedValues = params;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }
}

#undef P_GetAnglepv
void P_GetAnglepv(void *ptr, uint prop, angle_t *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_ANGLE;
        args.angleValues = params;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }
}

#undef P_GetFloatpv
void P_GetFloatpv(void *ptr, uint prop, float *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_FLOAT;
        args.floatValues = params;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }
}

#undef P_GetDoublepv
void P_GetDoublepv(void *ptr, uint prop, double *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_DOUBLE;
        args.doubleValues = params;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }
}

#undef P_GetPtrpv
void P_GetPtrpv(void *ptr, uint prop, void *params)
{
    if(ptr)
    {
        DmuArgs args(DMU_GetType(ptr), prop);
        args.valueType = DDVT_PTR;
        args.ptrValues = (void **)params;
        P_Callbackp(args.type, ptr, &args, getPropertyWorker);
    }
}

// p_data.cpp
DENG_EXTERN_C boolean P_MapExists(char const* uriCString);

DENG_EXTERN_C boolean P_MapIsCustom(char const* uriCString);
DENG_EXTERN_C AutoStr* P_MapSourceFile(char const* uriCString);
DENG_EXTERN_C boolean P_LoadMap(char const* uriCString);
DENG_EXTERN_C uint P_CountMapObjs(int entityId);

// p_mapdata.cpp
DENG_EXTERN_C byte P_GetGMOByte(int entityId, int elementIndex, int propertyId);
DENG_EXTERN_C short P_GetGMOShort(int entityId, int elementIndex, int propertyId);
DENG_EXTERN_C int P_GetGMOInt(int entityId, int elementIndex, int propertyId);
DENG_EXTERN_C fixed_t P_GetGMOFixed(int entityId, int elementIndex, int propertyId);
DENG_EXTERN_C angle_t P_GetGMOAngle(int entityId, int elementIndex, int propertyId);
DENG_EXTERN_C float P_GetGMOFloat(int entityId, int elementIndex, int propertyId);

// p_maputil.cpp
DENG_EXTERN_C void P_MobjLink(mobj_t* mo, byte flags);
DENG_EXTERN_C int P_MobjUnlink(mobj_t* mo);
DENG_EXTERN_C int P_MobjLinesIterator(mobj_t* mo, int (*callback) (Line*, void*), void* parameters);
DENG_EXTERN_C int P_MobjSectorsIterator(mobj_t* mo, int (*callback) (Sector*, void*), void* parameters);
DENG_EXTERN_C int P_LineMobjsIterator(Line *line, int (*callback) (mobj_t *, void *), void *parameters);
DENG_EXTERN_C int P_SectorTouchingMobjsIterator(Sector* sector, int (*callback) (mobj_t*, void*), void *parameters);
DENG_EXTERN_C BspLeaf* P_BspLeafAtPoint_FixedPrecisionXY(coord_t x, coord_t y);
DENG_EXTERN_C BspLeaf* P_BspLeafAtPoint_FixedPrecision(coord_t const point[2]);
DENG_EXTERN_C int P_MobjsBoxIterator(const AABoxd* box, int (*callback) (mobj_t*, void*), void* parameters);
DENG_EXTERN_C int P_LinesBoxIterator(const AABoxd* box, int (*callback) (Line*, void*), void* parameters);
DENG_EXTERN_C int P_PolyobjsBoxIterator(const AABoxd* box, int (*callback) (Polyobj*, void*), void* parameters);
DENG_EXTERN_C int P_PolyobjLinesBoxIterator(const AABoxd* box, int (*callback) (Line*, void*), void* parameters);
DENG_EXTERN_C int P_AllLinesBoxIterator(const AABoxd* box, int (*callback) (Line*, void*), void* parameters);
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
DENG_EXTERN_C void P_SetTraceOpening(Line* line);

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

#undef P_SetPolyobjCallback
DENG_EXTERN_C void P_SetPolyobjCallback(void (*func) (struct mobj_s *, void *, void *))
{
    Polyobj::setCollisionCallback(func);
}

#undef P_PolyobjUnlink
DENG_EXTERN_C void P_PolyobjUnlink(Polyobj *po)
{
    if(!po) return;
    po->unlink();
}

#undef P_PolyobjLink
DENG_EXTERN_C void P_PolyobjLink(Polyobj *po)
{
    if(!po) return;
    po->link();
}

#undef P_PolyobjByID
DENG_EXTERN_C Polyobj *P_PolyobjByID(int index)
{
    if(!theMap) return 0;
    return theMap->polyobjs().at(index);
}

#undef P_PolyobjByTag
DENG_EXTERN_C Polyobj *P_PolyobjByTag(int tag)
{
    if(!theMap) return 0;
    return theMap->polyobjByTag(tag);
}

#undef P_PolyobjMove
DENG_EXTERN_C boolean P_PolyobjMove(Polyobj *po, const_pvec3d_t xy)
{
    if(!po) return false;
    return po->move(xy);
}

#undef P_PolyobjMoveXY
DENG_EXTERN_C boolean P_PolyobjMoveXY(Polyobj *po, coord_t x, coord_t y)
{
    if(!po) return false;
    return po->move(x, y);
}

#undef P_PolyobjRotate
DENG_EXTERN_C boolean P_PolyobjRotate(Polyobj *po, angle_t angle)
{
    if(!po) return false;
    return po->rotate(angle);
}

#undef P_PolyobjFirstLine
DENG_EXTERN_C Line *P_PolyobjFirstLine(Polyobj *po)
{
    if(!po) return 0;
    return po->lines()[0];
}

#undef Line_PointDistance
DENG_EXTERN_C coord_t Line_PointDistance(Line *line, coord_t const point[2], coord_t *offset)
{
    DENG_ASSERT(line);
    return line->pointDistance(point, offset);
}

#undef Line_PointXYDistance
DENG_EXTERN_C coord_t Line_PointXYDistance(Line* line, coord_t x, coord_t y, coord_t* offset)
{
    DENG_ASSERT(line);
    return line->pointDistance(x, y, offset);
}

#undef Line_PointOnSide
DENG_EXTERN_C coord_t Line_PointOnSide(Line const *line, coord_t const point[2])
{
    DENG_ASSERT(line);
    if(!point)
    {
        LOG_AS("Line_PointOnSide");
        LOG_DEBUG("Invalid arguments, returning >0.");
        return 1;
    }
    return line->pointOnSide(point);
}

#undef Line_PointXYOnSide
DENG_EXTERN_C coord_t Line_PointXYOnSide(Line const *line, coord_t x, coord_t y)
{
    DENG_ASSERT(line);
    return line->pointOnSide(x, y);
}

#undef Line_BoxOnSide
DENG_EXTERN_C int Line_BoxOnSide(Line *line, AABoxd const *box)
{
    DENG_ASSERT(line && box);
    return line->boxOnSide(*box);
}

#undef Line_BoxOnSide_FixedPrecision
DENG_EXTERN_C int Line_BoxOnSide_FixedPrecision(Line *line, AABoxd const *box)
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

    Line_BoxOnSide,
    Line_BoxOnSide_FixedPrecision,
    Line_PointDistance,
    Line_PointXYDistance,
    Line_PointOnSide,
    Line_PointXYOnSide,
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

    P_BspLeafAtPoint_FixedPrecision,
    P_BspLeafAtPoint_FixedPrecisionXY,

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

    DMU_Str,
    DMU_GetType,
    P_ToIndex,
    P_ToPtr,
    P_Count,
    P_Callback,
    P_Callbackp,
    P_Iteratep,
    P_AllocDummy,
    P_FreeDummy,
    P_IsDummy,
    P_DummyExtraData,
    P_CountMapObjs,
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
