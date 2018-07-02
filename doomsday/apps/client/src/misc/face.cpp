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

#include "Face"
#include <de/mathutil.h>
#include <de/vector1.h> /// @todo remove me
#include <de/Log>
#include "HEdge"

namespace de {

DE_PIMPL_NOREF(Face)
{
    HEdge *hedge = nullptr;  ///< First half-edge in the face geometry.
    AABoxd bounds;           ///< Vertex bounding box.
    Vec2d center;         ///< Center of vertices.
};

Face::Face(Mesh &mesh)
    : MeshElement(mesh)
    , _hedgeCount(0)
    , d(new Impl())
{}

dint Face::hedgeCount() const
{
    return _hedgeCount;
}

HEdge *Face::hedge() const
{
    return d->hedge;
}

void Face::setHEdge(HEdge const *newHEdge)
{
    d->hedge = const_cast<HEdge *>(newHEdge);
}

AABoxd const &Face::bounds() const
{
    return d->bounds;
}

void Face::updateBounds()
{
    d->bounds.clear();

    if(!d->hedge) return; // Very odd...

    HEdge const *hedge = d->hedge;
    V2d_InitBoxXY(d->bounds.arvec2, hedge->origin().x, hedge->origin().y);

    while ((hedge = &hedge->next()) != d->hedge)
    {
        V2d_AddToBoxXY(d->bounds.arvec2, hedge->origin().x, hedge->origin().y);
    }
}

Vec2d const &Face::center() const
{
    return d->center;
}

void Face::updateCenter()
{
    // The center is the middle of our AABox.
    d->center = Vec2d(d->bounds.min)
              + (Vec2d(d->bounds.max) - Vec2d(d->bounds.min)) / 2;
}

bool Face::isConvex() const
{
    /// @todo Implement full conformance checking.
    return _hedgeCount > 2;
}

String Face::description() const
{
    String text = String::format("Face [%p] comprises %i half-edges", this, _hedgeCount);

    if (HEdge const *hedge = d->hedge)
    {
        do
        {
            Vec2d direction = hedge->origin() - d->center;
            coord_t angle      = M_DirectionToAngleXY(direction.x, direction.y);

            text += String::format("\n  [%p]: Angle %3.6f %s -> %s",
                                   hedge,
                                   angle,
                                   hedge->origin().asText().c_str(),
                                   hedge->twin().origin().asText().c_str());

        } while ((hedge = &hedge->next()) != d->hedge);
    }

    return text;
}

} // namespace de
