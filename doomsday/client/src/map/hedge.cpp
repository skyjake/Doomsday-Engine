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

void HEdge::wallSectionSectors(Sector **frontSec, Sector **backSec) const
{
    if(!d->lineSide)
        /// @throw MissingLineError Attempted with no line attributed.
        throw MissingLineSideError("HEdge::wallSectionSectors", "No line.side is attributed");

    if(d->lineSide->line().isFromPolyobj())
    {
        if(frontSec) *frontSec = bspLeaf().sectorPtr();
        if(backSec)  *backSec  = 0;
        return;
    }

    if(!d->lineSide->line().isSelfReferencing())
    {
        if(frontSec) *frontSec = bspLeaf().sectorPtr();
        if(backSec)  *backSec  = hasTwin()? twin().bspLeaf().sectorPtr() : 0;
        return;
    }

    // Special case: so called "self-referencing" lines use the sector's of the map line.
    if(frontSec) *frontSec = d->lineSide->sectorPtr();
    if(backSec)  *backSec  = d->lineSide->sectorPtr();
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

// ---------------------------------------------------------------------

#include "map/r_world.h" // R_GetVtxLineOwner
#include "map/lineowner.h"

static int compareIntercepts(void const *e1, void const *e2)
{
    ddouble const delta = (*reinterpret_cast<WallDivs::Intercept const *>(e1)) - (*reinterpret_cast<WallDivs::Intercept const *>(e2));
    if(delta > 0) return 1;
    if(delta < 0) return -1;
    return 0;
}

WallDivs::Intercept *WallDivs::find(ddouble distance) const
{
    for(int i = 0; i < _interceptCount; ++i)
    {
        Intercept *icpt = const_cast<Intercept *>(&_intercepts[i]);
        if(icpt->distance() == distance)
            return icpt;
    }
    return 0;
}

/**
 * Ensure the intercepts are sorted (in ascending distance order).
 */
void WallDivs::assertSorted() const
{
#ifdef DENG_DEBUG
    if(isEmpty()) return;

    WallDivs::Intercept *node = &first();
    ddouble farthest = node->distance();
    forever
    {
        DENG2_ASSERT(node->distance() >= farthest);
        farthest = node->distance();

        if(!node->hasNext()) break;
        node = &node->next();
    }
#endif
}

WallDivs::Intercept::Intercept(ddouble distance)
    : _distance(distance), _wallDivs(0)
{}

bool WallDivs::intercept(ddouble distance)
{
    if(find(distance))
        return false;

    Intercept *icpt = &_intercepts[_interceptCount++];
    icpt->_wallDivs = this;
    icpt->_distance = distance;
    return true;
}

void WallDivs::sort()
{
    if(count() < 2) return;

    // Sorting is required. This shouldn't take too long...
    // There seldom are more than two or three intercepts.
    qsort(_intercepts, _interceptCount, sizeof(*_intercepts), compareIntercepts);
    assertSorted();
}

#ifdef DENG_DEBUG
void WallDivs::printIntercepts() const
{
    // Stub.
}
#endif

WallDivs::Intercepts const &WallDivs::intercepts() const
{
    return _intercepts;
}

SectionEdge::SectionEdge(HEdge &hedge, int edge, int section)
    : _hedge(&hedge),
      _edge(edge),
      _section(section),
      _interceptCount(0),
      _firstIntercept(0),
      _lastIntercept(0)
{
    DENG_ASSERT(_hedge->hasLineSide() && _hedge->lineSide().hasSections());
}

WallDivs::Intercept &SectionEdge::firstDivision() const
{
    return _firstIntercept->next();
}

WallDivs::Intercept &SectionEdge::lastDivision() const
{
    return _lastIntercept->prev();
}

WallDivs::Intercept &SectionEdge::bottom() const
{
    return *_firstIntercept;
}

WallDivs::Intercept &SectionEdge::top() const
{
    return *_lastIntercept;
}

HEdge &SectionEdge::hedge() const
{
    return *_hedge;
}

int SectionEdge::section() const
{
    return _section;
}

Vector2d const &SectionEdge::origin() const
{
    return _hedge->vertex(_edge).origin();
}

coord_t SectionEdge::offset() const
{
    return _hedge->lineOffset() + (_edge? _hedge->length() : 0);
}

void SectionEdge::addPlaneIntercepts(coord_t bottom, coord_t top)
{
    if(!_hedge->hasLineSide()) return;

    Line::Side const &side = _hedge->lineSide();
    if(side.line().isFromPolyobj()) return;

    // Check for neighborhood division?
    if(_section == Line::Side::Middle && side.hasSections() && side.back().hasSections())
        return;

    // Only sections at line side edges can/should be split.
    if(!((_hedge == side.leftHEdge()  && _edge == HEdge::From) ||
         (_hedge == side.rightHEdge() && _edge == HEdge::To)))
        return;

    if(bottom >= top) return; // Obviously no division.

    Sector const *frontSec = side.sectorPtr();

    LineOwner::Direction direction(_edge? LineOwner::Previous : LineOwner::Next);
    // Retrieve the start owner node.
    LineOwner *base = R_GetVtxLineOwner(&side.line().vertex(_edge), &side.line());
    LineOwner *own = base;
    bool stopScan = false;
    do
    {
        own = &own->navigate(direction);

        if(own == base)
        {
            stopScan = true;
        }
        else
        {
            Line *iter = &own->line();

            if(iter->isSelfReferencing())
                continue;

            uint i = 0;
            do
            {
                // First front, then back.
                Sector *scanSec = 0;
                if(!i && iter->hasFrontSections() && iter->frontSectorPtr() != frontSec)
                    scanSec = iter->frontSectorPtr();
                else if(i && iter->hasBackSections() && iter->backSectorPtr() != frontSec)
                    scanSec = iter->backSectorPtr();

                if(scanSec)
                {
                    if(scanSec->ceiling().visHeight() - scanSec->floor().visHeight() > 0)
                    {
                        for(int j = 0; j < scanSec->planeCount() && !stopScan; ++j)
                        {
                            Plane const &plane = scanSec->plane(j);

                            if(plane.visHeight() > bottom && plane.visHeight() < top)
                            {
                                if(wallDivs.intercept(plane.visHeight()))
                                {
                                    // Have we reached the div limit?
                                    if(wallDivs.count() == WALLDIVS_MAX_INTERCEPTS)
                                        stopScan = true;
                                }
                            }

                            if(!stopScan)
                            {
                                // Clip a range bound to this height?
                                if(plane.type() == Plane::Floor && plane.visHeight() > bottom)
                                    bottom = plane.visHeight();
                                else if(plane.type() == Plane::Ceiling && plane.visHeight() < top)
                                    top = plane.visHeight();

                                // All clipped away?
                                if(bottom >= top)
                                    stopScan = true;
                            }
                        }
                    }
                    else
                    {
                        /**
                         * A zero height sector is a special case. In this
                         * instance, the potential division is at the height
                         * of the back ceiling. This is because elsewhere
                         * we automatically fix the case of a floor above a
                         * ceiling by lowering the floor.
                         */
                        coord_t z = scanSec->ceiling().visHeight();

                        if(z > bottom && z < top)
                        {
                            if(wallDivs.intercept(z))
                            {
                                // All clipped away.
                                stopScan = true;
                            }
                        }
                    }
                }
            } while(!stopScan && ++i < 2);

            // Stop the scan when a single sided line is reached.
            if(!iter->hasFrontSections() || !iter->hasBackSections())
                stopScan = true;
        }
    } while(!stopScan);
}

/**
 * Ensure the divisions do not exceed the specified range.
 */
void SectionEdge::assertDivisionsInRange(coord_t low, coord_t hi)
{
#ifdef DENG_DEBUG
    if(wallDivs.isEmpty()) return;

    WallDivs::Intercept *node = &wallDivs.first();
    forever
    {
        DENG2_ASSERT(node->distance() >= low && node->distance() <= hi);

        if(!node->hasNext()) break;
        node = &node->next();
    }
#else
    DENG2_UNUSED2(low, hi);
#endif
}

void SectionEdge::prepare(coord_t bottom, coord_t top)
{
    DENG_ASSERT(wallDivs.isEmpty());

    // Nodes are arranged according to their Z axis height in ascending order.
    // The first node is the bottom.
    wallDivs.intercept(bottom);

    // Add nodes for intercepts.
    addPlaneIntercepts(bottom, top);

    // The last node is the top.
    wallDivs.intercept(top);

    if(wallDivs.count() > 2)
    {
        wallDivs.sort();
        assertDivisionsInRange(bottom, top);
    }

    _firstIntercept = &wallDivs.first();
    _lastIntercept = &wallDivs.last();
    _interceptCount = wallDivs.count();
}
