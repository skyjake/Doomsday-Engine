/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * rend_fakeradio.c: Faked Radiosity Lighting
 *
 * Perhaps the most distinctive characteristic of radiosity lighting
 * is that the corners of a room are slightly dimmer than the rest of
 * the surfaces.  (It's not the only characteristic, however.)  We
 * will fake these shadowed areas by generating shadow polygons for
 * wall segments and determining, which subsector vertices will be
 * shadowed.
 *
 * In other words, walls use shadow polygons (over entire segs), while
 * planes use vertex lighting.  Since planes are usually tesselated
 * into a great deal of subsectors (and triangles), they are better
 * suited for vertex lighting.  In some cases we will be forced to
 * split a subsector into smaller pieces than strictly necessary in
 * order to achieve better accuracy in the shadow effect.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "m_vector.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
  PROF_RADIO_SUBSECTOR
END_PROF_TIMERS()

#define MIN_OPEN .1f
#define EDGE_OPEN_THRESHOLD 8   // world units (Z axis)

#define MINDIFF 8 // min plane height difference (world units)
#define INDIFF  8 // max plane height for indifference offset

// TYPES -------------------------------------------------------------------

typedef struct edge_s {
    boolean done;
    line_t *line;
    sector_t *sector;
    float   length;
    binangle_t diff;
} edge_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void scanEdges(shadowcorner_t topCorners[2],
                      shadowcorner_t bottomCorners[2],
                      shadowcorner_t sideCorners[2], edgespan_t spans[2],
                      line_t *line, boolean backSide);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     rendFakeRadio = true;   // cvar
float   rendFakeRadioDarkness = 1.2f;  // cvar

float   rendRadioLongWallMin = 400;
float   rendRadioLongWallMax = 1500;
float   rendRadioLongWallDiv = 30;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static sector_t *frontSector;
static float shadowSize, shadowDark;
static float fFloor, fCeil;

// CODE --------------------------------------------------------------------

void Rend_RadioRegister(void)
{
    C_VAR_INT("rend-fakeradio", &rendFakeRadio, 0, 0, 2);

    C_VAR_FLOAT("rend-fakeradio-darkness", &rendFakeRadioDarkness, 0, 0, 2);
}

static __inline float Rend_RadioShadowDarkness(float lightlevel)
{
    return (.65f - lightlevel * 0.3f) * rendFakeRadioDarkness;
}

void Rend_RadioInitForFrame(void)
{
#ifdef DD_PROFILE
{
    static int t;

    if(++t > 40)
    {
        t = 0;
        PRINT_PROF(PROF_RADIO_SUBSECTOR);
    }
}
#endif
}

/**
 * Called to update the shadow properties used when doing FakeRadio for the
 * given linedef.
 */
void Rend_RadioUpdateLinedef(line_t *line, boolean backSide)
{
    side_t         *s;

    if(!rendFakeRadio || levelFullBright || shadowSize <= 0 ||  // Disabled?
       !line)
        return;

    // Have we yet determined the shadow properties to be used with segs
    // on this sidedef?
    s = line->sides[backSide? BACK : FRONT];
    if(s->fakeRadioUpdateCount != frameCount)
    {   // Not yet. Calculate now.
        uint            i;

        for(i = 0; i < 2; ++i)
        {
            s->spans[i].length = line->length;
            s->spans[i].shift = 0;
        }

        scanEdges(s->topCorners, s->bottomCorners, s->sideCorners,
                  s->spans, line, backSide);
        s->fakeRadioUpdateCount = frameCount; // Mark as done.
    }
}

/**
 * Before calling the other rendering routines, this must be called to
 * initialize the state of the FakeRadio renderer.
 */
void Rend_RadioInitForSubsector(subsector_t *ssec)
{
    sector_t   *linkSec;
    float       sectorlight = ssec->sector->lightlevel;

    Rend_ApplyLightAdaptation(&sectorlight);

    // By default, the shadow is disabled.
    shadowSize = 0;

    if(!rendFakeRadio)
        return;                 // Disabled...

    if(sectorlight == 0)
        return;     // No point drawing shadows in a PITCH black sector.

    // Visible plane heights.
    linkSec = R_GetLinkedSector(ssec, PLN_FLOOR);
    fFloor = linkSec->SP_floorvisheight;
    linkSec = R_GetLinkedSector(ssec, PLN_CEILING);
    fCeil = linkSec->SP_ceilvisheight;

    if(fCeil <= fFloor)
        return;                 // A closed sector.

    frontSector = ssec->sector;

    // Determine the shadow properties.
    // \fixme Make cvars out of constants.
    shadowSize = 2 * (8 + 16 - sectorlight * 16);
    shadowDark = Rend_RadioShadowDarkness(sectorlight) *.8f;
}

/**
 * @return      <code>true</code> if the specified flat is non-glowing, i.e.
 *              not glowing or a sky.
 */
static __inline boolean Rend_RadioNonGlowingFlat(sector_t* sector, int plane)
{
    return !(sector->SP_planetexture(plane) <= 0 ||
             sector->SP_planeglow(plane) ||
             R_IsSkySurface(&sector->SP_planesurface(plane)));
}

/**
 * Set the vertex colors in the rendpoly.
 */
static void Rend_RadioSetColor(rendpoly_t *q, float darkness)
{
    int         i;

    // Clamp it.
    if(darkness < 0)
        darkness = 0;
    if(darkness > 1)
        darkness = 1;

    for(i = 0; i < q->numvertices; ++i)
    {
        // Shadows are black.
        memset(q->vertices[i].color.rgba, 0, 3);
        q->vertices[i].color.rgba[CA] = (DGLubyte) (255 * darkness);
    }
}

/**
 * @return          <code>true</code> if there is open space in the sector.
 */
static __inline boolean Rend_IsSectorOpen(sector_t *sector)
{
    return (sector &&
            sector->SP_ceilheight > sector->SP_floorheight);
}

/**
 * Set the rendpoly's X offset and texture size.
 *
 * @param length    If negative; implies that the texture is flipped
 *                  horizontally.
 */
static void Rend_RadioTexCoordX(rendpoly_t *q, float lineLength,
                                float segOffset)
{
    q->tex.width = lineLength;
    if(lineLength > 0)
        q->texoffx = segOffset;
    else
        q->texoffx = lineLength + segOffset;
}

/**
 * Set the rendpoly's Y offset and texture size.
 *
 * @param size      If negative; implies that the texture is flipped
 *                  vertically.
 */
static void Rend_RadioTexCoordY(rendpoly_t *q, float size)
{
    if((q->tex.height = size) > 0)
        q->texoffy = fCeil - q->vertices[1].pos[VZ];
    else
        q->texoffy = fFloor - q->vertices[1].pos[VZ];
}

static void Rend_RadioScanNeighbor(boolean scanTop, line_t *line, uint side,
                                   edge_t *edge, boolean toLeft)
{
#define SEP 10

    line_t     *iter;
    lineowner_t *own;
    binangle_t  diff = 0;
    float       lengthDelta = 0, gap = 0;
    float       iFFloor, iFCeil;
    float       iBFloor, iBCeil;
    int         scanSecSide = side;
    sector_t   *startSector = line->L_sector(side);
    sector_t   *scanSector;
    boolean     clockwise = toLeft;
    boolean     stopScan = false;
    boolean     closed;

    // Retrieve the start owner node.
    own = R_GetVtxLineOwner(line->L_v(side^!toLeft), line);
    do
    {
        // Select the next line.
        diff = (clockwise? own->angle : own->LO_prev->angle);
        iter = own->link[clockwise]->line;

        scanSecSide = (iter->L_frontsector == startSector);

        // Step over selfreferencing lines?
        while(iter->flags & LINEF_SELFREF)
        {
            own = own->link[clockwise];
            diff += (clockwise? own->angle : own->LO_prev->angle);
            iter = own->link[clockwise]->line;

            scanSecSide = (iter->L_frontsector == startSector);
        }

        // Determine the relative backsector.
        if(iter->L_side(scanSecSide))
            scanSector = iter->L_sector(scanSecSide);
        else
            scanSector = NULL;

        // Pick plane heights for relative offset comparison.
        if(!stopScan)
        {
            iFFloor = iter->L_frontsector->SP_floorvisheight;
            iFCeil  = iter->L_frontsector->SP_ceilvisheight;

            if(iter->L_backside)
            {
                iBFloor = iter->L_backsector->SP_floorvisheight;
                iBCeil  = iter->L_backsector->SP_ceilvisheight;
            }
            else
                iBFloor = iBCeil = 0;
        }

        lengthDelta = 0;
        if(!stopScan)
        {
            // This line will attribute to this seg's shadow edge.
            // Store identity for later use.
            edge->diff = diff;
            edge->line = iter;
            edge->sector = scanSector;

            closed = false;
            if(side == 0 && iter->L_backside != NULL)
            {
                if(scanTop)
                {
                    if(iBFloor >= fCeil)
                        closed = true;  // compared to "this" sector anyway
                }
                else
                {
                    if(iBCeil <= fFloor)
                        closed = true;  // compared to "this" sector anyway
                }
            }

            // Does this line's length contribute to the alignment of the
            // texture on the seg shadow edge being rendered?
            if(scanTop)
            {
                if(iter->L_backside &&
                   ((side == 0 && iter->L_backsector == line->L_frontsector &&
                    iFCeil >= fCeil) ||
                   (side == 1 && iter->L_backsector == line->L_backsector &&
                    iFCeil >= fCeil) ||
                    (side == 0 && closed == false && iter->L_backsector != line->L_frontsector &&
                    iBCeil >= fCeil &&
                    Rend_IsSectorOpen(iter->L_backsector))))
                {
                    gap += iter->length;  // Should we just mark it done instead?
                }
                else
                {
                    edge->length += iter->length + gap;
                    gap = 0;
                }
            }
            else
            {
                if(iter->L_backside &&
                   ((side == 0 && iter->L_backsector == line->L_frontsector &&
                    iFFloor <= fFloor) ||
                   (side == 1 && iter->L_backsector == line->L_backsector &&
                    iFFloor <= fFloor) ||
                   (side == 0 && closed == false && iter->L_backsector != line->L_frontsector &&
                    iBFloor <= fFloor &&
                    Rend_IsSectorOpen(iter->L_backsector))))
                {
                    gap += iter->length;  // Should we just mark it done instead?
                }
                else
                {
                    lengthDelta = iter->length + gap;
                    gap = 0;
                }
            }
        }

        // Time to stop?
        if(iter == line)
        {
            stopScan = true;
        }
        else
        {
            // Is this line coalignable?
            if(!(diff >= BANG_180 - SEP && diff <= BANG_180 + SEP))
                stopScan = true; // no.
            else if(scanSector)
            {
                // Perhaps its a closed edge?
                if(!Rend_IsSectorOpen(scanSector))
                {
                    stopScan = true;
                }
                else
                {
                    // A height difference from the start sector?
                    if(scanTop)
                    {
                        if(scanSector->SP_ceilvisheight != fCeil &&
                           scanSector->SP_floorvisheight <
                                startSector->SP_ceilvisheight)
                            stopScan = true;
                    }
                    else
                    {
                        if(scanSector->SP_floorvisheight != fFloor &&
                           scanSector->SP_ceilvisheight >
                                startSector->SP_floorvisheight)
                            stopScan = true;
                    }
                }
            }
        }

        // Swap to the iter line's owner node (i.e: around the corner)?
        if(!stopScan)
        {
            // Around the corner.
            if(own->link[clockwise] == iter->L_vo2)
                own = iter->L_vo1;
            else if(own->link[clockwise] == iter->L_vo1)
                own = iter->L_vo2;

            // Skip into the back neighbor sector of the iter line if
            // heights are within accepted range.
            if(scanSector && line->L_side(side^1) &&
               scanSector != line->L_sector(side^1) &&
                ((scanTop && scanSector->SP_ceilvisheight ==
                                startSector->SP_ceilvisheight) ||
                 (!scanTop && scanSector->SP_floorvisheight ==
                                startSector->SP_floorvisheight)))
            {
                // If the map is formed correctly, we should find a back
                // neighbor attached to this line. However, if this is not
                // the case and a line which SHOULD be two sided isn't, we
                // need to check whether there is a valid neighbor.
                line_t *backNeighbor =
                    R_FindLineNeighbor(startSector, iter, own, !toLeft, NULL);

                if(backNeighbor && backNeighbor != iter)
                {
                    // Into the back neighbor sector.
                    own = own->link[clockwise];
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
                               edge->line->vo[(edge->line->L_backside && edge->line->L_backsector == edge->sector)^!toLeft],
                               !toLeft, &edge->diff);
    }

#undef SEP
}

static void Rend_RadioScanNeighbors(shadowcorner_t top[2],
                                    shadowcorner_t bottom[2], line_t *line,
                                    uint side, edgespan_t spans[2],
                                    boolean toLeft)
{
    uint        i;
    edge_t      edges[2], *edge; // {bottom, top}
    edgespan_t *span;
    shadowcorner_t *corner;

    if(line->flags & LINEF_SELFREF)
        return;

    memset(edges, 0, sizeof(edges));

    Rend_RadioScanNeighbor(false, line, side, &edges[0], toLeft);
    Rend_RadioScanNeighbor(true, line, side, &edges[1], toLeft);

    for(i = 0; i < 2; ++i)      // 0=bottom, 1=top
    {
        corner = (i == 0 ? &bottom[!toLeft] : &top[!toLeft]);
        edge = &edges[i];
        span = &spans[i];

        // Increment the apparent line length/offset.
        span->length += edge->length;
        if(toLeft)
            span->shift += edge->length;

        // Compare the relative angle difference of this edge to determine
        // an "open-ness" factor.
        if(edge->line && edge->line != line)
        {
            if(edge->diff > BANG_180)
            {   // The corner between the walls faces outwards.
                corner->corner = -1;
            }
            else if(edge->diff == BANG_180)
            {   // Perfectly coaligned? Great.
                corner->corner = 0;
            }
            else if(edge->diff < BANG_45 / 5)
            {   // The difference is too small, there won't be a shadow.
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
        {   // Consider it coaligned.
            corner->corner = 0;
        }

        // Determine relative height offsets (affects shadow map selection).
        if(edge->sector)
        {
            corner->proximity = edge->sector;
            if(i == 0)          // floor
            {
                corner->pOffset = corner->proximity->SP_floorvisheight - fFloor;
                corner->pHeight = corner->proximity->SP_floorvisheight;
            }
            else                // ceiling
            {
                corner->pOffset = corner->proximity->SP_ceilvisheight - fCeil;
                corner->pHeight = corner->proximity->SP_ceilvisheight;
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
 * are composed of aligned lines.  It's important to note that the scanning
 * is done separately for the top/bottom edges (both in the left and right
 * direction) and the left/right edges.
 *
 * The length of the top/bottom edges are returned in the array 'spans'.
 *
 * This may look like a complicated operation (performed for all wall polys)
 * but in most cases this won't take long. Aligned neighbours are relatively
 * rare.
 */
static void scanEdges(shadowcorner_t topCorners[2],
                      shadowcorner_t bottomCorners[2],
                      shadowcorner_t sideCorners[2], edgespan_t spans[2],
                      line_t *line, boolean backSide)
{
    uint        i, sid = (backSide? BACK : FRONT);
    side_t     *side;
    line_t     *other;

    side = line->L_side(sid);

    memset(sideCorners, 0, sizeof(sideCorners));

    // Find the sidecorners first: left and right neighbour.
    for(i = 0; i < 2; ++i)
    {
        binangle_t diff = 0;

        other = R_FindSolidLineNeighbor(line->L_sector(sid), line,
                                        line->L_vo(i^sid), i, &diff);
        if(other && other != line)
        {
            if(diff > BANG_180)
            {   // The corner between the walls faces outwards.
                sideCorners[i].corner = -1;
            }
            else if(diff == BANG_180)
            {
                sideCorners[i].corner = 0;
            }
            else if(diff < BANG_45 / 5)
            {   // The difference is too small, there won't be a shadow.
                sideCorners[i].corner = 0;
            }
            else if(diff > BANG_90)
            {   // 90 degrees is the largest effective difference.
                sideCorners[i].corner = (float) BANG_90 / diff;
            }
            else
            {
                sideCorners[i].corner = (float) diff / BANG_90;
            }
        }
        else
            sideCorners[i].corner = 0;

        Rend_RadioScanNeighbors(topCorners, bottomCorners, line, sid, spans,
                                !i);
    }
}

/**
 * Long walls get slightly larger shadows. The bonus will simply be added
 * to the shadow size for the wall in question.
 */
static float Rend_RadioLongWallBonus(float span)
{
    float       limit;

    if(rendRadioLongWallDiv > 0 && span > rendRadioLongWallMin)
    {
        limit = span - rendRadioLongWallMin;
        if(limit > rendRadioLongWallMax)
            limit = rendRadioLongWallMax;
        return limit / rendRadioLongWallDiv;
    }
    return 0;
}

/**
 * Create the appropriate FakeRadio shadow polygons for the wall segment.
 * The quad must be initialized with all the necessary data (normally comes
 * directly from doRenderSegSection() in rend_main.c).
 */
void Rend_RadioSegSection(const rendpoly_t *origQuad, line_t *line, byte side,
                          float xOffset, float segLength)
{
#define BOTTOM          0           
#define TOP             1

    float       bFloor, bCeil, limit, size;
    rendpoly_t *quad;
    int         i, texture = 0;
    shadowcorner_t *topCn, *botCn, *sideCn;
    edgespan_t  *spans;
    float       shifts[2]; // {bottom, top}
    side_t     *sidedef;

    if(!rendFakeRadio || levelFullBright || shadowSize <= 0 ||  // Disabled?
       !line || !origQuad || (origQuad->flags & RPF_GLOW))
        return;

    sidedef = line->sides[side];
    topCn = sidedef->topCorners;
    botCn = sidedef->bottomCorners;
    sideCn = sidedef->sideCorners;
    spans = sidedef->spans;
    shifts[0] = spans[0].shift + xOffset;
    shifts[1] = spans[1].shift + xOffset;

    // Back sector visible plane heights.
    if(line->L_backside)
    {
        bFloor = line->L_backsector->SP_floorvisheight;
        bCeil  = line->L_backsector->SP_ceilvisheight;
    }
    else
        bFloor = bCeil = 0;

    quad = R_AllocRendPoly(RP_QUAD, true, 4);
    R_MemcpyRendPoly(quad, origQuad);

    // Init the quad.
    quad->flags = RPF_SHADOW;
    quad->texoffx = xOffset;
    quad->texoffy = 0;
    quad->tex.id = GL_PrepareLSTexture(LST_RADIO_CC, NULL);
    quad->tex.detail = NULL;
    quad->tex.width = line->length;
    quad->tex.height = shadowSize;
    quad->tex.masked = false;
    quad->lightListIdx = 0;
    quad->intertex.id = 0;
    quad->intertex.detail = NULL;

    // Fade the shadow out if the height is below the min height.
    if(quad->vertices[3].pos[VZ] - quad->vertices[0].pos[VZ] < EDGE_OPEN_THRESHOLD)
        Rend_RadioSetColor(quad, shadowDark *
                              ((quad->vertices[3].pos[VZ] - quad->vertices[0].pos[VZ]) / EDGE_OPEN_THRESHOLD));
    else
        Rend_RadioSetColor(quad, shadowDark);

    /*
     * Top Shadow
     */
    // The top shadow will reach this far down.
    size = shadowSize + Rend_RadioLongWallBonus(spans[TOP].length);
    limit = fCeil - size;
    if((quad->vertices[3].pos[VZ] > limit && quad->vertices[0].pos[VZ] < fCeil) &&
       Rend_RadioNonGlowingFlat(frontSector, PLN_CEILING))
    {
        Rend_RadioTexCoordY(quad, size);
        texture = LST_RADIO_OO;
        // Corners without a neighbour backsector
        if(sideCn[0].corner == -1 || sideCn[1].corner == -1)
        {   // At least one corner faces outwards
            texture = LST_RADIO_OO;
            Rend_RadioTexCoordX(quad, spans[TOP].length, shifts[TOP]);

            if((sideCn[0].corner == -1 && sideCn[1].corner == -1) ||
               (topCn[0].corner == -1 && topCn[1].corner == -1) )
            {   // Both corners face outwards
                texture = LST_RADIO_OO;//CC;
            }
            else if(sideCn[1].corner == -1)
            {   // right corner faces outwards
                if(-topCn[0].pOffset < 0 && botCn[0].pHeight < fCeil)
                {// Must flip horizontally!
                    Rend_RadioTexCoordX(quad, -spans[TOP].length, shifts[TOP]);
                    texture = LST_RADIO_OE;
                }
            }
            else  // left corner faces outwards
            {
                if(-topCn[1].pOffset < 0 && botCn[1].pHeight < fCeil)
                {
                    texture = LST_RADIO_OE;
                }
            }
        }
        else
        {   // Corners WITH a neighbour backsector
            Rend_RadioTexCoordX(quad, spans[TOP].length, shifts[TOP]);
            if(topCn[0].corner == -1 && topCn[1].corner == -1)
            {   // Both corners face outwards
                texture = LST_RADIO_OO;//CC;
            }
            else if(topCn[1].corner == -1 && topCn[0].corner > MIN_OPEN)
            {   // Right corner faces outwards
                texture = LST_RADIO_OO;
            }
            else if(topCn[0].corner == -1 && topCn[1].corner > MIN_OPEN)
            {   // Left corner faces outwards
                texture = LST_RADIO_OO;
            }
            // Open edges
            else if(topCn[0].corner <= MIN_OPEN && topCn[1].corner <= MIN_OPEN)
            {   // Both edges are open
                texture = LST_RADIO_OO;
                if(topCn[0].proximity && topCn[1].proximity)
                {
                    if(-topCn[0].pOffset >= 0 && -topCn[1].pOffset < 0)
                    {
                        texture = LST_RADIO_CO;
                        // The shadow can't go over the higher edge.
                        if(size > -topCn[0].pOffset)
                        {
                            if(-topCn[0].pOffset < INDIFF)
                                texture = LST_RADIO_OE;
                            else
                                Rend_RadioTexCoordY(quad, -topCn[0].pOffset);
                        }
                    }
                    else if(-topCn[0].pOffset < 0 && -topCn[1].pOffset >= 0)
                    {
                        // Must flip horizontally!
                        texture = LST_RADIO_CO;
                        Rend_RadioTexCoordX(quad, -spans[TOP].length, shifts[TOP]);

                        // The shadow can't go over the higher edge.
                        if(size > -topCn[1].pOffset)
                        {
                            if(-topCn[1].pOffset < INDIFF)
                                texture = LST_RADIO_OE;
                            else
                                Rend_RadioTexCoordY(quad, -topCn[1].pOffset);
                        }
                    }
                }
                else
                {
                    if(-topCn[0].pOffset < -MINDIFF)
                    {
                        // Must flip horizontally!
                        texture = LST_RADIO_OE;
                        Rend_RadioTexCoordX(quad, -spans[BOTTOM].length,
                                            shifts[BOTTOM]);
                    }
                    else if(-topCn[1].pOffset < -MINDIFF)
                        texture = LST_RADIO_OE;
                }
            }
            else if(topCn[0].corner <= MIN_OPEN)
            {
                if(-topCn[0].pOffset < 0)
                    texture = LST_RADIO_CO;
                else
                    texture = LST_RADIO_OO;

                // Must flip horizontally!
                Rend_RadioTexCoordX(quad, -spans[TOP].length, shifts[TOP]);
            }
            else if(topCn[1].corner <= MIN_OPEN)
            {
                if(-topCn[1].pOffset < 0)
                    texture = LST_RADIO_CO;
                else
                    texture = LST_RADIO_OO;
            }
            else  // C/C ???
            {
                texture = LST_RADIO_OO;
            }
        }

        quad->tex.id = GL_PrepareLSTexture(texture, NULL);
        if(rendFakeRadio != 2)
            RL_AddPoly(quad);
    }

    /*
     * Bottom Shadow
     */
    size = shadowSize + Rend_RadioLongWallBonus(spans[BOTTOM].length) / 2;
    limit = fFloor + size;
    if((quad->vertices[0].pos[VZ] < limit && quad->vertices[3].pos[VZ] > fFloor) &&
       Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR))
    {
        Rend_RadioTexCoordY(quad, -size);
        texture = LST_RADIO_OO;
        // Corners without a neighbour backsector
        if(sideCn[0].corner == -1 || sideCn[1].corner == -1)
        {   // At least one corner faces outwards
            texture = LST_RADIO_OO;
            Rend_RadioTexCoordX(quad, spans[BOTTOM].length, shifts[BOTTOM]);

            if((sideCn[0].corner == -1 && sideCn[1].corner == -1) ||
               (botCn[0].corner == -1 && botCn[1].corner == -1) )
            {   // Both corners face outwards
                texture = LST_RADIO_OO;//CC;
            }
            else if(sideCn[1].corner == -1) // right corner faces outwards
            {
                if(botCn[0].pOffset < 0 && topCn[0].pHeight > fFloor)
                {   // Must flip horizontally!
                    Rend_RadioTexCoordX(quad, -spans[BOTTOM].length, shifts[BOTTOM]);
                    texture = LST_RADIO_OE;
                }
            }
            else
            {   // left corner faces outwards
                if(botCn[1].pOffset < 0 && topCn[1].pHeight > fFloor)
                {
                    texture = LST_RADIO_OE;
                }
            }
        }
        else
        {   // Corners WITH a neighbour backsector
            Rend_RadioTexCoordX(quad, spans[BOTTOM].length, shifts[BOTTOM]);
            if(botCn[0].corner == -1 && botCn[1].corner == -1)
            {   // Both corners face outwards
                texture = LST_RADIO_OO;//CC;
            }
            else if(botCn[1].corner == -1 && botCn[0].corner > MIN_OPEN)
            {   // Right corner faces outwards
                texture = LST_RADIO_OO;
            }
            else if(botCn[0].corner == -1 && botCn[1].corner > MIN_OPEN)
            {   // Left corner faces outwards
                texture = LST_RADIO_OO;
            }
            // Open edges
            else if(botCn[0].corner <= MIN_OPEN && botCn[1].corner <= MIN_OPEN)
            {   // Both edges are open
                texture = LST_RADIO_OO;

                if(botCn[0].proximity && botCn[1].proximity)
                {
                    if(botCn[0].pOffset >= 0 && botCn[1].pOffset < 0)
                    {
                        texture = LST_RADIO_CO;
                        // The shadow can't go over the higher edge.
                        if(size > botCn[0].pOffset)
                        {
                            if(botCn[0].pOffset < INDIFF)
                                texture = LST_RADIO_OE;
                            else
                                Rend_RadioTexCoordY(quad, -botCn[0].pOffset);
                        }
                    }
                    else if(botCn[0].pOffset < 0 && botCn[1].pOffset >= 0)
                    {
                        // Must flip horizontally!
                        texture = LST_RADIO_CO;
                        Rend_RadioTexCoordX(quad, -spans[BOTTOM].length,
                                            shifts[BOTTOM]);

                        if(size > botCn[1].pOffset)
                        {
                            if(botCn[1].pOffset < INDIFF)
                                texture = LST_RADIO_OE;
                            else
                                Rend_RadioTexCoordY(quad, -botCn[1].pOffset);
                        }
                    }
                }
                else
                {
                    if(botCn[0].pOffset < -MINDIFF)
                    {
                        // Must flip horizontally!
                        texture = LST_RADIO_OE;
                        Rend_RadioTexCoordX(quad, -spans[BOTTOM].length,
                                            shifts[BOTTOM]);
                    }
                    else if(botCn[1].pOffset < -MINDIFF)
                        texture = LST_RADIO_OE;
                }
            }
            else if(botCn[0].corner <= MIN_OPEN) // Right Corner is Closed
            {
                if(botCn[0].pOffset < 0)
                    texture = LST_RADIO_CO;
                else
                    texture = LST_RADIO_OO;

                // Must flip horizontally!
                Rend_RadioTexCoordX(quad, -spans[BOTTOM].length, shifts[BOTTOM]);
            }
            else if(botCn[1].corner <= MIN_OPEN)  // Left Corner is closed
            {
                if(botCn[1].pOffset < 0)
                    texture = LST_RADIO_CO;
                else
                    texture = LST_RADIO_OO;
            }
            else  // C/C ???
            {
                texture = LST_RADIO_OO;
            }
        }

        quad->tex.id = GL_PrepareLSTexture(texture, NULL);
        if(rendFakeRadio != 2)
            RL_AddPoly(quad);
    }

    // Walls with glowing floor & ceiling get no side shadows.
    // Is there anything better we can do?
    if(!(Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR)) &&
       !(Rend_RadioNonGlowingFlat(frontSector, PLN_CEILING)))
    {
        R_FreeRendPoly(quad);
        return;
    }

    /*
     * Left/Right Shadows
     */
    size = shadowSize + Rend_RadioLongWallBonus(line->length);
    for(i = 0; i < 2; ++i)
    {
        float       shadowMul;

        quad->flags |= RPF_HORIZONTAL;
        quad->texoffy = quad->vertices[0].pos[VZ] - fFloor;
        quad->tex.height = fCeil - fFloor;

        // Left Shadow
        if(i == 0)
        {
            if(sideCn[0].corner > 0 && xOffset < size)
            {
                quad->texoffx = xOffset;
                // Make sure the shadow isn't too big
                if(size > line->length)
                {
                    if(sideCn[1].corner <= MIN_OPEN)
                        quad->tex.width = line->length;
                    else
                        quad->tex.width = line->length/2;
                }
                else
                    quad->tex.width = size;
            }
            else
                continue;  // Don't draw a left shadow
        }
        else // Right Shadow
        {
            if(sideCn[1].corner > 0 && xOffset + segLength > line->length - size)
            {
                quad->texoffx = -line->length + xOffset;
                // Make sure the shadow isn't too big
                if(size > line->length)
                {
                    if(sideCn[0].corner <= MIN_OPEN)
                        quad->tex.width = -line->length;
                    else
                        quad->tex.width = -(line->length/2);
                }
                else
                    quad->tex.width = -size;
            }
            else
                continue;  // Don't draw a right shadow
        }

        if(line->L_backside)
        {
            if(bFloor > fFloor && bCeil < fCeil)
            {
                if(Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR) &&
                   Rend_RadioNonGlowingFlat(frontSector, PLN_CEILING))
                {
                    texture = LST_RADIO_CC;
                }
                else if(!(Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR)))
                {
                    quad->texoffy = quad->vertices[0].pos[VZ] - fCeil;
                    quad->tex.height = -(fCeil - fFloor);
                    texture = LST_RADIO_CO;
                }
                else
                    texture = LST_RADIO_CO;
            }
            else if(bFloor > fFloor)
            {
                if(Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR) &&
                   Rend_RadioNonGlowingFlat(frontSector, PLN_CEILING))
                {
                    texture = LST_RADIO_CC;
                }
                else if(!(Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR)))
                {
                    quad->texoffy = quad->vertices[0].pos[VZ] - fCeil;
                    quad->tex.height = -(fCeil - fFloor);
                    texture = LST_RADIO_CO;
                }
                else
                    texture = LST_RADIO_CO;
            }
            else if(bCeil < fCeil)
            {
                if(Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR) &&
                   Rend_RadioNonGlowingFlat(frontSector, PLN_CEILING))
                {
                    texture = LST_RADIO_CC;
                }
                else if(!(Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR)))
                {
                    quad->texoffy = quad->vertices[0].pos[VZ] - fCeil;
                    quad->tex.height = -(fCeil - fFloor);
                    texture = LST_RADIO_CO;
                }
                else
                    texture = LST_RADIO_CO;
            }
        }
        else
        {
            if(!(Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR)))
            {
                Rend_RadioTexCoordY(quad, -(fCeil - fFloor));
                texture = LST_RADIO_CO;
            }
            else if(!(Rend_RadioNonGlowingFlat(frontSector, PLN_CEILING)))
                texture = LST_RADIO_CO;
            else
                texture = LST_RADIO_CC;
        }

        quad->tex.id = GL_PrepareLSTexture(texture, NULL);

        shadowMul = sideCn[i].corner;
        shadowMul *= shadowMul * shadowMul;
        Rend_RadioSetColor(quad, shadowMul * shadowDark);
        if(rendFakeRadio != 2)
            RL_AddPoly(quad);
    }
    R_FreeRendPoly(quad);

#undef BOTTOM   
#undef TOP
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
        return 0;               // Fully closed.

    if(fz >= bhz - EDGE_OPEN_THRESHOLD)
        return (bhz - fz) / EDGE_OPEN_THRESHOLD;

    if(fz <= bz)
        return 1 - (bz - fz) / EDGE_OPEN_THRESHOLD;

    if(fz <= bz + EDGE_OPEN_THRESHOLD)
        return 1 + (fz - bz) / EDGE_OPEN_THRESHOLD;

    // Fully open!
    return 2;
}

