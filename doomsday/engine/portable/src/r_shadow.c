/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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

/**
 * r_shadow.c: Runtime Map Shadowing (FakeRadio)
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_misc.h"
#include "de_play.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct boundary_s {
    vec2_t      left, right;
    vec2_t      a, b;
} boundary_t;

typedef struct segoverlapdata_s {
    struct segoverlapdata_s *next;
    boundary_t  bound;
    byte        overlap;
    seg_t      *seg;
} segoverlapdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static zblockset_t *shadowLinksBlockSet;

static segoverlapdata_t *unusedList = NULL;

// CODE --------------------------------------------------------------------

static __inline segoverlapdata_t* allocSegOverlapData(void)
{
    return M_Malloc(sizeof(segoverlapdata_t));
}

static __inline void freeSegOverlapData(segoverlapdata_t* data)
{
    M_Free(data);
}

static segoverlapdata_t *createOverlapDataForSeg(seg_t *seg)
{
    segoverlapdata_t   *data;

    // Have we got a reusable overlapdata?
    if(unusedList)
    {
        data = unusedList;
        unusedList = unusedList->next;
    }
    else
    {
        data = allocSegOverlapData();
    }

    data->seg = seg;
    data->overlap = 0;
    data->next = NULL;

    return data;
}

static void destroyOverlapData(segoverlapdata_t *data)
{
    if(data)
        freeSegOverlapData(data);
}

static void linkOverlapDataToList(segoverlapdata_t **list,
                                  segoverlapdata_t *data)
{
    data->next = (*list);
    (*list) = data;
}

static void destroyOverlapList(segoverlapdata_t **list, boolean reuse)
{
    segoverlapdata_t *data, *next;

    data = (*list);
    while(data)
    {
        next = data->next;
        if(reuse)
            linkOverlapDataToList(&unusedList, data);
        else
            destroyOverlapData(data);
        data = next;
    }
    (*list) = NULL;
}

/**
 * Line1 and line2 are the (dx,dy)s for two lines, connected at the
 * origin (0,0).  Dist1 and dist2 are the distances from these lines.
 * The returned point (in 'point') is dist1 away from line1 and dist2
 * from line2, while also being the nearest point to the origin (in
 * case the lines are parallel).
 */
void R_CornerNormalPoint(const pvec2_t line1, float dist1, const pvec2_t line2,
                         float dist2, pvec2_t point, pvec2_t lp, pvec2_t rp)
{
    float       len1, len2, plen;
    vec2_t      origin = { 0, 0 }, norm1, norm2;

    // Length of both lines.
    len1 = V2_Length(line1);
    len2 = V2_Length(line2);

    // Calculate normals for both lines.
    V2_Set(norm1, -line1[VY] / len1 * dist1, line1[VX] / len1 * dist1);
    V2_Set(norm2, line2[VY] / len2 * dist2, -line2[VX] / len2 * dist2);

    // Are the lines parallel?  If so, they won't connect at any
    // point, and it will be impossible to determine a corner point.
    if(V2_IsParallel(line1, line2))
    {
        // Just use a normal as the point.
        if(point)
            V2_Copy(point, norm1);
        if(lp)
            V2_Copy(lp, norm1);
        if(rp)
            V2_Copy(rp, norm1);
        return;
    }

    // Find the intersection of normal-shifted lines.  That'll be our
    // corner point.
    if(point)
        V2_Intersection(norm1, line1, norm2, line2, point);

    // Do we need to calculate the extended points, too?  Check that
    // the extensions don't bleed too badly outside the legal shadow
    // area.
    if(lp)
    {
        V2_Intersection(origin, line1, norm2, line2, lp);
        plen = V2_Length(lp);
        if(plen > 0 && plen > len1)
        {
            V2_Scale(lp, len1 / plen);
        }
    }
    if(rp)
    {
        V2_Intersection(norm1, line1, origin, line2, rp);
        plen = V2_Length(rp);
        if(plen > 0 && plen > len2)
        {
            V2_Scale(rp, len2 / plen);
        }
    }
}

