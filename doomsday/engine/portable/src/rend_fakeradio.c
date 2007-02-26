/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

typedef struct shadowcorner_s {
    float   corner;
    sector_t *proximity;
    float   pOffset;
    float   pHeight;
} shadowcorner_t;

typedef struct edgespan_s {
    float   length;
    float   shift;
} edgespan_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

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
    C_VAR_INT("rend-fakeradio", &rendFakeRadio, 0, 0, 1);

    C_VAR_FLOAT("rend-fakeradio-darkness", &rendFakeRadioDarkness, 0, 0, 2);
}

static __inline float Rend_RadioShadowDarkness(int lightlevel)
{
    return (.65f - lightlevel / 850.0f) * rendFakeRadioDarkness;
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
 * Before calling the other rendering routines, this must be called to
 * initialize the state of the FakeRadio renderer.
 */
void Rend_RadioInitForSubsector(subsector_t *ssec)
{
    sector_t *linkSec;
    int sectorlight = ssec->sector->lightlevel;

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
    // FIXME: Make cvars out of constants.
    shadowSize = 2 * (8 + 16 - sectorlight / 16);
    shadowDark = Rend_RadioShadowDarkness(sectorlight) *.8f;
}

/**
 * @return      <code>true</code> if the specified flat is non-glowing, i.e.
 *              not glowing or a sky.
 */
static __inline boolean Rend_RadioNonGlowingFlat(sector_t* sector, int plane)
{
    return !(sector->planes[plane]->surface.texture <= 0 ||
             sector->planes[plane]->glow ||
             R_IsSkySurface(&sector->planes[plane]->surface));
}

/**
 * Set the vertex colors in the rendpoly.
 */
static void Rend_RadioSetColor(rendpoly_t *q, float darkness)
{
    int     i;

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
 * Returns true if there is open space in the sector.
 */
static __inline boolean Rend_IsSectorOpen(sector_t *sector)
{
    return (sector &&
            sector->SP_ceilheight > sector->SP_floorheight);
}

/**
 * FIXME: No need to do this each frame. Set a flag in side_t->flags to
 *        denote this. Is sensitive to plane heights, surface properties
 *        (e.g. alpha) and surface texture properties.
 */
static boolean Rend_DoesMidTextureFillGap(line_t *line, int backside)
{
    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(line->L_backsector)
    {
        sector_t *front = line->sec[backside];
        sector_t *back  = line->sec[backside ^ 1];
        side_t   *side  = line->sides[backside];

        if(side->SW_middlepic != 0)
        {
            boolean masked;
            int     texheight;

            if(side->SW_middlepic > 0)
            {
                if(side->SW_middleisflat)
                    GL_PrepareFlat2(side->SW_middlepic, true);
                else
                    GL_GetTextureInfo(side->SW_middlepic);
                masked = texmask;
                texheight = texh;
            }
            else // It's a DDay texture.
            {
                masked = false;
                texheight = 64;
            }

            if(!side->blendmode && side->SW_middlergba[3] == 255 && !masked)
            {
                float openTop[2], gapTop[2];
                float openBottom[2], gapBottom[2];

                openTop[0] = openTop[1] = gapTop[0] = gapTop[1] =
                    MIN_OF(back->SP_ceilvisheight, front->SP_ceilvisheight);
                openBottom[0] = openBottom[1] = gapBottom[0] = gapBottom[1] =
                    MAX_OF(back->SP_floorvisheight, front->SP_floorvisheight);

                // Could the mid texture fill enough of this gap for us
                // to consider it completely closed?
                if(texheight >= (openTop[0] - openBottom[0]) &&
                   texheight >= (openTop[1] - openBottom[1]))
                {
                    // Possibly. Check the placement of the mid texture.
                    if(Rend_MidTexturePos
                       (&gapBottom[0], &gapBottom[1], &gapTop[0], &gapTop[1],
                        NULL, side->SW_middleoffy,
                        0 != (line->flags & ML_DONTPEGBOTTOM)))
                    {
                        if(openTop[0] >= gapTop[0] &&
                           openTop[1] >= gapTop[1] &&
                           openBottom[0] <= gapBottom[0] &&
                           openBottom[1] <= gapBottom[1])
                            return true;
                    }
                }
            }
        }
    }

    return false;
}

/**
 * Returns the corner shadow factor.
 */
static float Rend_RadioLineCorner(line_t *self, line_t *other,
                                  const sector_t *mySector)
{
    vertex_t   *myVtx[2], *otherVtx[2];
    boolean     flipped = false;

    float   oFFloor, oFCeil;
    float   oBFloor, oBCeil;

    binangle_t diff = self->angle - other->angle;

    // Sort the vertices so they can be compared consistently.
    R_OrderVertices(self, mySector, myVtx);
    R_OrderVertices(other, mySector, otherVtx);

    if(myVtx[0] == other->v[0] || myVtx[1] == other->v[1])
    {
        // The line normals are not facing the same direction.
        diff -= BANG_180;
    }
    if(myVtx[0] == otherVtx[1])
    {
        // The other is on our left side.
        // We want the difference: (leftmost wall) - (rightmost wall)
        if(self->L_frontsector == mySector)
            diff = -diff;
        else
            flipped = true;
    }
    else if(myVtx[1] == otherVtx[0])
    {
        if(self->L_backsector == mySector)
        {
            diff = -diff;
            flipped = true;
        }
    }

    if(diff > BANG_180)
    {
        // The corner between the walls faces outwards.
        return -1;
    }
    else if(diff == BANG_180)
    {
        return 0;
    }
    if(other->L_frontsector && other->L_backsector)
    {
        oFCeil  = other->L_frontsector->SP_ceilvisheight;
        oFFloor = other->L_frontsector->SP_floorvisheight;
        oBCeil  = other->L_backsector->SP_ceilvisheight;
        oBFloor = other->L_backsector->SP_floorvisheight;

        if((other->L_frontsector == mySector &&
            ((oBCeil > fFloor && oBFloor <= fFloor) ||
             (oBFloor < fCeil && oBCeil >= fCeil) ||
             (oBFloor < fCeil && oBCeil > fFloor))) ||
           (other->L_backsector == mySector &&
            ((oFCeil > fFloor && oFFloor <= fFloor) ||
             (oFFloor < fCeil && oFCeil >= fCeil) ||
             (oFFloor < fCeil && oFCeil > fFloor)))  )
        {
            if(Rend_IsSectorOpen(other->L_frontsector) &&
               Rend_IsSectorOpen(other->L_backsector))
            {
                if(!Rend_DoesMidTextureFillGap(other, other->L_frontsector != mySector))
                    return 0;
            }
        }
    }

    if(diff < BANG_45 / 5)
    {
        // The difference is too small, there won't be a shadow.
        return 0;
    }

    // 90 degrees is the largest effective difference.
    if(diff > BANG_90)
    {
        if(flipped)
        {
            // The difference is too small, there won't be a shadow.
            if(diff > BANG_180 - (BANG_45 /5))
                return 0;

            return (float) BANG_90 / diff;
        }
        else
            diff = BANG_90;
    }
    return (float) diff / BANG_90;
}

/**
 * Set the rendpoly's X offset and texture size.  A negative length
 * implies that the texture is flipped horizontally.
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
 * Set the rendpoly's Y offset and texture size.  A negative size
 * implies that the texture is flipped vertically.
 */
static void Rend_RadioTexCoordY(rendpoly_t *q, float size)
{
    if((q->tex.height = size) > 0)
        q->texoffy = fCeil - q->vertices[2].pos[VZ];
    else
        q->texoffy = fFloor - q->vertices[2].pos[VZ];
}

/**
 * Returns the side number (0, 1) of the neighbour line.
 */
static int R_GetAlignedNeighbor(line_t **neighbor, const line_t *line,
                                int side, boolean leftNeighbor)
{
    uint        i;
    side_t     *sid;

    if(!line->sides[side])
        return 0;

    sid = line->sides[side];

    *neighbor = sid->alignneighbor[leftNeighbor ? 0 : 1];
    if(!*neighbor)
        return 0;

    // We need to decide which side of the neighbor is chosen.
    for(i = 0; i < 2; ++i)
    {
        sid = (*neighbor)->sides[i];
        if(sid)
        {
            // Do the selection based on the backlink.
            if(sid->alignneighbor[leftNeighbor ? 1 : 0] == line)
                return i;
        }
    }

    // This is odd... There is no link back to the first line.
    // Better take no chances.
    *neighbor = NULL;
    return 0;
}

/**
 * Scan a set of aligned neighbours.  Scans simultaneously both the top and
 * bottom edges.  Looks a bit complicated, but that's because the algorithm
 * must handle both the left and right directions, and scans the top and
 * bottom edges at the same time.
 *
 * TODO: DJS
 *       We should stop the scan when a hack sector is encountered, at which
 *       point we should implicitly set the corner to -1.
 *       Clean this up... I've made a bit of mess.
 */
static void Rend_RadioScanNeighbors(shadowcorner_t top[2],
                                    shadowcorner_t bottom[2], line_t *line,
                                    unsigned int side, edgespan_t spans[2],
                                    boolean toLeft)
{
    struct edge_s {
        boolean done;
        line_t *line;
        side_t *side;
        sector_t *sector;
        float   length;
    } edges[2];                 // bottom, top

    line_t     *iter;
    sector_t   *scanSector;
    unsigned int i, scanSide;
    int         nIdx = (toLeft ? 0 : 1); // neighbour index
    float       gap[2];
    float       iFFloor, iFCeil;
    float       iBFloor, iBCeil;
    boolean     closed;

    edges[0].done = edges[1].done = false;
    edges[0].length = edges[1].length = 0;
    gap[0] = gap[1] = 0;

    // Use validcount to detect looped neighbours, which may occur
    // with strange map geometry (probably due to bugs in r_shadow.c).
    ++validcount;

    for(iter = line, scanSide = side; !(edges[0].done && edges[1].done);)
    {
        scanSector = iter->sec[scanSide==0? FRONT:BACK];

        // Should we stop?
        if(iter != line)
        {
            if(!Rend_IsSectorOpen(scanSector))
            {
                edges[0].done = edges[1].done = true;
            }
            else
            {
                if(scanSector->SP_floorvisheight != fFloor)
                    edges[0].done = true;
                if(scanSector->SP_ceilvisheight != fCeil)
                    edges[1].done = true;
            }

            if(edges[0].done && edges[1].done)
                break;

            // Look out for loops.
            if(iter->validcount == validcount)
                break;
            iter->validcount = validcount;
        }

        iFFloor = iter->L_frontsector->SP_floorvisheight;
        iFCeil  = iter->L_frontsector->SP_ceilvisheight;

        if(iter->L_backsector)
        {
            iBFloor = iter->L_backsector->SP_floorvisheight;
            iBCeil  = iter->L_backsector->SP_ceilvisheight;
        }
        else
            iBFloor = iBCeil = 0;

        // We'll do the bottom and top simultaneously.
        for(i = 0; i < 2; ++i)
        {
            if(edges[i].done)
                continue;

            edges[i].line = iter;
            if(iter->sides[scanSide])
                edges[i].side = iter->sides[scanSide];
            else
                edges[i].side = NULL;
            edges[i].sector = scanSector;

            if(iter == line)
                continue;

            if(i==0)  // Bottom
            {
                closed = false;
                if(side == 0 && iter->L_backsector != NULL)
                    if(iBCeil <= fFloor)
                        closed = true;  // compared to "this" sector anyway

                if((side == 0 && iter->L_backsector == line->L_frontsector &&
                    iFFloor <= fFloor) ||
                   (side == 1 && iter->L_backsector == line->L_backsector &&
                    iFFloor <= fFloor) ||
                   (side == 0 && closed == false && iter->L_backsector != NULL &&
                    iter->L_backsector != line->L_frontsector && iBFloor <= fFloor &&
                    Rend_IsSectorOpen(iter->L_backsector)))
                {
                    gap[i] += iter->length;  // Should we just mark it done instead?
                }
                else
                {
                    edges[i].length += iter->length + gap[i];
                    gap[i] = 0;
                }
            }
            else  // Top
            {
                closed = false;
                if(side == 0 && iter->L_backsector != NULL)
                    if(iBFloor >= fCeil)
                        closed = true;  // compared to "this" sector anyway

                if((side == 0 && iter->L_backsector == line->L_frontsector &&
                    iFCeil >= fCeil) ||
                   (side == 1 && iter->L_backsector == line->L_backsector &&
                    iFCeil >= fCeil) ||
                    (side == 0 && closed == false && iter->L_backsector != NULL &&
                    iter->L_backsector != line->L_frontsector && iBCeil >= fCeil &&
                    Rend_IsSectorOpen(iter->L_backsector)))
                {
                    gap[i] += iter->length;  // Should we just mark it done instead?
                }
                else
                {
                    edges[i].length += iter->length + gap[i];
                    gap[i] = 0;
                }
            }
        }

        scanSide = R_GetAlignedNeighbor(&iter, iter, scanSide, toLeft);

        // Stop the scan?
        if(!iter)
            break;
    }

    for(i = 0; i < 2; ++i)      // 0=bottom, 1=top
    {
        shadowcorner_t *corner = (i == 0 ? &bottom[nIdx] : &top[nIdx]);

        // Increment the apparent line length/offset.
        spans[i].length += edges[i].length;
        if(toLeft)
            spans[i].shift += edges[i].length;

        if(edges[i].side && edges[i].side->neighbor[nIdx])
        {
            if(edges[i].side->pretendneighbor[nIdx]) // It's a pretend neighbor.
                corner->corner = 0;
            else
            {
                corner->corner =
                    Rend_RadioLineCorner(edges[i].line,
                                         edges[i].side->neighbor[nIdx],
                                         edges[i].sector);
            }
        }
        else
        {
            corner->corner = 0;
        }

        if(edges[i].side && edges[i].side->proxsector[nIdx])
        {
            corner->proximity = edges[i].side->proxsector[nIdx];
            if(i == 0)          // floor
            {
                corner->pOffset = corner->proximity->SP_floorvisheight - fFloor;
                corner->pHeight = edges[i].side->proxsector[nIdx]->SP_floorvisheight;
            }
            else                // ceiling
            {
                corner->pOffset = corner->proximity->SP_ceilvisheight - fCeil;
                corner->pHeight = edges[i].side->proxsector[nIdx]->SP_ceilvisheight;
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
static void Rend_RadioScanEdges(shadowcorner_t topCorners[2],
                                shadowcorner_t bottomCorners[2],
                                shadowcorner_t sideCorners[2], line_t *line,
                                unsigned int side, edgespan_t spans[2])
{
    side_t     *sid;
    unsigned int i;

    sid = line->sides[side];

    memset(sideCorners, 0, sizeof(sideCorners));

    // Find the sidecorners first: left and right neighbour.
    for(i = 0; i < 2; ++i)
    {
        if(sid && sid->neighbor[i] && !sid->pretendneighbor[i])
        {
            sideCorners[i].corner =
                Rend_RadioLineCorner(line, sid->neighbor[i], frontSector);
        }
        else
            sideCorners[i].corner = 0;

        // Scan left/right (both top and bottom).
        Rend_RadioScanNeighbors(topCorners, bottomCorners, line, side, spans,
                                !i);
    }
}

/**
 * Long walls get slightly larger shadows. The bonus will simply be added
 * to the shadow size for the wall in question.
 */
static float Rend_RadioLongWallBonus(float span)
{
    float   limit;

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
 * directly from Rend_RenderWallSeg()).
 */
void Rend_RadioWallSection(const seg_t *seg, rendpoly_t *origQuad)
{
    sector_t   *backSector;
    float       bFloor, bCeil, limit, size;
    rendpoly_t *quad;
    int         i, texture = 0, sideNum;
    shadowcorner_t topCn[2], botCn[2], sideCn[2];
    edgespan_t  spans[2];        // bottom, top
    edgespan_t *floorSpan = &spans[0], *ceilSpan = &spans[1];

    if(!rendFakeRadio || levelFullBright || shadowSize <= 0 ||  // Disabled?
       origQuad->flags & RPF_GLOW || seg->linedef == NULL)
        return;

    backSector = seg->SG_backsector;

    // Choose the correct side.
    if(seg->linedef->L_frontsector == frontSector)
        sideNum = 0;
    else
        sideNum = 1;

    // Determine the shadow properties on the edges of the poly.
    for(i = 0; i < 2; ++i)
    {
        spans[i].length = seg->linedef->length;
        spans[i].shift = seg->offset;
    }
    Rend_RadioScanEdges(topCn, botCn, sideCn, seg->linedef, sideNum, spans);

    // Back sector visible plane heights.
    if(backSector)
    {
        bFloor = backSector->SP_floorvisheight;
        bCeil  = backSector->SP_ceilvisheight;
    }
    else
        bFloor = bCeil = 0;

    quad = R_AllocRendPoly(RP_QUAD, true, 4);
    R_MemcpyRendPoly(quad, origQuad);

    // Init the quad.
    quad->flags = RPF_SHADOW;
    quad->texoffx = seg->offset;
    quad->texoffy = 0;
    quad->tex.id = GL_PrepareLSTexture(LST_RADIO_CC);
    quad->tex.detail = NULL;
    quad->tex.width = seg->linedef->length;
    quad->tex.height = shadowSize;
    quad->lights = NULL;
    quad->intertex.id = 0;
    quad->intertex.detail = NULL;

    // Fade the shadow out if the height is below the min height.
    if(quad->vertices[2].pos[VZ] - quad->vertices[0].pos[VZ] < EDGE_OPEN_THRESHOLD)
        Rend_RadioSetColor(quad, shadowDark *
                              ((quad->vertices[2].pos[VZ] - quad->vertices[0].pos[VZ]) / EDGE_OPEN_THRESHOLD));
    else
        Rend_RadioSetColor(quad, shadowDark);

    /*
     * Top Shadow
     */
    // The top shadow will reach this far down.
    size = shadowSize + Rend_RadioLongWallBonus(ceilSpan->length);
    limit = fCeil - size;
    if((quad->vertices[2].pos[VZ] > limit && quad->vertices[0].pos[VZ] < fCeil) &&
       Rend_RadioNonGlowingFlat(frontSector, PLN_CEILING))
    {
        Rend_RadioTexCoordY(quad, size);
        texture = LST_RADIO_OO;
        // Corners without a neighbour backsector
        if(sideCn[0].corner == -1 || sideCn[1].corner == -1)
        {   // At least one corner faces outwards
            texture = LST_RADIO_OO;
            Rend_RadioTexCoordX(quad, ceilSpan->length, ceilSpan->shift);

            if((sideCn[0].corner == -1 && sideCn[1].corner == -1) ||
               (topCn[0].corner == -1 && topCn[1].corner == -1) )
            {   // Both corners face outwards
                texture = LST_RADIO_OO;//CC;
            }
            else if(sideCn[1].corner == -1)
            {   // right corner faces outwards
                if(-topCn[0].pOffset < 0 && botCn[0].pHeight < fCeil)
                {// Must flip horizontally!
                    Rend_RadioTexCoordX(quad, -ceilSpan->length, ceilSpan->shift);
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
            Rend_RadioTexCoordX(quad, ceilSpan->length, ceilSpan->shift);
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
                        Rend_RadioTexCoordX(quad, -ceilSpan->length, ceilSpan->shift);

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
                        Rend_RadioTexCoordX(quad, -floorSpan->length,
                                            floorSpan->shift);
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
                Rend_RadioTexCoordX(quad, -ceilSpan->length, ceilSpan->shift);
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

        quad->tex.id = GL_PrepareLSTexture(texture);
        RL_AddPoly(quad);
    }

    /*
     * Bottom Shadow
     */
    size = shadowSize + Rend_RadioLongWallBonus(floorSpan->length) / 2;
    limit = fFloor + size;
    if((quad->vertices[0].pos[VZ] < limit && quad->vertices[2].pos[VZ] > fFloor) &&
       Rend_RadioNonGlowingFlat(frontSector, PLN_FLOOR))
    {
        Rend_RadioTexCoordY(quad, -size);
        texture = LST_RADIO_OO;
        // Corners without a neighbour backsector
        if(sideCn[0].corner == -1 || sideCn[1].corner == -1)
        {   // At least one corner faces outwards
            texture = LST_RADIO_OO;
            Rend_RadioTexCoordX(quad, floorSpan->length, floorSpan->shift);

            if((sideCn[0].corner == -1 && sideCn[1].corner == -1) ||
               (botCn[0].corner == -1 && botCn[1].corner == -1) )
            {   // Both corners face outwards
                texture = LST_RADIO_OO;//CC;
            }
            else if(sideCn[1].corner == -1) // right corner faces outwards
            {
                if(botCn[0].pOffset < 0 && topCn[0].pHeight > fFloor)
                {   // Must flip horizontally!
                    Rend_RadioTexCoordX(quad, -floorSpan->length, floorSpan->shift);
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
            Rend_RadioTexCoordX(quad, floorSpan->length, floorSpan->shift);
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
                        Rend_RadioTexCoordX(quad, -floorSpan->length,
                                            floorSpan->shift);

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
                        Rend_RadioTexCoordX(quad, -floorSpan->length,
                                            floorSpan->shift);
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
                Rend_RadioTexCoordX(quad, -floorSpan->length, floorSpan->shift);
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

        quad->tex.id = GL_PrepareLSTexture(texture);
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
    size = shadowSize + Rend_RadioLongWallBonus(seg->linedef->length);
    for(i = 0; i < 2; ++i)
    {
        quad->flags |= RPF_HORIZONTAL;
        quad->texoffy = quad->vertices[0].pos[VZ] - fFloor;
        quad->tex.height = fCeil - fFloor;

        // Left Shadow
        if(i == 0)
        {
            if(sideCn[0].corner > 0 && seg->offset < size)
            {
                quad->texoffx = seg->offset;
                // Make sure the shadow isn't too big
                if(size > seg->linedef->length)
                {
                    if(sideCn[1].corner <= MIN_OPEN)
                        quad->tex.width = seg->linedef->length;
                    else
                        quad->tex.width = seg->linedef->length/2;
                }
                else
                    quad->tex.width = size;
            }
            else
                continue;  // Don't draw a left shadow
        }
        else // Right Shadow
        {
            if(sideCn[1].corner > 0 && seg->offset + seg->length > seg->linedef->length - size)
            {
                quad->texoffx = -seg->linedef->length + seg->offset;
                // Make sure the shadow isn't too big
                if(size > seg->linedef->length)
                {
                    if(sideCn[0].corner <= MIN_OPEN)
                        quad->tex.width = -seg->linedef->length;
                    else
                        quad->tex.width = -(seg->linedef->length/2);
                }
                else
                    quad->tex.width = -size;
            }
            else
                continue;  // Don't draw a right shadow
        }

        if(seg->SG_backsector)
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

        quad->tex.id = GL_PrepareLSTexture(texture);

        Rend_RadioSetColor(quad, sideCn[i].corner * shadowDark);
        RL_AddPoly(quad);
    }
    R_FreeRendPoly(quad);
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
    surface_t  *surface = &line->sides[backside? BACK:FRONT]->
                             sections[isCeiling? SEG_TOP:SEG_BOTTOM];

    if(fz < bz && surface->texture == 0 && !(surface->flags & SUF_TEXFIX))
        return 3; // Consider it fully open.

    // Is the back sector closed?
    if(front->SP_floorvisheight >= back->SP_ceilvisheight)
        if(R_IsSkySurface(&front->planes[isCeiling? PLN_FLOOR:PLN_CEILING]->surface))
        {
            if(R_IsSkySurface(&back->planes[isCeiling? PLN_FLOOR:PLN_CEILING]->surface))
                return 3; // Consider it fully open.
        }
        else
            return 1; // Consider it fully  closed.

    // Check for unmasked midtextures on twosided lines that completely
    // fill the gap between floor and ceiling (we don't want to give away
    // the location of any secret areas (false walls)).
    if(Rend_DoesMidTextureFillGap(line, backside))
        return 1; // Consider it fully  closed.

    return 0;
}

/**
 * Calculate the corner coordinates and add a new shadow polygon to the
 * rendering lists.
 */
static void Rend_RadioAddShadowEdge(shadowpoly_t *shadow, boolean isCeiling,
                                    float darkness, float sideOpen[2], float z)
{
    rendpoly_t *q;
    rendpoly_vertex_t *vtx;
    float       pos;
    uint        i;
    // Winding: 0 = left, 1 = right
    uint        wind;
    const uint *idx;
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
            V2_Sum(inner[i], shadow->outer[i]->pos, shadow->inoffset[i]);
        }
        else if(pos == 1)       // Same height on both sides.
        {
            V2_Sum(inner[i], shadow->outer[i]->pos, shadow->bextoffset[i]);
        }
        else                    // Fully, unquestionably open.
        {
            if(pos > 2) pos = 2;
            /*V2_Lerp(inner[i], shadow->bextoffset[i],
               shadow->extoffset[i], pos - 1); */
            V2_Sum(inner[i], shadow->outer[i]->pos, shadow->extoffset[i]);
        }
    }

    // What vertex winding order?
    // (for best results, the cross edge should always be the shortest).
    wind = (V2_Distance(inner[1], shadow->outer[1]->pos) >
                V2_Distance(inner[0], shadow->outer[0]->pos)? 1 : 0);

        // Initialize the rendpoly.
    q = R_AllocRendPoly(RP_FLAT, false, 4);
    q->flags = RPF_SHADOW;
    memset(&q->tex, 0, sizeof(q->tex));
    memset(&q->intertex, 0, sizeof(q->intertex));
    q->interpos = 0;
    q->lights = NULL;
    q->sector = NULL;
    memset(q->vertices, 0, q->numvertices * sizeof(rendpoly_vertex_t));

    vtx = q->vertices;
    idx = (isCeiling ? ceilIndices[wind] : floorIndices[wind]);

    // Left outer corner.
    vtx[idx[0]].pos[VX] = shadow->outer[0]->pos[VX];
    vtx[idx[0]].pos[VY] = shadow->outer[0]->pos[VY];
    vtx[idx[0]].pos[VZ] = z;
    vtx[idx[0]].color.rgba[CA] = (DGLubyte) (255 * darkness);   // Black.

    if(sideOpen[0] < 1)
        vtx[idx[0]].color.rgba[CA] *= 1 - sideOpen[0];

    // Right outer corner.
    vtx[idx[1]].pos[VX] = shadow->outer[1]->pos[VX];
    vtx[idx[1]].pos[VY] = shadow->outer[1]->pos[VY];
    vtx[idx[1]].pos[VZ] = z;
    vtx[idx[1]].color.rgba[CA] = (DGLubyte) (255 * darkness);

    if(sideOpen[1] < 1)
        vtx[idx[1]].color.rgba[CA] *= 1 - sideOpen[1];

    // Right inner corner.
    vtx[idx[2]].pos[VX] = inner[1][VX];
    vtx[idx[2]].pos[VY] = inner[1][VY];
    vtx[idx[2]].pos[VZ] = z;

    // Left inner corner.
    vtx[idx[3]].pos[VX] = inner[0][VX];
    vtx[idx[3]].pos[VY] = inner[0][VY];
    vtx[idx[3]].pos[VZ] = z;

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
        if(link->poly->visframe == (ushort) framecount)
            continue;

        // Now it will be rendered.
        shadow = link->poly;
        shadow->visframe = (ushort) framecount;

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
            plnHeight = SECT_PLANE_HEIGHT(shadowSec, pln);
            vec[VZ] = vy - plnHeight;

            // Glowing surfaces or missing textures shouldn't have shadows.
            if((suf->flags & SUF_NO_RADIO) ||
               !Rend_RadioNonGlowingFlat(subsector->sector, pln))
                continue;

            // Don't bother with planes facing away from the camera.
            if(M_DotProduct(vec, suf->normal) < 0)
                continue;

            line = shadow->line;
            if(line->L_backsector)
            {
                side = (shadow->flags & SHPF_FRONTSIDE) == 0;
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
                    front = line->sec[side];
                    back  = line->sec[side ^ 1];
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
                line = R_GetShadowNeighbor(shadow, i == 0, false);
                if(line && line->L_backsector)
                {
                    side = (line->L_frontsector != shadowSec);
                    front = line->sec[side];
                    back  = line->sec[side ^ 1];
                    setRelativeHeights(front, back, pln, &fz, &bz, &bhz);

                    hack = radioEdgeHackType(line, front, back, side, pln, fz, bz);
                    if(hack)
                        sideOpen[i] = hack - 1;
                    else
                        sideOpen[i] = radioEdgeOpenness(fz, bz, bhz);
                }
                else
                    sideOpen[i] = 0;
            }

            Rend_RadioAddShadowEdge(shadow, pln, 1 - open, sideOpen,
                                    plnHeight);
        }
    }

END_PROF( PROF_RADIO_SUBSECTOR );
}
