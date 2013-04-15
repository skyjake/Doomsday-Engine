/** @file vertex.cpp World Map Vertex.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <de/vector1.h> /// @todo Remove me.
#include <de/Vector>

#include "dd_share.h"
#include "map/line.h"

#include "map/vertex.h"

using namespace de;

DENG2_PIMPL(Vertex)
{
    /// Original index in the archived map.
    uint origIndex;

    Instance(Public *i)
        : Base(i),
          origIndex(0)
    {}
};

Vertex::Vertex(Vector2d const &origin)
    : MapElement(DMU_VERTEX), d(new Instance(this))
{
    _origin[VX] = origin.x;
    _origin[VY] = origin.y;
    _lineOwners = 0;
    _numLineOwners = 0;
}

vec2d_t const &Vertex::origin() const
{
    return _origin;
}

uint Vertex::lineOwnerCount() const
{
    return _numLineOwners;
}

void Vertex::countLineOwners(uint *oneSided, uint *twoSided) const
{
    if(!oneSided && !twoSided) return;

    uint ones = 0, twos = 0;

    if(LineOwner const *firstOwn = firstLineOwner())
    {
        LineOwner const *own = firstOwn;
        do
        {
            if(!own->line().hasFrontSections() || !own->line().hasBackSections())
            {
                ++ones;
            }
            else
            {
                ++twos;
            }
        } while((own = &own->next()) != firstOwn);
    }

    if(oneSided) *oneSided += ones;
    if(twoSided) *twoSided += twos;
}

LineOwner *Vertex::firstLineOwner() const
{
    return _lineOwners;
}

uint Vertex::origIndex() const
{
    return d->origIndex;
}

void Vertex::setOrigIndex(uint newIndex)
{
    d->origIndex = newIndex;
}

int Vertex::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_X:
        DMU_GetValue(DMT_VERTEX_ORIGIN, &_origin[VX], &args, 0);
        break;
    case DMU_Y:
        DMU_GetValue(DMT_VERTEX_ORIGIN, &_origin[VY], &args, 0);
        break;
    case DMU_XY:
        DMU_GetValue(DMT_VERTEX_ORIGIN, &_origin[VX], &args, 0);
        DMU_GetValue(DMT_VERTEX_ORIGIN, &_origin[VY], &args, 1);
        break;
    default:
        /// @throw UnknownPropertyError  The requested property does not exist.
        throw UnknownPropertyError("Vertex::property", QString("Property '%1' is unknown").arg(DMU_Str(args.prop)));
    }
    return false; // Continue iteration.
}

int Vertex::setProperty(setargs_t const &args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError("Vertex::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
}