void R_ShadowDelta(pvec2_t delta, linedef_t *line, sector_t *frontSector)
{
    if(line->L_frontsector == frontSector)
    {
        delta[VX] = line->dX;
        delta[VY] = line->dY;
    }
    else
    {
        delta[VX] = -line->dX;
        delta[VY] = -line->dY;
    }
}

/**
 * Returns a pointer to the sector the shadow polygon belongs in.
 */
sector_t *R_GetShadowSector(seg_t *seg, uint pln, boolean getLinked)
{
    if(getLinked)
        return R_GetLinkedSector(seg->subsector, pln);
    return (seg->SG_frontsector);
}

boolean R_ShadowCornerDeltas(pvec2_t left, pvec2_t right, seg_t *seg,
                             boolean leftCorner)
{
    sector_t       *sector = R_GetShadowSector(seg, 0, false);
    linedef_t      *neighbor;
    int             side = seg->side;

    // The line itself.
    R_ShadowDelta(leftCorner ? right : left, seg->lineDef, sector);

    // The neighbor.
    neighbor = R_FindLineNeighbor(seg->subsector->sector,
                                  seg->lineDef,
                                  seg->lineDef->vo[side^!leftCorner],
                                  !leftCorner, NULL);
    if(!neighbor)
    {
        // Should never happen...
        Con_Message("R_ShadowCornerDeltas: No %s neighbor for line %u!\n",
                    leftCorner? "left":"right",
                    (uint) GET_LINE_IDX(seg->lineDef));
        return false;
    }

    R_ShadowDelta(leftCorner ? left : right, neighbor, sector);

    // The left side is always flipped.
    V2_Scale(left, -1);
    return true;
}

/**
 * @return          The width (world units) of the shadow edge.
 *                  It is scaled depending on the length of the edge.
 */
float R_ShadowEdgeWidth(const pvec2_t edge)
{
    float       length = V2_Length(edge);
    float       normalWidth = 20;   //16;
    float       maxWidth = 60;
    float       w;

    // A long edge?
    if(length > 600)
    {
        w = length - 600;
        if(w > 1000)
            w = 1000;
        return normalWidth + w / 1000 * maxWidth;
    }

    return normalWidth;
}

/**
 * Sets the shadow edge offsets. If the associated line does not have
 * neighbors, it can't have a shadow.
 */
