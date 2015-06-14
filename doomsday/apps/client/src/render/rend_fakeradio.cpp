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

#include <cstring>
#include <QBitArray>
#include <de/concurrency.h>
#include <de/Vector>
#include <doomsday/console/var.h>
#include "clientapp.h"

#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "gl/sys_opengl.h"

#include "MaterialAnimator"
#include "MaterialVariantSpec"

#include "ConvexSubspace"
#include "Face"
#include "SectorCluster"
#include "Surface"
#include "WallEdge"
#include "world/lineowner.h"
#include "world/map.h"
#include "world/maputil.h"
#include "world/p_object.h"
#include "world/p_players.h"

#include "render/r_main.h"
#include "render/rend_main.h"
#include "render/rendpoly.h"
#include "render/shadowedge.h"

using namespace de;

#define MIN_OPEN                (.1f)

#define MINDIFF                 (8)  ///< Min plane height difference (world units).
#define INDIFF                  (8)  ///< Max plane height for indifference offset.

#define BOTTOM                  (0)
#define TOP                     (1)

struct edge_t
{
    Line *line;
    Sector *sector;
    dfloat length;
    binangle_t diff;
};

dint rendFakeRadio = true;              ///< cvar
dfloat rendFakeRadioDarkness = 1.2f;    ///< cvar
static byte devFakeRadioUpdate = true;  ///< cvar

static inline RenderSystem &rendSys()
{
    return ClientApp::renderSystem();
}

dfloat Rend_RadioCalcShadowDarkness(dfloat lightLevel)
{
    lightLevel += Rend_LightAdaptationDelta(lightLevel);
    return (0.6f - lightLevel * 0.4f) * 0.65f * ::rendFakeRadioDarkness;
}

/**
 * Set the vertex colors in the rendpoly.
 */
static void setRendpolyColor(Vector4f *colorCoords, duint num, dfloat darkness)
{
    DENG2_ASSERT(colorCoords);
    // Shadows are black.
    Vector4f const shadowColor(0, 0, 0, de::clamp(0.f, darkness, 1.f));
    for(duint i = 0; i < num; ++i)
    {
        colorCoords[i] = shadowColor;
    }
}

/**
 * Returns @c true if there is open space in the sector.
 */
static inline bool isSectorOpen(Sector const *sector)
{
    return (sector && sector->ceiling().height() > sector->floor().height());
}

/**
 * Set the rendpoly's X offset and texture size.
 *
 * @param lineLength  If negative; implies that the texture is flipped horizontally.
 * @param segOffset   Offset to the start of the segment.
 */
static inline dfloat calcTexCoordX(dfloat lineLength, dfloat segOffset)
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
static inline dfloat calcTexCoordY(dfloat z, dfloat bottom, dfloat top, dfloat texHeight)
{
    if(texHeight > 0) return top - z;
    return bottom - z;
}

