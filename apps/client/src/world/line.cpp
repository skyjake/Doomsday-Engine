/** @file line.cpp  World map line.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "world/line.h"
#include "world/maputil.h"
#include "world/plane.h"
#include "render/r_main.h"
#include "render/rend_fakeradio.h"

#include <doomsday/world/lineowner.h>
#include <doomsday/world/sector.h>

using namespace de;

void LineSide::updateRadioCorner(shadowcorner_t &sc, float openness, Plane *proximityPlane, bool top)
{
    DE_ASSERT(hasSector());
    sc.corner    = openness;
    sc.proximity = proximityPlane;
    if (sc.proximity)
    {
        // Determine relative height offsets (affects shadow map selection).
        sc.pHeight = sc.proximity->heightSmoothed();
        sc.pOffset = sc.pHeight - sector().plane(top? world::Sector::Ceiling : world::Sector::Floor)
            .as<Plane>().heightSmoothed();
    }
    else
    {
        sc.pOffset = 0;
        sc.pHeight = 0;
    }
}

void LineSide::setRadioEdgeSpan(bool top, bool right, double length)
{
    edgespan_t &span = radioData.spans[int(top)];
    span.length = length;
    if (!right)
    {
        span.shift = span.length;
    }
}

const shadowcorner_t &LineSide::radioCornerTop(bool right) const
{
    return radioData.topCorners[int(right)];
}

const shadowcorner_t &LineSide::radioCornerBottom(bool right) const
{
    return radioData.bottomCorners[int(right)];
}

const shadowcorner_t &LineSide::radioCornerSide(bool right) const
{
    return radioData.sideCorners[int(right)];
}

const edgespan_t &LineSide::radioEdgeSpan(bool top) const
{
    return radioData.spans[int(top)];
}

/**
 * Convert a corner @a angle to a "FakeRadio corner openness" factor.
 */
static float radioCornerOpenness(binangle_t angle)
{
    // Facing outwards?
    if (angle > BANG_180) return -1;

    // Precisely collinear?
    if (angle == BANG_180) return 0;

    // If the difference is too small consider it collinear (there won't be a shadow).
    if (angle < BANG_45 / 5) return 0;

    // 90 degrees is the largest effective difference.
    return (angle > BANG_90)? float( BANG_90 ) / angle : float( angle ) / BANG_90;
}

static inline binangle_t lineNeighborAngle(const LineSide &side, const Line *other, binangle_t diff)
{
    return (other && other != &side.line()) ? diff : 0 /*Consider it coaligned*/;
}

static binangle_t findSolidLineNeighborAngle(const LineSide &side, bool right)
{
    binangle_t diff = 0;
    const Line *other = R_FindSolidLineNeighbor(side.line(),
                                                *side.line().vertexOwner(int(right) ^ side.sideId()),
                                                right ? CounterClockwise : Clockwise, side.sectorPtr(), &diff);
    return lineNeighborAngle(side, other, diff);
}

/**
 * Returns @c true if there is open space in the sector.
 */
static inline bool sectorIsOpen(const world::Sector *sector)
{
    return (sector && sector->ceiling().height() > sector->floor().height());
}

struct edge_t
{
    Line *         line;
    world::Sector *sector;
    float          length;
    binangle_t     diff;
};