void R_ShadowEdges(seg_t *seg)
{
    vec2_t          left, right;
    int             edge, side = seg->side;
    uint            i, count;
    linedef_t      *line = seg->lineDef;
    lineowner_t    *base, *p, *boundryOwner = NULL;
    boolean         done;
    lineowner_t    *vo;

    for(edge = 0; edge < 2; ++edge) // left and right
    {
        vo = seg->lineDef->L_vo(edge^side);

        // The inside corner:
        if(R_ShadowCornerDeltas(left, right, seg, edge == 0))
        {
            R_CornerNormalPoint(left, R_ShadowEdgeWidth(left), right,
                                R_ShadowEdgeWidth(right), vo->shadowOffsets.inner,
                                edge == 0 ? vo->shadowOffsets.extended : NULL,
                                edge == 1 ? vo->shadowOffsets.extended : NULL);
        }
        else
        {   // An error in the map. Set the inside corner to the extoffset.
            V2_Copy(vo->shadowOffsets.inner,
                    vo->shadowOffsets.extended);
        }

        // The back extended offset(s):
        // Determine how many we'll need.
        base = R_GetVtxLineOwner(line->L_v(edge^side), line);
        count = 0;
        done = false;
        p = base->link[!edge];
        while(p != base && !done)
        {
            if(!(p->lineDef->L_frontside && p->lineDef->L_backside &&
                 p->lineDef->L_frontsector == p->lineDef->L_backsector))
            {
                if(count == 0)
                {   // Found the boundry line.
                    boundryOwner = p;
                }

                if(!p->lineDef->L_frontside || !p->lineDef->L_backside)
                    done = true; // Found the last one.
                else
                    count++;
            }

            if(!done)
                p = p->link[!edge];
        }

        // It is not always possible to calculate the back-extended
        // offset.
        if(count == 0)
        {
            // No back-extended, just use the plain extended offset.
            V2_Copy(vo->shadowOffsets.backExtended[0].offset,
                    vo->shadowOffsets.extended);
        }
        else
        {
            // We need at least one back extended offset.
            sector_t       *sector = R_GetShadowSector(seg, 0, false);
            linedef_t      *neighbor;
            boolean         leftCorner = (edge == 0);
            pvec2_t         delta;
            sector_t       *orientSec;

            // The line itself.
            R_ShadowDelta(leftCorner ? right : left, line, sector);

            // The left side is always flipped.
            if(!leftCorner)
                V2_Scale(left, -1);

            if(boundryOwner->lineDef->L_frontsector == sector)
                orientSec = (boundryOwner->lineDef->L_backside? boundryOwner->lineDef->L_backsector : NULL);
            else
                orientSec = (boundryOwner->lineDef->L_frontside? boundryOwner->lineDef->L_frontsector : NULL);

            p = boundryOwner;
            for(i = 0; i < count && i < MAX_BEXOFFSETS; ++i)
            {
                byte        otherside;
                // Get the next back neighbor.
                neighbor = NULL;
                p = p->link[!edge];
                do
                {
                    if(!(p->lineDef->L_frontside && p->lineDef->L_backside &&
                         p->lineDef->L_frontsector == p->lineDef->L_backsector))
                        neighbor = p->lineDef;
                    else
                        p = p->link[!edge];
                } while(!neighbor);

                // The back neighbor delta.
                delta = (leftCorner ? left : right);
                otherside = neighbor->L_v1 == line->L_v(edge^!side);
                if(neighbor->L_side(otherside) &&
                   orientSec == neighbor->L_sector(otherside))
                {
                    delta[VX] = neighbor->dX;
                    delta[VY] = neighbor->dY;
                }
                else
                {
                    delta[VX] = -neighbor->dX;
                    delta[VY] = -neighbor->dY;
                }

                // The left side is always flipped.
                if(leftCorner)
                    V2_Scale(left, -1);

                R_CornerNormalPoint(left, R_ShadowEdgeWidth(left),
                                    right, R_ShadowEdgeWidth(right),
                                    vo->shadowOffsets.backExtended[i].offset,
                                    NULL, NULL);

                // Update orientSec ready for the next iteration?
                if(i < count - 1)
                {
                    if(neighbor->L_frontsector == orientSec)
                        orientSec = (neighbor->L_backside? neighbor->L_backsector : NULL);
                    else
                        orientSec = (neighbor->L_frontside? neighbor->L_frontsector : NULL);
                }
            }
        }
    }
}

/**
 * Link a seg to an arbitary subsector for the purposes of shadowing.
 */
void R_LinkShadow(seg_t *seg, subsector_t *subsector)
{
    shadowlink_t       *link;

#ifdef _DEBUG
// Check the links for dupes!
{
shadowlink_t *i;

for(i = subsector->shadows; i; i = i->next)
    if(i->seg == seg)
        Con_Error("R_LinkShadow: Already here!!\n");
}
#endif

    // We'll need to allocate a new link.
    link = Z_BlockNewElement(shadowLinksBlockSet);

    // The links are stored into a linked list.
    link->next = subsector->shadows;
    subsector->shadows = link;
    link->seg = seg;
}

/**
 * If the shadow polygon (parm) contacts the subsector, link the poly
 * to the subsector's shadow list.
 */
boolean RIT_ShadowSubsectorLinker(subsector_t *subsector, void *parm)
{
    seg_t              *seg = (seg_t*) parm;

    R_LinkShadow(seg, subsector);
    return true;
}

/**
 * Moves inoffset appropriately.
 *
 * @return          @c true, if overlap resolving should continue to another
 *                  round of iteration.
 */
boolean R_ResolveStep(const pvec2_t outer, const pvec2_t inner,
                      pvec2_t offset)
{
#define RESOLVE_STEP 2

    float               span, distance;
    boolean             iterCont = true;

    span = distance = V2_Distance(outer, inner);
    if(span == 0)
        return false;

    distance /= 2;
    iterCont = false;

    V2_Scale(offset, distance / span);
    return iterCont;

#undef RESOLVE_STEP
}

