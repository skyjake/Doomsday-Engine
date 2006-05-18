/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * r_shadow.c: Runtime Map Shadowing (FakeRadio)
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_misc.h"

#include "p_bmap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct boundary_s {
    vec2_t  left, right;
    vec2_t  a, b;
} boundary_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Line1 and line2 are the (dx,dy)s for two lines, connected at the
 * origin (0,0).  Dist1 and dist2 are the distances from these lines.
 * The returned point (in 'point') is dist1 away from line1 and dist2
 * from line2, while also being the nearest point to the origin (in
 * case the lines are parallel).
 */
void R_CornerNormalPoint(const pvec2_t line1, float dist1, const pvec2_t line2,
                         float dist2, pvec2_t point, pvec2_t lp, pvec2_t rp)
{
    float   len1, len2, plen;
    vec2_t  origin = { 0, 0 }, norm1, norm2;

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
        V2_Copy(point, norm1);
        if(lp)
            V2_Copy(lp, norm1);
        if(rp)
            V2_Copy(rp, norm1);
        return;
    }

    // Find the intersection of normal-shifted lines.  That'll be our
    // corner point.
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

void R_ShadowDelta(pvec2_t delta, line_t *line, sector_t *frontSector)
{
    if(line->frontsector == frontSector)
    {
        delta[VX] = FIX2FLT(line->dx);
        delta[VY] = FIX2FLT(line->dy);
    }
    else
    {
        delta[VX] = -FIX2FLT(line->dx);
        delta[VY] = -FIX2FLT(line->dy);
    }
}

lineinfo_side_t *R_GetShadowLineSideInfo(shadowpoly_t *poly)
{
    return &LINE_INFO(poly->line)->side[poly->flags & SHPF_FRONTSIDE ? 0 : 1];
}

line_t *R_GetShadowNeighbor(shadowpoly_t *poly, boolean left, boolean back)
{
    lineinfo_side_t *side = R_GetShadowLineSideInfo(poly);
    line_t **neighbors = (back ? side->backneighbor : side->neighbor);

    return neighbors[left ? 0 : 1];
}

/*
 * Returns a pointer to the sector the shadow polygon belongs in.
 */
sector_t *R_GetShadowSector(shadowpoly_t *poly)
{
    return poly->flags & SHPF_FRONTSIDE ? poly->line->frontsector : poly->
        line->backsector;
}

/*
 * Returns a pointer to the sector in the left/right proximity.
 */
sector_t *R_GetShadowProximity(shadowpoly_t *poly, boolean left)
{
    lineinfo_side_t *side = R_GetShadowLineSideInfo(poly);

    return side->proxsector[left ? 0 : 1];
}

/*
 * May return false when dealing with backneighbors.
 */
boolean R_ShadowCornerDeltas(pvec2_t left, pvec2_t right, shadowpoly_t *poly,
                             boolean leftCorner, boolean back)
{
    sector_t *sector = R_GetShadowSector(poly);
    line_t *neighbor;

    // The line itself.
    R_ShadowDelta(leftCorner ? right : left, poly->line, sector);

    // The (back)neighbour.
    if(NULL == (neighbor = R_GetShadowNeighbor(poly, leftCorner, back)))
        return false;

    R_ShadowDelta(leftCorner ? left : right, neighbor,
                  !back ? sector : R_GetShadowProximity(poly, leftCorner));

    // The left side is always flipped.
    V2_Scale(left, -1);
    return true;
}

/*
 * Returns the width (world units) of the shadow edge.  It is scaled
 * depending on the length of the edge.
 */
