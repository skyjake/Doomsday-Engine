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

#include <de/vector1.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"
#include "de_play.h"

#include "gl/sys_opengl.h"
#include "MaterialSnapshot"
#include "MaterialVariantSpec"
#include "map/gamemap.h"
#include "map/lineowner.h"
#include "render/rendpoly.h"

#include "render/rend_fakeradio.h"

using namespace de;

#define MIN_OPEN                (.1f)
#define EDGE_OPEN_THRESHOLD     (8) // world units (Z axis)

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
    shadowcorner_t sideCorners[2], edgespan_t spans[2], Line::Side const &side);

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
    return (0.6f - lightLevel * 0.4f) * 0.65f * rendFakeRadioDarkness;
}

void Rend_RadioUpdateForLineSide(Line::Side &side)
{
    // Disabled completely?
    if(!rendFakeRadio || levelFullBright) return;

    // Updates are disabled?
    if(!devFakeRadioUpdate) return;

    // Sides without sectors don't need updating. $degenleaf
    if(!side.hasSector()) return;

    // Have already determined the shadow properties on this side?
    LineSideRadioData &frData = Rend_RadioDataForLineSide(side);
    if(frData.updateCount == frameCount) return;

    // Not yet - Calculate now.
    for(uint i = 0; i < 2; ++i)
    {
        frData.spans[i].length = side.line().length();
        frData.spans[i].shift = 0;
    }

    scanEdges(frData.topCorners, frData.bottomCorners, frData.sideCorners, frData.spans, side);
    frData.updateCount = frameCount; // Mark as done.
}

/**
 * Set the vertex colors in the rendpoly.
 */
