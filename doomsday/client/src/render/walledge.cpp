/** @file render/walledge.cpp Wall Edge Geometry.
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

#include <QtAlgorithms>

#include "Sector"

#include "map/r_world.h" // R_SideSectionCoords
#include "map/lineowner.h"

#include "render/rend_main.h" // rendLightWallAngleSmooth

#include "render/walledge.h"

using namespace de;

WallEdge::Intercept::Intercept(WallEdge *owner, double distance)
    : IHPlane::IIntercept(distance),
      _owner(owner)
{}

WallEdge::Intercept::Intercept(Intercept const &other)
    : IHPlane::IIntercept(other.distance()),
      _owner(other._owner) /// @todo Should not copy owner
{}

WallEdge &WallEdge::Intercept::owner() const
{
    return *_owner;
}

Vector3d WallEdge::Intercept::origin() const
{
    return Vector3d(owner().origin(), distance());
}

DENG2_PIMPL(WallEdge), public IHPlane
{
    Line::Side *lineSide;
    int section;
    int edge;
    coord_t lineOffset;
    Vertex *lineVertex;

    bool isValid;
    Vector2f materialOrigin;
    Vector3f edgeNormal;

    /// Data for the IHPlane model.
    struct HPlane
    {
        /// The partition line.
        Partition partition;

        /// Intercept points along the half-plane.
        WallEdge::Intercepts intercepts;

        /// Set to @c true when @var intercepts requires sorting.
        bool needSortIntercepts;

        HPlane() : needSortIntercepts(false) {}

        HPlane(HPlane const & other)
            : partition         (other.partition),
              intercepts        (other.intercepts),
              needSortIntercepts(other.needSortIntercepts)
        {}
    } hplane;

    Instance(Public *i, Line::Side *lineSide, int section, int edge,
             coord_t lineOffset, Vertex *lineVertex)
        : Base(i),
          lineSide(lineSide),
          section(section),
          edge(edge),
          lineOffset(lineOffset),
          lineVertex(lineVertex),
          isValid(false)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i),
          lineSide      (other.lineSide),
          section       (other.section),
          edge          (other.edge),
          lineOffset    (other.lineOffset),
          lineVertex    (other.lineVertex),
          isValid       (other.isValid),
          materialOrigin(other.materialOrigin),
          edgeNormal    (other.edgeNormal),
          hplane        (other.hplane)
    {}

    void verifyValid() const
    {
        if(!isValid)
        {
            /// @throw InvalidError  Invalid range geometry was specified.
            throw WallEdge::InvalidError("WallEdge::verifyValid", "Range geometry is not valid (top < bottom)");
        }
    }

    // Implements IHPlane
    void configure(Partition const &newPartition)
    {
        clearIntercepts();
        hplane.partition = newPartition;
    }

    // Implements IHPlane
    Partition const &partition() const
    {
        return hplane.partition;
    }

    WallEdge::Intercept *find(double distance)
    {
        for(int i = 0; i < hplane.intercepts.count(); ++i)
        {
            WallEdge::Intercept &icpt = hplane.intercepts[i];
            if(de::fequal(icpt.distance(), distance))
                return &icpt;
        }
        return 0;
    }

    // Implements IHPlane
    WallEdge::Intercept const *intercept(double distance)
    {
        if(find(distance))
            return 0;

        hplane.intercepts.append(WallEdge::Intercept(&self, distance));
        WallEdge::Intercept *newIntercept = &hplane.intercepts.last();

        // The addition of a new intercept means we'll need to resort.
        hplane.needSortIntercepts = true;
        return newIntercept;
    }

    // Implements IHPlane
    void sortAndMergeIntercepts()
    {
        // Any work to do?
        if(!hplane.needSortIntercepts) return;

        qSort(hplane.intercepts.begin(), hplane.intercepts.end());

        hplane.needSortIntercepts = false;
    }

    // Implements IHPlane
    void clearIntercepts()
    {
        hplane.intercepts.clear();
        // An empty intercept list is logically sorted.
        hplane.needSortIntercepts = false;
    }

    // Implements IHPlane
    WallEdge::Intercept const &at(int index) const
    {
        if(index >= 0 && index < interceptCount())
        {
            return hplane.intercepts[index];
        }
        /// @throw IHPlane::UnknownInterceptError The specified intercept index is not valid.
        throw IHPlane::UnknownInterceptError("HPlane2::at", QString("Index '%1' does not map to a known intercept (count: %2)")
                                                                .arg(index).arg(interceptCount()));
    }

    // Implements IHPlane
    int interceptCount() const
    {
        return hplane.intercepts.count();
    }

#ifdef DENG_DEBUG
    void printIntercepts() const
    {
        uint index = 0;
        foreach(WallEdge::Intercept const &icpt, hplane.intercepts)
        {
            LOG_DEBUG(" %u: >%1.2f ") << (index++) << icpt.distance();
        }
    }
#endif

    /**
     * Ensure all intercepts do not exceed the specified range.
     */
    void assertInterceptsInRange(coord_t low, coord_t hi) const
    {
#ifdef DENG_DEBUG
        foreach(WallEdge::Intercept const &icpt, hplane.intercepts)
        {
            DENG2_ASSERT(icpt.distance() >= low && icpt.distance() <= hi);
        }
#else
        DENG2_UNUSED2(low, hi);
#endif
    }

    void addPlaneIntercepts(coord_t bottom, coord_t top)
    {
        DENG_ASSERT(top > bottom);

        // Retrieve the start owner node.
        LineOwner *base = lineSide->line().vertexOwner(*lineVertex);
        if(!base) return;

        // Check for neighborhood division?
        if(section == Line::Side::Middle && lineSide->back().hasSector())
            return;

        Sector const *frontSec = lineSide->sectorPtr();
        LineOwner *own = base;
        bool stopScan = false;
        do
        {
            own = &own->navigate(edge? Anticlockwise : Clockwise);

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
                    if(!i && iter->hasFrontSector() && iter->frontSectorPtr() != frontSec)
                        scanSec = iter->frontSectorPtr();
                    else if(i && iter->hasBackSector() && iter->backSectorPtr() != frontSec)
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
                                    if(intercept(plane.visHeight()))
                                    {
                                        // Have we reached the div limit?
                                        if(interceptCount() == WALLEDGE_MAX_INTERCEPTS)
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
                                if(intercept(z))
                                {
                                    // All clipped away.
                                    stopScan = true;
                                }
                            }
                        }
                    }
                } while(!stopScan && ++i < 2);

                // Stop the scan when a side with no back sector is reached.
                if(!iter->hasFrontSector() || !iter->hasBackSector())
                    stopScan = true;
            }
        } while(!stopScan);
    }

