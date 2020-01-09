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

#include "render/trianglestripbuilder.h"

using namespace de;

DE_PIMPL(TriangleStripBuilder)
{
    ClockDirection direction;
    bool buildTexCoords;
    int initialReserveElements;

    std::unique_ptr<PositionBuffer> positions;
    std::unique_ptr<TexCoordBuffer> texcoords;

    Impl(Public *i, bool buildTexCoords)
        : Base(i)
        , direction(Clockwise)
        , buildTexCoords(buildTexCoords)
        , initialReserveElements(0)
    {}

    void reserveElements(int num)
    {
        if(num < 0) return; // Huh?

        // Time to allocate the buffers?
        if (!positions)
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
    : d(new Impl(this, buildTexCoords))
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

    const AbstractEdge::Event &from = edge.first();
    const AbstractEdge::Event &to   = edge.last();

    d->reserveElements(2);

    d->positions->append((d->direction == CounterClockwise? to : from).origin());
    d->positions->append((d->direction == CounterClockwise? from : to).origin());

    if(d->buildTexCoords)
    {
        double edgeLength = to.origin().z - from.origin().z;

        d->texcoords->append(edge.materialOrigin() + Vec2f(0, (d->direction == CounterClockwise? 0 : edgeLength)));
        d->texcoords->append(edge.materialOrigin() + Vec2f(0, (d->direction == CounterClockwise? edgeLength : 0)));
    }
}

int TriangleStripBuilder::numElements() const
{
    return d->positions? d->positions->sizei() : 0;
}

int TriangleStripBuilder::take(PositionBuffer **positions, TexCoordBuffer **texcoords)
{
    const int retNumElements = numElements();
    *positions = d->positions.release();
    if (texcoords)
    {
        *texcoords = d->texcoords.release();
    }
    return retNumElements;
}
