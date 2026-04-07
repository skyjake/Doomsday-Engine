/** @file contactspreader.cpp  World object => BSP leaf "contact" spreader.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "world/contactspreader.h"

#include "world/contact.h"
#include "world/subsector.h"
#include "render/rend_main.h"  // Rend_mapSurfaceMaterialSpec
#include "resource/materialanimator.h"
#include "render/walledge.h"

#include <doomsday/mesh/face.h>
#include <de/legacy/vector1.h>

using namespace de;
using world::World;

/**
 * On which side of the half-edge does the specified @a point lie?
 *
 * @param hedge  Half-edge to test.
 * @param point  Point to test in the map coordinate space.
 *
 * @return @c <0 Point is to the left/back of the segment.
 *         @c =0 Point lies directly on the segment.
 *         @c >0 Point is to the right/front of the segment.
 */
static ddouble pointOnHEdgeSide(const mesh::HEdge &hedge, const Vec2d &point)
{
    const Vec2d direction = hedge.twin().origin() - hedge.origin();

    ddouble pointV1[2]      = { point.x, point.y };
    ddouble fromOriginV1[2] = { hedge.origin().x, hedge.origin().y };
    ddouble directionV1[2]  = { direction.x, direction.y };
    return V2d_PointOnLineSide(pointV1, fromOriginV1, directionV1);
}

struct ContactSpreader
{
    const world::Blockmap &_blockmap;
    BitArray *_spreadBlocks = nullptr;

    struct SpreadState
    {
        Contact *contact = nullptr;
        AABoxd contactBounds;
    };
    SpreadState _spread;

    ContactSpreader(const world::Blockmap &blockmap, BitArray *spreadBlocks = nullptr)
        : _blockmap(blockmap)
    {
        _spreadBlocks = spreadBlocks;
    }

    /**
     * Spread contacts in the blockmap to any touched neighbors.
     *
     * @param box   Map space region in which to perform spreading.
     */
    void spread(const AABoxd &box)
    {
        const world::BlockmapCellBlock cellBlock = _blockmap.toCellBlock(box);

        world::BlockmapCell cell;
        for(cell.y = cellBlock.min.y; cell.y < cellBlock.max.y; ++cell.y)
        for(cell.x = cellBlock.min.x; cell.x < cellBlock.max.x; ++cell.x)
        {
            if(_spreadBlocks)
            {
                // Should we skip this cell?
                int cellIndex = _blockmap.toCellIndex(cell.x, cell.y);
                if(_spreadBlocks->testBit(cellIndex))
                    continue;

                // Mark the cell as processed.
                _spreadBlocks->setBit(cellIndex, true);
            }

            _blockmap.forAllInCell(cell, [this] (void *element)
            {
                spreadContact(*static_cast<Contact *>(element));
                return LoopContinue;
            });
        }
    }

private:
    /**
     * Link the contact in all non-degenerate subspaces which touch the linked
     * object (tests are done with subspace bounding boxes and the spread test).
     *
     * @param contact  Contact to be spread.
     */
    void spreadContact(Contact &contact)
    {
        ConvexSubspace &subspace = contact.objectBspLeafAtOrigin().subspace().as<ConvexSubspace>();

        R_ContactList(subspace, contact.type()).link(&contact);

        // Spread to neighboring BSP leafs.
        subspace.setValidCount(++World::validCount);

        _spread.contact       = &contact;
        _spread.contactBounds = contact.objectBounds();

        spreadInSubspace(subspace);
    }

