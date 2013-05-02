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
    WallDivs wallDivs; /// @todo does not belong here.

    HEdge *hedge;
    int edge;
    int section;

    bool isValid;
    Vector2f materialOrigin;
    int interceptCount;
    WallDivs::Intercept *firstIntercept;
    WallDivs::Intercept *lastIntercept;

    Instance(Public *i, HEdge &hedge, int edge, int section)
        : Base(i),
          hedge(&hedge),
          edge(edge),
          section(section),
          isValid(false), // Not yet prepared.
          interceptCount(0),
          firstIntercept(0),
          lastIntercept(0)
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
     * Ensure the divisions do not exceed the specified range.
     */
    void assertDivisionsInRange(coord_t low, coord_t hi) const
    {
#ifdef DENG_DEBUG
        if(wallDivs.isEmpty()) return;

        WallDivs::Intercept *icpt = &wallDivs.first();
        forever
        {
            DENG2_ASSERT(icpt->distance() >= low && icpt->distance() <= hi);

            if(!icpt->hasNext()) break;
            icpt = &icpt->next();
        }
#else
        DENG2_UNUSED2(low, hi);
#endif
    }

    void addPlaneIntercepts(coord_t bottom, coord_t top)
    {
        if(!hedge->hasLineSide()) return;

        Line::Side const &side = hedge->lineSide();
        if(side.line().isFromPolyobj()) return;

        // Check for neighborhood division?
        if(section == Line::Side::Middle && side.hasSections() && side.back().hasSections())
            return;

        // Only sections at line side edges can/should be split.
        if(!((hedge == side.leftHEdge()  && edge == HEdge::From) ||
             (hedge == side.rightHEdge() && edge == HEdge::To)))
            return;

        if(bottom >= top) return; // Obviously no division.

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
    return d->isValid? d->interceptCount - 2 : 0;
}

WallDivs::Intercept &SectionEdge::bottom() const
{
    d->verifyValid();
    return *d->firstIntercept;
}

WallDivs::Intercept &SectionEdge::top() const
{
    d->verifyValid();
    return *d->lastIntercept;
}

WallDivs::Intercept &SectionEdge::firstDivision() const
{
    d->verifyValid();
    return d->firstIntercept->next();
}

WallDivs::Intercept &SectionEdge::lastDivision() const
{
    d->verifyValid();
    return d->lastIntercept->prev();
}

Vector2f const &SectionEdge::materialOrigin() const
{
    d->verifyValid();
    return d->materialOrigin;
}

void SectionEdge::prepare()
{
    DENG_ASSERT(d->wallDivs.isEmpty());

    Sector *frontSec, *backSec;
    d->hedge->wallSectionSectors(&frontSec, &backSec);

    coord_t bottom, top;
    R_SideSectionCoords(d->hedge->lineSide(), d->section, frontSec, backSec,
                        &bottom, &top, &d->materialOrigin);

    d->isValid = (top >= bottom);
    if(!d->isValid) return;

    d->materialOrigin.x += float(d->hedge->lineOffset());

    // Intercepts are arranged in ascending distance order.

    // The first intercept is the bottom.
    d->wallDivs.intercept(bottom);

    // Add intercepts for neighboring planes (the "divisions").
    d->addPlaneIntercepts(bottom, top);

    // The last intercept is the top.
    d->wallDivs.intercept(top);

    if(d->wallDivs.count() > 2)
    {
        d->wallDivs.sort();
    }

    // Sanity check.
    d->assertDivisionsInRange(bottom, top);

    d->firstIntercept = &d->wallDivs.first();
    d->lastIntercept  = &d->wallDivs.last();
    d->interceptCount = d->wallDivs.count();
}
