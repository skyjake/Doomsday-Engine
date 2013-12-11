/** @file rend_fakeradio.cpp Faked Radiosity Lighting.
 *
 * @authors Copyright &copy; 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cstring>

#include <QBitArray>

#include <de/Vector>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"
#include "clientapp.h"

#include "gl/gl_texmanager.h"
#include "gl/sys_opengl.h"
#include "MaterialSnapshot"
#include "MaterialVariantSpec"

#include "Face"

#include "world/map.h"
#include "world/maputil.h"
#include "world/lineowner.h"
#include "BspLeaf"

#include "WallEdge"
#include "render/rendpoly.h"
#include "render/shadowedge.h"

#include "render/rend_fakeradio.h"

using namespace de;

#define MIN_OPEN                (.1f)

#define MINDIFF                 (8) // min plane height difference (world units)
#define INDIFF                  (8) // max plane height for indifference offset

#define BOTTOM                  (0)
#define TOP                     (1)

typedef struct edge_s {
    boolean done;
    Line *line;
    Sector *sector;
    float length;
    binangle_t diff;
} edge_t;

static void scanEdges(shadowcorner_t topCorners[2], shadowcorner_t bottomCorners[2],
    shadowcorner_t sideCorners[2], edgespan_t spans[2], LineSide const &side);

int rendFakeRadio = true; ///< cvar
float rendFakeRadioDarkness = 1.2f; ///< cvar

static byte devFakeRadioUpdate = true; ///< cvar

void Rend_RadioRegister()
{
    C_VAR_INT  ("rend-fakeradio",               &rendFakeRadio,         0, 0, 2);
    C_VAR_FLOAT("rend-fakeradio-darkness",      &rendFakeRadioDarkness, 0, 0, 2);

    C_VAR_BYTE ("rend-dev-fakeradio-update",    &devFakeRadioUpdate,    CVF_NO_ARCHIVE, 0, 1);
}

float Rend_RadioCalcShadowDarkness(float lightLevel)
{
    lightLevel += Rend_LightAdaptationDelta(lightLevel);
    return (0.6f - lightLevel * 0.4f) * 0.65f * rendFakeRadioDarkness;
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

/**
 * Set the vertex colors in the rendpoly.
 */
static void setRendpolyColor(Vector4f *colorCoords, uint num, float darkness)
{
    DENG_ASSERT(colorCoords != 0);
    // Shadows are black.
    Vector4f const shadowColor(0, 0, 0, de::clamp(0.f, darkness, 1.f));

    Vector4f *colorIt = colorCoords;
    for(uint i = 0; i < num; ++i, colorIt++)
    {
        *colorIt = shadowColor;
    }
}

/// @return  @c true, if there is open space in the sector.
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

/// @todo fixme: Should be rewritten to work at half-edge level.
/// @todo fixme: Should use the visual plane heights of sector clusters.
static void scanNeighbor(bool scanTop, LineSide const &side, edge_t *edge,
                         bool toLeft)
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

typedef struct {
    lightingtexid_t texture;
    bool horizontal;
    float shadowMul;
    float shadowDark;
    Vector2f texOrigin;
    Vector2f texDimensions;
    float sectionWidth;
} rendershadowseg_params_t;