private:
    Instance &operator = (Instance const &); // no assignment
};

WallEdge::WallEdge(Line::Side &lineSide, int section, int edge,
                   coord_t lineOffset, Vertex &lineVertex)
    : d(new Instance(this, &lineSide, section, edge, lineOffset, &lineVertex))
{
    DENG_ASSERT(lineSide.hasSections());
}

WallEdge::WallEdge(HEdge &hedge, int section, int edge)
    : d(new Instance(this, &hedge.lineSide(), section, edge,
                           hedge.lineOffset() + (edge? hedge.length() : 0),
                           edge? &hedge.twin().vertex() : &hedge.vertex()))
{
    DENG_ASSERT(hedge.hasLineSide() && hedge.lineSide().hasSections());
}

WallEdge::WallEdge(WallEdge const &other)
    : d(new Instance(this, *other.d))
{}

bool WallEdge::isValid() const
{
    return d->isValid;
}

Line::Side &WallEdge::lineSide() const
{
    return *d->lineSide;
}

Surface &WallEdge::surface() const
{
    return d->lineSide->surface(d->section);
}

int WallEdge::section() const
{
    return d->section;
}

Vector2d const &WallEdge::origin() const
{
    return d->partition().origin;
}

coord_t WallEdge::lineOffset() const
{
    return d->lineOffset;
}

int WallEdge::divisionCount() const
{
    return d->isValid? d->interceptCount() - 2 : 0;
}

WallEdge::Intercept const &WallEdge::bottom() const
{
    d->verifyValid();
    return d->hplane.intercepts[0];
}

WallEdge::Intercept const &WallEdge::top() const
{
    d->verifyValid();
    return d->hplane.intercepts[d->interceptCount()-1];
}

int WallEdge::firstDivision() const
{
    d->verifyValid();
    return 1;
}

int WallEdge::lastDivision() const
{
    d->verifyValid();
    return d->interceptCount()-2;
}

