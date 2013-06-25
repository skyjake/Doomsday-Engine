/** @file segment.cpp World map line segment.
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

#include <QMap>
#include <QtAlgorithms>

#include <de/binangle.h>

#include "dd_main.h" // App_World()
#include "BspLeaf"
#include "HEdge"
#include "Line"
#include "Sector"

#ifdef __CLIENT__
#  include "BiasSurface"
#endif

#include "world/segment.h"

using namespace de;

DENG2_PIMPL(Segment)
{
    /// Segment on the back side of this (if any). @todo remove me
    Segment *back;

    /// Segment flags.
    Flags flags;

    /// Map Line::Side attributed to the line segment (not owned).
    /// Can be @c 0 (signifying a partition line segment).
    Line::Side *lineSide;

    /// Distance along the attributed map line at which the half-edge vertex occurs.
    coord_t lineSideOffset;

    /// Half-edge attributed to the line segment (not owned).
    HEdge *hedge;

    /// World angle.
    angle_t angle;

    /// Accurate length of the segment.
    coord_t length;

#ifdef __CLIENT__
    BiasSurfaces biasSurfaces;
#endif

    Instance(Public *i)
        : Base(i),
          back(0),
          flags(0),
          lineSide(0),
          lineSideOffset(0),
          hedge(0),
          angle(0),
          length(0)
    {}

    ~Instance()
    {
#ifdef __CLIENT__
        qDeleteAll(biasSurfaces);
#endif
    }
};

Segment::Segment(Line::Side *lineSide, HEdge *hedge)
    : MapElement(DMU_SEGMENT),
      d(new Instance(this))
{
    d->lineSide = lineSide;
    d->hedge    = hedge;
}

HEdge &Segment::hedge() const
{
    if(d->hedge)
    {
        return *d->hedge;
    }
    /// @throw MissingHEdgeError Attempted with no half-edge attributed.
    throw MissingHEdgeError("Segment::hedge", "No half-edge is attributed");
}

bool Segment::hasBack() const
{
    return d->back != 0;
}

Segment &Segment::back() const
{
    DENG_ASSERT(d->back != 0);
    return *d->back;
}

void Segment::setBack(Segment *newBack)
{
    d->back = newBack;
}

Sector &Segment::sector() const
{
    return bspLeaf().sector();
}

Sector *Segment::sectorPtr() const
{
    return hasBspLeaf()? bspLeaf().sectorPtr() : 0;
}

bool Segment::hasLineSide() const
{
    return d->lineSide != 0;
}

Line::Side &Segment::lineSide() const
{
    if(d->lineSide)
    {
        return *d->lineSide;
    }
    /// @throw MissingLineError Attempted with no line attributed.
    throw MissingLineSideError("Segment::lineSide", "No line.side is attributed");
}

coord_t Segment::lineSideOffset() const
{
    return d->lineSideOffset;
}

void Segment::setLineSideOffset(coord_t newOffset)
{
    d->lineSideOffset = newOffset;
}

angle_t Segment::angle() const
{
    return d->angle;
}

void Segment::setAngle(angle_t newAngle)
{
    d->angle = newAngle;
}

coord_t Segment::length() const
{
    return d->length;
}

void Segment::setLength(coord_t newLength)
{
    d->length = newLength;

    /// @todo Is this still necessary?
    if(d->length == 0)
        d->length = 0.01f; // Hmm...
}

Segment::Flags Segment::flags() const
{
    return d->flags;
}

void Segment::setFlags(Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

#ifdef __CLIENT__

BiasSurface &Segment::biasSurface(int group)
{
    BiasSurfaces::iterator found = d->biasSurfaces.find(group);
    if(found != d->biasSurfaces.end())
    {
        return **found;
    }
    /// @throw InvalidGeometryGroupError Attempted with an invalid geometry group.
    throw UnknownGeometryGroupError("Segment::biasSurface", QString("Invalid group %1").arg(group));
}

void Segment::setBiasSurface(int group, BiasSurface *newBiasSurface)
{
    // Sanity check.
    DENG_ASSERT(group >= 0 && group < 3);

    if(d->biasSurfaces.contains(group))
    {
        delete d->biasSurfaces.take(group);
    }

    if(newBiasSurface)
    {
        d->biasSurfaces.insert(group, newBiasSurface);
    }
}

Segment::BiasSurfaces const &Segment::biasSurfaces() const
{
    return d->biasSurfaces;
}

#endif // __CLIENT__

coord_t Segment::pointDistance(const_pvec2d_t point, coord_t *offset) const
{
    DENG_ASSERT(point != 0);
    /// @todo Why are we calculating this every time?
    Vector2d direction = to().origin() - from().origin();

    coord_t fromOriginV1[2] = { from().origin().x, from().origin().y };
    coord_t directionV1[2]  = { direction.x, direction.y };
    return V2d_PointLineDistance(point, fromOriginV1, directionV1, offset);
}

coord_t Segment::pointOnSide(const_pvec2d_t point) const
{
    DENG_ASSERT(point != 0);
    /// @todo Why are we calculating this every time?
    Vector2d direction = to().origin() - from().origin();

    coord_t fromOriginV1[2] = { from().origin().x, from().origin().y };
    coord_t directionV1[2]  = { direction.x, direction.y };
    return V2d_PointOnLineSide(point, fromOriginV1, directionV1);
}

int Segment::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0: {
        Vertex *vtx = &from();
        args.setValue(DMT_SEGMENT_V, &vtx, 0);
        break; }
    case DMU_VERTEX1: {
        Vertex *vtx = &to();
        args.setValue(DMT_SEGMENT_V, &vtx, 0);
        break; }
    case DMU_LENGTH:
        args.setValue(DMT_SEGMENT_LENGTH, &d->length, 0);
        break;
    case DMU_OFFSET: {
        coord_t offset = d->lineSide? d->lineSideOffset : 0;
        args.setValue(DMT_SEGMENT_OFFSET, &offset, 0);
        break; }
    case DMU_SIDE:
        args.setValue(DMT_SEGMENT_SIDE, &d->lineSide, 0);
        break;
    case DMU_LINE: {
        Line *lineAdr = d->lineSide? &d->lineSide->line() : 0;
        args.setValue(DMT_SEGMENT_LINE, &lineAdr, 0);
        break; }
    case DMU_SECTOR: {
        Sector *sector = sectorPtr();
        args.setValue(DMT_SEGMENT_SECTOR, &sector, 0);
        break; }
    case DMU_ANGLE:
        args.setValue(DMT_SEGMENT_ANGLE, &d->angle, 0);
        break;
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}
