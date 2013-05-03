/** @file map/sectionedge.cpp World Map (Wall) Section Edge.
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

#include "Sector"

#include "map/r_world.h" // R_SideSectionCoords, R_GetVtxLineOwner
#include "map/lineowner.h"

#include "map/sectionedge.h"

using namespace de;

DENG2_PIMPL(SectionEdge)
{
    HEdge *hedge;
    int edge;
    int section;

    bool isValid;
    Vector2f materialOrigin;
    SectionEdge::Intercepts intercepts;

    Instance(Public *i, HEdge &hedge, int edge, int section)
        : Base(i),
          hedge(&hedge),
          edge(edge),
          section(section),
          isValid(false)
    {}

    void verifyValid() const
    {
        if(!isValid)
        {
            /// @throw InvalidError  Invalid range geometry was specified.
            throw SectionEdge::InvalidError("SectionEdge::verifyValid", "Range geometry is not valid (top < bottom)");
        }
    }

    /**
     * Ensure all intercepts do not exceed the specified range.
     */
    void assertInterceptsInRange(coord_t low, coord_t hi) const
    {
#ifdef DENG_DEBUG
        foreach(Intercept const &icpt, intercepts)
        {
            DENG2_ASSERT(icpt.distance >= low && icpt.distance <= hi);
        }
#else
        DENG2_UNUSED2(low, hi);
#endif
    }

    SectionEdge::Intercept *find(double distance)
    {
        for(int i = 0; i < intercepts.count(); ++i)
        {
            SectionEdge::Intercept &icpt = intercepts[i];
            if(de::fequal(icpt.distance, distance))
                return &icpt;
        }
        return 0;
    }

    bool intercept(double distance)
    {
        if(find(distance))
            return false;

        intercepts.append(SectionEdge::Intercept(distance));
        return true;
    }

    void addPlaneIntercepts(coord_t bottom, coord_t top)
    {
        DENG_ASSERT(top > bottom);

        Line::Side const &side = hedge->lineSide();
        if(side.line().isFromPolyobj()) return;

        // Check for neighborhood division?
        if(section == Line::Side::Middle && side.hasSections() && side.back().hasSections())
            return;

        // Only sections at line side edges can/should be split.
        if(!((hedge == side.leftHEdge()  && edge == HEdge::From) ||
             (hedge == side.rightHEdge() && edge == HEdge::To)))
            return;

        Sector const *frontSec = side.sectorPtr();

        LineOwner::Direction direction(edge? LineOwner::Previous : LineOwner::Next);
        // Retrieve the start owner node.
        LineOwner *base = R_GetVtxLineOwner(&side.line().vertex(edge), &side.line());
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
                                    if(intercept(plane.visHeight()))
                                    {
                                        // Have we reached the div limit?
                                        if(intercepts.count() == SECTIONEDGE_MAX_INTERCEPTS)
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

                // Stop the scan when a single sided line is reached.
                if(!iter->hasFrontSections() || !iter->hasBackSections())
                    stopScan = true;
            }
        } while(!stopScan);
    }
};

SectionEdge::SectionEdge(HEdge &hedge, int edge, int section)
    : d(new Instance(this, hedge, edge, section))
{
    DENG_ASSERT(hedge.hasLineSide() && hedge.lineSide().hasSections());
}

bool SectionEdge::isValid() const
{
    return d->isValid;
}

Line::Side &SectionEdge::lineSide() const
{
    return d->hedge->lineSide();
}

int SectionEdge::section() const
{
    return d->section;
}

Vector2d const &SectionEdge::origin() const
{
    return d->hedge->vertex(d->edge).origin();
}

coord_t SectionEdge::lineOffset() const
{
    return d->hedge->lineOffset() + (d->edge? d->hedge->length() : 0);
}

int SectionEdge::divisionCount() const
{
    return d->isValid? d->intercepts.count() - 2 : 0;
}

SectionEdge::Intercept &SectionEdge::bottom() const
{
    d->verifyValid();
    return d->intercepts.first();
}

SectionEdge::Intercept &SectionEdge::top() const
{
    d->verifyValid();
    return d->intercepts.last();
}

int SectionEdge::firstDivision() const
{
    d->verifyValid();
    return 1;
}

int SectionEdge::lastDivision() const
{
    d->verifyValid();
    return d->intercepts.count()-2;
}

Vector2f const &SectionEdge::materialOrigin() const
{
    d->verifyValid();
    return d->materialOrigin;
}

void SectionEdge::prepare()
{
    DENG_ASSERT(d->intercepts.isEmpty());

    Sector *frontSec, *backSec;
    d->hedge->wallSectionSectors(&frontSec, &backSec);

    coord_t bottom, top;
    R_SideSectionCoords(d->hedge->lineSide(), d->section, frontSec, backSec,
                        &bottom, &top, &d->materialOrigin);

    d->isValid = (top >= bottom);
    if(!d->isValid) return;

    d->materialOrigin.x += float(d->hedge->lineOffset());

    // Intercepts are sorted in ascending distance order.

    // The first intercept is the bottom.
    d->intercept(bottom);

    // Add intercepts for neighboring planes (the "divisions").
    d->addPlaneIntercepts(bottom, top);

    // The last intercept is the top.
    d->intercept(top);

    if(d->intercepts.count() > 2) // First two are always sorted.
    {
        // Sorting is required. This shouldn't take too long...
        // There seldom are more than two or three intercepts.
        qSort(d->intercepts.begin(), d->intercepts.end());
    }

    // Sanity check.
    d->assertInterceptsInRange(bottom, top);
}

SectionEdge::Intercepts const &SectionEdge::intercepts() const
{
    d->verifyValid();
    return d->intercepts;
}