static void setRendpolyColor(ColorRawf *rcolors, uint num, float const shadowRGB[3], float darkness)
{
    darkness = de::clamp(0.f, darkness, 1.f);

    for(uint i = 0; i < num; ++i)
    {
        rcolors[i].rgba[CR] = shadowRGB[CR];
        rcolors[i].rgba[CG] = shadowRGB[CG];
        rcolors[i].rgba[CB] = shadowRGB[CB];
        rcolors[i].rgba[CA] = darkness;
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
 * @param length  If negative; implies that the texture is flipped horizontally.
 */
static inline float calcTexCoordX(float lineLength, float segOffset)
{
    if(lineLength > 0) return segOffset;
    return lineLength + segOffset;
}

/**
 * Set the rendpoly's Y offset and texture size.
 *
 * @param size  If negative; implies that the texture is flipped vertically.
 */
static inline float calcTexCoordY(float z, float bottom, float top, float texHeight)
{
    if(texHeight > 0) return top - z;
    return bottom - z;
}

/// @todo This algorithm should be rewritten to work at HEdge level.
static void scanNeighbor(bool scanTop, Line::Side const &side, edge_t *edge,
                         bool toLeft)
{
    int const SEP = 10;

    Line *iter;
    binangle_t diff = 0;
    coord_t lengthDelta = 0, gap = 0;
    coord_t iFFloor, iFCeil;
    coord_t iBFloor, iBCeil;
    int scanSecSide = side.lineSideId();
    Sector const *startSector = side.sectorPtr();
    Sector const *scanSector;
    bool clockwise = toLeft;
    bool stopScan = false;
    bool closed;

    coord_t fFloor = startSector->floor().visHeight();
    coord_t fCeil  = startSector->ceiling().visHeight();

    // Retrieve the start owner node.
    LineOwner *own = R_GetVtxLineOwner(&side.vertex((int)!toLeft), &side.line());

    do
    {
        // Select the next line.
        diff = (clockwise? own->angle() : own->prev().angle());
        iter = &own->_link[(int)clockwise]->line();

        scanSecSide = (iter->hasFrontSector() && iter->frontSectorPtr() == startSector)? Line::Back : Line::Front;

        // Step over selfreferencing lines.
        while((!iter->hasFrontSector() && !iter->hasBackSector()) || // $degenleaf
              iter->isSelfReferencing())
        {
            own = own->_link[(int)clockwise];
            diff += (clockwise? own->angle() : own->prev().angle());
            iter = &own->_link[(int)clockwise]->line();

            scanSecSide = (iter->frontSectorPtr() == startSector);
        }

        // Determine the relative backsector.
        if(iter->hasSections(scanSecSide))
            scanSector = iter->sectorPtr(scanSecSide);
        else
            scanSector = NULL;

        // Pick plane heights for relative offset comparison.
        if(!stopScan)
        {
            iFFloor = iter->frontSector().floor().visHeight();
            iFCeil  = iter->frontSector().ceiling().visHeight();

            if(iter->hasBackSections())
            {
                iBFloor = iter->backSector().floor().visHeight();
                iBCeil  = iter->backSector().ceiling().visHeight();
            }
            else
                iBFloor = iBCeil = 0;
        }

        lengthDelta = 0;
        if(!stopScan)
        {
            // This line will attribute to this hedge's shadow edge.
            // Store identity for later use.
            edge->diff = diff;
            edge->line = iter;
            edge->sector = const_cast<Sector *>(scanSector);

            closed = false;
            if(side.isFront() && iter->hasBackSections())
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
            // texture on the hedge shadow edge being rendered?
            if(scanTop)
            {
                if(iter->hasBackSections() &&
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
                if(iter->hasBackSections() &&
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
                        if(scanSector->ceiling().visHeight() != fCeil &&
                           scanSector->floor().visHeight() <
                                startSector->ceiling().visHeight())
                            stopScan = true;
                    }
                    else
                    {
                        if(scanSector->floor().visHeight() != fFloor &&
                           scanSector->ceiling().visHeight() >
                                startSector->floor().visHeight())
                            stopScan = true;
                    }
                }
            }
        }

        // Swap to the iter line's owner node (i.e: around the corner)?
        if(!stopScan)
        {
            // Around the corner.
            if(own->_link[(int)clockwise] == iter->v2Owner())
                own = iter->v1Owner();
            else if(own->_link[(int)clockwise] == iter->v1Owner())
                own = iter->v2Owner();

            // Skip into the back neighbor sector of the iter line if
            // heights are within accepted range.
            if(scanSector && side.back().hasSections() &&
               scanSector != side.back().sectorPtr() &&
                ((scanTop && scanSector->ceiling().visHeight() ==
                                startSector->ceiling().visHeight()) ||
                 (!scanTop && scanSector->floor().visHeight() ==
                                startSector->floor().visHeight())))
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
                    own = own->_link[(int)clockwise];
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
                               edge->line->vertexOwner(int(edge->line->hasBackSections() && edge->line->backSectorPtr() == edge->sector) ^ (int)!toLeft),
                               !toLeft, &edge->diff);
    }
}

static void scanNeighbors(shadowcorner_t top[2], shadowcorner_t bottom[2],
    Line::Side const &side, edgespan_t spans[2], bool toLeft)
{
    if(side.line().isSelfReferencing()) return;

    coord_t fFloor = side.sector().floor().visHeight();
    coord_t fCeil  = side.sector().ceiling().visHeight();

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
                corner->pOffset = corner->proximity->floor().visHeight() - fFloor;
                corner->pHeight = corner->proximity->floor().visHeight();
            }
            else // Ceiling.
            {
                corner->pOffset = corner->proximity->ceiling().visHeight() - fCeil;
                corner->pHeight = corner->proximity->ceiling().visHeight();
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
    shadowcorner_t sideCorners[2], edgespan_t spans[2], Line::Side const &side)
{
    Line const &line = side.line();
    int const lineSideId = side.lineSideId();

    std::memset(sideCorners, 0, sizeof(shadowcorner_t) * 2);

    // Find the sidecorners first: left and right neighbour.
    for(int i = 0; i < 2; ++i)
    {
        binangle_t diff = 0;
        LineOwner *vo = line.vertexOwner(i ^ lineSideId);

        Line *other = R_FindSolidLineNeighbor(side.sectorPtr(), &line, vo,
                                              CPP_BOOL(i), &diff);
        if(other && other != &line)
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
    boolean horizontal;
    float shadowMul;
    float texWidth;
    float texHeight;
    float texOffset[2];
    float wallLength;
} rendershadowseg_params_t;

static void setTopShadowParams(rendershadowseg_params_t *p, float size, coord_t top,
    coord_t xOffset, coord_t segLength, coord_t fFloor, coord_t fCeil,
    shadowcorner_t const *botCn, shadowcorner_t const *topCn, shadowcorner_t const *sideCn,
    edgespan_t const *spans)
{
    p->shadowMul = 1;
    p->horizontal = false;
    p->texHeight = size;
    p->texOffset[VY] = calcTexCoordY(top, fFloor, fCeil, p->texHeight);
    p->wallLength = segLength;

    p->texture = LST_RADIO_OO;
    // Corners without a neighbour backsector
    if(sideCn[0].corner == -1 || sideCn[1].corner == -1)
    {
        // At least one corner faces outwards
        p->texture = LST_RADIO_OO;
        p->texWidth = spans[TOP].length;
        p->texOffset[VX] =
            calcTexCoordX(spans[TOP].length, spans[TOP].shift + xOffset);

        if((sideCn[0].corner == -1 && sideCn[1].corner == -1) ||
           (topCn[0].corner == -1 && topCn[1].corner == -1))
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(sideCn[1].corner == -1)
        {
            // right corner faces outwards
            if(-topCn[0].pOffset < 0 && botCn[0].pHeight < fCeil)
            {
                // Must flip horizontally!
                p->texWidth = -spans[TOP].length;
                p->texOffset[VX] =
                    calcTexCoordX(-spans[TOP].length, spans[TOP].shift + xOffset);
                p->texture = LST_RADIO_OE;
            }
        }
        else  // left corner faces outwards
        {
            if(-topCn[1].pOffset < 0 && botCn[1].pHeight < fCeil)
            {
                p->texture = LST_RADIO_OE;
            }
        }
    }
    else
    {
        // Corners WITH a neighbour backsector
        p->texWidth = spans[TOP].length;
        p->texOffset[VX] = calcTexCoordX(spans[TOP].length, spans[TOP].shift + xOffset);

        if(topCn[0].corner == -1 && topCn[1].corner == -1)
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(topCn[1].corner == -1 && topCn[0].corner > MIN_OPEN)
        {
            // Right corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        else if(topCn[0].corner == -1 && topCn[1].corner > MIN_OPEN)
        {
            // Left corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        // Open edges
        else if(topCn[0].corner <= MIN_OPEN && topCn[1].corner <= MIN_OPEN)
        {
            // Both edges are open
            p->texture = LST_RADIO_OO;
            if(topCn[0].proximity && topCn[1].proximity)
            {
                if(-topCn[0].pOffset >= 0 && -topCn[1].pOffset < 0)
                {
                    p->texture = LST_RADIO_CO;
                    // The shadow can't go over the higher edge.
                    if(size > -topCn[0].pOffset)
                    {
                        if(-topCn[0].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texHeight = -topCn[0].pOffset;
                            p->texOffset[VY] = calcTexCoordY(top, fFloor, fCeil, p->texHeight);
                        }
                    }
                }
                else if(-topCn[0].pOffset < 0 && -topCn[1].pOffset >= 0)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_CO;
                    p->texWidth = -spans[TOP].length;
                    p->texOffset[VX] = calcTexCoordX(-spans[TOP].length, spans[TOP].shift + xOffset);

                    // The shadow can't go over the higher edge.
                    if(size > -topCn[1].pOffset)
                    {
                        if(-topCn[1].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texHeight = -topCn[1].pOffset;
                            p->texOffset[VY] = calcTexCoordY(top, fFloor, fCeil, p->texHeight);
                        }
                    }
                }
            }
            else
            {
                if(-topCn[0].pOffset < -MINDIFF)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_OE;
                    p->texWidth = -spans[BOTTOM].length;
                    p->texOffset[VX] = calcTexCoordX(-spans[BOTTOM].length, spans[BOTTOM].shift + xOffset);
                }
                else if(-topCn[1].pOffset < -MINDIFF)
                {
                    p->texture = LST_RADIO_OE;
                }
            }
        }
        else if(topCn[0].corner <= MIN_OPEN)
        {
            if(-topCn[0].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;

            // Must flip horizontally!
            p->texWidth = -spans[TOP].length;
            p->texOffset[VX] = calcTexCoordX(-spans[TOP].length, spans[TOP].shift + xOffset);
        }
        else if(topCn[1].corner <= MIN_OPEN)
        {
            if(-topCn[1].pOffset < 0)
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

static void setBottomShadowParams(rendershadowseg_params_t *p, float size, coord_t top,
    coord_t xOffset, coord_t segLength, coord_t fFloor, coord_t fCeil,
    shadowcorner_t const *botCn, shadowcorner_t const *topCn, shadowcorner_t const *sideCn,
    edgespan_t const *spans)
{
    p->shadowMul = 1;
    p->horizontal = false;
    p->texHeight = -size;
    p->texOffset[VY] = calcTexCoordY(top, fFloor, fCeil, p->texHeight);
    p->wallLength = segLength;

    p->texture = LST_RADIO_OO;
    // Corners without a neighbour backsector
    if(sideCn[0].corner == -1 || sideCn[1].corner == -1)
    {
        // At least one corner faces outwards
        p->texture = LST_RADIO_OO;
        p->texWidth = spans[BOTTOM].length;
        p->texOffset[VX] = calcTexCoordX(spans[BOTTOM].length, spans[BOTTOM].shift + xOffset);

        if((sideCn[0].corner == -1 && sideCn[1].corner == -1) ||
           (botCn[0].corner == -1 && botCn[1].corner == -1) )
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(sideCn[1].corner == -1) // right corner faces outwards
        {
            if(botCn[0].pOffset < 0 && topCn[0].pHeight > fFloor)
            {
                // Must flip horizontally!
                p->texWidth = -spans[BOTTOM].length;
                p->texOffset[VX] = calcTexCoordX(-spans[BOTTOM].length, spans[BOTTOM].shift + xOffset);
                p->texture = LST_RADIO_OE;
            }
        }
        else
        {
            // left corner faces outwards
            if(botCn[1].pOffset < 0 && topCn[1].pHeight > fFloor)
            {
                p->texture = LST_RADIO_OE;
            }
        }
    }
    else
    {   // Corners WITH a neighbour backsector
        p->texWidth = spans[BOTTOM].length;
        p->texOffset[VX] = calcTexCoordX(spans[BOTTOM].length, spans[BOTTOM].shift + xOffset);
        if(botCn[0].corner == -1 && botCn[1].corner == -1)
        {
            // Both corners face outwards
            p->texture = LST_RADIO_OO;//CC;
        }
        else if(botCn[1].corner == -1 && botCn[0].corner > MIN_OPEN)
        {
            // Right corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        else if(botCn[0].corner == -1 && botCn[1].corner > MIN_OPEN)
        {
            // Left corner faces outwards
            p->texture = LST_RADIO_OO;
        }
        // Open edges
        else if(botCn[0].corner <= MIN_OPEN && botCn[1].corner <= MIN_OPEN)
        {
            // Both edges are open
            p->texture = LST_RADIO_OO;

            if(botCn[0].proximity && botCn[1].proximity)
            {
                if(botCn[0].pOffset >= 0 && botCn[1].pOffset < 0)
                {
                    p->texture = LST_RADIO_CO;
                    // The shadow can't go over the higher edge.
                    if(size > botCn[0].pOffset)
                    {
                        if(botCn[0].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texHeight = -botCn[0].pOffset;
                            p->texOffset[VY] = calcTexCoordY(top, fFloor, fCeil, p->texHeight);
                        }
                    }
                }
                else if(botCn[0].pOffset < 0 && botCn[1].pOffset >= 0)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_CO;
                    p->texWidth = -spans[BOTTOM].length;
                    p->texOffset[VX] = calcTexCoordX(-spans[BOTTOM].length, spans[BOTTOM].shift + xOffset);

                    if(size > botCn[1].pOffset)
                    {
                        if(botCn[1].pOffset < INDIFF)
                        {
                            p->texture = LST_RADIO_OE;
                        }
                        else
                        {
                            p->texHeight = -botCn[1].pOffset;
                            p->texOffset[VY] = calcTexCoordY(top, fFloor, fCeil, p->texHeight);
                        }
                    }
                }
            }
            else
            {
                if(botCn[0].pOffset < -MINDIFF)
                {
                    // Must flip horizontally!
                    p->texture = LST_RADIO_OE;
                    p->texWidth = -spans[BOTTOM].length;
                    p->texOffset[VX] = calcTexCoordX(-spans[BOTTOM].length, spans[BOTTOM].shift + xOffset);
                }
                else if(botCn[1].pOffset < -MINDIFF)
                {
                    p->texture = LST_RADIO_OE;
                }
            }
        }
        else if(botCn[0].corner <= MIN_OPEN) // Right Corner is Closed
        {
            if(botCn[0].pOffset < 0)
                p->texture = LST_RADIO_CO;
            else
                p->texture = LST_RADIO_OO;

            // Must flip horizontally!
            p->texWidth = -spans[BOTTOM].length;
            p->texOffset[VX] = calcTexCoordX(-spans[BOTTOM].length, spans[BOTTOM].shift + xOffset);
        }
        else if(botCn[1].corner <= MIN_OPEN)  // Left Corner is closed
        {
            if(botCn[1].pOffset < 0)
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

static void setSideShadowParams(rendershadowseg_params_t *p, float size, coord_t bottom,
    coord_t top, int rightSide, bool haveBottomShadower, bool haveTopShadower,
    coord_t xOffset, coord_t segLength, coord_t fFloor,
    coord_t fCeil, bool hasBackSector, coord_t bFloor, coord_t bCeil, coord_t const *lineLength,
    shadowcorner_t const *sideCn)
{
    p->shadowMul = sideCn[rightSide? 1 : 0].corner * .8f;
    p->shadowMul *= p->shadowMul * p->shadowMul;
    p->horizontal = true;
    p->texOffset[VY] = bottom - fFloor;
    p->texHeight = fCeil - fFloor;
    p->wallLength = segLength;

    if(rightSide)
    {
        // Right shadow.
        p->texOffset[VX] = -(*lineLength) + xOffset;
        // Make sure the shadow isn't too big
        if(size > *lineLength)
        {
            if(sideCn[0].corner <= MIN_OPEN)
                p->texWidth = -(*lineLength);
            else
                p->texWidth = -((*lineLength) / 2);
        }
        else
        {
            p->texWidth = -size;
        }
    }
    else
    {
        // Left shadow.
        p->texOffset[VX] = xOffset;
        // Make sure the shadow isn't too big
        if(size > *lineLength)
        {
            if(sideCn[1].corner <= MIN_OPEN)
                p->texWidth = *lineLength;
            else
                p->texWidth = (*lineLength) / 2;
        }
        else
        {
            p->texWidth = size;
        }
    }

    if(hasBackSector)
    {
        // There is a backside.
        if(bFloor > fFloor && bCeil < fCeil)
        {
            if(haveBottomShadower && haveTopShadower)
            {
                p->texture = LST_RADIO_CC;
            }
            else if(!haveBottomShadower)
            {
                p->texOffset[VY] = bottom - fCeil;
                p->texHeight = -(fCeil - fFloor);
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
                p->texOffset[VY] = bottom - fCeil;
                p->texHeight = -(fCeil - fFloor);
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
                p->texOffset[VY] = bottom - fCeil;
                p->texHeight = -(fCeil - fFloor);
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
            p->texHeight = -(fCeil - fFloor);
            p->texOffset[VY] = calcTexCoordY(top, fFloor, fCeil, p->texHeight);
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

static void quadTexCoords(rtexcoord_t *tc, rvertex_t const *rverts, float wallLength,
    float texWidth, float texHeight, float texOrigin[2][3], float const texOffset[2],
    boolean horizontal)
{
    if(horizontal)
    {
        // Special horizontal coordinates for wall shadows.
        tc[0].st[0] = tc[2].st[0] = rverts[0].pos[VX] - texOrigin[0][VX] + texOffset[VY] / texHeight;
        tc[0].st[1] = tc[1].st[1] = rverts[0].pos[VY] - texOrigin[0][VY] + texOffset[VX] / texWidth;

        tc[1].st[0] = tc[0].st[0] + (rverts[1].pos[VZ] - texOrigin[1][VZ]) / texHeight;
        tc[3].st[0] = tc[0].st[0] + (rverts[3].pos[VZ] - texOrigin[1][VZ]) / texHeight;
        tc[3].st[1] = tc[0].st[1] + wallLength / texWidth;
        tc[2].st[1] = tc[0].st[1] + wallLength / texWidth;
        return;
    }

    tc[0].st[0] = tc[1].st[0] = rverts[0].pos[VX] - texOrigin[0][VX] + texOffset[VX] / texWidth;
    tc[3].st[1] = tc[1].st[1] = rverts[0].pos[VY] - texOrigin[0][VY] + texOffset[VY] / texHeight;

    tc[3].st[0] = tc[2].st[0] = tc[0].st[0] + wallLength / texWidth;
    tc[2].st[1] = tc[3].st[1] + (rverts[1].pos[VZ] - rverts[0].pos[VZ]) / texHeight;
    tc[0].st[1] = tc[3].st[1] + (rverts[3].pos[VZ] - rverts[2].pos[VZ]) / texHeight;
}

static void drawWallSectionShadow(rvertex_t const *origVertices,
    rendershadowseg_params_t const &wsParms, RendRadioWallSectionParms const &parms)
{
    DENG_ASSERT(origVertices);

    float texOrigin[2][3];
    ColorRawf *rcolors;
    rtexcoord_t *rtexcoords;
    uint realNumVertices = 4;

    if(parms.wall.left.divCount || parms.wall.right.divCount)
        realNumVertices = 3 + parms.wall.left.divCount + 3 + parms.wall.right.divCount;
    else
        realNumVertices = 4;

    // Top left.
    texOrigin[0][VX] = origVertices[1].pos[VX];
    texOrigin[0][VY] = origVertices[1].pos[VY];
    texOrigin[0][VZ] = origVertices[1].pos[VZ];

    // Bottom right.
    texOrigin[1][VX] = origVertices[2].pos[VX];
    texOrigin[1][VY] = origVertices[2].pos[VY];
    texOrigin[1][VZ] = origVertices[2].pos[VZ];

    // Allocate enough for the divisions too.
    rtexcoords = R_AllocRendTexCoords(realNumVertices);
    rcolors = R_AllocRendColors(realNumVertices);

    quadTexCoords(rtexcoords, origVertices, wsParms.wallLength, wsParms.texWidth,
                  wsParms.texHeight, texOrigin, wsParms.texOffset,
                  wsParms.horizontal);

    setRendpolyColor(rcolors, 4, parms.shadowRGB, parms.shadowDark * wsParms.shadowMul);

    if(rendFakeRadio != 2)
    {
        // Write multiple polys depending on rend params.
        RL_LoadDefaultRtus();
        RL_Rtu_SetTextureUnmanaged(RTU_PRIMARY, GL_PrepareLSTexture(wsParms.texture),
                                   GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        if(parms.wall.left.divCount || parms.wall.right.divCount)
        {
            /*
             * Need to swap indices around into fans set the position
             * of the division vertices, interpolate texcoords and
             * color.
             */

            rvertex_t *rvertices = R_AllocRendVertices(realNumVertices);

            rtexcoord_t origTexCoords[4];
            std::memcpy(origTexCoords, rtexcoords, sizeof(rtexcoord_t) * 4);

            ColorRawf origColors[4];
            std::memcpy(origColors, rcolors, sizeof(ColorRawf) * 4);

            float const bL = origVertices[0].pos[VZ];
            float const tL = origVertices[1].pos[VZ];
            float const bR = origVertices[2].pos[VZ];
            float const tR = origVertices[3].pos[VZ];

            R_DivVerts(rvertices, origVertices, parms.wall.left.firstDiv, parms.wall.left.divCount, parms.wall.right.firstDiv, parms.wall.right.divCount);
            R_DivTexCoords(rtexcoords, origTexCoords, parms.wall.left.firstDiv, parms.wall.left.divCount, parms.wall.right.firstDiv, parms.wall.right.divCount, bL, tL, bR, tR);
            R_DivVertColors(rcolors, origColors, parms.wall.left.firstDiv, parms.wall.left.divCount, parms.wall.right.firstDiv, parms.wall.right.divCount, bL, tL, bR, tR);

            RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_SHADOW,
                3 + parms.wall.right.divCount, rvertices + 3 + parms.wall.left.divCount, rcolors + 3 + parms.wall.left.divCount,
                rtexcoords + 3 + parms.wall.left.divCount, NULL);
            RL_AddPolyWithCoords(PT_FAN, RPF_DEFAULT|RPF_SHADOW,
                3 + parms.wall.left.divCount, rvertices, rcolors, rtexcoords, NULL);

            R_FreeRendVertices(rvertices);
        }
        else
        {
            RL_AddPolyWithCoords(PT_TRIANGLE_STRIP, RPF_DEFAULT|RPF_SHADOW,
                4, origVertices, rcolors, rtexcoords, NULL);
        }
    }

    R_FreeRendTexCoords(rtexcoords);
    R_FreeRendColors(rcolors);
}

void Rend_RadioWallSection(rvertex_t const *rvertices, RendRadioWallSectionParms const &parms)
{
    DENG_ASSERT(rvertices);

    // Disabled?
    if(!rendFakeRadio || levelFullBright) return;

    coord_t const lineLength = parms.line->length();
    coord_t const fFloor     = parms.frontSec->floor().visHeight();
    coord_t const fCeil      = parms.frontSec->ceiling().visHeight();
    coord_t const bFloor     = (parms.backSec? parms.backSec->floor().visHeight() : 0);
    coord_t const bCeil      = (parms.backSec? parms.backSec->ceiling().visHeight()  : 0);

    bool const haveBottomShadower = Rend_RadioPlaneCastsShadow(parms.frontSec->floor());
    bool const haveTopShadower    = Rend_RadioPlaneCastsShadow(parms.frontSec->ceiling());

    /*
     * Top Shadow.
     */
    if(haveTopShadower)
    {
        if(rvertices[3].pos[VZ] > fCeil - parms.shadowSize &&
           rvertices[0].pos[VZ] < fCeil)
        {
            rendershadowseg_params_t wsParms;

            setTopShadowParams(&wsParms, parms.shadowSize, rvertices[1].pos[VZ],
                               parms.segOffset, parms.segLength, fFloor, fCeil,
                               parms.botCn, parms.topCn, parms.sideCn, parms.spans);
            drawWallSectionShadow(rvertices, wsParms, parms);
        }
    }

    /*
     * Bottom Shadow.
     */
    if(haveBottomShadower)
    {
        if(rvertices[0].pos[VZ] < fFloor + parms.shadowSize &&
           rvertices[3].pos[VZ] > fFloor)
        {
            rendershadowseg_params_t wsParms;

            setBottomShadowParams(&wsParms, parms.shadowSize, rvertices[1].pos[VZ],
                                  parms.segOffset, parms.segLength, fFloor, fCeil,
                                  parms.botCn, parms.topCn, parms.sideCn, parms.spans);
            drawWallSectionShadow(rvertices, wsParms, parms);
        }
    }

    // Walls unaffected by floor and ceiling shadow casters receive no
    // side shadows either. We could do better here...
    if(!haveBottomShadower && !haveTopShadower)
        return;

    /*
     * Left Shadow.
     */
    if(parms.sideCn[0].corner > 0 && parms.segOffset < parms.shadowSize)
    {
        rendershadowseg_params_t wsParms;

        setSideShadowParams(&wsParms, parms.shadowSize, rvertices[0].pos[VZ],
                            rvertices[1].pos[VZ], false,
                            haveBottomShadower, haveTopShadower, parms.segOffset, parms.segLength,
                            fFloor, fCeil, !!parms.backSec, bFloor, bCeil, &lineLength,
                            parms.sideCn);
        drawWallSectionShadow(rvertices, wsParms, parms);
    }

    /*
     * Right Shadow.
     */
    if(parms.sideCn[1].corner > 0 &&
       parms.segOffset + parms.segLength > lineLength - parms.shadowSize)
    {
        rendershadowseg_params_t wsParms;

        setSideShadowParams(&wsParms, parms.shadowSize, rvertices[0].pos[VZ],
                            rvertices[1].pos[VZ], true,
                            haveBottomShadower, haveTopShadower, parms.segOffset, parms.segLength,
                            fFloor, fCeil, !!parms.backSec, bFloor, bCeil, &lineLength,
                            parms.sideCn);
        drawWallSectionShadow(rvertices, wsParms, parms);
    }
}

/**
 * Returns a value in the range of 0...2, which depicts how open the
 * specified edge is. Zero means that the edge is completely closed: it is
 * facing a wall or is relatively distant from the edge on the other side.
 * Values between zero and one describe how near the other edge is. An
 * openness value of one means that the other edge is at the same height as
 * this one. 2 means that the other edge is past our height ("clearly open").
 */
static float radioEdgeOpenness(float fz, float bz, float bhz)
{
    if(fz <= bz - EDGE_OPEN_THRESHOLD || fz >= bhz)
        return 0; // Fully closed.

    if(fz >= bhz - EDGE_OPEN_THRESHOLD)
        return (bhz - fz) / EDGE_OPEN_THRESHOLD;

    if(fz <= bz)
        return 1 - (bz - fz) / EDGE_OPEN_THRESHOLD;

    if(fz <= bz + EDGE_OPEN_THRESHOLD)
        return 1 + (fz - bz) / EDGE_OPEN_THRESHOLD;

    // Fully open!
    return 2;
}

static void setRelativeHeights(Sector const *front, Sector const *back, boolean isCeiling,
    coord_t *fz, coord_t *bz, coord_t *bhz)
{
    if(fz)
    {
        *fz = front->plane(isCeiling? Plane::Ceiling:Plane::Floor).visHeight();
        if(isCeiling)
            *fz = -(*fz);
    }
    if(bz)
    {
        *bz = back->plane(isCeiling? Plane::Ceiling:Plane::Floor).visHeight();
        if(isCeiling)
            *bz = -(*bz);
    }
    if(bhz)
    {
        *bhz = back->plane(isCeiling? Plane::Floor:Plane::Ceiling).visHeight();
        if(isCeiling)
            *bhz = -(*bhz);
    }
}

static uint radioEdgeHackType(Line const *line, Sector const *front, Sector const *back,
    int backside, boolean isCeiling, float fz, float bz)
{
    Surface const &surface = line->side(backside).surface(isCeiling? Line::Side::Top : Line::Side::Bottom);

    if(fz < bz && !surface.hasMaterial())
        return 3; // Consider it fully open.

    // Is the back sector closed?
    if(front->floor().visHeight() >= back->ceiling().visHeight())
    {
        if(front->planeSurface(isCeiling? Plane::Floor:Plane::Ceiling).hasSkyMaskedMaterial())
        {
            if(back->planeSurface(isCeiling? Plane::Floor:Plane::Ceiling).hasSkyMaskedMaterial())
                return 3; // Consider it fully open.
        }
        else
        {
            return 1; // Consider it fully closed.
        }
    }

    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(R_MiddleMaterialCoversOpening(line->side(backside)))
        return 1; // Consider it fully closed.

    return 0;
}

/**
 * Construct and write a new shadow polygon to the rendering lists.
 */
static void addShadowEdge(Vector2d const inner[2], Vector2d const outer[2],
    coord_t innerLeftZ, coord_t innerRightZ, coord_t outerLeftZ, coord_t outerRightZ,
    float const sideOpen[2], float const edgeOpen[2], boolean isFloor,
    float const shadowRGB[3], float shadowDark)
{
    static uint const floorIndices[][4] = {{0, 1, 2, 3}, {1, 2, 3, 0}};
    static uint const ceilIndices[][4]  = {{0, 3, 2, 1}, {1, 0, 3, 2}};

    vec2f_t outerAlpha;
    V2f_Set(outerAlpha, MIN_OF(shadowDark * (1 - edgeOpen[0]), 1), MIN_OF(shadowDark * (1 - edgeOpen[1]), 1));

    if(!(outerAlpha[0] > .0001 && outerAlpha[1] > .0001)) return;

    // What vertex winding order? (0 = left, 1 = right)
    // (for best results, the cross edge should always be the shortest).
    uint winding = (Vector2d(outer[1] - inner[1]).length() > Vector2d(inner[0] - outer[0]).length()? 1 : 0);
    uint const *idx = (isFloor ? floorIndices[winding] : ceilIndices[winding]);

    rvertex_t rvertices[4];
    ColorRawf rcolors[4];

    // Left outer corner.
    rvertices[idx[0]].pos[VX] = outer[0][VX];
    rvertices[idx[0]].pos[VY] = outer[0][VY];
    rvertices[idx[0]].pos[VZ] = outerLeftZ;

    // Right outer corner.
    rvertices[idx[1]].pos[VX] = outer[1][VX];
    rvertices[idx[1]].pos[VY] = outer[1][VY];
    rvertices[idx[1]].pos[VZ] = outerRightZ;

    // Right inner corner.
    rvertices[idx[2]].pos[VX] = inner[1][VX];
    rvertices[idx[2]].pos[VY] = inner[1][VY];
    rvertices[idx[2]].pos[VZ] = innerRightZ;

    // Left inner corner.
    rvertices[idx[3]].pos[VX] = inner[0][VX];
    rvertices[idx[3]].pos[VY] = inner[0][VY];
    rvertices[idx[3]].pos[VZ] = innerLeftZ;

    // Light this polygon.
    for(uint i = 0; i < 4; ++i)
    {
        rcolors[idx[i]].rgba[CR] = (renderWireframe? 1 : shadowRGB[CR]);
        rcolors[idx[i]].rgba[CG] = (renderWireframe? 1 : shadowRGB[CG]);
        rcolors[idx[i]].rgba[CB] = (renderWireframe? 1 : shadowRGB[CB]);
    }

    // Right inner.
    rcolors[idx[2]].rgba[CA] = 0;

    // Left inner.
    rcolors[idx[3]].rgba[CA] = 0;

    // Left outer.
    rcolors[idx[0]].rgba[CA] = outerAlpha[0];
    if(sideOpen[0] < 1)
        rcolors[idx[0]].rgba[CA] *= 1 - sideOpen[0];

    // Right outer.
    rcolors[idx[1]].rgba[CA] = outerAlpha[1];
    if(sideOpen[1] < 1)
        rcolors[idx[1]].rgba[CA] *= 1 - sideOpen[1];

    if(rendFakeRadio == 2) return;

    RL_LoadDefaultRtus();
    RL_AddPoly(PT_FAN, RPF_DEFAULT | (!renderWireframe? RPF_SHADOW : 0), 4, rvertices, rcolors);
}

static void processEdgeShadow(BspLeaf const &bspLeaf, Line const *line,
    int side, int planeIndex, float shadowDark)
{
    DENG_ASSERT(line && line->hasSections(side));

    if(!(shadowDark > .0001)) return;

    Plane const &plane = line->sector(side).plane(planeIndex);
    Surface const *suf = &plane.surface();
    coord_t plnHeight  = plane.visHeight();

    // Polyobj surfaces never shadow.
    if(line->isFromPolyobj()) return;

    // Surfaces with a missing material don't shadow.
    if(!suf->hasMaterial()) return;

    // Missing, glowing or sky-masked materials are exempted.
    Material const &material = suf->material();
    if(material.isSkyMasked() || material.hasGlow()) return;

    // Determine the openness of the line. If this edge is edgeOpen,
    // there won't be a shadow at all. Open neighbours cause some
    // changes in the polygon corner vertices (placement, colour).
    Vector2d inner[2], outer[2];
    vec2f_t edgeOpen, sideOpen;
    Sector const *front = 0;
    Sector const *back = 0;
    coord_t fz = 0, bz = 0, bhz = 0;
    if(line->hasBackSections())
    {
        front = line->sectorPtr(side);
        back  = line->sectorPtr(side ^ 1);
        setRelativeHeights(front, back, planeIndex == Plane::Ceiling, &fz, &bz, &bhz);

        uint hackType = radioEdgeHackType(line, front, back, side, planeIndex == Plane::Ceiling, fz, bz);
        if(hackType)
        {
            V2f_Set(edgeOpen, hackType - 1, hackType - 1);
        }
        else
        {
            float openness = radioEdgeOpenness(fz, bz, bhz);
            V2f_Set(edgeOpen, openness, openness);
        }
    }
    else
    {
        V2f_Set(edgeOpen, 0, 0);
    }

    if(edgeOpen[0] >= 1 && edgeOpen[1] >= 1) return;

    // Find the neighbors of this edge and determine their 'openness'.
    sideOpen[0] = sideOpen[1] = 0;
    for(int i = 0; i < 2; ++i)
    {
        LineOwner *vo = line->vertexOwner(side^i)->_link[i^1];
        Line *neighbor = &vo->line();

        if(neighbor != line && !neighbor->hasBackSections() &&
           (neighbor->isBspWindow()) &&
           neighbor->frontSectorPtr() != bspLeaf.sectorPtr())
        {
            // A one-way window, edgeOpen side.
            sideOpen[i] = 1;
        }
        else if(!(neighbor == line || !neighbor->hasBackSections()))
        {
            int otherSide = (&line->vertex(i ^ side) == &neighbor->from()? i : i ^ 1);
            Sector *othersec = neighbor->sectorPtr(otherSide);

            if(R_MiddleMaterialCoversOpening(neighbor->side(otherSide ^ 1)))
            {
                sideOpen[i] = 0;
            }
            else if(neighbor->isSelfReferencing())
            {
                sideOpen[i] = 1;
            }
            else
            {
                // Its a normal neighbor.
                if(neighbor->sectorPtr(otherSide) != line->sectorPtr(side) &&
                   !((plane.type() == Plane::Floor && othersec->ceiling().visHeight() <= plane.visHeight()) ||
                     (plane.type() == Plane::Ceiling && othersec->floor().height() >= plane.visHeight())))
                {
                    front = line->sectorPtr(side);
                    back  = neighbor->sectorPtr(otherSide);

                    setRelativeHeights(front, back, planeIndex == Plane::Ceiling, &fz, &bz, &bhz);
                    sideOpen[i] = radioEdgeOpenness(fz, bz, bhz);
                }
            }
        }

        if(sideOpen[i] < 1)
        {
            vo = line->vertexOwner(i^side);
            if(i) vo = &vo->prev();

            inner[i] = line->vertexOrigin(i^side) + vo->innerShadowOffset();
        }
        else
        {
            inner[i] = line->vertexOrigin(i^side) + vo->extendedShadowOffset();
        }
    }

    outer[0] = line->vertexOrigin(side);
    outer[1] = line->vertexOrigin(side^1);

    // Shadows are black
    vec3f_t shadowRGB; V3f_Set(shadowRGB, 0, 0, 0);

    addShadowEdge(inner, outer, plnHeight, plnHeight, plnHeight, plnHeight, sideOpen, edgeOpen,
                  suf->normal()[VZ] > 0, shadowRGB, shadowDark);
}

static void drawLinkedEdgeShadows(BspLeaf const &bspLeaf, ShadowLink &link,
    byte const *doPlanes, float shadowDark)
{
    DENG_ASSERT(doPlanes);

    if(!(shadowDark > .0001f)) return;

    if(doPlanes[Plane::Floor])
    {
        processEdgeShadow(bspLeaf, link.line, link.side, Plane::Floor, shadowDark);
    }

    if(doPlanes[Plane::Ceiling])
    {
        processEdgeShadow(bspLeaf, link.line, link.side, Plane::Ceiling, shadowDark);
    }

    for(int pln = Plane::Middle; pln < bspLeaf.sector().planeCount(); ++pln)
    {
        processEdgeShadow(bspLeaf, link.line, link.side, pln, shadowDark);
    }

    // Mark it rendered for this frame.
    link.lineSide().setShadowVisCount(frameCount);
}

/**
 * @attention Do not use the global radio state in here, as @a bspLeaf can be
 * part of any sector, not the one chosen for wall rendering.
 */
void Rend_RadioBspLeafEdges(BspLeaf &bspLeaf)
{
    if(!rendFakeRadio || levelFullBright) return;

    static int doPlaneSize = 0;
    static byte *doPlanes = 0;

    Sector &sector = bspLeaf.sector();
    float sectorlight = sector.lightLevel();
    boolean workToDo = false;

    Rend_ApplyLightAdaptation(&sectorlight);

    // No point drawing shadows in a PITCH black sector.
    if(sectorlight == 0) return;

    // Determine the shadow properties.
    /// @todo Make cvars out of constants.
    //float shadowWallSize = 2 * (8 + 16 - sectorlight * 16);
    float shadowDark = Rend_RadioCalcShadowDarkness(sectorlight);

    // Any need to continue?
    if(!(shadowDark > .0001f)) return;

    Vector3f eyeToSurface(vOrigin[VX] - bspLeaf.center().x,
                          vOrigin[VZ] - bspLeaf.center().y);

    // Do we need to enlarge the size of the doPlanes array?
    if(sector.planeCount() > doPlaneSize)
    {
        if(!doPlaneSize)
            doPlaneSize = 2;
        else
            doPlaneSize *= 2;

        doPlanes = (byte *) Z_Realloc(doPlanes, doPlaneSize, PU_APPSTATIC);
    }

    std::memset(doPlanes, 0, doPlaneSize);

    // See if any of this BspLeaf's planes will get shadows.
    for(int pln = 0; pln < sector.planeCount(); ++pln)
    {
        Plane const &plane = sector.plane(pln);

        eyeToSurface.z = vOrigin[VY] - plane.visHeight();

        // Don't bother with planes facing away from the camera.
        if(eyeToSurface.dot(plane.surface().normal()) < 0) continue;

        doPlanes[pln] = true;
        workToDo = true;
    }

    if(!workToDo) return;

    // We need to check all the shadow lines linked to this BspLeaf for
    // the purpose of fakeradio shadowing.
    for(ShadowLink *link = bspLeaf.firstShadowLink(); link != NULL; link = link->next)
    {
        // Already rendered during the current frame? We only want to
        // render each shadow once per frame.
        if(link->lineSide().shadowVisCount() == frameCount) continue;

        drawLinkedEdgeShadows(bspLeaf, *link, doPlanes, shadowDark);
    }
}

#ifdef DENG_DEBUG
static void drawPoint(Vector3d const &point, int radius, float const color[4])
{
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

    // viewData->sideVec is to the left.
    Vector3d leftOff, rightOff, viewToCenter;
    for(int i = 0; i < 3; ++i)
    {
        leftOff[i]  = viewData->upVec[i] + viewData->sideVec[i];
        rightOff[i] = viewData->upVec[i] - viewData->sideVec[i];

        viewToCenter[i] = point[i] - vOrigin[i];
    }

    //float scale = float(V3d_DotProductf(viewToCenter, viewData->frontVec)) /
    //                V3f_DotProduct(viewData->frontVec, viewData->frontVec);

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

    if(!theMap) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_DYNAMIC),
                            GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);

    foreach(Line *line, theMap->lines())
    for(uint k = 0; k < 2; ++k)
    {
        Vertex &vtx = line->vertex(k);
        LineOwner const *base = vtx.firstLineOwner();
        LineOwner const *own = base;
        do
        {
            Vector2d xy = vtx.origin() + own->extendedShadowOffset();
            coord_t z = own->line().frontSector().floor().visHeight();
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
