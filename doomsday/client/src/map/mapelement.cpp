/** @file map/mapelement.cpp Base class for all map elements.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "map/mapelement.h"

using namespace de;

int MapElement::type() const
{
    return _type;
}

int MapElement::indexInArchive() const
{
    return _indexInArchive;
}

void MapElement::setIndexInArchive(int newIndex)
{
    _indexInArchive = newIndex;
}

int MapElement::indexInMap() const
{
    return _indexInMap;
}

void MapElement::setIndexInMap(int newIndex)
{
    _indexInMap = newIndex;
}

MapElement &MapElement::operator = (MapElement const &other)
{
    _type = other._type;
    // We retain our current indexes.
    return *this;
}

int MapElement::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_ARCHIVE_INDEX:
        DMU_GetValue(DMT_ARCHIVE_INDEX, &_indexInArchive, &args, 0);
        break;

    default:
        /// @throw UnknownPropertyError  The requested property is not readable.
        throw UnknownPropertyError(QString("%1::property").arg(DMU_Str(_type)),
                                   QString("Property '%1' is unknown/not readable").arg(DMU_Str(args.prop)));
    }

    return false; // Continue iteration.
}

int MapElement::setProperty(setargs_t const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError(QString("%1::setProperty").arg(DMU_Str(_type)),
                             QString("Property '%1' is unknown/not writable").arg(DMU_Str(args.prop)));
}