static void setRelativeHeights(sector_t *front, sector_t *back,
                               boolean isCeiling,
                               float *fz, float *bz, float *bhz)
{
    if(fz)
    {
        *fz  = front->planes[isCeiling? PLN_CEILING:PLN_FLOOR]->visheight;
        if(isCeiling)
            *fz = -(*fz);
    }
    if(bz)
    {
        *bz  =  back->planes[isCeiling? PLN_CEILING:PLN_FLOOR]->visheight;
        if(isCeiling)
            *bz = -(*bz);
    }
    if(bhz)
    {
        *bhz =  back->planes[isCeiling? PLN_FLOOR:PLN_CEILING]->visheight;
        if(isCeiling)
            *bhz = -(*bhz);
    }
}

static uint radioEdgeHackType(line_t *line, sector_t *front, sector_t *back,
                              int backside, boolean isCeiling, float fz,
                              float bz)
{
    surface_t  *surface = &line->L_side(backside)->
                             sections[isCeiling? SEG_TOP:SEG_BOTTOM];

    if(fz < bz && surface->SM_texture == 0 && !(surface->flags & SUF_TEXFIX))
        return 3; // Consider it fully open.

    // Is the back sector closed?
    if(front->SP_floorvisheight >= back->SP_ceilvisheight)
    {
        if(R_IsSkySurface(&front->planes[isCeiling? PLN_FLOOR:PLN_CEILING]->surface))
        {
            if(R_IsSkySurface(&back->planes[isCeiling? PLN_FLOOR:PLN_CEILING]->surface))
                return 3; // Consider it fully open.
        }
        else
            return 1; // Consider it fully closed.
    }

    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(Rend_DoesMidTextureFillGap(line, backside))
        return 1; // Consider it fully closed.

    return 0;
}

