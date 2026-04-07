/** @file mapelement.cpp  Base class for all map elements.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013-2016 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/mapelement.h"
#include "doomsday/world/map.h"

using namespace de;

namespace world {

DE_PIMPL_NOREF(MapElement)
{
    dint type;
    Map *map            = nullptr;
    dint indexInMap     = NoIndex;
    dint indexInArchive = NoIndex;

    Impl(dint type) : type(type) {}
};

MapElement::MapElement(dint type, MapElement *parent)
    : d(new Impl(type))
{
    setParent(parent);
}

MapElement::~MapElement()
{}

dint MapElement::type() const
{
    return d->type;
}

void MapElement::setParent(MapElement *newParent)
{
    if (newParent == this)
    {
        /// @throw InvalidParentError  Attempted to attribute *this* element as parent of itself.
        throw InvalidParentError("MapElement::setParent", "Cannot attribute 'this' map element as a parent of itself");
    }
    _parent = newParent;
}

bool MapElement::hasMap() const
{
    // If a parent is configured this property is delegated to the parent.
    if (_parent)
    {
        return _parent->hasMap();
    }
    return d->map != nullptr;
}

Map &MapElement::map() const
{
    // If a parent is configured this property is delegated to the parent.
    if (_parent)
    {
        return _parent->map();
    }
    if (d->map)
    {
        return *d->map;
    }
    /// @throw MissingMapError  Attempted with no map attributed.
    throw MissingMapError("MapElement::map", "No map is attributed");
}

void MapElement::setMap(Map *newMap)
{
    // If a parent is configured this property is delegated to the parent.
    if (!_parent)
    {
        d->map = newMap;
        return;
    }
    /// @throw WritePropertyError  Attempted to change a delegated property.
    throw WritePropertyError("MapElement::setMap", "The 'map' property has been delegated");
}

dint MapElement::indexInMap() const
{
    return d->indexInMap;
}

void MapElement::setIndexInMap(dint newIndex)
{
    d->indexInMap = newIndex;
}

dint MapElement::indexInArchive() const
{
    return d->indexInArchive;
}

void MapElement::setIndexInArchive(dint newIndex)
{
    d->indexInArchive = newIndex;
}

dint MapElement::property(DmuArgs &args) const
{
    switch (args.prop)
    {
    case DMU_ARCHIVE_INDEX:
        args.setValue(DMT_ARCHIVE_INDEX, &d->indexInArchive, 0);
        break;

    default:
        /// @throw UnknownPropertyError  The requested property is not readable.
        throw UnknownPropertyError(stringf("%s::property", DMU_Str(d->type)),
                                   stringf("'%s' is unknown/not readable", DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

dint MapElement::setProperty(const DmuArgs &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError(stringf("%s::setProperty", DMU_Str(d->type)),
                             stringf("'%s' is unknown/not writable", DMU_Str(args.prop)));
}

}  // namespace world

const char *DMU_Str(uint prop)
{
    static char propStr[40];

    struct prop_s {
        uint prop;
        const char *str;
    } props[] =
    {
        { DMU_NONE,              "(invalid)" },
        { DMU_VERTEX,            "DMU_VERTEX" },
        { DMU_SEGMENT,           "DMU_SEGMENT" },
        { DMU_LINE,              "DMU_LINE" },
        { DMU_SIDE,              "DMU_SIDE" },
        { DMU_SUBSPACE,          "DMU_SUBSPACE" },
        { DMU_SECTOR,            "DMU_SECTOR" },
        { DMU_PLANE,             "DMU_PLANE" },
        { DMU_SURFACE,           "DMU_SURFACE" },
        { DMU_MATERIAL,          "DMU_MATERIAL" },
        { DMU_SKY,               "DMU_SKY" },
        { DMU_LINE_BY_TAG,       "DMU_LINE_BY_TAG" },
        { DMU_SECTOR_BY_TAG,     "DMU_SECTOR_BY_TAG" },
        { DMU_LINE_BY_ACT_TAG,   "DMU_LINE_BY_ACT_TAG" },
        { DMU_SECTOR_BY_ACT_TAG, "DMU_SECTOR_BY_ACT_TAG" },
        { DMU_ARCHIVE_INDEX,     "DMU_ARCHIVE_INDEX" },
        { DMU_X,                 "DMU_X" },
        { DMU_Y,                 "DMU_Y" },
        { DMU_XY,                "DMU_XY" },
        { DMU_TANGENT_X,         "DMU_TANGENT_X" },
        { DMU_TANGENT_Y,         "DMU_TANGENT_Y" },
        { DMU_TANGENT_Z,         "DMU_TANGENT_Z" },
        { DMU_TANGENT_XYZ,       "DMU_TANGENT_XYZ" },
        { DMU_BITANGENT_X,       "DMU_BITANGENT_X" },
        { DMU_BITANGENT_Y,       "DMU_BITANGENT_Y" },
        { DMU_BITANGENT_Z,       "DMU_BITANGENT_Z" },
        { DMU_BITANGENT_XYZ,     "DMU_BITANGENT_XYZ" },
        { DMU_NORMAL_X,          "DMU_NORMAL_X" },
        { DMU_NORMAL_Y,          "DMU_NORMAL_Y" },
        { DMU_NORMAL_Z,          "DMU_NORMAL_Z" },
        { DMU_NORMAL_XYZ,        "DMU_NORMAL_XYZ" },
        { DMU_VERTEX0,           "DMU_VERTEX0" },
        { DMU_VERTEX1,           "DMU_VERTEX1" },
        { DMU_FRONT,             "DMU_FRONT" },
        { DMU_BACK,              "DMU_BACK" },
        { DMU_FLAGS,             "DMU_FLAGS" },
        { DMU_DX,                "DMU_DX" },
        { DMU_DY,                "DMU_DY" },
        { DMU_DXY,               "DMU_DXY" },
        { DMU_LENGTH,            "DMU_LENGTH" },
        { DMU_SLOPETYPE,         "DMU_SLOPETYPE" },
        { DMU_ANGLE,             "DMU_ANGLE" },
        { DMU_OFFSET,            "DMU_OFFSET" },
        { DMU_OFFSET_X,          "DMU_OFFSET_X" },
        { DMU_OFFSET_Y,          "DMU_OFFSET_Y" },
        { DMU_OFFSET_XY,         "DMU_OFFSET_XY" },
        { DMU_BLENDMODE,         "DMU_BLENDMODE" },
        { DMU_VALID_COUNT,       "DMU_VALID_COUNT" },
        { DMU_COLOR,             "DMU_COLOR" },
        { DMU_COLOR_RED,         "DMU_COLOR_RED" },
        { DMU_COLOR_GREEN,       "DMU_COLOR_GREEN" },
        { DMU_COLOR_BLUE,        "DMU_COLOR_BLUE" },
        { DMU_ALPHA,             "DMU_ALPHA" },
        { DMU_LIGHT_LEVEL,       "DMU_LIGHT_LEVEL" },
        { DMT_MOBJS,             "DMT_MOBJS" },
        { DMU_BOUNDING_BOX,      "DMU_BOUNDING_BOX" },
        { DMU_EMITTER,           "DMU_EMITTER" },
        { DMU_WIDTH,             "DMU_WIDTH" },
        { DMU_HEIGHT,            "DMU_HEIGHT" },
        { DMU_TARGET_HEIGHT,     "DMU_TARGET_HEIGHT" },
        { DMU_SPEED,             "DMU_SPEED" },
        { DMU_FLOOR_PLANE,       "DMU_FLOOR_PLANE" },
        { DMU_CEILING_PLANE,     "DMU_CEILING_PLANE" },
        { 0, NULL }
    };

    for (uint i = 0; props[i].str; ++i)
    {
        if (props[i].prop == prop)
            return props[i].str;
    }

    dd_snprintf(propStr, 40, "(unnamed %i)", prop);
    return propStr;
}
