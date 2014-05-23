/** @file rend_fakeradio.cpp  Faked Radiosity Lighting.
 *
 * @authors Copyright © 2004-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "render/rend_fakeradio.h"

#include "clientapp.h"
#include "gl/gl_main.h"

#include "world/map.h"
#include "world/lineowner.h"
#include "world/maputil.h"
#include "world/p_players.h"
#include "ConvexSubspace"
#include "Face"
#include "SectorCluster"
#include "Surface"

#include "render/rendersystem.h"
#include "render/r_main.h"
#include "render/rend_main.h"
#include "render/shadowedge.h"
#include "DrawLists"

#include <doomsday/console/var.h>
#include <de/Vector>

using namespace de;

#define MIN_OPEN (.1f)

#define MINDIFF  (8) // min plane height difference (world units)
#define INDIFF   (8) // max plane height for indifference offset

#define BOTTOM   (0)
#define TOP      (1)

// Cvars:
int rendFakeRadio = true;
static float rendFakeRadioDarkness = 1.2f;
static byte devFakeRadioUpdate = true;

/// @todo Make cvars out of constants.
float Rend_RadioCalcShadowDarkness(float lightLevel)
{
    lightLevel += Rend_LightAdaptationDelta(lightLevel);
    return (0.6f - lightLevel * 0.4f) * 0.65f * rendFakeRadioDarkness;
}

/**
 * Set the rendpoly's X offset and texture size.
 *
 * @param lineLength  If negative; implies that the texture is flipped horizontally.
 * @param segOffset   Offset to the start of the segment.
 */
static inline float calcTexCoordX(float lineLength, float segOffset)
{
    if(lineLength > 0) return segOffset;
    return lineLength + segOffset;
}

/**
 * Set the rendpoly's Y offset and texture size.
 *
 * @param z          Z height of the vertex.
 * @param bottom     Z height of the bottom of the wall section.
 * @param top        Z height of the top of the wall section.
 * @param texHeight  If negative; implies that the texture is flipped vertically.
 */
static inline float calcTexCoordY(float z, float bottom, float top, float texHeight)
{
    if(texHeight > 0) return top - z;
    return bottom - z;
}

/// @return  @c true iff there is open space in the sector.
static inline bool isSectorOpen(Sector const *sector)
{
    return (sector && sector->ceiling().height() > sector->floor().height());
}

struct edge_t
{
    dd_bool done;
    Line *line;
    Sector *sector;
    float length;
    binangle_t diff;
};