/**
 * Calculate the corner coordinates and add a new shadow polygon to the
 * rendering lists.
 */
static void Rend_RadioAddShadowEdge(shadowpoly_t *shadow, boolean isCeiling,
                                    float darkness, float sideOpen[2], float z)
{
    uint        i;
    uint        wind; // Winding: 0 = left, 1 = right
    const uint *idx;
    rendpoly_t *q;
    rendpoly_vertex_t *vtx;
    float       pos;
    static const uint floorIndices[][4] = {{0, 1, 2, 3}, {1, 2, 3, 0}};
    static const uint ceilIndices[][4]  = {{0, 3, 2, 1}, {1, 0, 3, 2}};
    vec2_t      inner[2];

    // Sector lightlevel affects the darkness of the shadows.
    if(darkness > 1)
        darkness = 1;

    darkness *= shadowDark;

    // Determine the inner shadow corners.
    for(i = 0; i < 2; ++i)
    {
        pos = sideOpen[i];
        if(pos < 1)             // Nearly closed.
        {
            /*V2_Lerp(inner[i], shadow->inoffset[i],
               shadow->bextoffset[i], pos);*/
            V2_Sum(inner[i], shadow->outer[i]->V_pos, shadow->inoffset[i]);
        }
        else if(pos == 1)       // Same height on both sides.
        {
            // We need to use a back extended offset but which one?
            // Walk around the vertex and choose the bextoffset for the
            // back neighbor at which plane heights differ.
            lineowner_t *base, *p;
            vertex_t   *vtx =
                shadow->seg->linedef->L_v(i^!(shadow->flags & SHPF_FRONTSIDE));
            uint        id;
            boolean     found;

            base = R_GetVtxLineOwner(vtx, shadow->seg->linedef);
            p = base->link[!i];
            id = 0;
            found = false;
            while(p != base && !found)
            {
                if(!(p->line->L_frontside && p->line->L_backside &&
                     p->line->L_frontsector == p->line->L_backsector))
                {
                    if(!p->line->L_backside)
                    {
                        if((isCeiling &&
                            p->line->L_frontsector->SP_ceilvisheight == z) ||
                           (!isCeiling &&
                            p->line->L_frontsector->SP_floorvisheight == z))
                            found = true;
                    }
                    else
                    {
                        vertex_t    *pvtx[2];

                        pvtx[0] = p->line->L_v1;
                        pvtx[1] = p->line->L_v2;

                        if((isCeiling &&
                            ((p->line->L_frontsector->SP_ceilvisheight < z ||
                              p->line->L_backsector->SP_ceilvisheight < z) ||
                             ((pvtx[i^1] == vtx &&
                               p->line->L_backsector->SP_floorvisheight >=
                               shadow->ssec->sector->SP_ceilvisheight) ||
                              (pvtx[i] == vtx &&
                               p->line->L_frontsector->SP_floorvisheight >=
                               shadow->ssec->sector->SP_ceilvisheight)))) ||
                           (!isCeiling &&
                            ((p->line->L_frontsector->SP_floorvisheight > z ||
                              p->line->L_backsector->SP_floorvisheight > z) ||
                             ((pvtx[i^1] == vtx &&
                               p->line->L_backsector->SP_ceilvisheight <=
                               shadow->ssec->sector->SP_floorvisheight) ||
                              (pvtx[i] == vtx &&
                               p->line->L_frontsector->SP_ceilvisheight <=
                               shadow->ssec->sector->SP_floorvisheight)))) ||
                           Rend_DoesMidTextureFillGap(p->line, pvtx[i] == vtx))
                        {
                            found = true;
                        }
                    }

                    if(!found)
                        id++;
                }

                if(!p->line->L_backside)
                    break;

                if(!found)
                    p = p->link[!i];
            }

            if(found)
            {
                // id is now the index + 1 into the side's bextoffset array.
                V2_Sum(inner[i], shadow->outer[i]->V_pos, shadow->bextoffset[i][id-1].offset);
            }
            else // Its an open edge.
            {
                V2_Sum(inner[i], shadow->outer[i]->V_pos, shadow->extoffset[i]);
            }
        }
        else                    // Fully, unquestionably open.
        {
            if(pos > 2) pos = 2;
            /*V2_Lerp(inner[i], shadow->bextoffset[i],
               shadow->extoffset[i], pos - 1); */
            V2_Sum(inner[i], shadow->outer[i]->V_pos, shadow->extoffset[i]);
        }
    }

    // What vertex winding order?
    // (for best results, the cross edge should always be the shortest).
    wind = (V2_Distance(inner[1], shadow->outer[1]->V_pos) >
                V2_Distance(inner[0], shadow->outer[0]->V_pos)? 1 : 0);

    // Initialize the rendpoly.
    q = R_AllocRendPoly(RP_FLAT, false, 4);
    if(!renderWireframe)
        q->flags = RPF_SHADOW;
    memset(&q->tex, 0, sizeof(q->tex));
    memset(&q->intertex, 0, sizeof(q->intertex));
    q->interpos = 0;
    q->lightListIdx = 0;
    memset(q->vertices, 0, q->numvertices * sizeof(rendpoly_vertex_t));

    q->normal[0] = 0;
    q->normal[1] = 0;
    q->normal[2] = (isCeiling? -1 : 1);

    vtx = q->vertices;
    idx = (isCeiling ? ceilIndices[wind] : floorIndices[wind]);

    // Left outer corner.
    vtx[idx[0]].pos[VX] = shadow->outer[0]->V_pos[VX];
    vtx[idx[0]].pos[VY] = shadow->outer[0]->V_pos[VY];
    vtx[idx[0]].pos[VZ] = z;
    vtx[idx[0]].color.rgba[CA] = (DGLubyte) (255 * darkness);

    if(renderWireframe)
        vtx[idx[0]].color.rgba[CR] = vtx[idx[0]].color.rgba[CG] =
            vtx[idx[0]].color.rgba[CB] = 255;

    if(sideOpen[0] < 1)
        vtx[idx[0]].color.rgba[CA] *= 1 - sideOpen[0];

    // Right outer corner.
    vtx[idx[1]].pos[VX] = shadow->outer[1]->V_pos[VX];
    vtx[idx[1]].pos[VY] = shadow->outer[1]->V_pos[VY];
    vtx[idx[1]].pos[VZ] = z;
    vtx[idx[1]].color.rgba[CA] = (DGLubyte) (255 * darkness);

    if(renderWireframe)
        vtx[idx[1]].color.rgba[CR] = vtx[idx[1]].color.rgba[CG] =
            vtx[idx[1]].color.rgba[CB] = 255;

    if(sideOpen[1] < 1)
        vtx[idx[1]].color.rgba[CA] *= 1 - sideOpen[1];

    // Right inner corner.
    vtx[idx[2]].pos[VX] = inner[1][VX];
    vtx[idx[2]].pos[VY] = inner[1][VY];
    vtx[idx[2]].pos[VZ] = z;

    if(renderWireframe)
        vtx[idx[2]].color.rgba[CR] = vtx[idx[2]].color.rgba[CG] =
            vtx[idx[2]].color.rgba[CB] = 255;

    // Left inner corner.
    vtx[idx[3]].pos[VX] = inner[0][VX];
    vtx[idx[3]].pos[VY] = inner[0][VY];
    vtx[idx[3]].pos[VZ] = z;

    if(renderWireframe)
        vtx[idx[3]].color.rgba[CR] = vtx[idx[3]].color.rgba[CG] =
            vtx[idx[3]].color.rgba[CB] = 255;

    if(rendFakeRadio != 2)
        RL_AddPoly(q);
    R_FreeRendPoly(q);
}