/// @todo fixme: Should be rewritten to work at half-edge level.
/// @todo fixme: Should use the visual plane heights of subsectors.
static void scanNeighbor(const LineSide &side, bool top, bool right, edge_t &edge)
{
    static const int SEP = 10;

    de::zap(edge);

    const ClockDirection direction   = (right ? CounterClockwise : Clockwise);
    const auto *         startSector = side.sectorPtr();
    const double         fFloor      = side.sector().floor().as<Plane>().heightSmoothed();
    const double         fCeil       = side.sector().ceiling().as<Plane>().heightSmoothed();

    double gap    = 0;
    auto *own = side.line().vertexOwner(side.vertex(int(right)));
    for (;;)
    {
        // Select the next line.
        binangle_t diff  = (direction == Clockwise ? own->angle() : own->prev()->angle());
        const Line *iter = &own->navigate(direction)->line().as<Line>();
        int scanSecSide = (iter->front().hasSector() && iter->front().sectorPtr() == startSector ? Line::Back : Line::Front);
        // Step selfreferencing lines.
        while ((!iter->front().hasSector() && !iter->back().hasSector()) || iter->isSelfReferencing())
        {
            own         = own->navigate(direction);
            diff       += (direction == Clockwise ? own->angle() : own->prev()->angle());
            iter        = &own->navigate(direction)->line().as<Line>();
            scanSecSide = (iter->front().sectorPtr() == startSector);
        }

        // Determine the relative backsector.
        const LineSide &scanSide = iter->side(scanSecSide).as<LineSide>();
        const auto *scanSector = scanSide.sectorPtr();

        // Select plane heights for relative offset comparison.
        const double iFFloor = iter->front().sector().floor  ().as<Plane>().heightSmoothed();
        const double iFCeil  = iter->front().sector().ceiling().as<Plane>().heightSmoothed();
        const auto * bsec    = iter->back().sectorPtr();
        const double iBFloor = (bsec ? bsec->floor  ().as<Plane>().heightSmoothed() : 0);
        const double iBCeil  = (bsec ? bsec->ceiling().as<Plane>().heightSmoothed() : 0);

        // Determine whether the relative back sector is closed.
        bool closed = false;
        if (side.isFront() && iter->back().hasSector())
        {
            closed = top? (iBFloor >= fCeil) : (iBCeil <= fFloor);  // Compared to "this" sector anyway.
        }

        // This line will attribute to this segment's shadow edge - remember it.
        edge.line   = const_cast<Line *>(iter);
        edge.diff   = diff;
        edge.sector = scanSide.sectorPtr();

        // Does this line's length contribute to the alignment of the texture on the
        // segment shadow edge being rendered?
        double lengthDelta = 0;
        if (top)
        {
            if (iter->back().hasSector()
                && (   (side.isFront() && iter->back().sectorPtr() == side.line().front().sectorPtr() && iFCeil >= fCeil)
                    || (side.isBack () && iter->back().sectorPtr() == side.line().back().sectorPtr () && iFCeil >= fCeil)
                    || (side.isFront() && closed == false && iter->back().sectorPtr() != side.line().front().sectorPtr()
                        && iBCeil >= fCeil && sectorIsOpen(iter->back().sectorPtr()))))
            {
                gap += iter->length();  // Should we just mark it done instead?
            }
            else
            {
                edge.length += iter->length() + gap;
                gap = 0;
            }
        }
        else
        {
            if (iter->back().hasSector()
                && (   (side.isFront() && iter->back().sectorPtr() == side.line().front().sectorPtr() && iFFloor <= fFloor)
                    || (side.isBack () && iter->back().sectorPtr() == side.line().back().sectorPtr () && iFFloor <= fFloor)
                    || (side.isFront() && closed == false && iter->back().sectorPtr() != side.line().front().sectorPtr()
                        && iBFloor <= fFloor && sectorIsOpen(iter->back().sectorPtr()))))
            {
                gap += iter->length();  // Should we just mark it done instead?
            }
            else
            {
                lengthDelta = iter->length() + gap;
                gap = 0;
            }
        }

        // Time to stop?
        if (iter == &side.line()) break;

        // Not coalignable?
        if (!(diff >= BANG_180 - SEP && diff <= BANG_180 + SEP)) break;

        // Perhaps a closed edge?
        if (scanSector)
        {
            if (!sectorIsOpen(scanSector)) break;

            // A height difference from the start sector?
            if (top)
            {
                if (scanSector->ceiling().as<Plane>().heightSmoothed() != fCeil
                    && scanSector->floor().as<Plane>().heightSmoothed() < startSector->ceiling().as<Plane>().heightSmoothed())
                {
                    break;
                }
            }
            else
            {
                if (scanSector->floor().as<Plane>().heightSmoothed() != fFloor
                    && scanSector->ceiling().as<Plane>().heightSmoothed() > startSector->floor().as<Plane>().heightSmoothed())
                {
                    break;
                }
            }
        }

        // Swap to the iter line's owner node (i.e., around the corner)?
        if (own->navigate(direction) == iter->v2Owner())
        {
            own = iter->v1Owner();
        }
        else if (own->navigate(direction) == iter->v1Owner())
        {
            own = iter->v2Owner();
        }

        // Skip into the back neighbor sector of the iter line if heights are within
        // the accepted range.
        if (scanSector && side.back().hasSector() && scanSector != side.back().sectorPtr()
            && (   ( top && scanSector->ceiling().as<Plane>().heightSmoothed() == startSector->ceiling().as<Plane>().heightSmoothed())
                || (!top && scanSector->floor  ().as<Plane>().heightSmoothed() == startSector->floor  ().as<Plane>().heightSmoothed())))
        {
            // If the map is formed correctly, we should find a back neighbor attached
            // to this line. However, if this is not the case and a line which *should*
            // be two sided isn't, we need to check whether there is a valid neighbor.
            Line *backNeighbor = R_FindLineNeighbor(*iter, *own, direction, startSector);

            if (backNeighbor && backNeighbor != iter)
            {
                // Into the back neighbor sector.
                own = own->navigate(direction);
                startSector = scanSector;
            }
        }

        // The last line was co-alignable so apply any length delta.
        edge.length += lengthDelta;
    }

    // Now we've found the furthest coalignable neighbor, select the back neighbor if
    // present for "edge open-ness" comparison.
    if (edge.sector)  // The back sector of the coalignable neighbor.
    {
        // Since we have the details of the backsector already, simply get the next
        // neighbor (it *is* the back neighbor).
        DE_ASSERT(edge.line);
        edge.line = R_FindLineNeighbor(*edge.line,
                                       *edge.line->vertexOwner(int(edge.line->back().hasSector() && edge.line->back().sectorPtr() == edge.sector) ^ int(right)),
                                       direction, edge.sector, &edge.diff);
    }
}

