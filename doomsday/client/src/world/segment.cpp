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
#  include "world/map.h"
#  include "BiasIllum"
#  include "BiasSource"
#  include "BiasTracker"
#endif

#include "world/segment.h"

using namespace de;

#ifdef __CLIENT__
struct GeometryGroup
{
    typedef QList<BiasIllum> BiasIllums;

    uint biasLastUpdateFrame;
    BiasIllums biasIllums;
    BiasTracker biasTracker;
};

/// Geometry group identifier => group data.
typedef QMap<int, GeometryGroup> GeometryGroups;
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
    /// Bias lighting data for each geometry group (i.e., each Line::Side section).
    GeometryGroups geomGroups;
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

#ifdef __CLIENT__
    /**
     * Retrieve geometry data by it's associated unique @a group identifier.
     *
     * @param group     Geometry group identifier.
     * @param canAlloc  @c true= to allocate if no data exists.
     */
    GeometryGroup *geometryGroup(int group, bool canAlloc = true)
    {
        DENG_ASSERT(group >= 0 && group < 3); // sanity check
        DENG_ASSERT(self.hasLineSide()); // sanity check

        GeometryGroups::iterator foundAt = geomGroups.find(group);
        if(foundAt != geomGroups.end())
        {
            return &*foundAt;
        }

        if(!canAlloc) return 0;

        // Number of bias illumination points for this geometry. Presently we
        // define a 1:1 mapping to strip geometry vertices.
        int const numBiasIllums = 4;

        GeometryGroup &newGeomGroup = *geomGroups.insert(group, GeometryGroup());
        newGeomGroup.biasIllums.reserve(numBiasIllums);
        for(int i = 0; i < numBiasIllums; ++i)
        {
            newGeomGroup.biasIllums.append(BiasIllum(&newGeomGroup.biasTracker));
        }

        return &newGeomGroup;
    }

    /**
     * @todo This could be enhanced so that only the lights on the right
     * side of the surface are taken into consideration.
     */
    void updateBiasContributors(GeometryGroup &geomGroup, int sectionIndex)
    {
        DENG_UNUSED(sectionIndex);

        // If the data is already up to date, nothing needs to be done.
        uint lastChangeFrame = self.map().biasLastChangeOnFrame();
        if(geomGroup.biasLastUpdateFrame == lastChangeFrame)
            return;

        geomGroup.biasLastUpdateFrame = lastChangeFrame;

        geomGroup.biasTracker.clearContributors();

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

            geomGroup.biasTracker.addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
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
    if(GeometryGroup *geomGroup = d->geometryGroup(group, false /*don't allocate*/))
    {
        geomGroup->biasTracker.updateAllContributors();
    }
}

BiasTracker *Segment::biasTracker(int group)
{
    if(GeometryGroup *geomGroup = d->geometryGroup(group, false /*don't allocate*/))
    {
        return &geomGroup->biasTracker;
    }
    return 0;
}

void Segment::lightBiasPoly(int group, Vector3f const *posCoords, Vector4f *colorCoords)
{
    DENG_ASSERT(hasLineSide()); // sanity check
    DENG_ASSERT(posCoords != 0 && colorCoords != 0);

    int const sectionIndex   = group;
    GeometryGroup *geomGroup = d->geometryGroup(sectionIndex);

    // Should we update?
    if(devUpdateBiasContributors)
    {
        d->updateBiasContributors(*geomGroup, sectionIndex);
    }

    Surface const &surface = d->lineSide->surface(sectionIndex);
    uint const biasTime = map().biasCurrentTime();

    Vector3f const *posIt = posCoords;
    Vector4f *colorIt     = colorCoords;
    for(int i = 0; i < geomGroup->biasIllums.count(); ++i, posIt++, colorIt++)
    {
        *colorIt += geomGroup->biasIllums[i].evaluate(*posIt, surface.normal(), biasTime);
    }

    // Any changes from contributors will have now been applied.
    geomGroup->biasTracker.markIllumUpdateCompleted();
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
