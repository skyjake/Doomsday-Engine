/** @file map/hedge.cpp World Map Geometry Half-Edge.
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

#include <de/Log>

#include "Sector"

#ifdef __CLIENT__
#  include "render/rend_bias.h"
#endif

#include "map/hedge.h"

using namespace de;

DENG2_PIMPL(HEdge)
{
    /// Map Line::Side attributed to the half-edge. Can be @c 0 (partition segment).
    Line::Side *lineSide;

    Instance(Public *i)
        : Base(i),
          lineSide(0)
    {}

    inline HEdge **neighborAdr(ClockDirection direction) {
        return direction == Clockwise? &self._next : &self._prev;
    }
};

HEdge::HEdge(Vertex &vertex, Line::Side *lineSide)
    : MapElement(DMU_HEDGE), d(new Instance(this))
{
    _vertex = &vertex;
    _next = 0;
    _prev = 0;
    _twin = 0;
    _bspLeaf = 0;
    _angle = 0;
    _length = 0;
    _lineOffset = 0;
#ifdef __CLIENT__
    std::memset(_bsuf, 0, sizeof(_bsuf));
#endif
    _flags = 0;

    d->lineSide = lineSide;
}

HEdge::~HEdge()
{
#ifdef __CLIENT__
    for(uint i = 0; i < 3; ++i)
    {
        if(_bsuf[i])
        {
            SB_DestroySurface(_bsuf[i]);
        }
    }
#endif
}

Vertex &HEdge::vertex() const
{
    return *_vertex;
}

bool HEdge::hasNeighbor(ClockDirection direction) const
{
    return (*d->neighborAdr(direction)) != 0;
}

HEdge &HEdge::neighbor(ClockDirection direction) const
{
    HEdge **neighborAdr = d->neighborAdr(direction);
    if(*neighborAdr)
    {
        return **neighborAdr;
    }
    /// @throw MissingNeighborError Attempted with no relevant neighbor attributed.
    throw MissingNeighborError("HEdge::neighbor", QString("No %1 neighbor is attributed").arg(direction == Clockwise? "Clockwise" : "Anticlockwise"));
}

bool HEdge::hasTwin() const
{
    return _twin != 0;
}

HEdge &HEdge::twin() const
{
    if(_twin)
    {
        return *_twin;
    }
    /// @throw MissingTwinError Attempted with no twin associated.
    throw MissingTwinError("HEdge::twin", "No twin half-edge is associated");
}

bool HEdge::hasBspLeaf() const
{
    return _bspLeaf != 0;
}

BspLeaf &HEdge::bspLeaf() const
{
    if(_bspLeaf)
    {
        return *_bspLeaf;
    }
    /// @throw MissingBspLeafError Attempted with no BSP leaf associated.
    throw MissingBspLeafError("HEdge::bspLeaf", "No BSP leaf is associated");
}

bool HEdge::hasLineSide() const
{
    return d->lineSide != 0;
}

Line::Side &HEdge::lineSide() const
{
    if(d->lineSide)
    {
        return *d->lineSide;
    }
    /// @throw MissingLineError Attempted with no line attributed.
    throw MissingLineSideError("HEdge::lineSide", "No line.side is attributed");
}

coord_t HEdge::lineOffset() const
{
    if(d->lineSide)
    {
        return _lineOffset;
    }
    /// @throw MissingLineError Attempted with no line attributed.
    throw MissingLineSideError("HEdge::lineOffset", "No line.side is attributed");
}

angle_t HEdge::angle() const
{
    return _angle;
}

coord_t HEdge::length() const
{
    return _length;
}

HEdge::Flags HEdge::flags() const
{
    return _flags;
}

void HEdge::setFlags(Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(_flags, flagsToChange, operation);
}

#ifdef __CLIENT__

BiasSurface &HEdge::biasSurfaceForGeometryGroup(uint groupId)
{
    if(groupId <= Line::Side::Top)
    {
        DENG2_ASSERT(_bsuf[groupId] != 0);
        return *_bsuf[groupId];
    }
    /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group id.
    throw UnknownGeometryGroupError("HEdge::biasSurfaceForGeometryGroup", QString("Invalid group id %1").arg(groupId));
}

#endif // __CLIENT__

coord_t HEdge::pointDistance(const_pvec2d_t point, coord_t *offset) const
{
    DENG_ASSERT(point != 0);
    /// @todo Why are we calculating this every time?
    Vector2d direction = twin().origin() - _vertex->origin();

    coord_t fromOriginV1[2] = { origin().x, origin().y };
    coord_t directionV1[2]  = { direction.x, direction.y };
    return V2d_PointLineDistance(point, fromOriginV1, directionV1, offset);
}

coord_t HEdge::pointOnSide(const_pvec2d_t point) const
{
    DENG_ASSERT(point != 0);
    /// @todo Why are we calculating this every time?
    Vector2d direction = twin().origin() - _vertex->origin();

    coord_t fromOriginV1[2] = { origin().x, origin().y };
    coord_t directionV1[2]  = { direction.x, direction.y };
    return V2d_PointOnLineSide(point, fromOriginV1, directionV1);
}

int HEdge::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_HEDGE_V, &_vertex, &args, 0);
        break;
    case DMU_VERTEX1: {
        Vertex *twinVertex = &twin().vertex();
        DMU_GetValue(DMT_HEDGE_V, &twinVertex, &args, 0);
        break; }
    case DMU_LENGTH:
        DMU_GetValue(DMT_HEDGE_LENGTH, &_length, &args, 0);
        break;
    case DMU_OFFSET: {
        coord_t offset = d->lineSide? _lineOffset : 0;
        DMU_GetValue(DMT_HEDGE_OFFSET, &offset, &args, 0);
        break; }
    case DMU_SIDE:
        DMU_GetValue(DMT_HEDGE_SIDE, &d->lineSide, &args, 0);
        break;
    case DMU_LINE: {
        Line *lineAdr = d->lineSide? &d->lineSide->line() : 0;
        DMU_GetValue(DMT_HEDGE_LINE, &lineAdr, &args, 0);
        break; }
    case DMU_SECTOR: {
        Sector *sector = sectorPtr();
        DMU_GetValue(DMT_HEDGE_SECTOR, &sector, &args, 0);
        break; }
    case DMU_ANGLE:
        DMU_GetValue(DMT_HEDGE_ANGLE, &_angle, &args, 0);
        break;
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}
