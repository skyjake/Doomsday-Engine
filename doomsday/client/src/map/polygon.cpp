/** @file map/face.cpp World Map Geometry Polygon.
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

#include <de/mathutil.h>
#include <de/vector1.h> /// @todo remove me

#include <de/Log>

#include "BspLeaf"
#include "HEdge"

#include "map/polygon.h"

namespace de {

DENG2_PIMPL(Polygon)
{
    /// Vertex bounding box in the map coordinate space.
    AABoxd aaBox;

    /// Center of vertices.
    Vector2d center;

    /// BSP leaf to which the polygon is attributed (if any).
    BspLeaf *bspLeaf;

    Instance(Public *i)
        : Base(i),
          bspLeaf(0)
    {}

    ~Instance()
    {
        // Clear the half-edges.
        if(self._hedge)
        {
            HEdge *he = self._hedge;
            if(!he->hasNext() || &he->next() == he)
            {
                delete he;
            }
            else
            {
                // Break the ring, if linked.
                if(he->hasPrev())
                {
                    he->prev().setNext(0);
                }

                while(he)
                {
                    HEdge *next = he->hasNext()? &he->next() : 0;
                    delete he;
                    he = next;
                }
            }
        }
    }
};

Polygon::Polygon()
    : _hedge(0), _hedgeCount(0), d(new Instance(this))
{}

bool Polygon::hasBspLeaf() const
{
    return d->bspLeaf != 0;
}

BspLeaf &Polygon::bspLeaf() const
{
    if(d->bspLeaf)
    {
        return *d->bspLeaf;
    }
    /// @throw MissingBspLeafError Attempted with no BSP leaf attributed.
    throw MissingBspLeafError("Polygon::bspLeaf", "No BSP leaf is attributed");
}

void Polygon::setBspLeaf(BspLeaf *newBspLeaf)
{
    d->bspLeaf = newBspLeaf;
}

HEdge *Polygon::firstHEdge() const
{
    return _hedge;
}

int Polygon::hedgeCount() const
{
    return _hedgeCount;
}

AABoxd const &Polygon::aaBox() const
{
    return d->aaBox;
}

void Polygon::updateAABox()
{
    d->aaBox.clear();

    HEdge *base = _hedge;
    if(!base) return; // Very odd...

    HEdge *hedgeIt = base;
    V2d_InitBoxXY(d->aaBox.arvec2, hedgeIt->origin().x, hedgeIt->origin().y);

    while((hedgeIt = &hedgeIt->next()) != base)
    {
        V2d_AddToBoxXY(d->aaBox.arvec2, hedgeIt->origin().x, hedgeIt->origin().y);
    }
}

Vector2d const &Polygon::center() const
{
    return d->center;
}

void Polygon::updateCenter()
{
    // The center is the middle of our AABox.
    d->center = Vector2d(d->aaBox.min) + (Vector2d(d->aaBox.max) - Vector2d(d->aaBox.min)) / 2;
}

bool Polygon::isConvex() const
{
    /// @todo Implement full conformance checking.
    return _hedgeCount > 2;
}

#ifdef DENG_DEBUG
void Polygon::print() const
{
    HEdge const *base = _hedge;
    if(!base) return;

    LOG_INFO("Half-edges:");
    HEdge const *hedgeIt = base;
    do
    {
        HEdge const &hedge = *hedgeIt;
        coord_t angle = M_DirectionToAngleXY(hedge.origin().x - d->center.x,
                                             hedge.origin().y - d->center.y);

        LOG_INFO("  [%p]: Angle %1.6f %s -> %s")
            << de::dintptr(&hedge) << angle
            << hedge.origin().asText() << hedge.twin().origin().asText();

    } while((hedgeIt = &hedgeIt->next()) != base);
}
#endif

} // namespace de
