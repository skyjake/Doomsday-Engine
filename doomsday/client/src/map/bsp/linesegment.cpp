/** @file map/bsp/linesegment.cpp BSP Builder Line Segment.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include <de/Observers>

#include "HEdge"

#include "map/bsp/linesegment.h"

namespace de {
namespace bsp {

DENG2_PIMPL(LineSegment),
DENG2_OBSERVES(Vertex, OriginChange)
{
    /// Vertexes of the line segment.
    Vertex *from, *to;

    Instance(Public *i, Vertex &from_, Vertex &to_)
        : Base(i), from(&from_), to(&to_)
    {
        from->audienceForOriginChange += this;
        to->audienceForOriginChange   += this;
    }

    ~Instance()
    {
        from->audienceForOriginChange -= this;
        to->audienceForOriginChange   -= this;
    }

    inline Vertex **vertexAdr(int edge) {
        return edge? &to : &from;
    }

    void replaceVertex(int edge, Vertex &newVertex)
    {
        Vertex **adr = vertexAdr(edge);

        if(*adr && *adr == &newVertex) return;

        if(*adr) (*adr)->audienceForOriginChange -= this;
        *adr = &newVertex;
        if(*adr) (*adr)->audienceForOriginChange += this;

        updateCache();
    }

    /**
     * To be called to update precalculated vectors, distances, etc...
     * following a dependent vertex origin change notification.
     *
     * @todo Optimize: defer until next accessed. -ds
     */
    void updateCache()
    {
        Vector2d tempDir = to->origin() - from->origin();
        V2d_Set(self.direction, tempDir.x, tempDir.y);

        self.pLength    = V2d_Length(self.direction);
        DENG2_ASSERT(self.pLength > 0);
        self.pAngle     = M_DirectionToAngle(self.direction);
        self.pSlopeType = M_SlopeType(self.direction);

        self.pPerp =  from->origin().y * self.direction[VX] - from->origin().x * self.direction[VY];
        self.pPara = -from->origin().x * self.direction[VX] - from->origin().y * self.direction[VY];
    }

    void vertexOriginChanged(Vertex &vertex, de::Vector2d const &oldOrigin, int changedAxes)
    {
        DENG2_UNUSED3(vertex, oldOrigin, changedAxes);
        updateCache();
    }
};

LineSegment::LineSegment(Vertex &from, Vertex &to, Line::Side *side,
                         Line::Side *sourceLineSide)
    : pLength(0),
      pAngle(0),
      pPara(0),
      pPerp(0),
      pSlopeType(ST_VERTICAL),
      nextOnSide(0),
      prevOnSide(0),
      bmapBlock(0),
      _lineSide(side),
      sourceLineSide(sourceLineSide),
      sector(0),
      _twin(0),
      hedge(0),
      d(new Instance(this, from, to))
{
    V2d_Set(direction, 0, 0);
    d->updateCache();
}

LineSegment::LineSegment(LineSegment const &other)
    : pLength(other.pLength),
      pAngle(other.pAngle),
      pPara(other.pPara),
      pPerp(other.pPerp),
      pSlopeType(other.pSlopeType),
      nextOnSide(other.nextOnSide),
      prevOnSide(other.prevOnSide),
      bmapBlock(other.bmapBlock),
      _lineSide(other._lineSide),
      sourceLineSide(other.sourceLineSide),
      sector(other.sector),
      _twin(other._twin),
      hedge(other.hedge),
      d(new Instance(this, *other.d->from, *other.d->to))
{
    V2d_Copy(direction, other.direction);
    d->updateCache();
}

LineSegment &LineSegment::operator = (LineSegment const &other)
{
    V2d_Copy(direction, other.direction);
    pLength = other.pLength;
    pAngle = other.pAngle;
    pPara = other.pPara;
    pPerp = other.pPerp;
    pSlopeType = other.pSlopeType;
    nextOnSide = other.nextOnSide;
    prevOnSide = other.prevOnSide;
    bmapBlock = other.bmapBlock;
    _lineSide = other._lineSide;
    sourceLineSide = other.sourceLineSide;
    sector = other.sector;
    _twin = other._twin;
    hedge = other.hedge;

    replaceFrom(*other.d->from);
    replaceTo(*other.d->to);

    d->updateCache();

    return *this;
}

Vertex &LineSegment::vertex(int to) const
{
    DENG_ASSERT(*d->vertexAdr(to) != 0);
    return **d->vertexAdr(to);
}

void LineSegment::replaceVertex(int to, Vertex &newVertex)
{
    d->replaceVertex(to, newVertex);
}

bool LineSegment::hasTwin() const
{
    return _twin != 0;
}

LineSegment &LineSegment::twin() const
{
    if(_twin)
    {
        return *_twin;
    }
    /// @throw MissingTwinError Attempted with no twin associated.
    throw MissingTwinError("LineSegment::twin", "No twin line segment is associated");
}

bool LineSegment::hasBspLeaf() const
{
    if(!hedge) return false;
    return hedge->hasBspLeaf();
}

BspLeaf &LineSegment::bspLeaf() const
{
    if(hedge)
    {
        return hedge->bspLeaf();
    }
    /// @throw MissingBspLeafError Attempted with no BSP leaf associated.
    throw MissingBspLeafError("LineSegment::bspLeaf", "No BSP leaf is associated");
}

bool LineSegment::hasMapLineSide() const
{
    return _lineSide != 0;
}

Line::Side &LineSegment::mapLineSide() const
{
    if(_lineSide)
    {
        return *_lineSide;
    }
    /// @throw MissingMapLineSideError Attempted with no line side attributed.
    throw MissingMapLineSideError("LineSegment::mapLineSide", "No map line side is attributed");
}

void LineSegment::ceaseVertexObservation()
{
    d->from->audienceForOriginChange -= d.get();
    d->to->audienceForOriginChange   -= d.get();
}

} // namespace bsp
} // namespace de
