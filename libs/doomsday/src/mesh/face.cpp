/** @file face.cpp  Mesh, face geometry.
 *
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/mesh/face.h"
#include "doomsday/mesh/hedge.h"

#include <de/legacy/mathutil.h>
#include <de/legacy/vector1.h> /// @todo remove me
#include <de/log.h>

namespace mesh {

using namespace de;

Face::Face(Mesh &mesh)
    : MeshElement(mesh)
{}

int Face::hedgeCount() const
{
    return _hedgeCount;
}

HEdge *Face::hedge() const
{
    return _hedge;
}

void Face::setHEdge(const HEdge *newHEdge)
{
    _hedge = const_cast<HEdge *>(newHEdge);
}

const AABoxd &Face::bounds() const
{
    return _bounds;
}

void Face::updateBounds()
{
    _bounds.clear();

    if (!_hedge) return; // Very odd...

    const HEdge *hedge = _hedge;
    V2d_InitBoxXY(_bounds.arvec2, hedge->origin().x, hedge->origin().y);

    while ((hedge = &hedge->next()) != _hedge)
    {
        V2d_AddToBoxXY(_bounds.arvec2, hedge->origin().x, hedge->origin().y);
    }
}

const Vec2d &Face::center() const
{
    return _center;
}

void Face::updateCenter()
{
    // The center is the middle of our AABox.
    _center = Vec2d(_bounds.min) + (Vec2d(_bounds.max) - Vec2d(_bounds.min)) / 2;
}

bool Face::isConvex() const
{
    return _hedgeCount > 2;
}

String Face::description() const
{
    String text = Stringf("Face [%p] comprises %i half-edges", this, _hedgeCount);

    if (const HEdge *hedge = _hedge)
    {
        do
        {
            Vec2d direction = hedge->origin() - _center;
            coord_t angle      = M_DirectionToAngleXY(direction.x, direction.y);

            text += Stringf("\n  [%p]: Angle %3.6f %s -> %s",
                                   hedge,
                                   angle,
                                   hedge->origin().asText().c_str(),
                                   hedge->twin().origin().asText().c_str());

        } while ((hedge = &hedge->next()) != _hedge);
    }

    return text;
}

} // namespace mesh