static void setTopShadowParams(rendershadowseg_params_t *p, float shadowSize,
    float shadowDark, coord_t top, coord_t xOffset, coord_t sectionWidth,
    coord_t fFloor, coord_t fCeil,
    LineSideRadioData const &frData)
{
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

static void setBottomShadowParams(rendershadowseg_params_t *p, float shadowSize,
    float shadowDark, coord_t top, coord_t xOffset, coord_t sectionWidth,
    coord_t fFloor, coord_t fCeil,
    LineSideRadioData const &frData)
{
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

static void setSideShadowParams(rendershadowseg_params_t *p, float shadowSize,
    float shadowDark, coord_t bottom, coord_t top, int rightSide,
    bool haveBottomShadower, bool haveTopShadower,
    coord_t xOffset, coord_t sectionWidth,
    coord_t fFloor, coord_t fCeil,
    bool hasBackSector, coord_t bFloor, coord_t bCeil,
    coord_t lineLength, LineSideRadioData const &frData)
{
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

static void quadTexCoords(Vector2f *tc, Vector3f const *rverts, float wallLength,
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

static void drawWallSectionShadow(Vector3f const *origVertices,
    WallEdge const &leftEdge, WallEdge const &rightEdge,
    rendershadowseg_params_t const &wsParms)
{
    DENG_ASSERT(origVertices);
    bool const mustSubdivide = (leftEdge.divisionCount() || rightEdge.divisionCount());

    uint realNumVertices = 4;
    if(mustSubdivide)
        realNumVertices = 3 + leftEdge.divisionCount() + 3 + rightEdge.divisionCount();
    else
        realNumVertices = 4;

    // Allocate enough for the divisions too.
    Vector2f *rtexcoords = R_AllocRendTexCoords(realNumVertices);
    Vector4f *rcolors = R_AllocRendColors(realNumVertices);

    quadTexCoords(rtexcoords, origVertices, wsParms.sectionWidth,
                  leftEdge.top().origin(), rightEdge.bottom().origin(),
                  wsParms.texOrigin, wsParms.texDimensions, wsParms.horizontal);

    setRendpolyColor(rcolors, 4, wsParms.shadowDark * wsParms.shadowMul);

    if(rendFakeRadio != 2)
    {
        // Write multiple polys depending on rend params.
        DrawListSpec listSpec;
        listSpec.group = ShadowGeom;
        listSpec.texunits[TU_PRIMARY] =
            GLTextureUnit(GL_PrepareLSTexture(wsParms.texture), gl::ClampToEdge, gl::ClampToEdge);

        DrawList &shadowList = ClientApp::renderSystem().drawLists().find(listSpec);

        if(mustSubdivide)
        {
            /*
             * Need to swap indices around into fans set the position
             * of the division vertices, interpolate texcoords and
             * color.
             */

            Vector3f *rvertices = R_AllocRendVertices(realNumVertices);

            Vector2f origTexCoords[4];
            std::memcpy(origTexCoords, rtexcoords, sizeof(Vector2f) * 4);

            Vector4f origColors[4];
            std::memcpy(origColors, rcolors, sizeof(Vector4f) * 4);

            R_DivVerts(rvertices, origVertices, leftEdge, rightEdge);
            R_DivTexCoords(rtexcoords, origTexCoords, leftEdge, rightEdge);
            R_DivVertColors(rcolors, origColors, leftEdge, rightEdge);

            shadowList.write(gl::TriangleFan,
                             BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                             Vector2f(1, 1), Vector2f(0, 0),
                             0, 3 + rightEdge.divisionCount(),
                             rvertices  + 3 + leftEdge.divisionCount(),
                             rcolors    + 3 + leftEdge.divisionCount(),
                             rtexcoords + 3 + leftEdge.divisionCount())
                      .write(gl::TriangleFan,
                             BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                             Vector2f(1, 1), Vector2f(0, 0),
                             0, 3 + leftEdge.divisionCount(),
                             rvertices, rcolors, rtexcoords);

            R_FreeRendVertices(rvertices);
        }
        else
        {
            shadowList.write(gl::TriangleStrip,
                             BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                             Vector2f(1, 1), Vector2f(0, 0),
                             0, 4,
                             origVertices, rcolors, rtexcoords);
        }
    }

    R_FreeRendTexCoords(rtexcoords);
    R_FreeRendColors(rcolors);
}

/// @todo fixme: Should use the visual plane heights of sector clusters.
void Rend_RadioWallSection(WallEdge const &leftEdge, WallEdge const &rightEdge,
    float shadowDark, float shadowSize)
{
    // Disabled?
    if(!rendFakeRadio || levelFullBright)
        return;

    if(shadowSize <= 0)
        return;

    LineSide &side = leftEdge.mapLineSide();
    HEdge const *hedge = side.leftHEdge();
    SectorCluster const *cluster = &hedge->face().mapElementAs<BspLeaf>().cluster();
    SectorCluster const *backCluster = 0;

    if(leftEdge.spec().section != LineSide::Middle && hedge->twin().hasFace())
    {
        backCluster = hedge->twin().face().mapElementAs<BspLeaf>().clusterPtr();
    }

    bool const haveBottomShadower = Rend_RadioPlaneCastsShadow(cluster->visFloor());
    bool const haveTopShadower    = Rend_RadioPlaneCastsShadow(cluster->visCeiling());

    // Walls unaffected by floor and ceiling shadow casters receive no
    // side shadows either. We could do better here...
    if(!haveBottomShadower && !haveTopShadower)
        return;

    coord_t const lineLength    = side.line().length();
    coord_t const sectionOffset = leftEdge.mapLineSideOffset();
    coord_t const sectionWidth  = de::abs(Vector2d(rightEdge.origin() - leftEdge.origin()).length());

    LineSideRadioData &frData = Rend_RadioDataForLineSide(side);

    coord_t const fFloor = cluster->visFloor().heightSmoothed();
    coord_t const fCeil  = cluster->visCeiling().heightSmoothed();
    coord_t const bFloor = (backCluster? backCluster->visFloor().heightSmoothed() : 0);
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
        if(rightEdge.top().z() > fCeil - shadowSize
           && leftEdge.bottom().z() < fCeil)
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
        if(leftEdge.bottom().z() < fFloor + shadowSize
           && rightEdge.top().z() > fFloor)
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
 * Construct and write a new shadow polygon to the rendering lists.
 */
static void writeShadowSection2(ShadowEdge const &leftEdge, ShadowEdge const &rightEdge,
    bool isFloor, float shadowDark)
{
    static uint const floorIndices[][4] = {{0, 1, 2, 3}, {1, 2, 3, 0}};
    static uint const ceilIndices[][4]  = {{0, 3, 2, 1}, {1, 0, 3, 2}};

    float const outerLeftAlpha  = de::min(shadowDark * (1 - leftEdge.sectorOpenness()), 1.f);
    float const outerRightAlpha = de::min(shadowDark * (1 - rightEdge.sectorOpenness()), 1.f);

    if(!(outerLeftAlpha > .0001 && outerRightAlpha > .0001)) return;

    // What vertex winding order? (0 = left, 1 = right)
    // (for best results, the cross edge should always be the shortest).
    uint winding = (rightEdge.length() > leftEdge.length()? 1 : 0);
    uint const *idx = (isFloor ? floorIndices[winding] : ceilIndices[winding]);

    Vector3f rvertices[4];

    // Left outer.
    rvertices[idx[0]] = leftEdge.outer();

    // Right outer.
    rvertices[idx[1]] = rightEdge.outer();

    // Right inner.
    rvertices[idx[2]] = rightEdge.inner();

    // Left inner.
    rvertices[idx[3]] = leftEdge.inner();

    Vector4f rcolors[4];
    if(renderWireframe)
    {
        // Draw shadow geometry white to assist visual debugging.
        static const Vector4f white(1, 1, 1, 1);
        for(uint i = 0; i < 4; ++i)
        {
            rcolors[idx[i]] = white;
        }
    }

    // Left outer.
    rcolors[idx[0]].w = outerLeftAlpha;
    if(leftEdge.openness() < 1)
        rcolors[idx[0]].w *= 1 - leftEdge.openness();

    // Right outer.
    rcolors[idx[1]].w = outerRightAlpha;
    if(rightEdge.openness() < 1)
        rcolors[idx[1]].w *= 1 - rightEdge.openness();

    if(rendFakeRadio == 2) return;

    ClientApp::renderSystem().drawLists()
              .find(DrawListSpec(renderWireframe? UnlitGeom : ShadowGeom))
                  .write(gl::TriangleFan,
                         BM_NORMAL, Vector2f(1, 1), Vector2f(0, 0),
                         Vector2f(1, 1), Vector2f(0, 0),
                         0, 4, rvertices, rcolors);
}

static void writeShadowSection(int planeIndex, LineSide const &side, float shadowDark)
{
    DENG_ASSERT(side.hasSections());
    DENG_ASSERT(!side.line().definesPolyobj());

    if(!(shadowDark > .0001)) return;

    if(!side.leftHEdge()) return;

    HEdge const *leftHEdge = side.leftHEdge();
    Plane const &plane     = side.sector().plane(planeIndex);
    Surface const *suf     = &plane.surface();

    // Surfaces with a missing material don't shadow.
    if(!suf->hasMaterial()) return;

    // Missing, glowing or sky-masked materials are exempted.
    Material const &material = suf->material();
    if(material.isSkyMasked() || material.hasGlow()) return;

    // If the sector containing the shadowing line section is fully closed (i.e., volume
    // is not positive) then skip shadow drawing entirely.
    /// @todo Encapsulate this logic in ShadowEdge -ds
    if(!leftHEdge->hasFace()) return;

    BspLeaf const &frontLeaf = leftHEdge->face().mapElementAs<BspLeaf>();
    if(!frontLeaf.hasCluster() || !frontLeaf.cluster().hasWorldVolume())
        return;

    static ShadowEdge leftEdge; // this function is called often; keep these around
    static ShadowEdge rightEdge;

    leftEdge.init(*leftHEdge, Line::From);
    rightEdge.init(*leftHEdge, Line::To);

    leftEdge.prepare(planeIndex);
    rightEdge.prepare(planeIndex);

    if(leftEdge.sectorOpenness() >= 1 && rightEdge.sectorOpenness() >= 1) return;

    writeShadowSection2(leftEdge, rightEdge, suf->normal()[VZ] > 0, shadowDark);
}

/**
 * @attention Do not use the global radio state in here, as @a bspLeaf can be
 * part of any sector, not the one chosen for wall rendering.
 */
void Rend_RadioBspLeafEdges(BspLeaf const &bspLeaf)
{
    if(!rendFakeRadio) return;
    if(levelFullBright) return;

    if(bspLeaf.shadowLines().isEmpty()) return;

    SectorCluster const &cluster = bspLeaf.cluster();
    float sectorlight = cluster.sector().lightLevel();

    // Determine the shadow properties.
    /// @todo Make cvars out of constants.
    //float shadowWallSize = 2 * (8 + 16 - sectorlight * 16);
    float shadowDark = Rend_RadioCalcShadowDarkness(sectorlight);

    // Any need to continue?
    if(shadowDark < .0001f) return;

    Vector3f const eyeToSurface(Vector2d(vOrigin.x, vOrigin.z) - bspLeaf.poly().center());

    // We need to check all the shadow lines linked to this BspLeaf for
    // the purpose of fakeradio shadowing.
    BspLeaf::ShadowLines const &shadowLines = bspLeaf.shadowLines();
    foreach(LineSide *side, shadowLines)
    {
        // Already rendered during the current frame? We only want to
        // render each shadow once per frame.
        if(side->shadowVisCount() != R_FrameCount())
        {
            side->setShadowVisCount(R_FrameCount());

            for(int pln = 0; pln < cluster.visPlaneCount(); ++pln)
            {
                Plane const &plane = cluster.visPlane(pln);
                if(Vector3f(eyeToSurface, vOrigin.y - plane.heightSmoothed())
                        .dot(plane.surface().normal()) >= 0)
                {
                    writeShadowSection(pln, *side, shadowDark);
                }
            }
        }
    }
}

#ifdef DENG_DEBUG
static void drawPoint(Vector3d const &point, int radius, float const color[4])
{
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

    Vector3d const leftOff      = viewData->upVec + viewData->sideVec;
    Vector3d const rightOff     = viewData->upVec - viewData->sideVec;

    //Vector3d const viewToCenter = point - vOrigin;
    //float scale = float(viewToCenter.dot(viewData->frontVec)) /
    //                viewData->frontVec.dot(viewData->frontVec);

    Vector3d finalPos( point.x, point.z, point.y );

    // The final radius.
    float radX = radius * 1;
    float radY = radX / 1.2f;

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
    static const float red[4] = { 1.f, .2f, .2f, 1.f};
    static const float yellow[4] = {.7f, .7f, .2f, 1.f};

    if(!App_World().hasMap()) return;

    Map &map = App_World().map();

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC),
                            gl::ClampToEdge, gl::ClampToEdge);
    glEnable(GL_TEXTURE_2D);

    /// @todo fixme: Should use the visual plane heights of sector clusters.
    foreach(Line *line, map.lines())
    for(uint k = 0; k < 2; ++k)
    {
        Vertex &vtx = line->vertex(k);
        LineOwner const *base = vtx.firstLineOwner();
        LineOwner const *own = base;
        do
        {
            Vector2d xy = vtx.origin() + own->extendedShadowOffset();
            coord_t z = own->line().frontSector().floor().heightSmoothed();
            drawPoint(Vector3d(xy.x, xy.y, z), 1, yellow);

            xy = vtx.origin() + own->innerShadowOffset();
            drawPoint(Vector3d(xy.x, xy.y, z), 1, red);

            own = &own->next();
        } while(own != base);
    }

    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}
#endif
