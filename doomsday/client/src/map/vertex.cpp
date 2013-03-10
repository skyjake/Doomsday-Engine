/** @file vertex.cpp Map Geometry Vertex
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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"

#include "map/vertex.h"

using namespace de;

DENG2_PIMPL(Vertex)
{
    Instance(Public *i) : Base(i)
    {}
};

Vertex::Vertex(coord_t x, coord_t y) : MapElement(DMU_VERTEX)
{
    _origin[VX] = x;
    _origin[VY] = y;
    lineOwners = 0;
    numLineOwners = 0;
    std::memset(&buildData, 0, sizeof(buildData));
}

vec2d_t const &Vertex::origin() const
{
    return _origin;
}

void Vertex::countLineOwners(uint *oneSided, uint *twoSided) const
{
    if(!oneSided && !twoSided) return;

    uint ones = 0, twos = 0;
    if(lineOwners)
    {
        LineOwner *vo = lineOwners;
        do
        {
            if(!vo->lineDef().L_frontsidedef || !vo->lineDef().L_backsidedef)
            {
                ++ones;
            }
            else
            {
                ++twos;
            }
        } while((vo = &vo->next()) != lineOwners);
    }

    if(oneSided) *oneSided += ones;
    if(twoSided) *twoSided += twos;
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

int Vertex::setProperty(setargs_t const & args)
{
    /// @throw WritePropertyError  The requested property is not writable.
    throw WritePropertyError("Vertex::setProperty", QString("Property '%1' is not writable").arg(DMU_Str(args.prop)));
}
