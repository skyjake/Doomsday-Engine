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

#include "map/hedge.h"

using namespace de;

DENG2_PIMPL(HEdge)
{
    /// Map Line::Side attributed to the half-edge. Can be @c 0 (mini-edge).
    Line::Side *lineSide;

    Instance(Public *i)
        : Base(i),
          lineSide(0)
    {}
};

HEdge::HEdge(Vertex &from, Line::Side *lineSide)
    : MapElement(DMU_HEDGE), d(new Instance(this))
{
    _from = &from;
    _to = 0;
    _next = 0;
    _prev = 0;
    _twin = 0;
    _bspLeaf = 0;
    _angle = 0;
    _length = 0;
    _lineOffset = 0;
    std::memset(_bsuf, 0, sizeof(_bsuf));
    _frameFlags = 0;

    d->lineSide = lineSide;
}

HEdge::HEdge(HEdge const &other)
    : MapElement(DMU_HEDGE), d(new Instance(this))
{
    _from = other._from;
    _to = other._to;
    _next = other._next;
    _prev = other._prev;
    _twin = other._twin;
    _bspLeaf = other._bspLeaf;
    _angle = other._angle;
    _length = other._length;
    _lineOffset = other._lineOffset;
    std::memcpy(_bsuf, other._bsuf, sizeof(_bsuf));
    _frameFlags = other._frameFlags;

    d->lineSide = other.d->lineSide;
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

Vertex &HEdge::vertex(int to)
{
    DENG_ASSERT((to? _to : _from) != 0);
    return to? *_to : *_from;
}

Vertex const &HEdge::vertex(int to) const
{
    return const_cast<Vertex const &>(const_cast<HEdge *>(this)->vertex(to));
}

HEdge &HEdge::next() const
{
    DENG2_ASSERT(_next != 0);
    return *_next;
}

HEdge &HEdge::prev() const
{
    DENG2_ASSERT(_prev != 0);
    return *_prev;
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

biassurface_t &HEdge::biasSurfaceForGeometryGroup(uint groupId)
{
    if(groupId <= Line::Side::Top)
    {
        DENG2_ASSERT(_bsuf[groupId]);
        return *_bsuf[groupId];
    }
    /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group id.
    throw UnknownGeometryGroupError("HEdge::biasSurfaceForGeometryGroup", QString("Invalid group id %1").arg(groupId));
}

coord_t HEdge::pointDistance(const_pvec2d_t point, coord_t *offset) const
{
    /// @todo Why are we calculating this every time?
    Vector2d direction = _to->origin() - _from->origin();

    coord_t fromOriginV1[2] = { fromOrigin().x, fromOrigin().y };
    coord_t directionV1[2]  = { direction.x, direction.y };
    return V2d_PointLineDistance(point, fromOriginV1, directionV1, offset);
}

coord_t HEdge::pointOnSide(const_pvec2d_t point) const
{
    DENG_ASSERT(point != 0);
    /// @todo Why are we calculating this every time?
    Vector2d direction = _to->origin() - _from->origin();

    coord_t fromOriginV1[2] = { fromOrigin().x, fromOrigin().y };
    coord_t directionV1[2]  = { direction.x, direction.y };
    return V2d_PointOnLineSide(point, fromOriginV1, directionV1);
}

int HEdge::property(setargs_t &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_HEDGE_V, &_from, &args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_HEDGE_V, &_to, &args, 0);
        break;
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
        Sector *sector = bspLeafSectorPtr();
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

// WallDivs ----------------------------------------------------------------
/// @todo Move the following to another file

#include "render/walldiv.h"

coord_t WallDivNode_Height(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    return node->height;
}

walldivnode_t *WallDivNode_Next(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx + 1 >= node->divs->num) return 0;
    return &node->divs->nodes[idx+1];
}

walldivnode_t *WallDivNode_Prev(walldivnode_t *node)
{
    DENG2_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx == 0) return 0;
    return &node->divs->nodes[idx-1];
}

uint WallDivs_Size(walldivs_t const *wd)
{
    DENG2_ASSERT(wd);
    return wd->num;
}

walldivnode_t *WallDivs_First(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    return &wd->nodes[0];
}

walldivnode_t *WallDivs_Last(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    return &wd->nodes[wd->num-1];
}

walldivs_t *WallDivs_Append(walldivs_t *wd, coord_t height)
{
    DENG2_ASSERT(wd);
    struct walldivnode_s *node = &wd->nodes[wd->num++];
    node->divs = wd;
    node->height = height;
    return wd;
}

void WallDivs_AssertSorted(walldivs_t *wd)
{
#ifdef DENG_DEBUG
    walldivnode_t *node = WallDivs_First(wd);
    coord_t highest = WallDivNode_Height(node);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        DENG2_ASSERT(node->height >= highest);
        highest = node->height;
    }
#else
    DENG_UNUSED(wd);
#endif
}

void WallDivs_AssertInRange(walldivs_t *wd, coord_t low, coord_t hi)
{
#ifdef DENG_DEBUG
    DENG2_ASSERT(wd);
    walldivnode_t *node = WallDivs_First(wd);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        DENG2_ASSERT(node->height >= low && node->height <= hi);
    }
#else
    DENG2_UNUSED3(wd, low, hi);
#endif
}

#ifdef DENG_DEBUG
void WallDivs_DebugPrint(walldivs_t *wd)
{
    DENG2_ASSERT(wd);
    LOG_DEBUG("WallDivs [%p]:") << wd;
    for(uint i = 0; i < wd->num; ++i)
    {
        walldivnode_t *node = &wd->nodes[i];
        LOG_DEBUG("  %i: %f") << i << node->height;
    }
}
#endif
