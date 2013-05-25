/** @file render/trianglestripbuilder.cpp Triangle Strip Geometry Builder.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#include <QScopedPointer>

#include "render/trianglestripbuilder.h"

namespace de {

DENG2_PIMPL(TriangleStripBuilder)
{
    ClockDirection direction;
    bool buildTexCoords;
    int initialReserveElements;

    QScopedPointer<PositionBuffer> positions;
    QScopedPointer<TexCoordBuffer> texcoords;

    Instance(Public *i, bool buildTexCoords)
        : Base(i),
          direction(Clockwise),
          buildTexCoords(buildTexCoords),
          initialReserveElements(0),
          positions(0),
          texcoords(0)
    {}

    void reserveElements(int num)
    {
        if(num < 0) return; // Huh?

        // Time to allocate the buffers?
        if(positions.isNull())
        {
            positions.reset(new PositionBuffer());
            if(buildTexCoords)
                texcoords.reset(new TexCoordBuffer());

            // The user may already know how many elements they will require.
            num += initialReserveElements;
        }

        // Reserve this many new elements.
        positions->reserve(positions->size() + num);
        if(buildTexCoords)
            texcoords->reserve(texcoords->size() + num);
    }
};

TriangleStripBuilder::TriangleStripBuilder(bool buildTexCoords)
    : d(new Instance(this, buildTexCoords))
{}

void TriangleStripBuilder::begin(ClockDirection direction, int reserveElements)
{
    d->direction = direction;
    d->initialReserveElements = de::max(reserveElements, 0);

    // Destroy any existing unclaimed strip geometry.
    d->positions.reset();
    d->texcoords.reset();
}

void TriangleStripBuilder::extend(AbstractEdge &edge)
{
    // Silently ignore invalid edges.
    if(!edge.isValid()) return;

    AbstractEdge::Event const &from = edge.first();
    AbstractEdge::Event const &to   = edge.last();

    d->reserveElements(2);

    d->positions->append(rvertex_t((d->direction == Anticlockwise? to : from).origin()));
    d->positions->append(rvertex_t((d->direction == Anticlockwise? from : to).origin()));

    if(d->buildTexCoords)
    {
        coord_t edgeLength = to.distance() - from.distance();

        d->texcoords->append(rtexcoord_s(edge.materialOrigin +
                                         Vector2f(0, (d->direction == Anticlockwise? 0 : edgeLength))));
        d->texcoords->append(rtexcoord_s(edge.materialOrigin +
                                         Vector2f(0, (d->direction == Anticlockwise? edgeLength : 0))));
    }
}

int TriangleStripBuilder::numElements() const
{
    return d->positions.isNull()? 0 : d->positions->size();
}

int TriangleStripBuilder::take(PositionBuffer **positions, TexCoordBuffer **texcoords)
{
    int retNumElements = numElements();

    *positions = d->positions.take();
    if(texcoords)
    {
        *texcoords = d->texcoords.take();
    }
    return retNumElements;
}

} // namespace de
