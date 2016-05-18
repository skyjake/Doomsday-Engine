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

MapLink::MapLink(Map *map)
{
    setMap(map);
}

bool MapLink::hasMap() const
{
    return _map != nullptr;
}

Map &MapLink::map() const
{
    if(hasMap()) return *_map;
    /// @throw MissingMapError  Attempted with no map attributed.
    throw MissingMapError("MapLink::map", "No map is attributed");
}

Map *MapLink::mapPtr() const
{
    return hasMap() ? &map() : nullptr;
}

void MapLink::setMap(Map *newMap)
{
    _map = newMap;
}

//---------------------------------------------------------------------------------------

MapElement::MapElement(dint dmuType, Map *map)
    : DmuObject(dmuType)
    , MapLink(map)
{}

}  // namespace world

//---------------------------------------------------------------------------------------

namespace de {

DENG2_PIMPL_NOREF(DmuObject)
{
    dint type;
    DmuObject *parent = nullptr;

    dint indexInMap = NoIndex;
    dint indexInArchive = NoIndex;

    Instance(dint type) : type(type) {}
};

DmuObject::DmuObject(dint type, dint indexInMap)
    : d(new Instance(type))
{
    setIndexInMap(indexInMap);
}

dint DmuObject::type() const
{
    return d->type;
}

String DmuObject::describe() const
{
    return "abstract DmuObject";
}

String DmuObject::description(dint /*verbosity*/) const
{
    String desc = describe();
    if(indexInMap() != NoIndex)
    {
        desc += String(" #%1").arg(indexInMap());
    }
    return desc;
}

bool DmuObject::hasParent() const
{
    return d->parent != nullptr;
}

DmuObject &DmuObject::parent()
{
    return const_cast<DmuObject &>(const_cast<DmuObject const *>(this)->parent());
}

DmuObject const &DmuObject::parent() const
{
    if(d->parent) return *d->parent;
    /// @throw MissingParentError  Attempted with no parent element is attributed.
    throw MissingParentError("DmuObject::parent", "No parent map element is attributed");
}

void DmuObject::setParent(DmuObject *newParent)
{
    if(newParent == this)
    {
        /// @throw InvalidParentError  Attempted to attribute *this* element as parent of itself.
        throw InvalidParentError("DmuObject::setParent", "Cannot attribute 'this' map element as a parent of itself");
    }
    d->parent = newParent;
}

dint DmuObject::indexInMap() const
{
    return d->indexInMap;
}

void DmuObject::setIndexInMap(dint newIndex)
{
    d->indexInMap = newIndex;
}

dint DmuObject::indexInArchive() const
{
    return d->indexInArchive;
}

void DmuObject::setIndexInArchive(dint newIndex)
{
    d->indexInArchive = newIndex;
}

dint DmuObject::property(DmuArgs &args) const
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

dint DmuObject::setProperty(DmuArgs const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError(QString("%1::setProperty").arg(DMU_Str(d->type)),
                             QString("'%1' is unknown/not writable").arg(DMU_Str(args.prop)));
}

}  // namespace de
