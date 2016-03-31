/** @file mapelement.cpp  Base class for all map elements.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"
#include "world/mapelement.h"

#include "world/map.h"

using namespace de;

namespace world {

DENG2_PIMPL_NOREF(MapElement)
{
    dint type;
    MapElement *parent  = nullptr;
    Map *map            = nullptr;
    dint indexInMap     = NoIndex;
    dint indexInArchive = NoIndex;

    Instance(dint type) : type(type) {}
};

MapElement::MapElement(dint type, MapElement *parent)
    : d(new Instance(type))
{
    setParent(parent);
}

MapElement::~MapElement()
{}

dint MapElement::type() const
{
    return d->type;
}

bool MapElement::hasParent() const
{
    return d->parent != nullptr;
}

MapElement &MapElement::parent()
{
    return const_cast<MapElement &>(const_cast<MapElement const *>(this)->parent());
}

MapElement const &MapElement::parent() const
{
    if(d->parent) return *d->parent;
    /// @throw MissingParentError  Attempted with no parent element is attributed.
    throw MissingParentError("MapElement::parent", "No parent map element is attributed");
}

void MapElement::setParent(MapElement *newParent)
{
    if(newParent == this)
    {
        /// @throw InvalidParentError  Attempted to attribute *this* element as parent of itself.
        throw InvalidParentError("MapElement::setParent", "Cannot attribute 'this' map element as a parent of itself");
    }
    d->parent = newParent;
}

bool MapElement::hasMap() const
{
    // If a parent is configured this property is delegated to the parent.
    if(d->parent)
    {
        return d->parent->hasMap();
    }
    return d->map != nullptr;
}

Map &MapElement::map() const
{
    // If a parent is configured this property is delegated to the parent.
    if(d->parent)
    {
        return d->parent->map();
    }
    if(d->map)
    {
        return *d->map;
    }
    /// @throw MissingMapError  Attempted with no map attributed.
    throw MissingMapError("MapElement::map", "No map is attributed");
}

void MapElement::setMap(Map *newMap)
{
    // If a parent is configured this property is delegated to the parent.
    if(!d->parent)
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
    switch(args.prop)
    {
    case DMU_ARCHIVE_INDEX:
        args.setValue(DMT_ARCHIVE_INDEX, &d->indexInArchive, 0);
        break;

    default:
        /// @throw UnknownPropertyError  The requested property is not readable.
        throw UnknownPropertyError(QString("%1::property").arg(DMU_Str(d->type)),
                                   QString("'%1' is unknown/not readable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

dint MapElement::setProperty(DmuArgs const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError(QString("%1::setProperty").arg(DMU_Str(d->type)),
                             QString("'%1' is unknown/not writable").arg(DMU_Str(args.prop)));
}

}  // namespace world