/// @todo fixme: Should be rewritten to work at half-edge level.
/// @todo fixme: Should use the visual plane heights of sector clusters.
static void scanNeighbor(bool scanTop, LineSide const &side, edge_t *edge, bool toLeft)
{
    int const SEP = 10;

    ClockDirection const direction = toLeft? Clockwise : Anticlockwise;
    Line *iter;
    binangle_t diff = 0;
    coord_t lengthDelta = 0, gap = 0;
    coord_t iFFloor, iFCeil;
    coord_t iBFloor, iBCeil;
    int scanSecSide = side.sideId();
    Sector const *startSector = side.sectorPtr();
    Sector const *scanSector;
    bool stopScan = false;
    bool closed;

    coord_t fFloor = startSector->floor().heightSmoothed();
    coord_t fCeil  = startSector->ceiling().heightSmoothed();

    // Retrieve the start owner node.
    LineOwner *own = side.line().vertexOwner(side.vertex((int)!toLeft));

    do
    {
        // Select the next line.
        diff = (direction == Clockwise? own->angle() : own->prev().angle());
        iter = &own->navigate(direction).line();

        scanSecSide = (iter->hasFrontSector() && iter->frontSectorPtr() == startSector)? Line::Back : Line::Front;

        // Step over selfreferencing lines.
        while((!iter->hasFrontSector() && !iter->hasBackSector()) || // $degenleaf
              iter->isSelfReferencing())
        {
            own = &own->navigate(direction);
            diff += (direction == Clockwise? own->angle() : own->prev().angle());
            iter = &own->navigate(direction).line();

            scanSecSide = (iter->frontSectorPtr() == startSector);
        }

        // Determine the relative backsector.
        if(iter->side(scanSecSide).hasSector())
            scanSector = iter->side(scanSecSide).sectorPtr();
        else
            scanSector = 0;

        // Pick plane heights for relative offset comparison.
        if(!stopScan)
        {
            iFFloor = iter->frontSector().floor().heightSmoothed();
            iFCeil  = iter->frontSector().ceiling().heightSmoothed();

            if(iter->hasBackSector())
            {
                iBFloor = iter->backSector().floor().heightSmoothed();
                iBCeil  = iter->backSector().ceiling().heightSmoothed();
            }
            else
                iBFloor = iBCeil = 0;
        }

        lengthDelta = 0;
        if(!stopScan)
        {
            // This line will attribute to this segment's shadow edge.
            // Store identity for later use.
            edge->diff = diff;
            edge->line = iter;
            edge->sector = const_cast<Sector *>(scanSector);

            closed = false;
            if(side.isFront() && iter->hasBackSector())
            {
                if(scanTop)
                {
                    if(iBFloor >= fCeil)
                        closed = true; // Compared to "this" sector anyway
                }
                else
                {
                    if(iBCeil <= fFloor)
                        closed = true; // Compared to "this" sector anyway
                }
            }

            // Does this line's length contribute to the alignment of the
            // texture on the segment shadow edge being rendered?
            if(scanTop)
            {
                if(iter->hasBackSector() &&
                   ((side.isFront() && iter->backSectorPtr() == side.line().frontSectorPtr() &&
                    iFCeil >= fCeil) ||
                   (side.isBack() && iter->backSectorPtr() == side.line().backSectorPtr() &&
                    iFCeil >= fCeil) ||
                    (side.isFront() && closed == false && iter->backSectorPtr() != side.line().frontSectorPtr() &&
                    iBCeil >= fCeil &&
                     isSectorOpen(iter->backSectorPtr()))))
                {
                    gap += iter->length(); // Should we just mark it done instead?
                }
                else
                {
                    edge->length += iter->length() + gap;
                    gap = 0;
                }
            }
            else
            {
                if(iter->hasBackSector() &&
                   ((side.isFront() && iter->backSectorPtr() == side.line().frontSectorPtr() &&
                    iFFloor <= fFloor) ||
                   (side.isBack() && iter->backSectorPtr() == side.line().backSectorPtr() &&
                    iFFloor <= fFloor) ||
                   (side.isFront() && closed == false && iter->backSectorPtr() != side.line().frontSectorPtr() &&
                    iBFloor <= fFloor &&
                    isSectorOpen(iter->backSectorPtr()))))
                {
                    gap += iter->length(); // Should we just mark it done instead?
                }
                else
                {
                    lengthDelta = iter->length() + gap;
                    gap = 0;
                }
            }
        }

        // Time to stop?
        if(iter == &side.line())
        {
            stopScan = true;
        }
        else
        {
            // Is this line coalignable?
            if(!(diff >= BANG_180 - SEP && diff <= BANG_180 + SEP))
            {
                stopScan = true; // no.
            }
            else if(scanSector)
            {
                // Perhaps its a closed edge?
                if(!isSectorOpen(scanSector))
                {
                    stopScan = true;
                }
                else
                {
                    // A height difference from the start sector?
                    if(scanTop)
                    {
                        if(scanSector->ceiling().heightSmoothed() != fCeil &&
                           scanSector->floor().heightSmoothed() <
                                startSector->ceiling().heightSmoothed())
                            stopScan = true;
                    }
                    else
                    {
                        if(scanSector->floor().heightSmoothed() != fFloor &&
                           scanSector->ceiling().heightSmoothed() >
                                startSector->floor().heightSmoothed())
                            stopScan = true;
                    }
                }
            }
        }

        // Swap to the iter line's owner node (i.e: around the corner)?
        if(!stopScan)
        {
            // Around the corner.
            if(&own->navigate(direction) == iter->v2Owner())
                own = iter->v1Owner();
            else if(&own->navigate(direction) == iter->v1Owner())
                own = iter->v2Owner();

            // Skip into the back neighbor sector of the iter line if
            // heights are within accepted range.
            if(scanSector && side.back().hasSector() &&
               scanSector != side.back().sectorPtr() &&
                ((scanTop && scanSector->ceiling().heightSmoothed() ==
                                startSector->ceiling().heightSmoothed()) ||
                 (!scanTop && scanSector->floor().heightSmoothed() ==
                                startSector->floor().heightSmoothed())))
            {
                // If the map is formed correctly, we should find a back
                // neighbor attached to this line. However, if this is not
                // the case and a line which SHOULD be two sided isn't, we
                // need to check whether there is a valid neighbor.
                Line *backNeighbor =
                    R_FindLineNeighbor(startSector, iter, own, !toLeft);

                if(backNeighbor && backNeighbor != iter)
                {
                    // Into the back neighbor sector.
                    own = &own->navigate(direction);
                    startSector = scanSector;
                }
            }

            // The last line was co-alignable so apply any length delta.
            edge->length += lengthDelta;
        }
    } while(!stopScan);

    // Now we've found the furthest coalignable neighbor, select the back
    // neighbor if present for "edge open-ness" comparison.
    if(edge->sector) // the back sector of the coalignable neighbor.
    {
        // Since we have the details of the backsector already, simply
        // get the next neighbor (it IS the backneighbor).
        edge->line =
            R_FindLineNeighbor(edge->sector, edge->line,
                               edge->line->vertexOwner(int(edge->line->hasBackSector() && edge->line->backSectorPtr() == edge->sector) ^ (int)!toLeft),
                               !toLeft, &edge->diff);
    }
}