WallEdge::Intercept const &WallEdge::at(int index) const
{
    d->verifyValid();
    return d->hplane.intercepts[index];
}

WallEdge::Intercepts const &WallEdge::intercepts() const
{
    d->verifyValid();
    return d->hplane.intercepts;
}

/**
 * Determines whether normal smoothing should be performed for the given pair of
 * map surfaces (which are assumed to share an edge).
 *
 * Yes if the angle between the two lines is less than 45 degrees.
 * @todo Should be user customizable with a Material property. -ds
 *
 * @param sufA       The "left"  map surface which shares an edge with @a sufB.
 * @param sufB       The "right" map surface which shares an edge with @a sufA.
 * @param angleDiff  Angle difference (i.e., normal delta) between the two surfaces.
 */
static bool shouldSmoothNormals(Surface &sufA, Surface &sufB, binangle_t angleDiff)
{
    DENG_UNUSED(sufA);
    DENG_UNUSED(sufB);
    return INRANGE_OF(angleDiff, BANG_180, BANG_45);
}

/**
 * Find the neighbor surface for the specified edge which we will use to calculate
 * the smoothed edge normal.
 *
 * @todo: Use the half-edge rings instead of LineOwners.
 */
static Surface *findBlendNeighbor(Line::Side const &side, int section, int edge,
                                  binangle_t *diff = 0)
{
    // Is smoothing disabled?
    if(!rendLightWallAngleSmooth)
        return 0;

    // Polyobj lines have no owner rings.
    if(side.line().definesPolyobj())
        return 0;

    LineOwner const *farVertOwner = side.line().vertexOwner(side.lineSideId() ^ edge);

    if(diff) *diff = 0;

    Line *neighbor;
    if(R_SideBackClosed(side))
    {
        neighbor = R_FindSolidLineNeighbor(side.sectorPtr(), &side.line(), farVertOwner, edge, diff);
    }
    else
    {
        neighbor = R_FindLineNeighbor(side.sectorPtr(), &side.line(), farVertOwner, edge, diff);
    }

    // No suitable line neighbor?
    if(!neighbor) return 0;

    // Choose the correct side of the neighbor (determined by which vertex is shared).
    Line::Side *otherSide;
    if(&neighbor->vertex(edge ^ 1) == &side.vertex(edge))
        otherSide = &neighbor->front();
    else
        otherSide = &neighbor->back();

    // We can only smooth if the neighbor has a surface.
    if(!otherSide->hasSections()) return 0;

    return &otherSide->surface(section);
}

void WallEdge::prepare()
{
    coord_t bottom, top;
    R_SideSectionCoords(*d->lineSide, d->section, &bottom, &top, &d->materialOrigin);

    d->isValid = (top >= bottom);
    if(!d->isValid) return;

    d->materialOrigin.x += d->lineOffset;

    d->configure(Partition(d->lineVertex->origin(), Vector2d(0, top - bottom)));

    // Intercepts are sorted in ascending distance order.

    // The first intercept is the bottom.
    d->intercept(bottom);

    if(top > bottom)
    {
        // Add intercepts for neighboring planes (the "divisions").
        d->addPlaneIntercepts(bottom, top);
    }

    // The last intercept is the top.
    d->intercept(top);

    if(d->interceptCount() > 2) // First two are always sorted.
    {
        // Sorting may be required. This shouldn't take too long...
        // There seldom are more than two or three intercepts.
        d->sortAndMergeIntercepts();
    }

    // Sanity check.
    d->assertInterceptsInRange(bottom, top);

    /**
     * Determine the edge normal.
     * @todo Do not assume the neighbor is the middle section of @var otherSide.
     * @todo Cache the smoothed normal value somewhere.
     */
    Surface &surface = d->lineSide->surface(d->section);
    binangle_t angleDiff;
    Surface *blendSurface = findBlendNeighbor(*d->lineSide, d->section, d->edge, &angleDiff);

    if(blendSurface && shouldSmoothNormals(surface, *blendSurface, angleDiff))
    {
        // Average normals.
        d->edgeNormal = Vector3f(surface.normal() + blendSurface->normal()) / 2;
    }
    else
    {
        d->edgeNormal = surface.normal();
    }
}

Vector2f const &WallEdge::materialOrigin() const
{
    d->verifyValid();
    return d->materialOrigin;
}

Vector3f const &WallEdge::normal() const
{
    d->verifyValid();
    return d->edgeNormal;
}
