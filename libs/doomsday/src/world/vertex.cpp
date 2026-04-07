/** @file vertex.cpp  World map vertex.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/world/vertex.h"
#include "doomsday/world/line.h"
#include "doomsday/world/lineowner.h"  /// @todo remove me
#include "doomsday/world/sector.h"

#include <de/vector.h>

namespace world {

using namespace de;

Vertex::Vertex(mesh::Mesh &mesh, const Vec2d &origin)
    : MapElement(DMU_VERTEX)
    , mesh::MeshElement(mesh)
{
    _origin = origin;
}

const Vec2d &Vertex::origin() const
{
    return _origin;
}

void Vertex::setOrigin(const Vec2d &newOrigin)
{
    if(_origin != newOrigin)
    {
        _origin = newOrigin;
        DE_NOTIFY_VAR(OriginChange, i)
        {
            i->vertexOriginChanged(*this);
        }
    }
}

int Vertex::property(world::DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_X:
        args.setValue(DMT_VERTEX_ORIGIN, &_origin.x, 0);
        break;
    case DMU_Y:
        args.setValue(DMT_VERTEX_ORIGIN, &_origin.y, 0);
        break;
    case DMU_XY:
        args.setValue(DMT_VERTEX_ORIGIN, &_origin.x, 0);
        args.setValue(DMT_VERTEX_ORIGIN, &_origin.y, 1);
        break;
    default:
        return MapElement::property(args);
    }
    return false;  // Continue iteration.
}

// ---------------------------------------------------------------------------

duint Vertex::lineOwnerCount() const
{
    return _numLineOwners;
}

void Vertex::countLineOwners()
{
    _onesOwnerCount = _twosOwnerCount = 0;

    if(const LineOwner *firstOwn = firstLineOwner())
    {
        const LineOwner *own = firstOwn;
        do
        {
            if(!own->line().front().hasSector() || !own->line().back().hasSector())
            {
                _onesOwnerCount += 1;
            }
            else
            {
                _twosOwnerCount += 1;
            }
        } while((own = own->next()) != firstOwn);
    }
}

LineOwner *Vertex::firstLineOwner() const
{
    return _lineOwners;
}

} // namespace world