/**
 * The array of polys given as the parameter contains the shadow polygons
 * of one sector. If the polygons overlap, we will iteratively resolve the
 * overlaps by moving the inner corner points closer to the outer corner
 * points. Other corner points remain unmodified.
 */
void R_ResolveOverlaps(segoverlapdata_t **list, sector_t *sector)
{
#define OVERLAP_LEFT    0x01
#define OVERLAP_RIGHT   0x02
#define OVERLAP_ALL     (OVERLAP_LEFT | OVERLAP_RIGHT)
#define EPSILON         .01f

    boolean             done;
    int                 tries;
    uint                i;
    float               s, t;
    vec2_t              a, b;
    linedef_t          *line;
    segoverlapdata_t   *data;

    if(!list)
        return;

    // We don't want to stay here forever.
    done = false;
    for(tries = 0; tries < 100 && !done; ++tries)
    {
        // We will set this to false if we notice that overlaps still
        // exist.
        done = true;

        // Calculate the boundaries.
        for(data = (*list); data; data = data->next)
        {
            seg_t              *seg = data->seg;
            boundary_t         *bound = &data->bound;
            byte                side = seg->side;
            vertex_t           *vtx0, *vtx1;

            vtx0 = seg->lineDef->L_v(side);
            vtx1 = seg->lineDef->L_v(side^1);

            V2_Set(bound->left, vtx0->V_pos[VX], vtx0->V_pos[VY]);
            V2_Sum(bound->a, seg->lineDef->L_vo(side)->shadowOffsets.inner,
                   bound->left);

            V2_Set(bound->right, vtx1->V_pos[VX], vtx1->V_pos[VY]);
            V2_Sum(bound->b, seg->lineDef->L_vo(side^1)->shadowOffsets.inner,
                   bound->right);

            data->overlap = 0;
        }

        // Find the overlaps.
        for(data = (*list); data; data = data->next)
        {
            seg_t              *seg = data->seg;
            boundary_t         *bound = &data->bound;

            for(i = 0; i < sector->lineDefCount; ++i)
            {
                line = sector->lineDefs[i];
                if(seg->lineDef == line)
                    continue;

                if(LINE_SELFREF(line))
                    continue;

                if((data->overlap & OVERLAP_ALL) == OVERLAP_ALL)
                    break;

                V2_Set(a, line->L_v1pos[VX], line->L_v1pos[VY]);
                V2_Set(b, line->L_v2pos[VX], line->L_v2pos[VY]);

                // Try the left edge of the shadow.
                V2_Intercept2(bound->left, bound->a, a, b, NULL, &s, &t);
                if(s > 0 && s < 1 && t >= EPSILON && t <= 1 - EPSILON)
                    data->overlap |= OVERLAP_LEFT;

                // Try the right edge of the shadow.
                V2_Intercept2(bound->right, bound->b, a, b, NULL, &s, &t);
                if(s > 0 && s < 1 && t >= EPSILON && t <= 1 - EPSILON)
                    data->overlap |= OVERLAP_RIGHT;
            }
        }

        // Adjust the overlapping inner points.
        for(data = (*list); data; data = data->next)
        {
            seg_t              *seg = data->seg;
            boundary_t         *bound = &data->bound;
            byte                side = seg->side;

            if(data->overlap & OVERLAP_LEFT)
            {
                if(R_ResolveStep(bound->left, bound->a,
                                 seg->lineDef->L_vo(side)->shadowOffsets.inner))
                    done = false;
            }

            if(data->overlap & OVERLAP_RIGHT)
            {
                if(R_ResolveStep(bound->right, bound->b,
                                 seg->lineDef->L_vo(side^1)->shadowOffsets.inner))
                    done = false;
            }
        }
    }

#undef OVERLAP_LEFT
#undef OVERLAP_RIGHT
#undef OVERLAP_ALL
#undef EPSILON
}

/**
 * Find all segs that will cast fakeradio edge shadows.
 */
