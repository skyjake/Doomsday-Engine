/** @file map/face.cpp World Map Face Geometry.
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
#include "Mesh"

#include "Face"

namespace de {

DENG2_PIMPL_NOREF(Face)
{
    /// Mesh which owns the face.
    Mesh *mesh;

    /// First half-edge in the face geometry.
    HEdge *hedge;

    /// MapElement to which the half-edge is attributed (if any).
    MapElement *mapElement;

    /// Vertex bounding box.
    AABoxd aaBox;

    /// Center of vertices.
    Vector2d center;

    Instance(Mesh &mesh)
        : mesh(&mesh),
          hedge(0),
          mapElement(0)
    {}
};

Face::Face(Mesh &mesh)
    : _hedgeCount(0), d(new Instance(mesh))
{}

Mesh &Face::mesh() const
{
    DENG_ASSERT(d->mesh != 0);
    return *d->mesh;
}

HEdge *Face::hedge() const
{
    return d->hedge;
}

void Face::setHEdge(HEdge *newHEdge)
{
    d->hedge = newHEdge;
}

int Face::hedgeCount() const
{
    return _hedgeCount;
}

AABoxd const &Face::aaBox() const
{
    return d->aaBox;
}

void Face::updateAABox()
{
    d->aaBox.clear();

    if(!d->hedge) return; // Very odd...

    HEdge *hedgeIt = d->hedge;
    V2d_InitBoxXY(d->aaBox.arvec2, hedgeIt->origin().x, hedgeIt->origin().y);

    while((hedgeIt = &hedgeIt->next()) != d->hedge)
    {
        V2d_AddToBoxXY(d->aaBox.arvec2, hedgeIt->origin().x, hedgeIt->origin().y);
    }
}

Vector2d const &Face::center() const
{
    return d->center;
}

void Face::updateCenter()
{
    // The center is the middle of our AABox.
    d->center = Vector2d(d->aaBox.min) + (Vector2d(d->aaBox.max) - Vector2d(d->aaBox.min)) / 2;
}

bool Face::isConvex() const
{
    /// @todo Implement full conformance checking.
    return _hedgeCount > 2;
}

MapElement *Face::mapElement() const
{
    return d->mapElement;
}

void Face::setMapElement(MapElement *newMapElement)
{
    d->mapElement = newMapElement;
}

#ifdef DENG_DEBUG
void Face::print() const
{
    if(!d->hedge) return;

    LOG_INFO("Half-edges:");
    HEdge const *hedgeIt = d->hedge;
    do
    {
        HEdge const &hedge = *hedgeIt;
        coord_t angle = M_DirectionToAngleXY(hedge.origin().x - d->center.x,
                                             hedge.origin().y - d->center.y);

        LOG_INFO("  [%p]: Angle %1.6f %s -> %s")
            << de::dintptr(&hedge) << angle
            << hedge.origin().asText() << hedge.twin().origin().asText();

    } while((hedgeIt = &hedgeIt->next()) != d->hedge);
}
#endif

} // namespace de