    void maybeSpreadOverEdge(mesh::HEdge *hedge)
    {
        DE_ASSERT(_spread.contact != 0);

        if (!hedge) return;

        auto &subspace = hedge->face().mapElementAs<ConvexSubspace>();
        auto &subsec   = subspace.subsector().as<Subsector>();

        // There must be a back BSP leaf to spread to.
        if (!hedge->hasTwin() || !hedge->twin().hasFace() || !hedge->twin().face().hasMapElement())
            return;

        auto &backSubspace = hedge->twin().face().mapElementAs<ConvexSubspace>();
        auto &backSubsec   = backSubspace.subsector().as<Subsector>();

        // Which way does the spread go?
        if (!(subspace.validCount() == World::validCount && backSubspace.validCount() != World::validCount))
        {
            return; // Not eligible for spreading.
        }

        // Is the leaf on the back side outside the origin's AABB?
        const AABoxd &bounds = backSubspace.poly().bounds();
        if (   bounds.maxX <= _spread.contactBounds.minX
            || bounds.minX >= _spread.contactBounds.maxX
            || bounds.maxY <= _spread.contactBounds.minY
            || bounds.minY >= _spread.contactBounds.maxY)
            return;

        // Too far from the edge?
        const coord_t length   = (hedge->twin().origin() - hedge->origin()).length();
        const coord_t distance = pointOnHEdgeSide(*hedge, _spread.contact->objectOrigin()) / length;
        if (abs(distance) >= _spread.contact->objectRadius())
            return;

        // Do not spread if the sector on the back side is closed with no height.
        if(!backSubsec.hasWorldVolume())
            return;

        if (   backSubsec.visCeiling().heightSmoothed() <= subsec.visFloor  ().heightSmoothed()
            || backSubsec.visFloor  ().heightSmoothed() >= subsec.visCeiling().heightSmoothed())
            return;

        // Are there line side surfaces which should prevent spreading?
        if (hedge->hasMapElement())
        {
            const LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();

            // On which side of the line are we? (distance is from segment to origin).
            const auto &facingLineSide = seg.line().side(seg.lineSide().sideId() ^ (distance < 0));

            // One-way window?
            if (!facingLineSide.back().hasSections())
                return;

            const Subsector &fromSubsec = facingLineSide.isFront() ? subsec : backSubsec;
            const Subsector &toSubsec   = facingLineSide.isFront() ? backSubsec : subsec;

            // Might a material cover the opening?
            if (facingLineSide.hasSections() && facingLineSide.middle().hasMaterial())
            {
                // Stretched middles always cover the opening.
                if (facingLineSide.isFlagged(SDF_MIDDLE_STRETCH))
                    return;

                // Determine the opening between the visual sector planes at this edge.
                coord_t openBottom;
                if (toSubsec.visFloor().heightSmoothed() > fromSubsec.visFloor().heightSmoothed())
                {
                    openBottom = toSubsec.visFloor().heightSmoothed();
                }
                else
                {
                    openBottom = fromSubsec.visFloor().heightSmoothed();
                }

                coord_t openTop;
                if (toSubsec.visCeiling().heightSmoothed() < fromSubsec.visCeiling().heightSmoothed())
                {
                    openTop = toSubsec.visCeiling().heightSmoothed();
                }
                else
                {
                    openTop = fromSubsec.visCeiling().heightSmoothed();
                }

                MaterialAnimator &matAnimator = *facingLineSide.middle().as<Surface>().materialAnimator();
                        //.as<ClientMaterial>().getAnimator(Rend_MapSurfaceMaterialSpec());

                // Ensure we have up to date info about the material.
                matAnimator.prepare();

                if (matAnimator.isOpaque() && matAnimator.dimensions().y >= openTop - openBottom)
                {
                    // Possibly; check the placement.
                    WallEdge edge(WallSpec::fromMapSide(facingLineSide.as<LineSide>(), LineSide::Middle),
                                     *facingLineSide.leftHEdge(), Line::From);

                    if (edge.isValid() && edge.top().z() > edge.bottom().z() &&
                        edge.top().z() >= openTop && edge.bottom().z() <= openBottom)
                        return;
                }
            }
        }

        // During the next step this contact will spread from the back leaf.
        backSubspace.setValidCount(World::validCount);

        R_ContactList(backSubspace, _spread.contact->type()).link(_spread.contact);

        spreadInSubspace(backSubspace);
    }

    /**
     * Attempt to spread the obj from the given contact from the source subspace
     * and into the (relative) back subsapce.
     *
     * @param subspace  Convex subspace to attempt to spread over to.
     *
     * @return  Always @c true. (This function is also used as an iterator.)
     */
    void spreadInSubspace(ConvexSubspace &subspace)
    {
        auto *base = subspace.poly().hedge();
        auto *hedge = base;
        do
        {
            maybeSpreadOverEdge(hedge);

        } while((hedge = &hedge->next()) != base);
    }
};

void spreadContacts(const world::Blockmap &blockmap, const AABoxd &region, BitArray *spreadBlocks)
{
    ContactSpreader(blockmap, spreadBlocks).spread(region);
}