/// @todo fixme: Should use the visual plane heights of sector clusters.
static void scanNeighbors(shadowcorner_t top[2], shadowcorner_t bottom[2],
    LineSide const &side, edgespan_t spans[2], bool toLeft)
{
    if(side.line().isSelfReferencing()) return;

    coord_t fFloor = side.sector().floor().heightSmoothed();
    coord_t fCeil  = side.sector().ceiling().heightSmoothed();

    edge_t edges[2]; // {bottom, top}
    std::memset(edges, 0, sizeof(edges));

    scanNeighbor(false, side, &edges[0], toLeft);
    scanNeighbor(true,  side, &edges[1], toLeft);

    for(uint i = 0; i < 2; ++i)
    {
        shadowcorner_t *corner = (i == 0 ? &bottom[(int)!toLeft] : &top[(int)!toLeft]);
        edge_t *edge = &edges[i];
        edgespan_t *span = &spans[i];

        // Increment the apparent line length/offset.
        span->length += edge->length;
        if(toLeft)
            span->shift += edge->length;

        // Compare the relative angle difference of this edge to determine
        // an "open-ness" factor.
        if(edge->line && edge->line != &side.line())
        {
            if(edge->diff > BANG_180)
            {
                // The corner between the walls faces outwards.
                corner->corner = -1;
            }
            else if(edge->diff == BANG_180)
            {
                // Perfectly coaligned? Great.
                corner->corner = 0;
            }
            else if(edge->diff < BANG_45 / 5)
            {
                // The difference is too small, there won't be a shadow.
                corner->corner = 0;
            }
            // 90 degrees is the largest effective difference.
            else if(edge->diff > BANG_90)
            {
                corner->corner = (float) BANG_90 / edge->diff;
            }
            else
            {
                corner->corner = (float) edge->diff / BANG_90;
            }
        }
        else
        {
            // Consider it coaligned.
            corner->corner = 0;
        }

        // Determine relative height offsets (affects shadow map selection).
        if(edge->sector)
        {
            corner->proximity = edge->sector;
            if(i == 0) // Floor.
            {
                corner->pOffset = corner->proximity->floor().heightSmoothed() - fFloor;
                corner->pHeight = corner->proximity->floor().heightSmoothed();
            }
            else // Ceiling.
            {
                corner->pOffset = corner->proximity->ceiling().heightSmoothed() - fCeil;
                corner->pHeight = corner->proximity->ceiling().heightSmoothed();
            }
        }
        else
        {
            corner->proximity = NULL;
            corner->pOffset = 0;
            corner->pHeight = 0;
        }
    }
}

/**
 * To determine the dimensions of a shadow, we'll need to scan edges. Edges
 * are composed of aligned lines. It's important to note that the scanning
 * is done separately for the top/bottom edges (both in the left and right
 * direction) and the left/right edges.
 *
 * The length of the top/bottom edges are returned in the array 'spans'.
 *
 * This may look like a complicated operation (performed for all wall polys)
 * but in most cases this won't take long. Aligned neighbours are relatively
 * rare.
 */
static void scanEdges(shadowcorner_t topCorners[2], shadowcorner_t bottomCorners[2],
    shadowcorner_t sideCorners[2], edgespan_t spans[2], LineSide const &side)
{
    int const lineSideId = side.sideId();

    std::memset(sideCorners, 0, sizeof(shadowcorner_t) * 2);

    // Find the sidecorners first: left and right neighbour.
    for(int i = 0; i < 2; ++i)
    {
        binangle_t diff = 0;
        LineOwner *vo = side.line().vertexOwner(i ^ lineSideId);

        Line *other = R_FindSolidLineNeighbor(side.sectorPtr(), &side.line(), vo,
                                              CPP_BOOL(i), &diff);
        if(other && other != &side.line())
        {
            if(diff > BANG_180)
            {
                // The corner between the walls faces outwards.
                sideCorners[i].corner = -1;
            }
            else if(diff == BANG_180)
            {
                sideCorners[i].corner = 0;
            }
            else if(diff < BANG_45 / 5)
            {
                // The difference is too small, there won't be a shadow.
                sideCorners[i].corner = 0;
            }
            else if(diff > BANG_90)
            {
                // 90 degrees is the largest effective difference.
                sideCorners[i].corner = (float) BANG_90 / diff;
            }
            else
            {
                sideCorners[i].corner = (float) diff / BANG_90;
            }
        }
        else
        {
            sideCorners[i].corner = 0;
        }

        scanNeighbors(topCorners, bottomCorners, side, spans, !i);
    }
}

