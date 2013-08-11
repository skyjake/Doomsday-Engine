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

#include "HEdge"
#include "Line"

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
    /// Half-edge attributed to the line segment (not owned).
    HEdge *hedge;

#ifdef __CLIENT__
    /// Distance along the attributed map line at which the half-edge vertex occurs.
    coord_t lineSideOffset;

    /// Accurate length of the segment.
    coord_t length;

    /// Bias lighting data for each geometry group (i.e., each Line::Side section).
    GeometryGroups geomGroups;
    Flags flags;
#endif

    Instance(Public *i, HEdge *hedge)
        : Base(i),
          hedge(hedge)
#ifdef __CLIENT__
         ,lineSideOffset(0),
          length(0),
          flags(0)
#endif
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

        Surface const &surface = self.lineSide().middle();
        Vector2d const &from   = hedge->origin();
        Vector2d const &to     = hedge->twin().origin();
        Vector2d const center  = (from + to) / 2;

        foreach(BiasSource *source, self.map().biasSources())
        {
            // If the source is too weak we will ignore it completely.
            if(source->intensity() <= 0)
                continue;

            Vector3d sourceToSurface = (source->origin() - center).normalize();

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

Segment::Segment(Line::Side &lineSide, HEdge &hedge)
    : MapElement(DMU_SEGMENT, &lineSide),
      d(new Instance(this, &hedge))
{}

/*
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
*/

Line::Side &Segment::lineSide() const
{
    return *this->parent().as<Line::Side>();
}

#ifdef __CLIENT__

coord_t Segment::lineSideOffset() const
{
    return d->lineSideOffset;
}

void Segment::setLineSideOffset(coord_t newOffset)
{
    d->lineSideOffset = newOffset;
}

coord_t Segment::length() const
{
    return d->length;
}

void Segment::setLength(coord_t newLength)
{
    d->length = newLength;
}

Segment::Flags Segment::flags() const
{
    return d->flags;
}

void Segment::setFlags(Flags flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

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
    DENG_ASSERT(posCoords != 0 && colorCoords != 0);

    int const sectionIndex   = group;
    GeometryGroup *geomGroup = d->geometryGroup(sectionIndex);

    // Should we update?
    if(devUpdateBiasContributors)
    {
        d->updateBiasContributors(*geomGroup, sectionIndex);
    }

    Surface const &surface = lineSide().surface(sectionIndex);
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