uint R_FindShadowEdges(void)
{
    uint                i, counter = 0;
    segoverlapdata_t   *list = NULL;

    unusedList = NULL;

    for(i = 0; i < numSectors; ++i)
    {
        sector_t           *sec = SECTOR_PTR(i);
        subsector_t      **ssecp;
        uint                numShadowSegsInSector = 0;

        // Use validCount to make sure we only allocate one shadowpoly
        // per line, side.
        ++validCount;

        // Iterate all the subsectors of the sector.
        ssecp = sec->ssectors;
        while(*ssecp)
        {
            subsector_t *ssec = *ssecp;
            seg_t     **segp;

            // Iterate all the segs of the subsector.
            segp = ssec->segs;
            while(*segp)
            {
                seg_t      *seg = *segp;

                // Minisegs don't get shadows, even then, only one.
                if(seg->lineDef && seg->side == 0 &&
                   !((seg->lineDef->validCount == validCount) ||
                     LINE_SELFREF(seg->lineDef)))
                {
                    linedef_t     *line = seg->lineDef;
                    boolean     frontside = (line->L_frontsector == sec);

                    // If the line hasn't got two neighbors, it won't get a
                    // shadow.
                    if(!(line->vo[0]->LO_next->lineDef == line ||
                         line->vo[1]->LO_next->lineDef == line))
                    {
                        segoverlapdata_t   *data;

                        // This seg will get a shadow. Increment counter (we'll
                        // return this count).
                        seg->flags |= SEGF_SHADOW;
                        numShadowSegsInSector++;
                        line->validCount = validCount;

                        // The outer vertices are just the beginning and end of
                        // the line.
                        R_ShadowEdges(seg);
                        data = createOverlapDataForSeg(seg);
                        linkOverlapDataToList(&list, data);
                    }
                }

                *segp++;
            }

            *ssecp++;
        }

        if(numShadowSegsInSector > 0)
        {
            // Make sure they don't overlap each other.
            R_ResolveOverlaps(&list, sec);

            destroyOverlapList(&list, true);
            counter += numShadowSegsInSector;
        }
    }

    destroyOverlapList(&unusedList, false);

    VERBOSE(Con_Printf("R_FindShadowEdges: Shadowing segs #%i .\n", counter));

    return counter;
}

/**
 * Calculate sector edge shadow points, create the shadow polygons and link
 * them to the subsectors.
 */
void R_InitSectorShadows(void)
{
    uint            startTime = Sys_GetRealTime();

    uint            i;
    vec2_t          bounds[2], point;
    byte            side;
    vertex_t       *vtx0, *vtx1;

    // Find all segs that will cast fakeradio edge shadows.
    R_FindShadowEdges();

    /**
     * The algorithm:
     *
     * 1. Use the subsector blockmap to look for all the blocks that are
     *    within the shadow's bounding box.
     *
     * 2. Check the subsectors whose sector == shadow's sector.
     *
     * 3. If one of the shadow's points is in the subsector, or the
     *    shadow's edges cross one of the subsector's edges (not parallel),
     *    link the shadow to the subsector.
     */
    shadowLinksBlockSet = Z_BlockCreate(sizeof(shadowlink_t), 1024, PU_LEVEL);

    for(i = 0; i < numSegs; ++i)
    {
        seg_t          *seg = &segs[i];

        if(seg->flags & SEGF_SHADOW)
        {
            side = seg->side;
            vtx0 = seg->lineDef->L_v(0^side);
            vtx1 = seg->lineDef->L_v(1^side);

            V2_Set(point, vtx0->V_pos[VX], vtx0->V_pos[VY]);
            V2_InitBox(bounds, point);

            // Use the extended points, they are wider than inoffsets.
            V2_Sum(point, point, seg->lineDef->L_vo(side)->shadowOffsets.extended);
            V2_AddToBox(bounds, point);

            V2_Set(point, vtx1->V_pos[VX], vtx1->V_pos[VY]);
            V2_AddToBox(bounds, point);

            V2_Sum(point, point, seg->lineDef->L_vo(side^1)->shadowOffsets.extended);
            V2_AddToBox(bounds, point);

            P_SubsectorsBoxIteratorv(bounds, R_GetShadowSector(seg, 0, false),
                                     RIT_ShadowSubsectorLinker, seg);
        }
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitSectorShadows: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}