/**
 * Render the shadowpolygons linked to the subsector, if they haven't
 * already been rendered.
 *
 * Don't use the global radio state in here, the subsector can be part of
 * any sector, not the one chosen for wall rendering.
 */
void Rend_RadioSubsectorEdges(subsector_t *subsector)
{
    uint        i, pln, hack, side;
    float       open, sideOpen[2], vec[3];
    float       fz, bz, bhz, plnHeight;
    sector_t   *shadowSec, *front, *back;
    line_t     *line;
    surface_t  *suf;
    shadowlink_t *link;
    shadowpoly_t *shadow;

    if(!rendFakeRadio || levelFullBright)
        return;

BEGIN_PROF( PROF_RADIO_SUBSECTOR );

    // We need to check all the shadowpolys linked to this subsector.
    for(link = subsector->shadows; link != NULL; link = link->next)
    {
        // Already rendered during the current frame? We only want to
        // render each shadow once per frame.
        if(link->poly->visframe == (ushort) frameCount)
            continue;

        // Now it will be rendered.
        shadow = link->poly;

        // Unless seg is part of a polyobject.
        if(shadow->seg->flags & SEGF_POLYOBJ)
            continue;

        shadow->visframe = (ushort) frameCount;

        // Determine the openness of the line and its neighbors.  If
        // this edge is open, there won't be a shadow at all.  Open
        // neighbours cause some changes in the polygon corner
        // vertices (placement, colour).

        vec[VX] = vx - subsector->midpoint.pos[VX];
        vec[VY] = vz - subsector->midpoint.pos[VY];

        for(pln = 0; pln < subsector->sector->planecount; ++pln)
        {
            suf = &subsector->sector->planes[pln]->surface;

            shadowSec = R_GetShadowSector(shadow, pln, true);
            plnHeight = shadowSec->SP_planevisheight(pln);
            vec[VZ] = vy - plnHeight;

            // Glowing surfaces or missing textures shouldn't have shadows.
            if((suf->flags & SUF_NO_RADIO) ||
               !Rend_RadioNonGlowingFlat(subsector->sector, pln))
                continue;

            // Don't bother with planes facing away from the camera.
            if(M_DotProduct(vec, suf->normal) < 0)
                continue;

            line = shadow->seg->linedef;
            side = (shadow->flags & SHPF_FRONTSIDE) == 0;

            if(line->L_backside)
            {
                if(subsector->sector->subsgroups[subsector->group].linked[pln] &&
                   !devNoLinkedSurfaces)
                {
                    if(side == 0)
                    {
                        front = shadowSec;
                        back  = line->L_backsector;
                    }
                    else
                    {
                        front = line->L_frontsector;
                        back  = shadowSec;
                    }
                }
                else
                {
                    front = line->L_sector(side);
                    back  = line->L_sector(side ^ 1);
                }
                setRelativeHeights(front, back, pln, &fz, &bz, &bhz);

                hack =
                    radioEdgeHackType(line, front, back, side, pln, fz, bz);
                if(hack)
                    open = hack - 1;
                else
                    open = radioEdgeOpenness(fz, bz, bhz);
            }
            else
                open = 0;

            if(open >= 1)
                continue;

            // What about the neighbours?
            for(i = 0; i < 2; ++i)
            {
                line_t *neighbor =
                    R_FindLineNeighbor(shadow->ssec->sector, line,
                                       line->L_vo(side^i), i, NULL);
                if(neighbor && neighbor->L_backside)
                {
                    uint side2 = (neighbor->L_frontsector != shadowSec);

                    front = neighbor->L_sector(side2);
                    back  = neighbor->L_sector(side2 ^ 1);
                    setRelativeHeights(front, back, pln, &fz, &bz, &bhz);

                    hack = radioEdgeHackType(neighbor, front, back, side2, pln, fz, bz);
                    if(hack)
                        sideOpen[i] = hack - 1;
                    else
                        sideOpen[i] = radioEdgeOpenness(fz, bz, bhz);
                }
                else
                    sideOpen[i] = 0;//(!neighbor? 2 : 0);
            }

            Rend_RadioAddShadowEdge(shadow, pln, 1 - open, sideOpen,
                                    plnHeight);
        }
    }

END_PROF( PROF_RADIO_SUBSECTOR );
}