float R_ShadowEdgeWidth(const pvec2_t edge)
{
    float   length = V2_Length(edge);
    float   normalWidth = 20;   //16;
    float   maxWidth = 60;
    float   w;

    // A short edge?
    if(length < normalWidth * 2)
        return length / 2;

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

/*
 * Sets the shadow edge offsets.  If the associated line does not have
 * neighbors, it can't have a shadow.
 */
void R_ShadowEdges(shadowpoly_t *poly)
{
    vec2_t  left, right;
    int     side;

    for(side = 0; side < 2; side++) // left and right
    {
        // The inside corner.
        R_ShadowCornerDeltas(left, right, poly, side == 0, false);
        R_CornerNormalPoint(left, R_ShadowEdgeWidth(left), right,
                            R_ShadowEdgeWidth(right), poly->inoffset[side],
                            side == 0 ? poly->extoffset[side] : NULL,
                            side == 1 ? poly->extoffset[side] : NULL);

        // It is not always possible to calculate the back-extended
        // offset.
        if(R_ShadowCornerDeltas(left, right, poly, side == 0, true))
        {
            R_CornerNormalPoint(left, R_ShadowEdgeWidth(left), right,
                                R_ShadowEdgeWidth(right),
                                poly->bextoffset[side], NULL, NULL);
        }
        else
        {
            // No back-extended, just use the plain extended offset.
            V2_Copy(poly->bextoffset[side], poly->extoffset[side]);
        }
    }
    /*
       // Right corner.
       R_ShadowCornerDeltas(left, right, poly, false, false);
       R_CornerNormalPoint(left, 16, right, 16, poly->inoffset[1],
       NULL, poly->extoffset[1]);

       R_ShadowCornerDeltas(left, right, poly, false, true);
       R_CornerNormalPoint(left, 16, right, 16, poly->bextoffset[1], 0, 0);
     */
}

/*
 * Link a shadowpoly to a subsector.
 */
void R_LinkShadow(shadowpoly_t *poly, subsector_t *subsector)
{
    subsectorinfo_t *info = SUBSECT_INFO(subsector);
    shadowlink_t *link;

#ifdef _DEBUG
    // Check the links for dupes!
    {
        shadowlink_t *i;

        for(i = info->shadows; i; i = i->next)
            if(i->poly == poly)
                Con_Error("R_LinkShadow: Already here!!\n");
    }
#endif

    // We'll need to allocate a new link.
    link = Z_Malloc(sizeof(*link), PU_LEVEL, NULL);

    // The links are stored into a linked list.
    link->next = info->shadows;
    info->shadows = link;
    link->poly = poly;
}

/*
 * If the shadow polygon (parm) contacts the subsector, link the poly
 * to the subsector's shadow list.
 */
boolean RIT_ShadowSubsectorLinker(subsector_t *subsector, void *parm)
{
    shadowpoly_t *poly = parm;
    vec2_t  corners[4], a, b, mid;
    int     i, j;

    R_LinkShadow(poly, subsector);
    return true;

    // Use the extended points, they are wider than inoffsets.
    V2_Set(corners[0], FIX2FLT(poly->outer[0]->x), FIX2FLT(poly->outer[0]->y));
    V2_Set(corners[1], FIX2FLT(poly->outer[1]->x), FIX2FLT(poly->outer[1]->y));
    V2_Sum(corners[2], corners[1], poly->extoffset[1]);
    V2_Sum(corners[3], corners[0], poly->extoffset[0]);

    V2_Sum(mid, corners[0], corners[2]);
    V2_Scale(mid, .5f);

    for(i = 0; i < 4; i++)
    {
        V2_Subtract(corners[i], corners[i], mid);
        V2_Scale(corners[i], .995f);
        V2_Sum(corners[i], corners[i], mid);
    }

    // Any of the corner points in the subsector?
    for(i = 0; i < 4; i++)
    {
        if(R_PointInSubsector
           (FRACUNIT * corners[i][VX], FRACUNIT * corners[i][VY]) == subsector)
        {
            // Good enough.
            R_LinkShadow(poly, subsector);
            return true;
        }
    }

    // Do a more elaborate line intersection test.  It's possible that
    // the shadow's corners are outside a subsector, but the shadow
    // still contacts the subsector.

    for(j = 0; j < subsector->numverts; j++)
    {
        V2_Set(a, subsector->verts[j].x, subsector->verts[j].y);
        V2_Set(b, subsector->verts[(j + 1) % subsector->numverts].x,
               subsector->verts[(j + 1) % subsector->numverts].y);

        for(i = 0; i < 4; i++)
        {
            /*          k = V2_Intercept(a, b, corners[i], corners[(i + 1) % 4], NULL);
               m = V2_Intercept(corners[i], corners[(i + 1) % 4], a, b, NULL);

               // Is the intersection between points 'a' and 'b'.
               if(k >= 0 && k <= 1 && m >= 0 && m <= 1) */

            if(V2_Intercept2
               (a, b, corners[i], corners[(i + 1) % 4], NULL, NULL, NULL))
            {
                // There is contact!
                R_LinkShadow(poly, subsector);
                return true;
            }
        }
    }

    // Continue with the iteration, maybe some other subsectors will
    // contact it.
    return true;
}

/*
 * Moves inoffset appropriately.  Returns true if overlap resolving
 * should continue to another round of iteration.
 */
boolean R_ResolveStep(const pvec2_t outer, const pvec2_t inner, pvec2_t offset)
{
#define RESOLVE_STEP 2
    float   span, distance;
    boolean iterCont = true;

    span = distance = V2_Distance(outer, inner);
    if(span == 0)
        return false;

    //  if(distance <= RESOLVE_STEP * 2)
    //{
    distance /= 2;
    iterCont = false;
    /*  }
       else
       {
       distance -= RESOLVE_STEP;
       } */

    V2_Scale(offset, distance / span);
    return iterCont;
}

/*
 * The array of polys given as the parameter contains the shadow
 * polygons of one sector.  If the polygons overlap, we will
 * iteratively resolve the overlaps by moving the inner corner points
 * closer to the outer corner points.  Other corner points remain
 * unmodified.
 */
void R_ResolveOverlaps(shadowpoly_t *polys, int count, sector_t *sector)
{
#define OVERLAP_LEFT    0x01
#define OVERLAP_RIGHT   0x02
#define OVERLAP_ALL     (OVERLAP_LEFT | OVERLAP_RIGHT)
#define EPSILON         .01f
    boolean done = false;
    int     i, k, tries;
    boundary_t *boundaries, *bound; //, *other;
    byte   *overlaps;
    float   s, t;
    vec2_t  a, b;
    line_t *line;

    // Were any polygons provided?
    if(!count)
        return;

    boundaries = malloc(sizeof(*boundaries) * count);
    overlaps = malloc(count);

    // We don't want to stay here forever.
    for(tries = 0; tries < 100 && !done; tries++)
    {
        // We will set this to false if we notice that overlaps still
        // exist.
        done = true;

        // Calculate the boundaries.
        for(i = 0, bound = boundaries; i < count; i++, bound++)
        {
            V2_SetFixed(bound->left, polys[i].outer[0]->x,
                        polys[i].outer[0]->y);
            V2_Sum(bound->a, polys[i].inoffset[0], bound->left);

            V2_SetFixed(bound->right, polys[i].outer[1]->x,
                        polys[i].outer[1]->y);
            V2_Sum(bound->b, polys[i].inoffset[1], bound->right);
        }
        memset(overlaps, 0, count);

        // Find the overlaps.
        for(i = 0, bound = boundaries; i < count; i++, bound++)
        {
            /*for(k = 0, other = boundaries; k < count; k++, other++) */

            for(k = 0; k < sector->linecount; k++)
            {
                //if(i == k) continue;
                line = sector->Lines[k];
                if(polys[i].line == line)
                    continue;

                if(LINE_INFO(line)->selfrefhackroot)
                    continue;

                if((overlaps[i] & OVERLAP_ALL) == OVERLAP_ALL)
                    break;

                V2_SetFixed(a, line->v1->x, line->v1->y);
                V2_SetFixed(b, line->v2->x, line->v2->y);

                // Try the left edge of the shadow.
                V2_Intercept2(bound->left, bound->a, a /*other->left */ ,
                              b /*other->right */ , NULL, &s, &t);
                if(s > 0 && s < 1 && t >= EPSILON && t <= 1 - EPSILON)
                    overlaps[i] |= OVERLAP_LEFT;

                // Try the right edge of the shadow.
                V2_Intercept2(bound->right, bound->b, a /*other->left */ ,
                              b /*other->right */ , NULL, &s, &t);
                if(s > 0 && s < 1 && t >= EPSILON && t <= 1 - EPSILON)
                    overlaps[i] |= OVERLAP_RIGHT;
            }
        }

        // Adjust the overlapping inner points.
        for(i = 0, bound = boundaries; i < count; i++, bound++)
        {
            if(overlaps[i] & OVERLAP_LEFT)
            {
                if(R_ResolveStep(bound->left, bound->a, polys[i].inoffset[0]))
                    done = false;
            }
            if(overlaps[i] & OVERLAP_RIGHT)
            {
                if(R_ResolveStep(bound->right, bound->b, polys[i].inoffset[1]))
                    done = false;
            }
        }
    }

    free(boundaries);
    free(overlaps);
}

/*
 * New shadowpolys will be allocated from the storage.  If it is NULL,
 * the number of polys required is returned.
 */
int R_MakeShadowEdges(shadowpoly_t *storage)
{
    int     i, j, counter;
    sector_t *sector;
    line_t *line;
    lineinfo_side_t *info;
    boolean frontside;
    shadowpoly_t *poly, *sectorFirst, *allocator = storage;

    for(i = 0, counter = 0; i < numsectors; i++)
    {
        sector = SECTOR_PTR(i);
        sectorFirst = allocator;

        // Iterate all the lines of the sector.
        for(j = 0; j < sector->linecount; j++)
        {
            line = sector->Lines[j];
            frontside = (line->frontsector == sector);
            info = &LINE_INFO(line)->side[frontside ? 0 : 1];

            // If the line hasn't got two neighbors, it won't get a
            // shadow.
            if(!info->neighbor[0] || !info->neighbor[1])
                continue;

            // This side will get a shadow.  Increment counter (we'll
            // return this count).
            counter++;

            if(!allocator)
                continue;

            // Get a new shadow poly.
            poly = allocator++;

            poly->line = line;
            poly->flags = (frontside ? SHPF_FRONTSIDE : 0);
            poly->visframe = framecount - 1;

            // The outer vertices are just the beginning and end of
            // the line.
            R_OrderVertices(line, sector, poly->outer);

            R_ShadowEdges(poly);
        }

        if(allocator)
        {
            // If shadows were created, make sure they don't overlap
            // each other.
            R_ResolveOverlaps(sectorFirst, allocator - sectorFirst, sector);
        }
    }
    return counter;
}

/*
 * Calculate sector edge shadow points, create the shadow polygons and
 * link them to the subsectors.
 */
void R_InitSectorShadows(void)
{
    int     i, maxCount;
    shadowpoly_t *shadows, *poly;
    vec2_t  bounds[2], point;

    // Find out the number of shadowpolys we'll require.
    maxCount = R_MakeShadowEdges(NULL);

    // Allocate just enough memory.
    shadows = Z_Calloc(sizeof(shadowpoly_t) * maxCount, PU_LEVEL, NULL);
    VERBOSE(Con_Printf("R_InitSectorShadows: %i shadowpolys.\n", maxCount));

    // This'll make 'em for real.
    R_MakeShadowEdges(shadows);

    /*
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
    for(i = 0, poly = shadows; i < maxCount; i++, poly++)
    {
        V2_Set(point, FIX2FLT(poly->outer[0]->x), FIX2FLT(poly->outer[0]->y));
        V2_InitBox(bounds, point);

        // Use the extended points, they are wider than inoffsets.
        V2_Sum(point, point, poly->extoffset[0]);
        V2_AddToBox(bounds, point);

        V2_Set(point, FIX2FLT(poly->outer[1]->x), FIX2FLT(poly->outer[1]->y));
        V2_AddToBox(bounds, point);

        V2_Sum(point, point, poly->extoffset[1]);
        V2_AddToBox(bounds, point);

        P_SubsectorBoxIteratorv(bounds, R_GetShadowSector(poly),
                                RIT_ShadowSubsectorLinker, poly);
    }
}
