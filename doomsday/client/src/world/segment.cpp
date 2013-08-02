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
#  include "BiasTracker"
#  include "world/map.h"
#endif

#include "world/segment.h"

using namespace de;

#ifdef __CLIENT__
typedef QMap<int, BiasTracker *> BiasTrackers;
#endif

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
    BiasTrackers biasTrackers;
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
        qDeleteAll(biasTrackers);
#endif
    }

#ifdef __CLIENT__
    /**
     * Retrieve the bias tracker for the specified geometry @a group.
     *
     * @param group     Geometry group identifier for the bias tracker.
     * @param canAlloc  @c true= to allocate if no tracker exists.
     */
    BiasTracker *biasTracker(int group, bool canAlloc = true)
    {
        DENG_ASSERT(group >= 0 && group < 3); // sanity check
        DENG_ASSERT(self.hasLineSide()); // sanity check

        BiasTrackers::iterator foundAt = biasTrackers.find(group);
        if(foundAt != biasTrackers.end())
        {
            return *foundAt;
        }

        if(!canAlloc) return 0;

        BiasTracker *newTracker = new BiasTracker(4);
        biasTrackers.insert(group, newTracker);
        return newTracker;
    }

    /**
     * @todo This could be enhanced so that only the lights on the right
     * side of the surface are taken into consideration.
     */
    void updateAffected(BiasTracker &bsuf, int group)
    {
        DENG_UNUSED(group);

        // If the data is already up to date, nothing needs to be done.
        uint lastChangeFrame = self.map().biasLastChangeOnFrame();
        if(bsuf.lastUpdateOnFrame() == lastChangeFrame)
            return;

        bsuf.setLastUpdateOnFrame(lastChangeFrame);

        bsuf.clearContributors();

        Surface const &surface = lineSide->middle();
        Vector2d const &from   = self.from().origin();
        Vector2d const &to     = self.to().origin();

        foreach(BiasSource *source, self.map().biasSources())
        {
            // If the source is too weak we will ignore it completely.
            if(source->intensity() <= 0)
                continue;

            Vector3d sourceToSurface = (source->origin() - self.center()).normalize();

            // Calculate minimum 2D distance to the segment.
            coord_t distance = 0;
            for(int k = 0; k < 2; ++k)
            {
                coord_t len = (Vector2d(source->origin()) - (!k? from : to)).length();
                if(k == 0 || len < distance)
                    distance = len;
            }

            if(sourceToSurface.dot(surface.normal()) < 0)
                continue;

            bsuf.addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
        }
    }
#endif
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

void Segment::updateBiasAfterGeometryMove(int group)
{
    if(BiasTracker *biasTracker = d->biasTracker(group, false /*don't allocate*/))
    {
        biasTracker->updateAllContributors();
    }
}

void Segment::applyBiasDigest(BiasDigest &changes)
{
    foreach(BiasTracker *biasTracker, d->biasTrackers)
    {
        biasTracker->applyChanges(changes);
    }
}

void Segment::lightBiasPoly(int group, int vertCount, rvertex_t const *positions,
    ColorRawf *colors)
{
    DENG_ASSERT(hasLineSide()); // sanity check

    BiasTracker *tracker = d->biasTracker(group);

    // Should we update?
    //if(devUpdateAffected)
    {
        d->updateAffected(*tracker, group);
    }

    Surface &surface = d->lineSide->middle();
    tracker->lightPoly(surface.normal(), map().biasCurrentTime(),
                       vertCount, positions, colors);
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
