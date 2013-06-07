/** @file data/face.cpp Mesh Geometry Face.
 *
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include <de/mathutil.h>
#include <de/vector1.h> /// @todo remove me

#include <de/Log>

#include "HEdge"

#include "map/face.h"

namespace de {

DENG2_PIMPL_NOREF(Face)
{
    /// First half-edge in the face geometry.
    HEdge *hedge;

    /// Vertex bounding box.
    AABoxd aaBox;

    /// Center of vertices.
    Vector2d center;

    Instance() : hedge(0)
    {}
};

Face::Face(Mesh &mesh)
    : Mesh::Element(mesh),
      _hedgeCount(0),
      d(new Instance())
{}

int Face::hedgeCount() const
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

AABoxd const &Face::aaBox() const
{
    return d->aaBox;
}

void Face::updateAABox()
{
    d->aaBox.clear();

    if(!d->hedge) return; // Very odd...

    HEdge const *hedge = d->hedge;
    V2d_InitBoxXY(d->aaBox.arvec2, hedge->origin().x, hedge->origin().y);

    while((hedge = &hedge->next()) != d->hedge)
    {
        V2d_AddToBoxXY(d->aaBox.arvec2, hedge->origin().x, hedge->origin().y);
    }
}

Vector2d const &Face::center() const
{
    return d->center;
}

void Face::updateCenter()
{
    // The center is the middle of our AABox.
    d->center = Vector2d(d->aaBox.min)
              + (Vector2d(d->aaBox.max) - Vector2d(d->aaBox.min)) / 2;
}

bool Face::isConvex() const
{
    /// @todo Implement full conformance checking.
    return _hedgeCount > 2;
}

String Face::description() const
{
    String text = String("Face [0x%1] comprises %2 half-edges")
            .arg(de::dintptr(this), 0, 16).arg(_hedgeCount);

    if(HEdge const *hedge = d->hedge)
    {
        do
        {
            Vector2d direction = hedge->origin() - d->center;
            coord_t angle      = M_DirectionToAngleXY(direction.x, direction.y);

            text += String("\n  [0x%1]: Angle %2.6f %3 -> %4")
                        .arg(de::dintptr(hedge), 0, 16)
                        .arg(angle)
                        .arg(hedge->origin().asText())
                        .arg(hedge->twin().origin().asText());

        } while((hedge = &hedge->next()) != d->hedge);
    }

    return text;
}

} // namespace de