void Rend_RadioUpdateForLineSide(LineSide &side)
{
    // Disabled completely?
    if(!rendFakeRadio || levelFullBright) return;

    // Updates are disabled?
    if(!devFakeRadioUpdate) return;

    // Sides without sectors don't need updating. $degenleaf
    if(!side.hasSector()) return;

    // Have already determined the shadow properties on this side?
    LineSideRadioData &frData = Rend_RadioDataForLineSide(side);
    if(frData.updateCount == R_FrameCount()) return;

    // Not yet - Calculate now.
    for(uint i = 0; i < 2; ++i)
    {
        frData.spans[i].length = side.line().length();
        frData.spans[i].shift = 0;
    }

    scanEdges(frData.topCorners, frData.bottomCorners, frData.sideCorners, frData.spans, side);
    frData.updateCount = R_FrameCount(); // Mark as done.
}

void rendershadowseg_params_t::setupForTop(float shadowSize, float shadowDark,
    coord_t top, coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil,
    LineSideRadioData const &frData)
{
    rendershadowseg_params_t *p = this;

    p->shadowDark = shadowDark;
    p->shadowMul = 1;
    p->horizontal = false;
    p->texDimensions.y = shadowSize;
    p->texOrigin.y = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
    p->sectionWidth = sectionWidth;

    p->texture = LST_RADIO_OO;
    // Corners without a neighbor back sector
    if(frData.sideCorners[0].corner == -1 || frData.sideCorners[1].corner == -1)
    {
        // At least one corner faces outwards
        p->texture = LST_RADIO_OO;
        p->texDimensions.x = frData.spans[TOP].length;
        p->texOrigin.x = calcTexCoordX(frData.spans[TOP].length, frData.spans[TOP].shift + xOffset);

        if((frData.sideCorners[0].corner == -1 && frData.sideCorners[1].corner == -1) ||
           (frData.topCorners[0].corner == -1 && frData.topCorners[1].corner == -1))
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(frData.sideCorners[1].corner == -1)
        {
            // right corner faces outwards
            if(-frData.topCorners[0].pOffset < 0 && frData.bottomCorners[0].pHeight < fCeil)
            {
                // Must flip horizontally!
                p->texDimensions.x = -frData.spans[TOP].length;
                p->texOrigin.x = calcTexCoordX(-frData.spans[TOP].length, frData.spans[TOP].shift + xOffset);
                p->texture = LST_RADIO_OE;
            }
        }
        else
        {
            // left corner faces outwards
            if(-frData.topCorners[1].pOffset < 0 && frData.bottomCorners[1].pHeight < fCeil)
            {
                p->texture = LST_RADIO_OE;
            }
        }
    }
    else
    {
        // Corners WITH a neighbor back sector
        p->texDimensions.x = frData.spans[TOP].length;
        p->texOrigin.x = calcTexCoordX(frData.spans[TOP].length, frData.spans[TOP].shift + xOffset);

        if(frData.topCorners[0].corner == -1 && frData.topCorners[1].corner == -1)
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(frData.topCorners[1].corner == -1 && frData.topCorners[0].corner > MIN_OPEN)
        {
            // Right corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        else if(frData.topCorners[0].corner == -1 && frData.topCorners[1].corner > MIN_OPEN)
        {
            // Left corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        // Open edges
        else if(frData.topCorners[0].corner <= MIN_OPEN && frData.topCorners[1].corner <= MIN_OPEN)
        {
            // Both edges are open
            p->texture = LST_RADIO_OO;
            if(frData.topCorners[0].proximity && frData.topCorners[1].proximity)
            {
                if(-frData.topCorners[0].pOffset >= 0 && -frData.topCorners[1].pOffset < 0)
                {
                    p->texture = LST_RADIO_CO;
                    // The shadow can't go over the higher edge.
                    if(shadowSize > -frData.topCorners[0].pOffset)
                    {
                        if(-frData.topCorners[0].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texDimensions.y = -frData.topCorners[0].pOffset;
                            p->texOrigin.y = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
                        }
                    }
                }
                else if(-frData.topCorners[0].pOffset < 0 && -frData.topCorners[1].pOffset >= 0)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_CO;
                    p->texDimensions.x = -frData.spans[TOP].length;
                    p->texOrigin.x = calcTexCoordX(-frData.spans[TOP].length,
                                                   frData.spans[TOP].shift + xOffset);

                    // The shadow can't go over the higher edge.
                    if(shadowSize > -frData.topCorners[1].pOffset)
                    {
                        if(-frData.topCorners[1].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texDimensions.y = -frData.topCorners[1].pOffset;
                            p->texOrigin.y = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
                        }
                    }
                }
            }
            else
            {
                if(-frData.topCorners[0].pOffset < -MINDIFF)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_OE;
                    p->texDimensions.x = -frData.spans[BOTTOM].length;
                    p->texOrigin.x = calcTexCoordX(-frData.spans[BOTTOM].length,
                                                   frData.spans[BOTTOM].shift + xOffset);
                }
                else if(-frData.topCorners[1].pOffset < -MINDIFF)
                {
                    p->texture = LST_RADIO_OE;
                }
            }
        }
        else if(frData.topCorners[0].corner <= MIN_OPEN)
        {
            if(-frData.topCorners[0].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;

            // Must flip horizontally!
            p->texDimensions.x = -frData.spans[TOP].length;
            p->texOrigin.x = calcTexCoordX(-frData.spans[TOP].length,
                                           frData.spans[TOP].shift + xOffset);
        }
        else if(frData.topCorners[1].corner <= MIN_OPEN)
        {
            if(-frData.topCorners[1].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;
        }
        else  // C/C ???
        {
            p->texture = LST_RADIO_OO;
        }
    }
}

void rendershadowseg_params_t::setupForBottom(float shadowSize, float shadowDark,
    coord_t top, coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil,
    LineSideRadioData const &frData)
{
    rendershadowseg_params_t *p = this;

    p->shadowDark = shadowDark;
    p->shadowMul = 1;
    p->horizontal = false;
    p->texDimensions.y = -shadowSize;
    p->texOrigin.y = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
    p->sectionWidth = sectionWidth;

    p->texture = LST_RADIO_OO;
    // Corners without a neighbor back sector
    if(frData.sideCorners[0].corner == -1 || frData.sideCorners[1].corner == -1)
    {
        // At least one corner faces outwards
        p->texture = LST_RADIO_OO;
        p->texDimensions.x = frData.spans[BOTTOM].length;
        p->texOrigin.x = calcTexCoordX(frData.spans[BOTTOM].length,
                                       frData.spans[BOTTOM].shift + xOffset);

        if((frData.sideCorners[0].corner == -1 && frData.sideCorners[1].corner == -1) ||
           (frData.bottomCorners[0].corner == -1 && frData.bottomCorners[1].corner == -1) )
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(frData.sideCorners[1].corner == -1) // right corner faces outwards
        {
            if(frData.bottomCorners[0].pOffset < 0 && frData.topCorners[0].pHeight > fFloor)
            {
                // Must flip horizontally!
                p->texDimensions.x = -frData.spans[BOTTOM].length;
                p->texOrigin.x = calcTexCoordX(-frData.spans[BOTTOM].length,
                                               frData.spans[BOTTOM].shift + xOffset);
                p->texture = LST_RADIO_OE;
            }
        }
        else
        {
            // left corner faces outwards
            if(frData.bottomCorners[1].pOffset < 0 && frData.topCorners[1].pHeight > fFloor)
            {
                p->texture = LST_RADIO_OE;
            }
        }
    }
    else
    {
        // Corners WITH a neighbor back sector
        p->texDimensions.x = frData.spans[BOTTOM].length;
        p->texOrigin.x = calcTexCoordX(frData.spans[BOTTOM].length,
                                       frData.spans[BOTTOM].shift + xOffset);
        if(frData.bottomCorners[0].corner == -1 && frData.bottomCorners[1].corner == -1)
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(frData.bottomCorners[1].corner == -1 && frData.bottomCorners[0].corner > MIN_OPEN)
        {
            // Right corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        else if(frData.bottomCorners[0].corner == -1 && frData.bottomCorners[1].corner > MIN_OPEN)
        {
            // Left corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        // Open edges
        else if(frData.bottomCorners[0].corner <= MIN_OPEN && frData.bottomCorners[1].corner <= MIN_OPEN)
        {
            // Both edges are open
            p->texture = LST_RADIO_OO;

            if(frData.bottomCorners[0].proximity && frData.bottomCorners[1].proximity)
            {
                if(frData.bottomCorners[0].pOffset >= 0 && frData.bottomCorners[1].pOffset < 0)
                {
                    p->texture = LST_RADIO_CO;
                    // The shadow can't go over the higher edge.
                    if(shadowSize > frData.bottomCorners[0].pOffset)
                    {
                        if(frData.bottomCorners[0].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texDimensions.y = -frData.bottomCorners[0].pOffset;
                            p->texOrigin.y = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
                        }
                    }
                }
                else if(frData.bottomCorners[0].pOffset < 0 && frData.bottomCorners[1].pOffset >= 0)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_CO;
                    p->texDimensions.x = -frData.spans[BOTTOM].length;
                    p->texOrigin.x = calcTexCoordX(-frData.spans[BOTTOM].length,
                                                   frData.spans[BOTTOM].shift + xOffset);

                    if(shadowSize > frData.bottomCorners[1].pOffset)
                    {
                        if(frData.bottomCorners[1].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texDimensions.y = -frData.bottomCorners[1].pOffset;
                            p->texOrigin.y = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
                        }
                    }
                }
            }
            else
            {
                if(frData.bottomCorners[0].pOffset < -MINDIFF)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_OE;
                    p->texDimensions.x = -frData.spans[BOTTOM].length;
                    p->texOrigin.x = calcTexCoordX(-frData.spans[BOTTOM].length,
                                                   frData.spans[BOTTOM].shift + xOffset);
                }
                else if(frData.bottomCorners[1].pOffset < -MINDIFF)
                {
                    p->texture = LST_RADIO_OE;
                }
            }
        }
        else if(frData.bottomCorners[0].corner <= MIN_OPEN) // Right Corner is Closed
        {
            if(frData.bottomCorners[0].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;

            // Must flip horizontally!
            p->texDimensions.x = -frData.spans[BOTTOM].length;
            p->texOrigin.x = calcTexCoordX(-frData.spans[BOTTOM].length,
                                           frData.spans[BOTTOM].shift + xOffset);
        }
        else if(frData.bottomCorners[1].corner <= MIN_OPEN)  // Left Corner is closed
        {
            if(frData.bottomCorners[1].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;
        }
        else  // C/C ???
        {
            p->texture = LST_RADIO_OO;
        }
    }
}

void rendershadowseg_params_t::setupForSide(float shadowSize, float shadowDark,
    coord_t bottom, coord_t top, int rightSide, bool haveBottomShadower, bool haveTopShadower,
    coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil,
    bool hasBackSector, coord_t bFloor, coord_t bCeil,
    coord_t lineLength, LineSideRadioData const &frData)
{
    rendershadowseg_params_t *p = this;

    p->shadowDark = shadowDark;
    p->shadowMul = frData.sideCorners[rightSide? 1 : 0].corner * .8f;
    p->shadowMul *= p->shadowMul * p->shadowMul;
    p->horizontal = true;
    p->texOrigin.y = bottom - fFloor;
    p->texDimensions.y = fCeil - fFloor;
    p->sectionWidth = sectionWidth;

    if(rightSide)
    {
        // Right shadow.
        p->texOrigin.x = -lineLength + xOffset;
        // Make sure the shadow isn't too big
        if(shadowSize > lineLength)
        {
            if(frData.sideCorners[0].corner <= MIN_OPEN)
                p->texDimensions.x = -lineLength;
            else
                p->texDimensions.x = -(lineLength / 2);
        }
        else
        {
            p->texDimensions.x = -shadowSize;
        }
    }
    else
    {
        // Left shadow.
        p->texOrigin.x = xOffset;
        // Make sure the shadow isn't too big
        if(shadowSize > lineLength)
        {
            if(frData.sideCorners[1].corner <= MIN_OPEN)
                p->texDimensions.x = lineLength;
            else
                p->texDimensions.x = lineLength / 2;
        }
        else
        {
            p->texDimensions.x = shadowSize;
        }
    }

    if(hasBackSector)
    {
        // There is a back sector.
        if(bFloor > fFloor && bCeil < fCeil)
        {
            if(haveBottomShadower && haveTopShadower)
            {
                p->texture = LST_RADIO_CC;
            }
            else if(!haveBottomShadower)
            {
                p->texOrigin.y = bottom - fCeil;
                p->texDimensions.y = -(fCeil - fFloor);
                p->texture = LST_RADIO_CO;
            }
            else
            {
                p->texture = LST_RADIO_CO;
            }
        }
        else if(bFloor > fFloor)
        {
            if(haveBottomShadower && haveTopShadower)
            {
                p->texture = LST_RADIO_CC;
            }
            else if(!haveBottomShadower)
            {
                p->texOrigin.y = bottom - fCeil;
                p->texDimensions.y = -(fCeil - fFloor);
                p->texture = LST_RADIO_CO;
            }
            else
            {
                p->texture = LST_RADIO_CO;
            }
        }
        else if(bCeil < fCeil)
        {
            if(haveBottomShadower && haveTopShadower)
            {
                p->texture = LST_RADIO_CC;
            }
            else if(!haveBottomShadower)
            {
                p->texOrigin.y = bottom - fCeil;
                p->texDimensions.y = -(fCeil - fFloor);
                p->texture = LST_RADIO_CO;
            }
            else
            {
                p->texture = LST_RADIO_CO;
            }
        }
    }
    else
    {
        if(!haveBottomShadower)
        {
            p->texDimensions.y = -(fCeil - fFloor);
            p->texOrigin.y = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
            p->texture = LST_RADIO_CO;
        }
        else if(!haveTopShadower)
        {
            p->texture = LST_RADIO_CO;
        }
        else
        {
            p->texture = LST_RADIO_CC;
        }
    }
}

static float shadowEdgeOpacity(float darkness, ShadowEdge const &edge)
{
    float const openness       = edge.openness();
    float const sectorOpenness = edge.sectorOpenness();

    float opacity = de::min(darkness * (1 - sectorOpenness), 1.f);
    if(openness < 1)
    {
        opacity *= 1 - openness;
    }
    return opacity;
}

static void writeShadowPrimitive(LineSide const &side, int planeIndex, float shadowDark)
{
    DENG2_ASSERT(side.hasSections());
    DENG2_ASSERT(!side.line().definesPolyobj());

    RenderSystem &rendSys = ClientApp::renderSystem();
    WorldVBuf &vbuf       = rendSys.worldVBuf();

    if(shadowDark < .0001f) return;

    // Only sides with a subspace edge cast shadow.
    if(!side.leftHEdge()) return;
    HEdge const &hedge = *side.leftHEdge();

    // Surfaces with missing, sky-masked or glowing materials aren't shadowed.
    Surface const &surface = side.sector().plane(planeIndex).surface();
    if(!surface.hasMaterial()) return;
    if(surface.material().isSkyMasked()) return;
    if(surface.material().hasGlow()) return;

    /// Skip shadowing in clusters with zero volume. (@todo Move to ShadowEdge -ds).
    if(!hedge.hasFace()) return;
    if(!hedge.face().hasMapElement()) return;
    if(!hedge.face().mapElementAs<ConvexSubspace>().cluster().hasWorldVolume())
        return;

    static ShadowEdge leftEdge; // this function is called often; keep these around
    static ShadowEdge rightEdge;

    leftEdge.init(hedge, Line::From);
    rightEdge.init(hedge, Line::To);

    leftEdge.prepare(planeIndex);
    rightEdge.prepare(planeIndex);

    if(leftEdge.sectorOpenness() >= 1 && rightEdge.sectorOpenness() >= 1)
        return;

    // Determine shadow edge opacity (a function of edge "openness").
    float const leftOpacity  = shadowEdgeOpacity(shadowDark, leftEdge);
    if(leftOpacity < .0001f) return;

    float const rightOpacity = shadowEdgeOpacity(shadowDark, rightEdge);
    if(rightOpacity < .0001f) return;

    // Process but don't draw?
    if(rendFakeRadio > 1) return;

    // This edge will be shadowed.

    static uint const floorIndices[][4] = {{0, 1, 2, 3}, {1, 2, 3, 0}};
    static uint const  ceilIndices[][4] = {{0, 3, 2, 1}, {1, 0, 3, 2}};

    // What vertex winding order? (0 = left, 1 = right)
    // (for best results, the cross edge should always be the shortest).
    uint const winding = (rightEdge.length() > leftEdge.length()? 1 : 0);
    bool const isFloor = surface.normal().z > 0;
    uint const *idx    = (isFloor? floorIndices[winding] : ceilIndices[winding]);

    // Shadows are black (unless we're in wireframe; use white for visual debugging).
    Vector3f const black(0, 0, 0), white(1, 1, 1);
    Vector3f const &color = !renderWireframe? black : white;

    WorldVBuf::Index *indices = rendSys.indicePool().alloc(4);

    vbuf.reserveElements(4, indices);
    vbuf[indices[idx[0]]].pos  = leftEdge.outer();
    vbuf[indices[idx[0]]].rgba = Vector4f(color, leftOpacity);

    vbuf[indices[idx[1]]].pos  = rightEdge.outer();
    vbuf[indices[idx[1]]].rgba = Vector4f(color, rightOpacity);

    vbuf[indices[idx[2]]].pos  = rightEdge.inner();
    vbuf[indices[idx[2]]].rgba = Vector4f(color, 0);

    vbuf[indices[idx[3]]].pos  = leftEdge.inner();
    vbuf[indices[idx[3]]].rgba = Vector4f(color, 0);

    //rendSys.drawLists().find(DrawListSpec(!renderWireframe? ShadowGeom : UnlitGeom))
    //            .write(gl::TriangleFan, 4, indices, vbuf);

    rendSys.indicePool().release(indices);
}

/**
 * @attention Do not use the global radio state in here, as @a subspace can be
 * part of any Sector, not the one chosen for wall rendering.
 */
void Rend_RadioSubspaceEdges(ConvexSubspace const &subspace)
{
    if(!rendFakeRadio) return;
    if(levelFullBright) return;

    ConvexSubspace::ShadowLines const &shadowLines = subspace.shadowLines();
    if(shadowLines.isEmpty()) return;

    SectorCluster &cluster = subspace.cluster();
    float const ambientLightLevel = cluster.lightSourceIntensity();

    // Determine the shadow darkness factor.
    float const shadowDark = Rend_RadioCalcShadowDarkness(ambientLightLevel);
    if(shadowDark < .0001f) return;

    Vector3f const eyeToSurface(vOrigin.xz() - subspace.poly().center());

    // We need to check all the shadow lines linked to this subspace.
    foreach(LineSide *side, shadowLines)
    {
        // Already drawn this frame?
        if(side->shadowVisCount() == R_FrameCount()) continue;
        side->setShadowVisCount(R_FrameCount());

        for(int planeIdx = 0; planeIdx < cluster.visPlaneCount(); ++planeIdx)
        {
            Plane const &plane = cluster.visPlane(planeIdx);
            if(Vector3f(eyeToSurface, vOrigin.y - plane.heightSmoothed())
                    .dot(plane.surface().normal()) >= 0)
            {
                writeShadowPrimitive(*side, planeIdx, shadowDark);
            }
        }
    }
}

#ifdef DENG2_DEBUG
static void drawPoint(Vector3d const &point, int radius, Vector4f const &color)
{
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
    Vector3d const leftOff     = viewData->upVec + viewData->sideVec;
    Vector3d const rightOff    = viewData->upVec - viewData->sideVec;

    // The final radius.
    float const radX = radius * 1;
    float const radY = radX / 1.2f;

    DENG2_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glColor4f(color.x, color.y, color.z, color.w);

    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex3d(point.x + radX * leftOff.x,
                   point.z + radY * leftOff.y,
                   point.y + radX * leftOff.z);
        glTexCoord2f(1, 0);
        glVertex3d(point.x + radX * rightOff.x,
                   point.z + radY * rightOff.y,
                   point.y + radX * rightOff.z);
        glTexCoord2f(1, 1);
        glVertex3d(point.x - radX * leftOff.x,
                   point.z - radY * leftOff.y,
                   point.y - radX * leftOff.z);
        glTexCoord2f(0, 1);
        glVertex3d(point.x - radX * rightOff.x,
                   point.z - radY * rightOff.y,
                   point.y - radX * rightOff.z);
    glEnd();
}

void Rend_DrawShadowOffsetVerts()
{
    static Vector4f const red(1, 0.2f, 0.2f, 1);
    static Vector4f const yellow(0.7f, 0.7f, 0.2f, 1);

    if(!App_WorldSystem().hasMap()) return;

    Map &map = App_WorldSystem().map();

    DENG2_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC),
                            gl::ClampToEdge, gl::ClampToEdge);

    glEnable(GL_TEXTURE_2D);

    /// @todo fixme: Should use the visual plane heights of sector clusters.
    foreach(Line *line, map.lines())
    for(int i = 0; i < 2; ++i)
    {
        Vertex const &vtx     = line->vertex(i);
        LineOwner const *base = vtx.firstLineOwner();
        LineOwner const *own  = base;
        do
        {
            coord_t z = own->line().frontSector().floor().heightSmoothed();

            drawPoint(Vector3d(vtx.origin() + own->extendedShadowOffset(), z), 1, yellow);
            drawPoint(Vector3d(vtx.origin() + own->innerShadowOffset   (), z), 1, red);

            own = &own->next();
        } while(own != base);
    }

    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
#endif

void Rend_RadioRegister()
{
    C_VAR_INT  ("rend-fakeradio",               &rendFakeRadio,         0, 0, 2);
    C_VAR_FLOAT("rend-fakeradio-darkness",      &rendFakeRadioDarkness, 0, 0, 2);
    C_VAR_BYTE ("rend-dev-fakeradio-update",    &devFakeRadioUpdate,    CVF_NO_ARCHIVE, 0, 1);
}