/// @todo fixme: Should be rewritten to work at half-edge level.
/// @todo fixme: Should use the visual plane heights of sector clusters.
static void scanNeighbor(bool scanTop, LineSide const &side, edge_t *edge, bool toLeft)
{
    dint const SEP = 10;

    ClockDirection const direction = toLeft? Clockwise : Anticlockwise;
    Line *iter;
    binangle_t diff = 0;
    coord_t lengthDelta = 0, gap = 0;
    coord_t iFFloor, iFCeil;
    coord_t iBFloor, iBCeil;
    dint scanSecSide = side.sideId();
    Sector const *startSector = side.sectorPtr();
    Sector const *scanSector  = nullptr;
    bool stopScan = false;
    bool closed   = false;

    coord_t const fFloor = startSector->floor  ().heightSmoothed();
    coord_t const fCeil  = startSector->ceiling().heightSmoothed();

    // Retrieve the start owner node.
    LineOwner *own = side.line().vertexOwner(side.vertex((dint)!toLeft));
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
            scanSector = nullptr;

        // Pick plane heights for relative offset comparison.
        if(!stopScan)
        {
            iFFloor = iter->frontSector().floor  ().heightSmoothed();
            iFCeil  = iter->frontSector().ceiling().heightSmoothed();

            if(iter->hasBackSector())
            {
                iBFloor = iter->backSector().floor  ().heightSmoothed();
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
            edge->diff   = diff;
            edge->line   = iter;
            edge->sector = const_cast<Sector *>(scanSector);

            closed = false;
            if(side.isFront() && iter->hasBackSector())
            {
                if(scanTop)
                {
                    if(iBFloor >= fCeil)
                        closed = true;  // Compared to "this" sector anyway
                }
                else
                {
                    if(iBCeil <= fFloor)
                        closed = true;  // Compared to "this" sector anyway
                }
            }

            // Does this line's length contribute to the alignment of the texture on the
            // segment shadow edge being rendered?
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
                    gap += iter->length();  // Should we just mark it done instead?
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
                    gap += iter->length();  // Should we just mark it done instead?
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
                stopScan = true;  // no.
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
                           scanSector->floor().heightSmoothed() < startSector->ceiling().heightSmoothed())
                        {
                            stopScan = true;
                        }
                    }
                    else
                    {
                        if(scanSector->floor().heightSmoothed() != fFloor &&
                           scanSector->ceiling().heightSmoothed() > startSector->floor().heightSmoothed())
                        {
                            stopScan = true;
                        }
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

            // Skip into the back neighbor sector of the iter line if heights are within
            // the accepted range.
            if(scanSector && side.back().hasSector() &&
               scanSector != side.back().sectorPtr() &&
                (   ( scanTop && scanSector->ceiling().heightSmoothed() == startSector->ceiling().heightSmoothed())
                 || (!scanTop && scanSector->floor  ().heightSmoothed() == startSector->floor  ().heightSmoothed())))
            {
                // If the map is formed correctly, we should find a back neighbor attached
                // to this line. However, if this is not the case and a line which SHOULD
                // be two sided isn't, we need to check whether there is a valid neighbor.
                Line *backNeighbor = R_FindLineNeighbor(startSector, iter, own, !toLeft);

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

    // Now we've found the furthest coalignable neighbor, select the back neighbor if present
    // for "edge open-ness" comparison.
    if(edge->sector)  // the back sector of the coalignable neighbor.
    {
        // Since we have the details of the backsector already, simply get the next neighbor
        // (it IS the backneighbor).
        edge->line = R_FindLineNeighbor(edge->sector, edge->line,
                                        edge->line->vertexOwner(dint(edge->line->hasBackSector() && edge->line->backSectorPtr() == edge->sector) ^ (dint)!toLeft),
                                        !toLeft, &edge->diff);
    }
}

/// @todo fixme: Should use the visual plane heights of sector clusters.
static void scanNeighbors(shadowcorner_t top[2], shadowcorner_t bottom[2], LineSide const &side,
    edgespan_t spans[2], bool toLeft)
{
    if(side.line().isSelfReferencing()) return;

    coord_t fFloor = side.sector().floor  ().heightSmoothed();
    coord_t fCeil  = side.sector().ceiling().heightSmoothed();

    edge_t edges[2/*bottom, top*/]; std::memset(edges, 0, sizeof(edges));

    scanNeighbor(false, side, &edges[0], toLeft);
    scanNeighbor(true,  side, &edges[1], toLeft);

    for(duint i = 0; i < 2; ++i)
    {
        shadowcorner_t *corner = (i == 0 ? &bottom[(dint)!toLeft] : &top[(dint)!toLeft]);
        edge_t *edge     = &edges[i];
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
                corner->corner = dfloat( BANG_90 ) / edge->diff;
            }
            else
            {
                corner->corner = dfloat( edge->diff ) / BANG_90;
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
            else  // Ceiling.
            {
                corner->pOffset = corner->proximity->ceiling().heightSmoothed() - fCeil;
                corner->pHeight = corner->proximity->ceiling().heightSmoothed();
            }
        }
        else
        {
            corner->proximity = nullptr;
            corner->pOffset = 0;
            corner->pHeight = 0;
        }
    }
}

/**
 * To determine the dimensions of a shadow, we'll need to scan edges. Edges are composed
 * of aligned lines. It's important to note that the scanning is done separately for the
 * top/bottom edges (both in the left and right direction) and the left/right edges.
 *
 * The length of the top/bottom edges are returned in the array 'spans'.
 *
 * This may look like a complicated operation (performed for all wall polys) but in most
 * cases this won't take long. Aligned neighbours are relatively rare.
 */
static void scanEdges(shadowcorner_t topCorners[2], shadowcorner_t bottomCorners[2],
    shadowcorner_t sideCorners[2], edgespan_t spans[2], LineSide const &side)
{
    dint const lineSideId = side.sideId();

    std::memset(sideCorners, 0, sizeof(shadowcorner_t) * 2);

    // Find the sidecorners first: left and right neighbour.
    for(dint i = 0; i < 2; ++i)
    {
        binangle_t diff = 0;
        LineOwner *vo   = side.line().vertexOwner(i ^ lineSideId);
        Line *other     = R_FindSolidLineNeighbor(side.sectorPtr(), &side.line(), vo, CPP_BOOL(i), &diff);

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
                sideCorners[i].corner = dfloat( BANG_90 ) / diff;
            }
            else
            {
                sideCorners[i].corner = dfloat( diff ) / BANG_90;
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
    if(!::rendFakeRadio || ::levelFullBright) return;

    // Updates are disabled?
    if(!::devFakeRadioUpdate) return;

    // Sides without sectors don't need updating. $degenleaf
    if(!side.hasSector()) return;

    // Have already determined the shadow properties on this side?
    LineSideRadioData &frData = Rend_RadioDataForLineSide(side);
    if(frData.updateCount == R_FrameCount()) return;

    // Not yet - Calculate now.
    for(dint i = 0; i < 2; ++i)
    {
        frData.spans[i].length = side.line().length();
        frData.spans[i].shift  = 0;
    }

    scanEdges(frData.topCorners, frData.bottomCorners, frData.sideCorners, frData.spans, side);
    frData.updateCount = R_FrameCount();  // Mark as done.
}

struct rendershadowseg_params_t
{
    lightingtexid_t texture;
    bool horizontal;
    dfloat shadowMul;
    dfloat shadowDark;
    Vector2f texOrigin;
    Vector2f texDimensions;
    dfloat sectionWidth;
};

static void setTopShadowParams(rendershadowseg_params_t *p, dfloat shadowSize, dfloat shadowDark,
    coord_t top, coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil,
    LineSideRadioData const &frData)
{
    p->shadowDark      = shadowDark;
    p->shadowMul       = 1;
    p->horizontal      = false;
    p->texDimensions.y = shadowSize;
    p->texOrigin.y     = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
    p->sectionWidth    = sectionWidth;

    p->texture = LST_RADIO_OO;
    // Corners without a neighbor back sector
    if(frData.sideCorners[0].corner == -1 || frData.sideCorners[1].corner == -1)
    {
        // At least one corner faces outwards
        p->texture         = LST_RADIO_OO;
        p->texDimensions.x = frData.spans[TOP].length;
        p->texOrigin.x     = calcTexCoordX(frData.spans[TOP].length, frData.spans[TOP].shift + xOffset);

        if((frData.sideCorners[0].corner == -1 && frData.sideCorners[1].corner == -1) ||
           (frData.topCorners [0].corner == -1 && frData.topCorners [1].corner == -1))
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
                p->texOrigin.x     = calcTexCoordX(-frData.spans[TOP].length, frData.spans[TOP].shift + xOffset);
                p->texture         = LST_RADIO_OE;
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
        p->texOrigin.x     = calcTexCoordX(frData.spans[TOP].length, frData.spans[TOP].shift + xOffset);

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
                            p->texOrigin.y     = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
                        }
                    }
                }
                else if(-frData.topCorners[0].pOffset < 0 && -frData.topCorners[1].pOffset >= 0)
                {
                    // Must flip horizontally!
                    p->texture         = LST_RADIO_CO;
                    p->texDimensions.x = -frData.spans[TOP].length;
                    p->texOrigin.x     = calcTexCoordX(-frData.spans[TOP].length, frData.spans[TOP].shift + xOffset);

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
                            p->texOrigin.y     = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
                        }
                    }
                }
            }
            else
            {
                if(-frData.topCorners[0].pOffset < -MINDIFF)
                {
                    // Must flip horizontally!
                    p->texture         = LST_RADIO_OE;
                    p->texDimensions.x = -frData.spans[BOTTOM].length;
                    p->texOrigin.x     = calcTexCoordX(-frData.spans[BOTTOM].length, frData.spans[BOTTOM].shift + xOffset);
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
            p->texOrigin.x     = calcTexCoordX(-frData.spans[TOP].length, frData.spans[TOP].shift + xOffset);
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

static void setBottomShadowParams(rendershadowseg_params_t *p, dfloat shadowSize, dfloat shadowDark,
    coord_t top, coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil,
    LineSideRadioData const &frData)
{
    p->shadowDark      = shadowDark;
    p->shadowMul       = 1;
    p->horizontal      = false;
    p->texDimensions.y = -shadowSize;
    p->texOrigin.y     = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
    p->sectionWidth    = sectionWidth;

    p->texture = LST_RADIO_OO;
    // Corners without a neighbor back sector
    if(frData.sideCorners[0].corner == -1 || frData.sideCorners[1].corner == -1)
    {
        // At least one corner faces outwards
        p->texture         = LST_RADIO_OO;
        p->texDimensions.x = frData.spans[BOTTOM].length;
        p->texOrigin.x     = calcTexCoordX(frData.spans[BOTTOM].length, frData.spans[BOTTOM].shift + xOffset);

        if((frData.sideCorners  [0].corner == -1 && frData.sideCorners  [1].corner == -1) ||
           (frData.bottomCorners[0].corner == -1 && frData.bottomCorners[1].corner == -1) )
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(frData.sideCorners[1].corner == -1)  // right corner faces outwards
        {
            if(frData.bottomCorners[0].pOffset < 0 && frData.topCorners[0].pHeight > fFloor)
            {
                // Must flip horizontally!
                p->texDimensions.x = -frData.spans[BOTTOM].length;
                p->texOrigin.x     = calcTexCoordX(-frData.spans[BOTTOM].length, frData.spans[BOTTOM].shift + xOffset);
                p->texture         = LST_RADIO_OE;
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
        p->texOrigin.x     = calcTexCoordX(frData.spans[BOTTOM].length, frData.spans[BOTTOM].shift + xOffset);
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
                            p->texOrigin.y     = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
                        }
                    }
                }
                else if(frData.bottomCorners[0].pOffset < 0 && frData.bottomCorners[1].pOffset >= 0)
                {
                    // Must flip horizontally!
                    p->texture         = LST_RADIO_CO;
                    p->texDimensions.x = -frData.spans[BOTTOM].length;
                    p->texOrigin.x     = calcTexCoordX(-frData.spans[BOTTOM].length, frData.spans[BOTTOM].shift + xOffset);

                    if(shadowSize > frData.bottomCorners[1].pOffset)
                    {
                        if(frData.bottomCorners[1].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texDimensions.y = -frData.bottomCorners[1].pOffset;
                            p->texOrigin.y     = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
                        }
                    }
                }
            }
            else
            {
                if(frData.bottomCorners[0].pOffset < -MINDIFF)
                {
                    // Must flip horizontally!
                    p->texture         = LST_RADIO_OE;
                    p->texDimensions.x = -frData.spans[BOTTOM].length;
                    p->texOrigin.x     = calcTexCoordX(-frData.spans[BOTTOM].length, frData.spans[BOTTOM].shift + xOffset);
                }
                else if(frData.bottomCorners[1].pOffset < -MINDIFF)
                {
                    p->texture = LST_RADIO_OE;
                }
            }
        }
        else if(frData.bottomCorners[0].corner <= MIN_OPEN)  // Right Corner is Closed
        {
            if(frData.bottomCorners[0].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;

            // Must flip horizontally!
            p->texDimensions.x = -frData.spans[BOTTOM].length;
            p->texOrigin.x     = calcTexCoordX(-frData.spans[BOTTOM].length, frData.spans[BOTTOM].shift + xOffset);
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

static void setSideShadowParams(rendershadowseg_params_t *p, dfloat shadowSize, dfloat shadowDark,
    coord_t bottom, coord_t top, dint rightSide, bool haveBottomShadower, bool haveTopShadower,
    coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil, bool hasBackSector,
    coord_t bFloor, coord_t bCeil, coord_t lineLength, LineSideRadioData const &frData)
{
    p->shadowDark      = shadowDark;
    p->shadowMul       = de::cubed(frData.sideCorners[rightSide? 1 : 0].corner * .8f);
    p->horizontal      = true;
    p->texOrigin.y     = bottom - fFloor;
    p->texDimensions.y = fCeil - fFloor;
    p->sectionWidth    = sectionWidth;

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
                p->texOrigin.y     = bottom - fCeil;
                p->texDimensions.y = -(fCeil - fFloor);
                p->texture         = LST_RADIO_CO;
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
                p->texOrigin.y     = bottom - fCeil;
                p->texDimensions.y = -(fCeil - fFloor);
                p->texture         = LST_RADIO_CO;
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
                p->texOrigin.y     = bottom - fCeil;
                p->texDimensions.y = -(fCeil - fFloor);
                p->texture         = LST_RADIO_CO;
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
            p->texOrigin.y     = calcTexCoordY(top, fFloor, fCeil, p->texDimensions.y);
            p->texture         = LST_RADIO_CO;
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

static void quadTexCoords(Vector2f *tc, Vector3f const *rverts, dfloat wallLength,
    Vector3f const &texTopLeft, Vector3f const &texBottomRight,
    Vector2f const &texOrigin, Vector2f const &texDimensions, bool horizontal)
{
    if(horizontal)
    {
        // Special horizontal coordinates for wall shadows.
        tc[0].x = tc[2].x = rverts[0].x - texTopLeft.x + texOrigin.y / texDimensions.y;
        tc[0].y = tc[1].y = rverts[0].y - texTopLeft.y + texOrigin.x / texDimensions.x;

        tc[1].x = tc[0].x + (rverts[1].z - texBottomRight.z) / texDimensions.y;
        tc[3].x = tc[0].x + (rverts[3].z - texBottomRight.z) / texDimensions.y;
        tc[3].y = tc[0].y + wallLength / texDimensions.x;
        tc[2].y = tc[0].y + wallLength / texDimensions.x;
        return;
    }

    tc[0].x = tc[1].x = rverts[0].x - texTopLeft.x + texOrigin.x / texDimensions.x;
    tc[3].y = tc[1].y = rverts[0].y - texTopLeft.y + texOrigin.y / texDimensions.y;

    tc[3].x = tc[2].x = tc[0].x + wallLength / texDimensions.x;
    tc[2].y = tc[3].y + (rverts[1].z - rverts[0].z) / texDimensions.y;
    tc[0].y = tc[3].y + (rverts[3].z - rverts[2].z) / texDimensions.y;
}

static void drawWallSectionShadow(Vector3f const *posCoords, WallEdge const &leftEdge, WallEdge const &rightEdge,
    rendershadowseg_params_t const &wsParms)
{
    DENG2_ASSERT(posCoords);
    bool const mustSubdivide    = (leftEdge.divisionCount() || rightEdge.divisionCount());
    duint const realNumVertices = (mustSubdivide? 3 + leftEdge.divisionCount() + 3 + rightEdge.divisionCount() : 4);

    // Allocate enough for the divisions too.
    Vector4f *colors    = R_AllocRendColors   (realNumVertices);
    Vector2f *texCoords = R_AllocRendTexCoords(realNumVertices);

    quadTexCoords(texCoords, posCoords, wsParms.sectionWidth,
                  leftEdge.top().origin(), rightEdge.bottom().origin(),
                  wsParms.texOrigin, wsParms.texDimensions, wsParms.horizontal);

    setRendpolyColor(colors, 4, wsParms.shadowDark * wsParms.shadowMul);

    if(rendFakeRadio != 2)
    {
        // Write multiple polys depending on rend params.
        DrawListSpec listSpec;
        listSpec.group = ShadowGeom;
        listSpec.texunits[TU_PRIMARY] = GLTextureUnit(GL_PrepareLSTexture(wsParms.texture), gl::ClampToEdge, gl::ClampToEdge);
        DrawList &shadowList = rendSys().drawLists().find(listSpec);

        if(mustSubdivide)
        {
            // Need to swap indices around into fans set the position of the division vertices,
            // interpolate texcoords and color.
            duint const numLeftVerts  = 3 + leftEdge .divisionCount();
            duint const numRightVerts = 3 + rightEdge.divisionCount();

            Vector3f *rvertices = R_AllocRendVertices(realNumVertices);

            Vector2f origTexCoords[4]; std::memcpy(origTexCoords, texCoords, sizeof(Vector2f) * 4);
            Vector4f origColors[4];    std::memcpy(origColors,    colors,    sizeof(Vector4f) * 4);

            R_DivVerts     (rvertices, posCoords,     leftEdge, rightEdge);
            R_DivTexCoords (texCoords, origTexCoords, leftEdge, rightEdge);
            R_DivVertColors(colors,    origColors,    leftEdge, rightEdge);

            Store &buffer = rendSys().buffer();
            {
                duint base = buffer.allocateVertices(numRightVerts);
                DrawList::Indices indices;
                indices.resize(numRightVerts);
                for(duint i = 0; i < numRightVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords   [indices[i]] = rvertices[numLeftVerts + i];
                    buffer.colorCoords [indices[i]] = (colors  [numLeftVerts + i] * 255).toVector4ub();
                    buffer.texCoords[0][indices[i]] = texCoords[numLeftVerts + i];
                }
                shadowList.write(buffer, gl::TriangleFan, indices);
            }
            {
                duint base = buffer.allocateVertices(numLeftVerts);
                DrawList::Indices indices;
                indices.resize(numLeftVerts);
                for(duint i = 0; i < numLeftVerts; ++i)
                {
                    indices[i] = base + i;
                    buffer.posCoords   [indices[i]] = rvertices[i];
                    buffer.colorCoords [indices[i]] = (colors  [i] * 255).toVector4ub();
                    buffer.texCoords[0][indices[i]] = texCoords[i];
                }
                shadowList.write(buffer, gl::TriangleFan, indices);
            }

            R_FreeRendVertices(rvertices);
        }
        else
        {
            Store &buffer = rendSys().buffer();
            duint base = buffer.allocateVertices(4);
            DrawList::Indices indices;
            indices.resize(4);
            for(duint i = 0; i < 4; ++i)
            {
                indices[i] = base + i;
                buffer.posCoords   [indices[i]] = posCoords[i];
                buffer.colorCoords [indices[i]] = (colors  [i] * 255).toVector4ub();
                buffer.texCoords[0][indices[i]] = texCoords[i];
            }
            shadowList.write(buffer, gl::TriangleStrip, indices);
        }
    }

    R_FreeRendColors   (colors);
    R_FreeRendTexCoords(texCoords);
}

/// @todo fixme: Should use the visual plane heights of sector clusters.
void Rend_RadioWallSection(WallEdge const &leftEdge, WallEdge const &rightEdge,
    dfloat shadowDark, dfloat shadowSize)
{
    // Disabled?
    if(!::rendFakeRadio || ::levelFullBright) return;
    if(shadowSize <= 0) return;

    LineSide &side = leftEdge.mapLineSide();
    HEdge const *hedge = side.leftHEdge();
    SectorCluster const *cluster = &hedge->face().mapElementAs<ConvexSubspace>().cluster();
    SectorCluster const *backCluster = nullptr;

    if(leftEdge.spec().section != LineSide::Middle && hedge->twin().hasFace())
    {
        backCluster = hedge->twin().face().mapElementAs<ConvexSubspace>().clusterPtr();
    }

    bool const haveBottomShadower = cluster->visFloor  ().castsShadow();
    bool const haveTopShadower    = cluster->visCeiling().castsShadow();

    // Walls unaffected by floor and ceiling shadow casters receive no
    // side shadows either. We could do better here...
    if(!haveBottomShadower && !haveTopShadower)
        return;

    coord_t const lineLength    = side.line().length();
    coord_t const sectionOffset = leftEdge.mapLineSideOffset();
    coord_t const sectionWidth  = de::abs((rightEdge.origin() - leftEdge.origin()).length());

    LineSideRadioData &frData = Rend_RadioDataForLineSide(side);

    coord_t const fFloor = cluster->visFloor  ().heightSmoothed();
    coord_t const fCeil  = cluster->visCeiling().heightSmoothed();
    coord_t const bFloor = (backCluster? backCluster->visFloor  ().heightSmoothed() : 0);
    coord_t const bCeil  = (backCluster? backCluster->visCeiling().heightSmoothed() : 0);

    Vector3f rvertices[4] = {
         leftEdge.bottom().origin(),
            leftEdge.top().origin(),
        rightEdge.bottom().origin(),
           rightEdge.top().origin()
    };

    // Top Shadow?
    if(haveTopShadower)
    {
        if(rightEdge.top().z() > fCeil - shadowSize && leftEdge.bottom().z() < fCeil)
        {
            rendershadowseg_params_t parms;

            setTopShadowParams(&parms, shadowSize, shadowDark,
                               leftEdge.top().z(), sectionOffset, sectionWidth,
                               fFloor, fCeil, frData);
            drawWallSectionShadow(rvertices, leftEdge, rightEdge, parms);
        }
    }

    // Bottom Shadow?
    if(haveBottomShadower)
    {
        if(leftEdge.bottom().z() < fFloor + shadowSize && rightEdge.top().z() > fFloor)
        {
            rendershadowseg_params_t parms;

            setBottomShadowParams(&parms, shadowSize, shadowDark,
                                  leftEdge.top().z(), sectionOffset, sectionWidth,
                                  fFloor, fCeil, frData);
            drawWallSectionShadow(rvertices, leftEdge, rightEdge, parms);
        }
    }

    // Left Shadow?
    if(frData.sideCorners[0].corner > 0 && sectionOffset < shadowSize)
    {
        rendershadowseg_params_t parms;

        setSideShadowParams(&parms, shadowSize, shadowDark,
                            leftEdge.bottom().z(), leftEdge.top().z(), false,
                            haveBottomShadower, haveTopShadower,
                            sectionOffset, sectionWidth,
                            fFloor, fCeil, backCluster != 0, bFloor, bCeil, lineLength,
                            frData);
        drawWallSectionShadow(rvertices, leftEdge, rightEdge, parms);
    }

    // Right Shadow?
    if(frData.sideCorners[1].corner > 0 &&
       sectionOffset + sectionWidth > lineLength - shadowSize)
    {
        rendershadowseg_params_t parms;

        setSideShadowParams(&parms, shadowSize, shadowDark,
                            leftEdge.bottom().z(), leftEdge.top().z(), true,
                            haveBottomShadower, haveTopShadower, sectionOffset, sectionWidth,
                            fFloor, fCeil, backCluster != 0, bFloor, bCeil, lineLength,
                            frData);
        drawWallSectionShadow(rvertices, leftEdge, rightEdge, parms);
    }
}

/**
 * Determines whether FakeRadio flat, shadow geometry should be drawn between the vertices of
 * the given half-edges @a hEdges and prepares the ShadowEdges @a edges accordingly.
 *
 * @param edges             ShadowEdge descriptors for both edges { left, right }.
 * @param hEdges            Half-edge accessors for both edges { left, right }.
 * @param sectorPlaneIndex  Logical index of the sector plane to consider a shadow for.
 * @param shadowDark        Shadow darkness factor.
 *
 * @return  @c true if one or both edges are partially in shadow.
 */
static bool prepareFlatShadowEdges(ShadowEdge edges[2], HEdge const *hEdges[2], dint sectorPlaneIndex,
    dfloat shadowDark)
{
    DENG2_ASSERT(edges && hEdges && hEdges[0] && hEdges[1]);

    // If the sector containing the shadowing line section is fully closed (i.e., volume is
    // not positive) then skip shadow drawing entirely.
    /// @todo Encapsulate this logic in ShadowEdge -ds
    if(!hEdges[0]->hasFace() || !hEdges[0]->face().hasMapElement())
        return false;

    if(!hEdges[0]->face().mapElementAs<ConvexSubspace>().cluster().hasWorldVolume())
        return false;

    for(dint i = 0; i < 2; ++i)
    {
        edges[i].init(*hEdges[i], i);
        edges[i].prepare(sectorPlaneIndex);
    }
    return (edges[0].shadowStrength(shadowDark) >= .0001 && edges[1].shadowStrength(shadowDark) >= .0001);
}

#if 0
static inline dfloat flatShadowSize(dfloat sectorLight)
{
    return 2 * (8 + 16 - sectorlight * 16); /// @todo Make cvars out of constants.
}
#endif

static DrawList::Indices makeFlatShadowGeometry(Store &verts, gl::Primitive &primitive,
    ShadowEdge const edges[2], dfloat shadowDark, bool haveFloor)
{
    static duint const floorIndices[][4] = { { 0, 1, 2, 3 }, { 1, 2, 3, 0 } };
    static duint const ceilIndices [][4] = { { 0, 3, 2, 1 }, { 1, 0, 3, 2 } };

    static Vector4ub const white(255, 255, 255, 0);
    static Vector4ub const black(  0,   0,   0, 0);

    // What vertex winding order (0 = left, 1 = right)? (For best results, the cross edge
    // should always be the shortest.)
    duint const winding = (edges[1].length() > edges[0].length()? 1 : 0);
    duint const *idx    = (haveFloor ? floorIndices[winding] : ceilIndices[winding]);

    // Assign indices.
    duint base = verts.allocateVertices(4);
    DrawList::Indices indices;
    indices.resize(4);
    for(duint i = 0; i < 4; ++i)
    {
        indices[i] = base + i;
    }

    //
    // Build the geometry.
    //
    primitive = gl::TriangleFan;
    verts.posCoords[indices[idx[0]]] = edges[0].outer();
    verts.posCoords[indices[idx[1]]] = edges[1].outer();
    verts.posCoords[indices[idx[2]]] = edges[1].inner();
    verts.posCoords[indices[idx[3]]] = edges[0].inner();
    // Set uniform color.
    Vector4ub const &uniformColor = (::renderWireframe? white : black);  // White to assist visual debugging.
    for(duint i = 0; i < 4; ++i)
    {
        verts.colorCoords[indices[i]] = uniformColor;
    }
    // Set outer edge opacity:
    for(duint i = 0; i < 2; ++i)
    {
        verts.colorCoords[indices[idx[i]]].w = dbyte( edges[i].shadowStrength(shadowDark) * 255 );
    }

    return indices;
}

void Rend_RadioSubspaceEdges(ConvexSubspace const &subspace)
{
    if(!::rendFakeRadio) return;
    if(::levelFullBright) return;

    if(!subspace.shadowLineCount()) return;

    SectorCluster &cluster = subspace.cluster();

    // Determine the shadow properties.
    dfloat const shadowDark = Rend_RadioCalcShadowDarkness(cluster.lightSourceIntensity());
    // Any need to continue?
    if(shadowDark < .0001f) return;

    static ShadowEdge shadowEdges[2/*left, right*/];  // Keep these around (needed often).

    // Can skip drawing for Planes that do not face the viewer - find the 2D vector to subspace center.
    auto const eyeToSubspace = Vector2f(Rend_EyeOrigin().xz() - subspace.poly().center());

    // All shadow geometry uses the same texture (i.e., none) - use the same list.
    DrawList &shadowList     = rendSys().drawLists().find(DrawListSpec(::renderWireframe? UnlitGeom : ShadowGeom));

    // Process all LineSides linked to this subspace as potential shadow casters.
    subspace.forAllShadowLines([&cluster, &shadowDark, &eyeToSubspace, &shadowList] (LineSide &side)
    {
        DENG2_ASSERT(side.hasSections() && !side.line().definesPolyobj() && side.leftHEdge());

        // Process each only once per frame (we only want to draw a shadow set once).
        if(side.shadowVisCount() != R_FrameCount())
        {
            side.setShadowVisCount(R_FrameCount());  // Mark processed.

            for(dint pln = 0; pln < cluster.visPlaneCount(); ++pln)
            {
                Plane const &plane = cluster.visPlane(pln);

                // Some Planes should not receive FakeRadio shadowing.
                if(!plane.receivesShadow()) continue;

                // Skip Planes facing away from the viewer.
                if(Vector3f(eyeToSubspace, Rend_EyeOrigin().y - plane.heightSmoothed())
                        .dot(plane.surface().normal()) >= 0)
                {
                    HEdge const *hEdges[2/*left, right*/] = { side.leftHEdge(), side.leftHEdge() };

                    if(prepareFlatShadowEdges(shadowEdges, hEdges, pln, shadowDark))
                    {
                        bool const haveFloor = plane.surface().normal()[2] > 0;

                        // Make geometry.
                        Store &buffer = rendSys().buffer();
                        gl::Primitive primitive;
                        DrawList::Indices indices =
                            makeFlatShadowGeometry(buffer, primitive, shadowEdges, shadowDark, haveFloor);

                        // Skip drawing entirely?
                        if(::rendFakeRadio == 2) continue;

                        // Write geometry.
                        shadowList.write(buffer, primitive, indices);
                    }
                }
            }
        }
        return LoopContinue;
    });
}

#ifdef DENG_DEBUG
static void drawPoint(Vector3d const &point, dint radius, dfloat const color[4])
{
    viewdata_t const *viewData  = R_ViewData(::viewPlayer - ::ddPlayers);
    Vector3d const leftOff      = viewData->upVec + viewData->sideVec;
    Vector3d const rightOff     = viewData->upVec - viewData->sideVec;

    //Vector3d const viewToCenter = point - Rend_EyeOrigin();
    //dfloat scale = dfloat(viewToCenter.dot(viewData->frontVec)) /
    //                viewData->frontVec.dot(viewData->frontVec);

    Vector3d finalPos( point.x, point.z, point.y );

    // The final radius.
    dfloat radX = radius * 1;
    dfloat radY = radX / 1.2f;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glColor4fv(color);

    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex3d(finalPos.x + radX * leftOff.x,
                   finalPos.y + radY * leftOff.y,
                   finalPos.z + radX * leftOff.z);
        glTexCoord2f(1, 0);
        glVertex3d(finalPos.x + radX * rightOff.x,
                   finalPos.y + radY * rightOff.y,
                   finalPos.z + radX * rightOff.z);
        glTexCoord2f(1, 1);
        glVertex3d(finalPos.x - radX * leftOff.x,
                   finalPos.y - radY * leftOff.y,
                   finalPos.z - radX * leftOff.z);
        glTexCoord2f(0, 1);
        glVertex3d(finalPos.x - radX * rightOff.x,
                   finalPos.y - radY * rightOff.y,
                   finalPos.z - radX * rightOff.z);
    glEnd();
}

void Rend_DrawShadowOffsetVerts()
{
    static dfloat const red[4]    = { 1.f, .2f, .2f, 1.f };
    static dfloat const yellow[4] = { .7f, .7f, .2f, 1.f };

    if(!App_WorldSystem().hasMap()) return;
    Map &map = App_WorldSystem().map();

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC),
                            gl::ClampToEdge, gl::ClampToEdge);
    glEnable(GL_TEXTURE_2D);

    /// @todo fixme: Should use the visual plane heights of sector clusters.
    map.forAllLines([] (Line &line)
    {
        for(dint i = 0; i < 2; ++i)
        {
            Vertex &vtx = line.vertex(i);

            LineOwner const *base = vtx.firstLineOwner();
            LineOwner const *own  = base;
            do
            {
                Vector2d xy = vtx.origin() + own->extendedShadowOffset();
                coord_t z   = own->line().frontSector().floor().heightSmoothed();
                drawPoint(Vector3d(xy.x, xy.y, z), 1, yellow);

                xy = vtx.origin() + own->innerShadowOffset();
                drawPoint(Vector3d(xy.x, xy.y, z), 1, red);

                own = &own->next();
            } while(own != base);
        }
        return LoopContinue;
    });

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