/**
 * To determine the dimensions of a shadow, we'll need to scan edges. Edges are composed
 * of aligned lines. It's important to note that the scanning is done separately for the
 * top/bottom edges (both in the left and right direction) and the left/right edges.
 *
 * The length of the top/bottom edges are returned in the array 'spans'.
 *
 * This may look like a complicated operation (performed for all line sides) but in most
 * cases this won't take long. Aligned neighbours are relatively rare.
 *
 * @todo fixme: Should use the visual plane heights of subsectors.
 */
void LineSide::updateRadioForFrame(int frameNumber)
{
    // Disabled completely?
    if (!::rendFakeRadio || ::levelFullBright) return;

    // Updates are disabled?
    if (!::devFakeRadioUpdate) return;

    // Sides without sectors don't need updating.
    if (!hasSector()) return;

    // Sides of self-referencing lines do not receive shadows. (Not worth it?).
    if (line().isSelfReferencing()) return;

    // Have already determined the shadow properties?
    if (radioData.updateFrame == frameNumber) return;
    radioData.updateFrame = frameNumber;  // Mark as done.

    // Process the side corners first.
    setRadioCornerSide(false/*left*/, radioCornerOpenness(findSolidLineNeighborAngle(*this, false/*left*/)));
    setRadioCornerSide(true/*right*/, radioCornerOpenness(findSolidLineNeighborAngle(*this, true/*right*/)));

    // Top and bottom corners are somewhat more complex as we must traverse neighbors
    // to find the extent of the coalignable surfaces for texture mapping/selection.
    for (int i = 0; i < 2; ++i)
    {
        const bool rightEdge = i != 0;

        edge_t bottom; scanNeighbor(*this, false/*bottom*/, rightEdge, bottom);
        edge_t top;    scanNeighbor(*this, true/*top*/    , rightEdge, top   );

        setRadioEdgeSpan(false/*left*/, rightEdge, line().length() + bottom.length);
        setRadioEdgeSpan(true/*right*/, rightEdge, line().length() + top   .length);

        setRadioCornerBottom(rightEdge, radioCornerOpenness(lineNeighborAngle(*this, bottom.line, bottom.diff)),
                             bottom.sector ? &bottom.sector->floor().as<Plane>() : nullptr);

        setRadioCornerTop   (rightEdge, radioCornerOpenness(lineNeighborAngle(*this, top   .line, top   .diff)),
                             top.sector ? &top.sector->ceiling().as<Plane>() : nullptr);
    }
}

bool Line::isShadowCaster() const
{
    if (definesPolyobj()) return false;
    if (isSelfReferencing()) return false;

    // Lines with no other neighbor do not qualify as shadow casters.
    if (&v1Owner()->next()->line() == this || &v2Owner()->next()->line() == this)
        return false;

    return true;
}
