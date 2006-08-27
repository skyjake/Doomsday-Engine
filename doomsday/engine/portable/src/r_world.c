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
 * r_world.c: World Setup and Refresh
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <assert.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define DOMINANT_SIZE 1000

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    R_PrepareSubsector(subsector_t *sub);
void    R_FindLineNeighbors(sector_t *sector, line_t *line,
                            struct line_s **neighbors, int alignment);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     rendSkyLight = 1;       // cvar

char    currentLevelId[64];

boolean firstFrameAfterLoad;
boolean levelSetup;

sectorinfo_t    *secinfo;
seginfo_t       *seginfo;
subsectorinfo_t *subsecinfo;
lineinfo_t      *lineinfo;
vertexowner_t   *vertexowners;
nodeindex_t     *linelinks;         // indices to roots

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean noSkyColorGiven;
static byte skyColorRGB[4], balancedRGB[4];
static float skyColorBalance;
static float mapBounds[4];

// CODE --------------------------------------------------------------------

#ifdef _MSC_VER
#  pragma optimize("g", off)
#endif

/*
 * We mustn't create links which form loops. This will start looking
 * from destlink, and if it finds startsec we're in trouble.
 */
boolean R_IsValidLink(sector_t *startsec, sector_t *destlink, int plane)
{
    sector_t *sec = destlink;
    sector_t *link;
    sectorinfo_t *sin;

    for(;;)
    {
        sin = SECT_INFO(sec);
        // Advance to the linked sector.
        if(!sin->planeinfo[plane].linked)
            break;
        link = sin->planeinfo[plane].linked;

        // Is there an illegal linkage?
        if(sec == link || startsec == link)
            return false;
        sec = link;
    }
    // No problems encountered.
    return true;
}

/*
 * Called whenever the sector changes.
 *
 * This routine handles plane hacks where all of the sector's
 * lines are twosided and missing upper or lower textures.
 *
 * NOTE: This does not support sectors with disjoint groups of
 *       lines (eg a sector with a "control" sector such as the
 *       forcefields in ETERNAL.WAD MAP01).
 */
void R_SetSectorLinks(sector_t *sec)
{
    int     i = GET_SECTOR_IDX(sec), k;
    sector_t *back;
    boolean hackfloor, hackceil;
    side_t *sid, *frontsid, *backsid;
    sector_t *floorlink_candidate = 0, *ceillink_candidate = 0;
    //return; //---DEBUG---

    // Must have a valid sector!
    if(!sec || !sec->linecount || secinfo[i].permanentlink)
        return;                 // Can't touch permanent links.

    hackfloor = (!R_IsSkySurface(&sec->SP_floorsurface));
    hackceil = (!R_IsSkySurface(&sec->SP_ceilsurface));
    for(k = 0; k < sec->linecount; k++)
    {
        if(!hackfloor && !hackceil)
            break;
        // We are only interested in two-sided lines.
        if(!(sec->Lines[k]->frontsector && sec->Lines[k]->backsector))
            continue;

        // Check the vertex line owners for both verts.
        // We are only interested in lines that do NOT share either vertex
        // with a one-sided line (ie, its not "anchored").
        if(vertexowners[GET_VERTEX_IDX(sec->Lines[k]->v1)].anchored ||
           vertexowners[GET_VERTEX_IDX(sec->Lines[k]->v2)].anchored)
            return;

        // Check which way the line is facing.
        sid = SIDE_PTR(sec->Lines[k]->sidenum[0]);
        if(sid->sector == sec)
        {
            frontsid = sid;
            backsid = SIDE_PTR(sec->Lines[k]->sidenum[1]);
        }
        else
        {
            frontsid = SIDE_PTR(sec->Lines[k]->sidenum[1]);
            backsid = sid;
        }
        back = backsid->sector;
        if(back == sec)
            return;

        // Check that there is something on the other side.
        if(back->planes[PLN_CEILING].height == back->planes[PLN_FLOOR].height)
            return;
        // Check the conditions that prevent the invis plane.
        if(back->planes[PLN_FLOOR].height == sec->planes[PLN_FLOOR].height)
        {
            hackfloor = false;
        }
        else
        {
            if(back->planes[PLN_FLOOR].height > sec->planes[PLN_FLOOR].height)
                sid = frontsid;
            else
                sid = backsid;

            if(sid->bottom.texture || sid->middle.texture)
                hackfloor = false;
            else if(R_IsValidLink(sec, back, PLN_FLOOR))
                floorlink_candidate = back;
        }

        if(back->planes[PLN_CEILING].height == sec->planes[PLN_CEILING].height)
            hackceil = false;
        else
        {
            if(back->planes[PLN_CEILING].height < sec->planes[PLN_CEILING].height)
                sid = frontsid;
            else
                sid = backsid;
            if(sid->top.texture || sid->middle.texture)
                hackceil = false;
            else if(R_IsValidLink(sec, back, PLN_CEILING))
                ceillink_candidate = back;
        }
    }
    if(hackfloor)
    {
        if(floorlink_candidate == SECT_INFO(sec)->containsector)
            secinfo[i].planeinfo[PLN_FLOOR].linked = floorlink_candidate;

        /*      if(floorlink_candidate)
           Con_Printf("LF:%i->%i\n",
           i, GET_SECTOR_IDX(floorlink_candidate)); */
    }
    if(hackceil)
    {
        if(ceillink_candidate == SECT_INFO(sec)->containsector)
            secinfo[i].planeinfo[PLN_CEILING].linked = ceillink_candidate;

        /*      if(ceillink_candidate)
           Con_Printf("LC:%i->%i\n",
           i, GET_SECTOR_IDX(ceillink_candidate)); */
    }
}

#ifdef _MSC_VER
#  pragma optimize("", on)
#endif

/*
 * Returns a pointer to the list of points. It must be used.
 */
fvertex_t *edgeClipper(int *numpoints, fvertex_t * points, int numclippers,
                       fdivline_t * clippers)
{
    unsigned char sidelist[MAX_POLY_SIDES];
    int     i, k, num = *numpoints;

    // We'll clip the polygon with each of the divlines. The left side of
    // each divline is discarded.
    for(i = 0; i < numclippers; i++)
    {
        fdivline_t *curclip = clippers + i;

        // First we'll determine the side of each vertex. Points are allowed
        // to be on the line.
        for(k = 0; k < num; k++)
        {
            sidelist[k] = P_FloatPointOnLineSide(points + k, curclip);
        }

        for(k = 0; k < num; k++)
        {
            int     startIdx = k, endIdx = k + 1;

            // Check the end index.
            if(endIdx == num)
                endIdx = 0;     // Wrap-around.

            // Clipping will happen when the ends are on different sides.
            if(sidelist[startIdx] != sidelist[endIdx])
            {
                fvertex_t newvert;

                // Find the intersection point of intersecting lines.
                P_FloatInterceptVertex(points + startIdx, points + endIdx,
                                       curclip, &newvert);

                // Add the new vertex. Also modify the sidelist.
                points =
                    (fvertex_t *) realloc(points, (++num) * sizeof(fvertex_t));
                if(num >= MAX_POLY_SIDES)
                    Con_Error("Too many points in clipper.\n");

                // Make room for the new vertex.
                memmove(points + endIdx + 1, points + endIdx,
                        (num - endIdx - 1) * sizeof(fvertex_t));
                memcpy(points + endIdx, &newvert, sizeof(newvert));

                memmove(sidelist + endIdx + 1, sidelist + endIdx,
                        num - endIdx - 1);
                sidelist[endIdx] = 1;

                // Skip over the new vertex.
                k++;
            }
        }

        // Now we must discard the points that are on the wrong side.
        for(k = 0; k < num; k++)
            if(!sidelist[k])
            {
                memmove(points + k, points + k + 1,
                        (num - k - 1) * sizeof(fvertex_t));
                memmove(sidelist + k, sidelist + k + 1, num - k - 1);
                num--;
                k--;
            }
    }
    // Screen out consecutive identical points.
    for(i = 0; i < num; i++)
    {
        int     previdx = i - 1;

        if(previdx < 0)
            previdx = num - 1;
        if(points[i].x == points[previdx].x &&
           points[i].y == points[previdx].y)
        {
            // This point (i) must be removed.
            memmove(points + i, points + i + 1,
                    sizeof(fvertex_t) * (num - i - 1));
            num--;
            i--;
        }
    }
    *numpoints = num;
    return points;
}

void R_ConvexClipper(subsector_t *ssec, int num, divline_t * list)
{
    int     numclippers = num + ssec->linecount;
    int     i, numedgepoints;
    fvertex_t *edgepoints;
    fdivline_t *clippers =
        Z_Malloc(numclippers * sizeof(fdivline_t), PU_STATIC, 0);

    // Convert the divlines to float, in reverse order.
    for(i = 0; i < numclippers; i++)
    {
        if(i < num)
        {
            clippers[i].x = FIX2FLT(list[num - i - 1].x);
            clippers[i].y = FIX2FLT(list[num - i - 1].y);
            clippers[i].dx = FIX2FLT(list[num - i - 1].dx);
            clippers[i].dy = FIX2FLT(list[num - i - 1].dy);
        }
        else
        {
            seg_t  *seg = SEG_PTR(ssec->firstline + i - num);

            clippers[i].x = FIX2FLT(seg->v1->x);
            clippers[i].y = FIX2FLT(seg->v1->y);
            clippers[i].dx = FIX2FLT(seg->v2->x - seg->v1->x);
            clippers[i].dy = FIX2FLT(seg->v2->y - seg->v1->y);
        }
    }

    // Setup the 'worldwide' polygon.
    numedgepoints = 4;
    edgepoints = malloc(numedgepoints * sizeof(fvertex_t));

    edgepoints[0].x = -32768;
    edgepoints[0].y = 32768;

    edgepoints[1].x = 32768;
    edgepoints[1].y = 32768;

    edgepoints[2].x = 32768;
    edgepoints[2].y = -32768;

    edgepoints[3].x = -32768;
    edgepoints[3].y = -32768;

    // Do some clipping, <snip> <snip>
    edgepoints = edgeClipper(&numedgepoints, edgepoints, numclippers, clippers);

    if(!numedgepoints)
    {
        int idx = GET_SUBSECTOR_IDX(ssec);

        printf("All clipped away: subsector %i\n", idx);
        ssec->numverts = 0;
        ssec->verts = 0;
        //ssec->origverts = 0;
        //ssec->diffverts = 0;
    }
    else
    {
        // We need these with dynamic lights.
        ssec->verts = Z_Malloc(sizeof(fvertex_t) * numedgepoints,
                               PU_LEVELSTATIC, 0);
        memcpy(ssec->verts, edgepoints, sizeof(fvertex_t) * numedgepoints);
        ssec->numverts = numedgepoints;

        R_PrepareSubsector(ssec);
    }

    // We're done, free the edgepoints memory.
    free(edgepoints);
    Z_Free(clippers);
}

void R_PrepareSubsector(subsector_t *sub)
{
    int     j, num = sub->numverts;

    // Find the center point. First calculate the bounding box.
    sub->bbox[0].x = sub->bbox[1].x = sub->verts[0].x;
    sub->bbox[0].y = sub->bbox[1].y = sub->verts[0].y;
    sub->midpoint.x = sub->verts[0].x;
    sub->midpoint.y = sub->verts[0].y;
    for(j = 1; j < num; j++)
    {
        if(sub->verts[j].x < sub->bbox[0].x)
            sub->bbox[0].x = sub->verts[j].x;
        if(sub->verts[j].y < sub->bbox[0].y)
            sub->bbox[0].y = sub->verts[j].y;
        if(sub->verts[j].x > sub->bbox[1].x)
            sub->bbox[1].x = sub->verts[j].x;
        if(sub->verts[j].y > sub->bbox[1].y)
            sub->bbox[1].y = sub->verts[j].y;
        sub->midpoint.x += sub->verts[j].x;
        sub->midpoint.y += sub->verts[j].y;
    }
    sub->midpoint.x /= num;
    sub->midpoint.y /= num;
}

void R_PolygonizeWithoutCarving()
{
    int     i;
    int     j;
    subsector_t *sub;

    for(i = numsubsectors -1; i >= 0; --i)
    {
        sub = SUBSECTOR_PTR(i);
        sub->numverts = sub->linecount;
        sub->verts = Z_Malloc(sizeof(fvertex_t) * sub->linecount,
                                   PU_LEVELSTATIC, 0);
        for(j = 0; j < sub->linecount; j++)
        {
            sub->verts[j].x = FIX2FLT(SEG_PTR(sub->firstline + j)->v1->x);
            sub->verts[j].y = FIX2FLT(SEG_PTR(sub->firstline + j)->v1->y);
        }

        R_PrepareSubsector(sub);
    }
}

/*
 * Recursively polygonizes all ceilings and floors.
 */
void R_CreateFloorsAndCeilings(uint bspnode, int numdivlines,
                               divline_t * divlines)
{
    node_t *nod;
    divline_t *childlist, *dl;
    int     childlistsize = numdivlines + 1;

    // If this is a subsector we are dealing with, begin carving with the
    // given list.
    if(bspnode & NF_SUBSECTOR)
    {
        // We have arrived at a subsector. The divline list contains all
        // the partition lines that carve out the subsector.
        R_ConvexClipper(SUBSECTOR_PTR(bspnode & ~NF_SUBSECTOR),
                        numdivlines, divlines);
        // This leaf is done.
        return;
    }

    // Get a pointer to the node.
    nod = NODE_PTR(bspnode);

    // Allocate a new list for each child.
    childlist = malloc(childlistsize * sizeof(divline_t));

    // Copy the previous lines, from the parent nodes.
    if(divlines)
        memcpy(childlist, divlines, numdivlines * sizeof(divline_t));

    dl = childlist + numdivlines;
    dl->x = nod->x;
    dl->y = nod->y;
    // The right child gets the original line (LEFT side clipped).
    dl->dx = nod->dx;
    dl->dy = nod->dy;
    R_CreateFloorsAndCeilings(nod->children[0], childlistsize, childlist);

    // The left side. We must reverse the line, otherwise the wrong
    // side would get clipped.
    dl->dx = -nod->dx;
    dl->dy = -nod->dy;
    R_CreateFloorsAndCeilings(nod->children[1], childlistsize, childlist);

    // We are finishing with this node, free the allocated list.
    free(childlist);
}

/*
 * Fixing the sky means that for adjacent sky sectors the lower sky
 * ceiling is lifted to match the upper sky. The raising only affects
 * rendering, it has no bearing on gameplay.
 */
void R_SkyFix(void)
{
    boolean adjusted;
    int     i, f, b;

    // Check all things first (only check sky ceilings).
    for(i = 0; i < numsectors; i++)
    {
        sector_t *sec = SECTOR_PTR(i);
        mobj_t *it;

        // Must have a sky ceiling.
        if(!R_IsSkySurface(&sec->SP_ceilsurface))
            continue;
        // Check that all the things in the sector fit in.
        for(it = sec->thinglist; it; it = it->snext)
        {
            b = it->height >> FRACBITS;
            f = (sec->SP_ceilheight >> FRACBITS) + sec->skyfix.ceilOffset -
                (sec->SP_floorheight >> FRACBITS);
            if(b > f)
            {
                // Must increase skyfix.
                sec->skyfix.ceilOffset += b - f;
                if(verbose)
                {
                    Con_Printf("S%i: (mo)skyfix to %i (ceil=%i)\n",
                               GET_SECTOR_IDX(sec), sec->skyfix.ceilOffset,
                               (sec->SP_ceilheight >> FRACBITS) + sec->skyfix.ceilOffset);
                }
            }
        }
    }

    // We'll do this as long as we must to be sure all the sectors are fixed.
    // Do ceilings first.
    do
    {
        adjusted = false;

        // We need to check all the linedefs.
        for(i = 0; i < numlines; i++)
        {
            line_t *line = LINE_PTR(i);
            sector_t *front = line->frontsector, *back = line->backsector;
            int     fix = 0;

            // The conditions: must have two sides.
            if(!front || !back)
                continue;

            // Both the front and back sectors must have the sky ceiling.
            if(!R_IsSkySurface(&front->SP_ceilsurface) ||
               !R_IsSkySurface(&back->SP_ceilsurface))
                continue;

            f = (front->SP_ceilheight >> FRACBITS) + front->skyfix.ceilOffset;
            b = (back->SP_ceilheight >> FRACBITS) + back->skyfix.ceilOffset;
            if(f < b)
            {
                fix = b - (front->SP_ceilheight >> FRACBITS);
                if(fix > front->skyfix.ceilOffset)
                {
                    front->skyfix.ceilOffset = fix;
                    if(verbose)
                    {
                        Con_Printf("S%i: skyfix to %i (ceil=%i)\n",
                                   GET_SECTOR_IDX(front), front->skyfix.ceilOffset,
                                   (front->SP_ceilheight >> FRACBITS) +
                                   front->skyfix.ceilOffset);
                    }
                    adjusted = true;
                }
            }
            else if(f > b)
            {
                fix = f - (back->SP_ceilheight >> FRACBITS);
                if(fix > back->skyfix.ceilOffset)
                {
                    back->skyfix.ceilOffset = fix;
                    if(verbose)
                    {
                        Con_Printf("S%i: skyfix to %i (ceil=%i)\n",
                                   GET_SECTOR_IDX(back), back->skyfix.ceilOffset,
                                   (back->SP_ceilheight >> FRACBITS) +
                                   back->skyfix.ceilOffset);
                    }
                    adjusted = true;
                }
            }
        }
    }
    while(adjusted);

    // Now floors.
    do
    {
        adjusted = false;

        // We need to check all the linedefs.
        for(i = 0; i < numlines; i++)
        {
            line_t *line = LINE_PTR(i);
            sector_t *front = line->frontsector, *back = line->backsector;
            int     fix = 0;

            // The conditions: must have two sides.
            if(!front || !back)
                continue;

            // Both the front and back sectors must have the sky floor.
            if(!R_IsSkySurface(&front->SP_floorsurface) ||
               !R_IsSkySurface(&back->SP_floorsurface))
                continue;

            f = (front->SP_floorheight >> FRACBITS) + front->skyfix.floorOffset;
            b = (back->SP_floorheight >> FRACBITS) + back->skyfix.floorOffset;
            if(f > b)
            {
                fix = b - (front->SP_floorheight >> FRACBITS);
                if(fix < front->skyfix.floorOffset)
                {
                    front->skyfix.floorOffset = fix;
                    if(verbose)
                    {
                        Con_Printf("S%i: skyfix to %i (floor=%i)\n",
                                   GET_SECTOR_IDX(front), front->skyfix.floorOffset,
                                   (front->SP_floorheight >> FRACBITS) +
                                   front->skyfix.floorOffset);
                    }
                    adjusted = true;
                }
            }
            else if(f < b)
            {
                fix = f - (back->SP_floorheight >> FRACBITS);
                if(fix < back->skyfix.floorOffset)
                {
                    back->skyfix.floorOffset = fix;
                    if(verbose)
                    {
                        Con_Printf("S%i: skyfix to %i (floor=%i)\n",
                                   GET_SECTOR_IDX(back), back->skyfix.floorOffset,
                                   (back->SP_floorheight >> FRACBITS) +
                                   -back->skyfix.floorOffset);
                    }
                    adjusted = true;
                }
            }
        }
    }
    while(adjusted);
}

static float TriangleArea(fvertex_t * o, fvertex_t * s, fvertex_t * t)
{
    fvertex_t a = { s->x - o->x, s->y - o->y }, b =
    {
    t->x - o->x, t->y - o->y};
    float   area = (a.x * b.y - b.x * a.y) / 2;

    if(area < 0)
        area = -area;
    return area;
}

/*
 * Returns true if 'base' is a good tri-fan base.
 */
int R_TestTriFan(subsector_t *sub, int base)
{
#define TRIFAN_LIMIT    0.1
    int     i, a, b;

    if(sub->numverts == 3)
        return true;            // They're all valid.
    // Higher vertex counts need checking.
    for(i = 0; i < sub->numverts - 2; i++)
    {
        a = base + 1 + i;
        b = a + 1;
        if(a >= sub->numverts)
            a -= sub->numverts;
        if(b >= sub->numverts)
            b -= sub->numverts;
        if(TriangleArea(sub->verts + base, sub->verts + a, sub->verts + b) <=
           TRIFAN_LIMIT)
            return false;
    }
    // Whole triangle fan checked out OK, must be good.
    return true;
}

void R_SubsectorPlanes(void)
{
    int     i, k, num;
    subsector_t *sub;
    fvertex_t buf[RL_MAX_POLY_SIDES];

    for(i = 0; i < numsubsectors; i++)
    {
        sub = SUBSECTOR_PTR(i);
        num = sub->numverts;
        // We need to find a good tri-fan base vertex.
        // (One that doesn't generate zero-area triangles).
        // We'll test each one and pick the first good one.
        for(k = 0; k < num; k++)
        {
            if(R_TestTriFan(sub, k))
            {
                // Yes! This'll do nicely. Change the order of the
                // vertices so that k comes first.
                if(k)           // Need to change?
                {
                    memcpy(buf, sub->verts, sizeof(fvertex_t) * num);
                    memcpy(sub->verts, buf + k, sizeof(fvertex_t) * (num - k));
                    memcpy(sub->verts + (num - k), buf, sizeof(fvertex_t) * k);
                }
                goto ddSP_NextSubSctr;
            }
        }
        // There was no match. Bugger. We need to use the subsector
        // midpoint as the base. It's always valid.
        sub->flags |= DDSUBF_MIDPOINT;
        //Con_Message("Using midpoint for subsctr %i.\n", i);

      ddSP_NextSubSctr:;
    }
}

void R_SetVertexOwner(int idx, sector_t *secptr)
{
    int     i;
    vertexowner_t *own;
    int    *list;
    int     sector;

    if(!secptr)
        return;
    sector = GET_SECTOR_IDX(secptr);
    own = vertexowners + idx;
    // Has this sector been already registered?
    for(i = 0; i < own->num; i++)
        if(own->list[i] == sector)
            return;             // Yes, we can exit.
    // Add a new owner.
    own->num++;
    // Allocate a new list.
    list = Z_Malloc(sizeof(int) * own->num, PU_LEVELSTATIC, 0);
    // If there are previous references, copy them.
    if(own->num > 1)
    {
        memcpy(list, own->list, sizeof(int) * (own->num - 1));
        // Free the old list.
        Z_Free(own->list);
    }
    own->list = list;
    own->list[own->num - 1] = sector;
}

void R_SetVertexLineOwner(int idx, line_t *lineptr)
{
    int     i;
    vertexowner_t *own;
    int    *list;
    int     line;

    if(!lineptr)
        return;

    line = GET_LINE_IDX(lineptr);
    own = vertexowners + idx;

    // If this is a one-sided line then this is an "anchored" vertex.
    if(!(lineptr->frontsector && lineptr->backsector))
        own->anchored = true;

    // Has this line been already registered?
    for(i = 0; i < own->numlines; i++)
        if(own->linelist[i] == line)
            return;             // Yes, we can exit.

    // Add a new owner.
    own->numlines++;
    // Allocate a new list.
    list = Z_Malloc(sizeof(int) * own->numlines, PU_LEVELSTATIC, 0);
    // If there are previous references, copy them.
    if(own->numlines > 1)
    {
        memcpy(list, own->linelist, sizeof(int) * (own->numlines - 1));
        // Free the old list.
        Z_Free(own->linelist);
    }
    own->linelist = list;
    own->linelist[own->numlines - 1] = line;
}

static vertex_t *rootVtx;
/*
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (int*) line indexs.
 */
static int C_DECL lineAngleSorter(const void *a, const void *b)
{
    fixed_t dx, dy;
    binangle_t angleA, angleB;
    line_t *lineA = LINE_PTR(*(int *)a);
    line_t *lineB = LINE_PTR(*(int *)b);

    if(lineA->v1 == rootVtx)
    {
        dx = lineA->v2->x - rootVtx->x;
        dy = lineA->v2->y - rootVtx->y;
    }
    else
    {
        dx = lineA->v1->x - rootVtx->x;
        dy = lineA->v1->y - rootVtx->y;
    }
    angleA = bamsAtan2(-(dx >> 13), dy >> 13);

    if(lineB->v1 == rootVtx)
    {
        dx = lineB->v2->x - rootVtx->x;
        dy = lineB->v2->y - rootVtx->y;
    }
    else
    {
        dx = lineB->v1->x - rootVtx->x;
        dy = lineB->v1->y - rootVtx->y;
    }
    angleB = bamsAtan2(-(dx >> 13), dy >> 13);

    return (angleB - angleA);
}

/*
 * Generates an array of sector references for each vertex. The list
 * includes all the sectors the vertex belongs to.
 *
 * Generates an array of line references for each vertex. The list
 * includes all the lines the vertex belongs to sorted by angle.
 * (the list is arranged in clockwise order, east = 0).
 */
void R_InitVertexOwners(void)
{
    int     i, k, p, v[2];
    sector_t      *sec;
    vertexowner_t *own;

    // Allocate enough memory.
    vertexowners = Z_Malloc(sizeof(vertexowner_t) * numvertexes,
                                   PU_LEVEL, 0);
    memset(vertexowners, 0, sizeof(vertexowner_t) * numvertexes);

    for(i = 0, sec = sectors; i < numsectors; ++i, sec++)
    {
        // Traversing the line list will do fine.
        for(k = 0; k < sec->linecount; ++k)
        {
            line_t* line = sec->Lines[k];
            v[0] = GET_VERTEX_IDX(line->v1);
            v[1] = GET_VERTEX_IDX(line->v2);
            for(p = 0; p < 2; p++)
            {
                R_SetVertexOwner(v[p], line->frontsector);
                R_SetVertexOwner(v[p], line->backsector);
                R_SetVertexLineOwner(v[p], line);
            }
        }
    }

    // Sort lineowner lists for each vertex clockwise based on line angle,
    // so we can walk around the lines attached to each vertex.
    // qsort is fast enough? (need to profile large maps)
    for(i = 0, own = vertexowners; i < numvertexes; ++i, own++)
    {
        rootVtx = &vertexes[i];
        if(own->numlines > 1)
            qsort(own->linelist, own->numlines, sizeof(int), lineAngleSorter);
    }
}

/*boolean DD_SubContainTest(sector_t *innersec, sector_t *outersec)
   {
   int i, k;
   boolean contained;
   float in[4], out[4];
   subsector_t *isub, *osub;

   // Test each subsector of innersec against all subsectors of outersec.
   for(i=0; i<numsubsectors; i++)
   {
   isub = SUBSECTOR_PTR(i);
   contained = false;
   // Only accept innersec's subsectors.
   if(isub->sector != innersec) continue;
   for(k=0; k<numsubsectors && !contained; k++)
   {
   osub = SUBSECTOR_PTR(i);
   // Only accept outersec's subsectors.
   if(osub->sector != outersec) continue;
   // Test if isub is inside osub.
   if(isub->bbox[BLEFT] >= osub->bbox[BLEFT]
   && isub->bbox[BRIGHT] <= osub->bbox[BRIGHT]
   && isub->bbox[BTOP] >= osub->bbox[BTOP]
   && isub->bbox[BBOTTOM] <= osub->bbox[BBOTTOM])
   {
   // This is contained.
   contained = true;
   }
   }
   if(!contained) return false;
   }
   // All tested subsectors were contained!
   return true;
   } */

/*
 * The test is done on subsectors.
 */
sector_t *R_GetContainingSectorOf(sector_t *sec)
{
    int     i;
    float   cdiff = -1, diff;
    float   inner[4], outer[4];
    sector_t *other, *closest = NULL;

    memcpy(inner, secinfo[GET_SECTOR_IDX(sec)].bounds, sizeof(inner));

    // Try all sectors that fit in the bounding box.
    for(i = 0; i < numsectors; i++)
    {
        other = SECTOR_PTR(i);
        if(!other->linecount || SECT_INFO(other)->unclosed)
            continue;
        if(other == sec)
            continue;           // Don't try on self!
        memcpy(outer, secinfo[i].bounds, sizeof(outer));
        if(inner[BLEFT] >= outer[BLEFT] && inner[BRIGHT] <= outer[BRIGHT] &&
           inner[BTOP] >= outer[BTOP] && inner[BBOTTOM] <= outer[BBOTTOM])
        {
            // Inside! Now we must test each of the subsectors. Otherwise
            // we can't be sure...
            /*if(DD_SubContainTest(sec, other))
               { */
            // Sec is totally and completely inside other!
            diff = M_BoundingBoxDiff(inner, outer);
            if(cdiff < 0 || diff <= cdiff)
            {
                closest = other;
                cdiff = diff;
            }
            //}
        }
    }
    return closest;
}

void R_InitSectorInfo(void)
{
    int     i, k;
    sectorinfo_t *info;
    sector_t *sec, *other;
    line_t *lin;
    boolean dohack;
    boolean unclosed;

    secinfo = Z_Calloc(sizeof(sectorinfo_t) * numsectors, PU_LEVEL, 0);

    // Calculate bounding boxes for all sectors.
    // Check for unclosed sectors.
    for(i = 0; i < numsectors; i++)
    {
        P_SectorBoundingBox(SECTOR_PTR(i), secinfo[i].bounds);

        if(i == 0)
        {
            // The first sector is used as is.
            memcpy(mapBounds, secinfo[i].bounds, sizeof(mapBounds));
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(mapBounds, secinfo[i].bounds);
        }

        // Detect unclosed sectors.
        unclosed = false;
        if(!(SECTOR_PTR(i)->linecount >= 3))
            unclosed = true;
        else
        {
            // TODO:
            // Add algorithm to check for unclosed sectors here.
            // Perhaps have a look at glBSP.
        }

        if(unclosed)
            secinfo[i].unclosed = true;
    }

    for(i = 0, info = secinfo; i < numsectors; i++, info++)
    {
        sec = SECTOR_PTR(i);
        if(!sec->linecount)
            continue;

        // Is this sector completely contained by another?
        info->containsector = R_GetContainingSectorOf(sec);

        dohack = true;
        for(k = 0; k < sec->linecount; k++)
        {
            lin = sec->Lines[k];

            if(!lin->frontsector || !lin->backsector ||
               lin->frontsector != lin->backsector)
            {
                dohack = false;
                break;
            }
        }

        if(dohack)
        {
            // Link all planes permanently.
            info->permanentlink = true;
            for(k = 0; k < NUM_PLANES; ++k)
                info->planeinfo[k].linked = info->containsector;

            Con_Printf("Linking S%i planes permanently to S%i\n", i,
                       GET_SECTOR_IDX(info->containsector));
        }

        // Is this sector large enough to be a dominant light source?
        if(info->lightsource == NULL &&
           (R_IsSkySurface(&sec->SP_ceilsurface) ||
            R_IsSkySurface(&sec->SP_floorsurface)) &&
           info->bounds[BRIGHT] - info->bounds[BLEFT] > DOMINANT_SIZE &&
           info->bounds[BBOTTOM] - info->bounds[BTOP] > DOMINANT_SIZE)
        {
            // All sectors touching this one will be affected.
            for(k = 0; k < sec->linecount; k++)
            {
                other = sec->Lines[k]->frontsector;
                if(!other || other == sec)
                {
                    other = sec->Lines[k]->backsector;
                    if(!other || other == sec)
                        continue;
                }
                SECT_INFO(other)->lightsource = sec;
            }
        }
    }
}

void R_InitSegInfo(void)
{
    int i, k, j, n;
    seginfo_t *inf;

    seginfo = Z_Calloc(numsegs * sizeof(seginfo_t), PU_LEVEL, NULL);

    for(i = 0, inf = seginfo; i < numsegs; ++i, ++inf)
    {
        for(k = 0; k < 4; ++k)
        {
/*            inf->illum[0][k].front =
                inf->illum[1][k].front =
                inf->illum[2][k].front = SEG_PTR(i)->frontsector;*/

            for(j = 0; j < 3; ++j)
            {
                inf->illum[j][k].flags = VIF_STILL_UNSEEN;

                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                {
                    inf->illum[j][k].casted[n].source = -1;
                }
            }
        }
    }
}

void R_InitPlaneIllumination(subsector_t *sub, int planeid)
{
    int i, j;
    subsectorinfo_t *ssecinfo = SUBSECT_INFO(sub);
    planeinfo_t *plane = &ssecinfo->plane[planeid];

    plane->illumination = Z_Calloc(ssecinfo->numvertices * sizeof(vertexillum_t),
                                   PU_LEVELSTATIC, NULL);

    for(i = 0; i < ssecinfo->numvertices; ++i)
    {
        plane->illumination[i].flags |= VIF_STILL_UNSEEN;

        for(j = 0; j < MAX_BIAS_AFFECTED; ++j)
            plane->illumination[i].casted[j].source = -1;
    }
}

void R_InitPlanePolys(subsector_t *subsector)
{
    int     numvrts, i;
    fvertex_t *vrts, *vtx, *pv;
    subsectorinfo_t *ssecinfo = SUBSECT_INFO(subsector);

    // Take the subsector's vertices.
    numvrts = subsector->numverts;
    vrts = subsector->verts;

    // Copy the vertices to the poly.
    if(subsector->flags & DDSUBF_MIDPOINT)
    {
        // Triangle fan base is the midpoint of the subsector.
        ssecinfo->numvertices = 2 + numvrts;
        ssecinfo->vertices =
            Z_Malloc(sizeof(fvertex_t) * ssecinfo->numvertices, PU_LEVELSTATIC, 0);

        memcpy(ssecinfo->vertices, &subsector->midpoint, sizeof(fvertex_t));

        vtx = vrts;
        pv = ssecinfo->vertices + 1;
    }
    else
    {
        ssecinfo->numvertices = numvrts;
        ssecinfo->vertices =
            Z_Malloc(sizeof(fvertex_t) * ssecinfo->numvertices, PU_LEVELSTATIC, 0);

        // The first vertex is always the same: vertex zero.
        pv = ssecinfo->vertices;
        memcpy(pv, &vrts[0], sizeof(*pv));

        vtx = vrts + 1;
        pv++;
        numvrts--;
    }

    // Add the rest of the vertices.
    for(; numvrts > 0; numvrts--, vtx++, pv++)
        memcpy(pv, vtx, sizeof(*vtx));

    if(subsector->flags & DDSUBF_MIDPOINT)
    {
        // Re-add the first vertex so the triangle fan wraps around.
        memcpy(pv, &ssecinfo->vertices[1], sizeof(*pv));
    }

    // Initialize the illumination for the subsector.
    for(i = 0; i < NUM_PLANES; ++i)
        R_InitPlaneIllumination(subsector, i);

    // FIXME: $nplanes
    // Initialize the plane types.
    ssecinfo->plane[PLN_FLOOR].type = PLN_FLOOR;
    ssecinfo->plane[PLN_CEILING].type = PLN_CEILING;
}

void R_InitSubsectorInfo(void)
{
    int     i;
    subsectorinfo_t *info;

    i = sizeof(subsectorinfo_t) * numsubsectors;
#ifdef _DEBUG
    Con_Printf("R_InitSubsectorInfo: %i bytes.\n", i);
#endif
    subsecinfo = Z_Calloc(i, PU_LEVEL, NULL);

#ifdef _DEBUG
    Z_CheckHeap();
#endif

    for(i = 0, info = subsecinfo; i < numsubsectors; i++, info++)
    {
        R_InitPlanePolys(SUBSECTOR_PTR(i));
    }

#ifdef _DEBUG
    Z_CheckHeap();
#endif
}

/*
 *  Mapinfo must be set.
 */
void R_SetupFog(void)
{
    int     flags;

    if(!mapinfo)
    {
        // Go with the defaults.
        Con_Execute(CMDS_DDAY,"fog off", true);
        return;
    }

    // Check the flags.
    flags = mapinfo->flags;
    if(flags & MIF_FOG)
    {
        // Setup fog.
        Con_Execute(CMDS_DDAY, "fog on", true);
        Con_Executef(CMDS_DDAY, true, "fog start %f", mapinfo->fog_start);
        Con_Executef(CMDS_DDAY, true, "fog end %f", mapinfo->fog_end);
        Con_Executef(CMDS_DDAY, true, "fog density %f", mapinfo->fog_density);
        Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                     mapinfo->fog_color[0] * 255, mapinfo->fog_color[1] * 255,
                     mapinfo->fog_color[2] * 255);
    }
    else
    {
        Con_Execute(CMDS_DDAY, "fog off", true);
    }
}

/*
 * Scans all sectors for any supported DOOM.exe renderer hacks.
 * Updates sectorinfo accordingly.
 *
 * This algorithm detects self-referencing sector hacks used for
 * fake 3D structures.
 *
 * NOTE: Self-referencing sectors where all lines in the sector
 *       are self-referencing are NOT handled by this algorthim.
 *       Those kind of hacks are detected in R_InitSectorInfo().
 *
 * TODO: DJS - We need to collect subsectors into "mutually
 * exclusive groups" and instead of linking whole sectors to
 * another for rendering we should instead link each subsector
 * exclusion group to another sector (in most cases these can be
 * permanent links). This will allow us to support 99% of DOOM
 * rendering hacks cleanly.
 *
 * This algorthim already collects the complex case of
 * self-referencing lines attached to none referencing lines
 * into potential exclusion groups, another algorthim is needed
 * to collect non-anchored self-referencing lines into similar
 * groups (the easy part).
 *
 * From these line groups, we need to collect subsectors
 * into groups where all segs are in this group AND THIS side
 * (or self-referencing) is in this sector.
 */
void R_RationalizeSectors(void)
{
    int     i, j, k, l, m;
    int count;
    int     numroots;
    sectorinfo_t *info;
    sector_t *sec;
    line_t *lin;
    line_t **collectedLines;
    int      maxNumLines = 20; // Initial size of the "run" (line list).
    lineinfo_t *linfo;
    boolean selfRefHack;

    // Allocate some memory for the line "run" (line list).
    collectedLines = M_Malloc(maxNumLines * sizeof(line_t*));

    for(i = 0, info = secinfo; i < numsectors; ++i, info++)
    {
        sec = SECTOR_PTR(i);
        if(!sec->linecount)
            continue;

        // Detect self-referencing "root" lines.
        // NOTE: We need to find ALL of them.
        selfRefHack = false;
        numroots = 0;

        for(k = 0; k < sec->linecount; ++k)
        {
            lin = sec->Lines[k];
            linfo = LINE_INFO(lin);

            if(lin->frontsector && lin->backsector &&
               lin->frontsector == lin->backsector &&
               lin->backsector == sec)
            {
                vertexowner_t *ownerA, *ownerB;
                // The line properties indicate that this might be a
                // self-referencing, hack sector.

                // Make sure this line isn't isolated
                // (ie both vertexes arn't endpoints).
                ownerA = &vertexowners[GET_VERTEX_IDX(lin->v1)];
                ownerB = &vertexowners[GET_VERTEX_IDX(lin->v2)];
                if(!(ownerA->numlines == 1 && ownerB->numlines == 1))
                {
                    // Also, this line could split a sector and both ends
                    // COULD be vertexes that make up the sector outline.
                    // So, check all line owners of each vertex.

                    // Test simple case - single line dividing a sector
                    if(!(ownerA->num == 1 && ownerB->num == 1))
                    {
                        line_t *owner;
                        boolean ok = true;
                        boolean ok2 = true;

                        // Ok, need to check for neighbors.
                        // Test all the line owners to see that they arn't
                        // "real" twosided lines.

                        // TODO: This approach is too simple. We should instead
                        //       sort the vertexe's line owners based on angle
                        //       after init.
                        if(ownerA->num > 1)
                        {
                            count = 0;
                            for(j = 0; j < ownerA->numlines && ok; ++j)
                            {
                                owner = LINE_PTR(ownerA->linelist[j]);
                                if(owner != lin)
                                if((owner->frontsector == sec ||
                                    (owner->backsector && owner->backsector == sec)))
                                {
                                    ++count;
                                    if(count > 1)
                                        ok = false;
                                }
                            }
                        }

                        if(ok && ownerB->num > 1)
                        {
                            count = 0;
                            for(j = 0; j < ownerB->numlines && ok2; ++j)
                            {
                                owner = LINE_PTR(ownerB->linelist[j]);
                                if(owner != lin)
                                if((owner->frontsector == sec ||
                                    (owner->backsector && owner->backsector == sec)))
                                {
                                    ++count;
                                    if(count > 1)
                                        ok2 = false;
                                }
                            }
                        }

                        if(ok && ok2)
                        {
                            selfRefHack = true;
                            linfo->selfrefhackroot = true;
                            VERBOSE2(Con_Message("L%i selfref root to S%i\n",
                                        GET_LINE_IDX(lin), GET_SECTOR_IDX(sec)));
                            ++numroots;
                        }
                    }
                }
            }
        }

        if(selfRefHack)
        {
            vertexowner_t *ownerA, *ownerB;
            line_t *line;
            line_t *next;
            vertexowner_t *owner;
            boolean scanOwners, addToCollection, found;

            // Mark this sector as a self-referencing hack.
            info->selfRefHack = true;

            // Now look for lines connected to this root line (and any
            // subsequent lines that connect to those) that match the
            // requirements for a selfreferencing hack.

            // Once all lines have been collected - promote them to
            // self-referencing hack lines for this sector.
            for(k = 0; k < sec->linecount; ++k)
            {
                lin = sec->Lines[k];
                linfo = LINE_INFO(lin);
                if(!linfo->selfrefhackroot)
                    continue;

                if(lin->validcount)
                    continue; // We've already found this hack group.

                ownerA = &vertexowners[GET_VERTEX_IDX(lin->v1)];
                ownerB = &vertexowners[GET_VERTEX_IDX(lin->v2)];
                for(l = 0; l < 2; ++l)
                {
                    // Clear the collectedLines list.
                    memset(collectedLines, 0, sizeof(collectedLines));
                    count = 0;

                    line = lin;
                    scanOwners = true;

                    if(l == 0)
                        owner = ownerA;
                    else
                        owner = ownerB;

                    for(j = 0; j < owner->numlines && scanOwners; ++j)
                    {
                        next = LINE_PTR(owner->linelist[j]);
                        if(next == line || next == lin)
                            continue;

                        line = next;

                        if(!(line->frontsector == sec &&
                           (line->backsector && line->backsector == sec)))
                            continue;

                        // Is this line already in the current run? (self collision)
                        addToCollection = true;
                        for(m = 0; m < count && addToCollection; ++m)
                        {
                            if(line == collectedLines[m])
                            {
                                int n, o, p, q;
                                line_t *lcand = NULL;
                                vertexowner_t *vown = NULL;

                                // Pick another candidate to continue line collection.

                                // Logic: Work backwards through the collected line
                                // list, checking the number of line owners of each
                                // vertex. If we find one with >2 line owners - check
                                // the lines to see if it would be a good place to
                                // recommence line collection (a self-referencing line,
                                // same sector).
                                pickNewCandiate:;
                                found = false;
                                for(n = count-1; n >= 0 && !found; --n)
                                {
                                    for(q= 0; q < 2 && !found; ++q)
                                    {
                                        if(q == 0)
                                            vown = &vertexowners[GET_VERTEX_IDX(collectedLines[n]->v1)];
                                        else
                                            vown = &vertexowners[GET_VERTEX_IDX(collectedLines[n]->v2)];

                                        if(vown && vown->numlines > 2)
                                        {
                                            for(o = 0; o <= vown->numlines && !found; ++o)
                                            {
                                                lcand = LINE_PTR(vown->linelist[o]);
                                                if(lcand &&
                                                   lcand->frontsector && lcand->backsector &&
                                                   lcand->frontsector == lcand->backsector &&
                                                   lcand->frontsector == sec)
                                                {
                                                    // Have we already collected it?
                                                    found = true;
                                                    for(p = count-1; p >=0  && found; --p)
                                                        if(lcand == collectedLines[p])
                                                            found = false; // no good
                                                }
                                            }
                                        }
                                    }
                                }

                                if(found)
                                {
                                    // Found a suitable one.
                                    line = lcand;
                                    owner = vown;
                                    addToCollection = true;
                                }
                                else
                                {
                                    // We've found all the lines involved in
                                    // this self-ref hack.
                                    if(count >= 1)
                                    {
                                        // Promote all lines in the collection
                                        // to self-referencing root lines.
                                        while(count--)
                                        {
                                            LINE_INFO(collectedLines[count])->selfrefhackroot = true;
                                            VERBOSE2(Con_Message("  L%i selfref root to S%i\n",
                                                                 GET_LINE_IDX(collectedLines[count]),
                                                                 GET_SECTOR_IDX(sec)));

                                            // Use validcount to mark them as done.
                                            collectedLines[count]->validcount = true;
                                        }
                                    }
                                    // We are done with this group, don't collect.
                                    addToCollection = false;

                                    // We are done with this vertex for this root.
                                    scanOwners = false;
                                    count = 0;
                                }
                            }
                        }

                        if(addToCollection)
                        {
                            if(++count > maxNumLines)
                            {
                                // Allocate some more memory.
                                maxNumLines *= 2;
                                if(maxNumLines < count)
                                    maxNumLines = count;

                                collectedLines =
                                    M_Realloc(collectedLines, sizeof(line_t*) * maxNumLines);
                            }

                            collectedLines[count-1] = line;

                            if(LINE_INFO(line)->selfrefhackroot)
                                goto pickNewCandiate;
                        }

                        if(scanOwners)
                        {
                            // Get the vertexowner info for the other vertex.
                            if(owner - vertexowners == GET_VERTEX_IDX(line->v1))
                                owner = &vertexowners[GET_VERTEX_IDX(line->v2)];
                            else
                                owner = &vertexowners[GET_VERTEX_IDX(line->v1)];

                            j = -1; // Start from the begining with this vertex.
                        }
                    }
                }
            }
        }
    }

    // Free temporary storage.
    M_Free(collectedLines);
}

/*
 * Mapinfo must be set.
 */
void R_SetupSky(void)
{
    int     i, k;
    int     skyTex;

    if(!mapinfo)
    {
        // Use the defaults.
        Rend_SkyParams(DD_SKY, DD_HEIGHT, .666667f);
        Rend_SkyParams(DD_SKY, DD_HORIZON, 0);
        Rend_SkyParams(0, DD_ENABLE, 0);
        Rend_SkyParams(0, DD_TEXTURE, R_TextureNumForName("SKY1"));
        Rend_SkyParams(0, DD_MASK, DD_NO);
        Rend_SkyParams(0, DD_OFFSET, 0);
        Rend_SkyParams(1, DD_DISABLE, 0);

        // There is no sky color.
        noSkyColorGiven = true;
        return;
    }

    Rend_SkyParams(DD_SKY, DD_HEIGHT, mapinfo->sky_height);
    Rend_SkyParams(DD_SKY, DD_HORIZON, mapinfo->horizon_offset);
    for(i = 0; i < 2; i++)
    {
        k = mapinfo->sky_layers[i].flags;
        if(k & SLF_ENABLED)
        {
            skyTex = R_TextureNumForName(mapinfo->sky_layers[i].
                                               texture);
            if(skyTex == -1)
            {
                Con_Message("R_SetupSky: Invalid/missing texture \"%s\"\n",
                            mapinfo->sky_layers[i].texture);
                skyTex = R_TextureNumForName("SKY1");
            }

            Rend_SkyParams(i, DD_ENABLE, 0);
            Rend_SkyParams(i, DD_TEXTURE, skyTex);
            Rend_SkyParams(i, DD_MASK, k & SLF_MASKED ? DD_YES : DD_NO);
            Rend_SkyParams(i, DD_OFFSET, mapinfo->sky_layers[i].offset);
            Rend_SkyParams(i, DD_COLOR_LIMIT,
                           mapinfo->sky_layers[i].color_limit);
        }
        else
        {
            Rend_SkyParams(i, DD_DISABLE, 0);
        }
    }

    // Any sky models to setup? Models will override the normal
    // sphere.
    R_SetupSkyModels(mapinfo);

    // How about the sky color?
    noSkyColorGiven = true;
    for(i = 0; i < 3; i++)
    {
        skyColorRGB[i] = (byte) (255 * mapinfo->sky_color[i]);
        if(mapinfo->sky_color[i] > 0)
            noSkyColorGiven = false;
    }

    // Calculate a balancing factor, so the light in the non-skylit
    // sectors won't appear too bright.
    if(false &&
       (mapinfo->sky_color[0] > 0 || mapinfo->sky_color[1] > 0 ||
        mapinfo->sky_color[2] > 0))
    {
        skyColorBalance =
            (0 +
             (mapinfo->sky_color[0] * 2 + mapinfo->sky_color[1] * 3 +
              mapinfo->sky_color[2] * 2) / 7) / 1;
    }
    else
    {
        skyColorBalance = 1;
    }
}

/*
 * Returns pointers to the line's vertices in such a fashion that
 * verts[0] is the leftmost vertex and verts[1] is the rightmost
 * vertex, when the line lies at the edge of `sector.'
 */
void R_OrderVertices(line_t *line, const sector_t *sector, vertex_t *verts[2])
{
    if(sector == line->frontsector)
    {
        verts[0] = line->v1;
        verts[1] = line->v2;
    }
    else
    {
        verts[0] = line->v2;
        verts[1] = line->v1;
    }
}

/*
 * A neighbour is a line that shares a vertex with 'line', and faces
 * the specified sector.  Finds both the left and right neighbours.
 */
void R_FindLineNeighbors(sector_t *sector, line_t *line,
                         struct line_s **neighbors, int alignment)
{
    struct line_s *other;
    vertex_t *vtx[2];
    int     j;

    // We want to know which vertex is the leftmost/rightmost one.
    R_OrderVertices(line, sector, vtx);

    // Find the real neighbours, which are in the same sector
    // as this line.
    for(j = 0; j < sector->linecount; j++)
    {
        other = sector->Lines[j];
        if(other == line)
            continue;

        // Is this a valid neighbour?
        if(other->frontsector == other->backsector)
            continue;

        // Do we need to test the line alignment?
        if(alignment)
        {
#define SEP 10
            binangle_t diff = LINE_INFO(line)->angle - LINE_INFO(other)->angle;

            /*if(!(diff < SEP && diff > BANG_MAX - SEP) &&
               !(diff < BANG_180 + SEP && diff > BANG_180 - SEP))
               continue; // Misaligned. */

            if(alignment < 0)
                diff -= BANG_180;
            if(other->frontsector != sector)
                diff -= BANG_180;
            if(!(diff < SEP || diff > BANG_MAX - SEP))
                continue;       // Misaligned.
        }

        // It's our 'left' neighbour if it shares v1.
        if(other->v1 == vtx[0] || other->v2 == vtx[0])
            neighbors[0] = other;

        // It's our 'right' neighbour if it shares v2.
        if(other->v1 == vtx[1] || other->v2 == vtx[1])
            neighbors[1] = other;

        // Do we have everything we want?
        if(neighbors[0] && neighbors[1])
            break;
    }
}

static boolean R_IsEquivalent(line_t *a, line_t *b)
{
    return ((a->v1 == b->v1 && a->v2 == b->v2) ||
            (a->v1 == b->v2 && a->v2 == b->v1));
}

/*
 * Browse through the lines in backSector.  The backNeighbor is the
 * line that 1) isn't realNeighbor and 2) connects to commonVertex.
 */
static void R_FindBackNeighbor(sector_t *backSector, line_t *self,
                               line_t *realNeighbor, vertex_t *commonVertex,
                               line_t **backNeighbor)
{
    int     i;
    line_t *line;

    for(i = 0; i < backSector->linecount; i++)
    {
        line = backSector->Lines[i];
        if(R_IsEquivalent(line, realNeighbor) || R_IsEquivalent(line, self))
            continue;
        if(LINE_INFO(line)->selfrefhackroot)
            continue;

        if(line->v1 == commonVertex || line->v2 == commonVertex)
        {
            *backNeighbor = line;
            return;
        }
    }
}

/*
 * Calculate accurate lengths for all lines.
 */
void R_InitLineInfo(void)
{
    line_t *line;
    int     i;
    lineinfo_t *info;

    // Allocate memory for the line info.
    lineinfo = Z_Calloc(sizeof(lineinfo_t) * numlines, PU_LEVEL, NULL);

    // Calculate the accurate length of each line.
    for(i = 0, info = lineinfo; i < numlines; i++, info++)
    {
        line = LINE_PTR(i);
        info->length = P_AccurateDistance(line->dx, line->dy);
        info->angle = bamsAtan2(-(line->dx >> 13), line->dy >> 13);
    }
}

/*
 * Find line neighbours, which will be used in the FakeRadio calculations.
 */
void R_InitLineNeighbors(void)
{
    line_t *line, *other;
    sector_t *sector;
    int     i, k, j, m;
    lineinfo_t *info;
    lineinfo_side_t *side;
    vertexowner_t *owner;
    vertex_t *vertices[2];

    // Find neighbours. We'll do this sector by sector.
    for(k = 0; k < numsectors; k++)
    {
        sector = SECTOR_PTR(k);
        for(i = 0; i < sector->linecount; i++)
        {
            line = sector->Lines[i];
            info = LINE_INFO(line);

            // Which side is this?
            side =
                (line->frontsector ==
                 sector ? &info->side[0] : &info->side[1]);

            R_FindLineNeighbors(sector, line, side->neighbor, 0);

            R_OrderVertices(line, sector, vertices);

            // Figure out the sectors in the proximity.
            for(j = 0; j < 2; j++)
            {
                // Neighbour must be two-sided.
                if(side->neighbor[j] && side->neighbor[j]->frontsector &&
                   side->neighbor[j]->backsector &&
                   !LINE_INFO(side->neighbor[j])->selfrefhackroot)
                {
                    side->proxsector[j] =
                        (side->neighbor[j]->frontsector ==
                         sector ? side->neighbor[j]->backsector : side->
                         neighbor[j]->frontsector);

                    // Find the backneighbour.  They are the
                    // neighbouring lines in the backsectors of the
                    // neighbour lines.
                    R_FindBackNeighbor(side->proxsector[j], line,
                                       side->neighbor[j], vertices[j],
                                       &side->backneighbor[j]);

                    /*assert(side->backneighbor[j] != line); */
                }
                else
                {
                    side->proxsector[j] = NULL;
                }
            }

            // Look for aligned neighbours.  They are side-specific.
            for(j = 0; j < 2; j++)
            {
                owner = vertexowners + GET_VERTEX_IDX(vertices[j]);
                for(m = 0; m < owner->num; m++)
                {
                    //if(owner->list[m] == k) continue;
                    R_FindLineNeighbors(SECTOR_PTR(owner->list[m]), line,
                                        side->alignneighbor,
                                        side == &info->side[0] ? 1 : -1);
                }
            }

            /*          // How about the other sector?
               if(!line->backsector || !line->frontsector)
               continue; // Single-sided.

               R_FindLineNeighbors(line->frontsector == sector?
               line->backsector : line->frontsector,
               line, info->backneighbor); */

            // Attempt to find "pretend" neighbors for this line, if "real"
            // ones have not been found.
            // NOTE: selfrefhackroot lines don't have neighbors.
            //       They can only be "pretend" neighbors.

            // Pretend neighbors are selfrefhackroot lines but BOTH vertices
            // are owned by this sector and ONE of the vertexes is owned by
            // this line.
            if((!side->neighbor[0] || !side->neighbor[1]) && !info->selfrefhackroot)
            {
                // Check all lines.
                for(j = 0; j < numlines; ++j)
                {
                    other = LINE_PTR(j);
                    if(LINE_INFO(other)->selfrefhackroot &&
                        (other->v1 == line->v1 || other->v1 == line->v2 ||
                         other->v2 == line->v1 || other->v2 == line->v2))
                    {
                        boolean ok, ok2;
                        ok = ok2 = false;
                        owner = &vertexowners[GET_VERTEX_IDX(other->v1)];
                        for(m = 0; m < owner->num && !ok; ++m)
                            if(owner->list[m] == k) // k == sector id
                                ok = true;

                        if(ok)
                        {
                            owner = &vertexowners[GET_VERTEX_IDX(other->v2)];
                            for(m = 0; m < owner->num && !ok2; ++m)
                                if(owner->list[m] == k) // k == sector id
                                    ok2 = true;
                        }

                        if(ok && ok2)
                        {
#if _DEBUG
                            VERBOSE2(Con_Message("L%i is a pretend neighbor to L%i\n",
                                         GET_LINE_IDX(other), GET_LINE_IDX(line)));
#endif

                            if(other->v1 == vertices[0] || other->v2 == vertices[0])
                            {
                                side->neighbor[0] = other;
                                side->pretendneighbor[0] = true;
                            }
                            else
                            {
                                side->neighbor[1] = other;
                                side->pretendneighbor[1] = true;
                            }
                        }
                    }
                }
            }
        }
    }

#ifdef _DEBUG
    if(verbose >= 1)
    {
        for(i = 0; i < numlines; i++)
        {
            for(k = 0; k < 2; k++)
            {
                line = LINE_PTR(i);
                side = LINE_INFO(line)->side + k;
                if(side->alignneighbor[0] || side->alignneighbor[1])
                    Con_Printf("Line %i/%i: l=%i r=%i\n", i, k,
                               side->alignneighbor[0] ? GET_LINE_IDX(side->
                                                                     alignneighbor
                                                                     [0]) : -1,
                               side->alignneighbor[1] ? GET_LINE_IDX(side->
                                                                     alignneighbor
                                                                     [1]) :
                               -1);
            }
        }
    }
#endif
}

/*
 * This routine is called from the Game to polygonize the current
 * level.  Creates floors and ceilings and fixes the adjacent sky
 * sector heights.  Creates a big enough dlBlockLinks.  Reads mapinfo
 * and does the necessary setup.
 */
void R_SetupLevel(char *level_id, int flags)
{
    int     i;

    if(flags & DDSLF_INITIALIZE)
    {
        // Switch to fast malloc mode in the zone. This is intended for large
        // numbers of mallocs with no frees in between.
        Z_EnableFastMalloc(false);

        // A new level is about to be setup.
        levelSetup = true;

        // This is called before anything is actually done.
        if(loadInStartupMode)
            Con_StartupInit();
        return;
    }

    // First check for some special tasks.
    if(flags & DDSLF_INIT_LINKS)
    {
        // Init polyobj blockmap.
        P_InitPolyBlockMap();

        // Initialize node piles and line rings.
        NP_Init(&thingnodes, 256);  // Allocate a small pile.
        NP_Init(&linenodes, numlines + 1000);

        // Allocate the rings.
        linelinks = Z_Malloc(sizeof(*linelinks) * numlines, PU_LEVEL, 0);
        for(i = 0; i < numlines; i++)
            linelinks[i] = NP_New(&linenodes, NP_ROOT_NODE);
        return;                 // Do nothing else.
    }
    if(flags & DDSLF_SERVER_ONLY)
    {
        // Only init server data.
        Sv_InitPools();
        return;
    }
    if(flags & DDSLF_AFTER_LOADING)
    {
        // Loading a game usually destroys all thinkers. Until a proper
        // savegame system handled by the engine is introduced we'll have
        // to resort to re-initializing the most important stuff.
        P_SpawnTypeParticleGens();
        return;
    }
    if(flags & DDSLF_FINALIZE)
    {
        if(loadInStartupMode)
            Con_StartupDone();

        // Recalculate the light range mod matrix.
        Rend_CalcLightRangeModMatrix(NULL);

        // Update all sectors. Set intial values of various tracked
        // and interpolated properties (lighting, smoothed planes etc).
        for(i = 0; i < numsectors; ++i)
            R_UpdateSector(SECTOR_PTR(i), true);

        // Run any commands specified in Map Info.
        if(mapinfo && mapinfo->execute)
            Con_Execute(CMDS_DED, mapinfo->execute, true);

        // The level setup has been completed.  Run the special level
        // setup command, which the user may alias to do something
        // useful.
        if(level_id && level_id[0])
        {
            char    cmd[80];

            sprintf(cmd, "init-%s", level_id);
            if(Con_IsValidCommand(cmd))
            {
                Con_Executef(CMDS_DED, false, cmd);
            }
        }

        // Clear any input events that might have accumulated during the
        // setup period.
        DD_ClearEvents();

        // Now that the setup is done, let's reset the tictimer so it'll
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Kill all local commands.
        for(i = 0; i < MAXPLAYERS; i++)
        {
            clients[i].numTics = 0;
        }

        // Reset the level tick timer.
        levelTime = 0;

        // We've finished setting up the level
        levelSetup = false;

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;

        // Switch back to normal malloc mode in the zone. Z_Malloc will look
        // for free blocks in the entire zone and purge purgable blocks.
        Z_EnableFastMalloc(false);
        return;
    }

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(i = 0; i < MAXPLAYERS; i++)
        {
            if(!(players[i].flags & DDPF_LOCAL) && clients[i].connected)
            {
#ifdef _DEBUG
                Con_Printf("Cl%i NOT READY ANY MORE!\n", i);
#endif
                clients[i].ready = false;
            }
        }
    }

    Con_InitProgress("Setting up level...", 100);
    strcpy(currentLevelId, level_id);

    Con_Progress(10, 0);

    // Polygonize.
    if(flags & DDSLF_POLYGONIZE)
    {
        if(flags & DDSLF_DONT_CLIP)
            R_PolygonizeWithoutCarving();
        else
            R_CreateFloorsAndCeilings(numnodes - 1, 0, NULL);
    }
    Con_Progress(10, 0);

    // Init Particle Generator links.
    PG_InitForLevel();

    // Make sure subsector floors and ceilings will be rendered
    // correctly.
    R_SubsectorPlanes();

    // The map bounding box will be updated during sector info
    // initialization.
    memset(mapBounds, 0, sizeof(mapBounds));
    R_InitSectorInfo();

    R_InitSegInfo();
    R_InitSubsectorInfo();
    R_InitLineInfo();

    // Compose the vertex owners array.
    R_InitVertexOwners();

    // Init blockmap for searching subsectors.
    P_InitSubsectorBlockMap();

    R_RationalizeSectors();
    R_InitLineNeighbors();  // Must follow R_RationalizeSectors.
    R_InitSectorShadows();

    Con_Progress(10, 0);

    if(flags & DDSLF_FIX_SKY)
        R_SkyFix();
    if(flags & DDSLF_REVERB)
        S_CalcSectorReverbs();

    DL_InitLinks();

    Cl_Reset();
    RL_DeleteLists();
    GL_DeleteRawImages();
    Con_Progress(10, 0);

    // See what mapinfo says about this level.
    mapinfo = Def_GetMapInfo(level_id);
    if(!mapinfo)
        mapinfo = Def_GetMapInfo("*");

    // Setup accordingly.
    R_SetupFog();
    R_SetupSky();

    if(mapinfo)
    {
        mapgravity = mapinfo->gravity * FRACUNIT;
        r_ambient = mapinfo->ambient * 255;
    }
    else
    {
        // No map info found, set some basic stuff.
        mapgravity = FRACUNIT;
        r_ambient = 0;
    }

    // Invalidate old cmds.
    if(isServer)
    {
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].ingame)
                clients[i].runTime = SECONDS_TO_TICKS(gameTime);
    }

    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    P_SpawnTypeParticleGens();
    P_SpawnMapParticleGens(level_id);

    // Make sure that the next frame doesn't use a filtered viewer.
    R_ResetViewer();

    // Texture animations should begin from their first step.
    R_ResetAnimGroups();

    // Do network init: Initialize Delta Pools.
    if(!(flags & DDSLF_NO_SERVER))
    {
        Sv_InitPools();
    }

    // Tell shadow bias to initialize the bias light sources.
    SB_InitForLevel(R_GetUniqueLevelID());

    // Initialize the lighting grid.
    LG_Init();

    Con_Progress(10, 0);        // 50%.
}

void R_ClearSectorFlags(void)
{
    int     i;
    sectorinfo_t *sinf;

    for(i = 0, sinf = secinfo; i < numsectors; i++, sinf++)
    {
        // Clear all flags that can be cleared before each frame.
        sinf->flags &= ~SIF_FRAME_CLEAR;
    }
}

sector_t *R_GetLinkedSector(sector_t *startsec, int plane)
{
    sector_t *sec = startsec;
    sector_t *link;
    sectorinfo_t *sin;

    for(;;)
    {
        sin = SECT_INFO(sec);

        if(!sin->planeinfo[plane].linked)
            return sec;
        link = sin->planeinfo[plane].linked;

#ifdef _DEBUG
        if(sec == link || startsec == link)
        {
            Con_Error("R_GetLinkedSector: linked to self! (%s)\n",
                      (plane == PLN_FLOOR ? "flr" :
                       plane == PLN_CEILING ? "ceil" : "mid"));
            return startsec;
        }
#endif
        sec = link;
    }
}

void R_UpdateSector(sector_t* sec, boolean forceUpdate)
{
    int i, j;
    int setGlow[NUM_PLANES];
    sectorinfo_t *sin = SECT_INFO(sec);

    // Check if there are any lightlevel or color changes.
    if(forceUpdate ||
       (sec->lightlevel != sin->oldlightlevel ||
        sec->rgb[0] != sin->oldrgb[0] ||
        sec->rgb[1] != sin->oldrgb[1] ||
        sec->rgb[2] != sin->oldrgb[2]))
    {
        sin->flags |= SIF_LIGHT_CHANGED;
        sin->oldlightlevel = sec->lightlevel;
        memcpy(sin->oldrgb, sec->rgb, 3);

        LG_SectorChanged(sec, sin);
    }
    else
    {
        sin->flags &= ~SIF_LIGHT_CHANGED;
    }

    // For each plane.
    for(i = 0; i < NUM_PLANES; ++i)
    {
        // Surface color change?
        // TODO: when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        if(forceUpdate ||
           (sec->planes[i].surface.rgba[0] != sin->planeinfo[i].oldrgb[0] ||
            sec->planes[i].surface.rgba[1] != sin->planeinfo[i].oldrgb[1] ||
            sec->planes[i].surface.rgba[2] != sin->planeinfo[i].oldrgb[2]))
        {
            memcpy(sin->planeinfo[i].oldrgb, sec->planes[i].surface.rgba, 3);
        }

        // Geometry change?
        if(forceUpdate ||
           sec->planes[i].height != sin->planeinfo[i].oldheight[1])
        {
            ddplayer_t *player;

            // Check if there are any camera players in this sector. If their
            // height is now above the ceiling/below the floor they are now in
            // the void.
            for(j = 0; j < MAXPLAYERS; ++j)
            {
                player = &players[j];
                if(!player->ingame || !player->mo || !player->mo->subsector)
                    continue;

                if((player->flags & DDPF_CAMERA) && player->mo->subsector->sector == sec &&
                   (player->mo->pos[VZ] > sec->SP_ceilheight ||
                    player->mo->pos[VZ] < sec->SP_floorheight))
                {
                    player->invoid = true;
                }
            }

            P_PlaneChanged(sec, i);
        }

        // Update glowing status?
        // FIXME

        // Any change to the texture or glow properties?
        // TODO: Implement Decoration{ Glow{}} definitions.
        setGlow[i] = 0;
        // The order of these tests is important.
        if(forceUpdate || (sec->planes[i].surface.texture != sin->planeinfo[i].oldpic))
        {
            // Check if the new texture is declared as glowing.
            // NOTE: Currently, we always discard the glow settings of the
            //       previous flat after a texture change.

            // Now that glows are properties of the sector this does not
            // need to be the case. If we expose these properties via DMU
            // they could be changed at any time. However in order to support
            // flats that are declared as glowing we would need some method
            // of telling Doomsday IF we want to inherit these properties when
            // the plane flat changes...
            if(R_GraphicResourceFlags(RC_FLAT, sec->planes[i].surface.texture) & TXF_GLOW)
            {
                // The new texture is glowing.

                // Default height factor is 4
                sec->planes[i].glow = 4;

                // Always use the average colour.
                GL_GetFlatColor(sec->planes[i].surface.texture, sec->planes[i].glowrgb);

                // Do we need to update the plane glow flags?
                if(sin->planeinfo[i].oldpic)
                {
                    if(!(R_GraphicResourceFlags(RC_FLAT, sin->planeinfo[i].oldpic) & TXF_GLOW))
                        setGlow[i] = 1; // Turn the subsector plane glow flags on
                }
                else
                    setGlow[i] = 1;
            }
            else if(sin->planeinfo[i].oldpic && (R_GraphicResourceFlags(RC_FLAT, sin->planeinfo[i].oldpic) & TXF_GLOW))
            {
                // The old texture was glowing but the new one is not.
                // Clear the glow properties for this plane.
                sec->planes[i].glow = 0;
                memset(sec->planes[i].glowrgb, 0, 3);

                setGlow[i] = -1; // Turn the subsector plane glow flags off
            }

            sin->planeinfo[i].oldpic = sec->planes[i].surface.texture;
        }
        else if((R_GraphicResourceFlags(RC_FLAT, sec->planes[i].surface.texture) & TXF_GLOW) != (sec->planes[i].glow != 0))
        {
            // The glow property of the current flat has been changed
            // since last update.

            // NOTE:
            // This approach is hardly optimal but since flats will
            // rarely/if ever have this property changed during normal
            // gameplay (the RENDER_GLOWFLATS text string is depreciated and
            // the only time this property might change is after a console
            // RESET) so it doesn't matter.
            if(!(R_GraphicResourceFlags(RC_FLAT, sec->planes[i].surface.texture) & TXF_GLOW) && sec->planes[i].glow != 0)
            {
                // The current flat is no longer glowing
                sec->planes[i].glow = 0;
                memset(sec->planes[i].glowrgb, 0, 3);
                setGlow[i] = -1; // Turn the subsector plane glow flags off
            }
            else if((R_GraphicResourceFlags(RC_FLAT, sec->planes[i].surface.texture) & TXF_GLOW) && sec->planes[i].glow == 0)
            {
                // The current flat is now glowing
                sec->planes[i].glow = 4;
                GL_GetFlatColor(sec->planes[i].surface.texture, sec->planes[i].glowrgb);
                setGlow[i] = 1; // Turn the subsector plane glow flags on
            }
        }
    }

    // Do we need to update the subsector plane glow flags?
    if(setGlow[PLN_FLOOR] != 0 || setGlow[PLN_CEILING] != 0)
    {
        for(i = 0; i < sec->subscount; ++i)
        {
            subsector_t *sub = sec->subsectors[i];
            subsectorinfo_t *subInfo = SUBSECT_INFO(sub);

            // Update the plane's glow flag?
            // For each plane
            for(j = 0; j < NUM_PLANES; ++j)
            {
                if(setGlow[j] == 1)
                    subInfo->plane[j].flags |= RPF_GLOW;
                else if(setGlow[j] == -1)
                    subInfo->plane[j].flags &= ~RPF_GLOW;
            }
        }
    }

    if(!sin->permanentlink) // Assign new links
    {
        // For each plane
        for(i = 0; i < NUM_PLANES; ++i)
            sin->planeinfo[i].linked = NULL;
        R_SetSectorLinks(sec);
    }
}

/*
 * All links will be updated every frame (sectorheights may change at
 * any time without notice).
 */
void R_UpdatePlanes(void)
{
    // Nothing to do.
}

/*
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const char *R_GetCurrentLevelID(void)
{
    return currentLevelId;
}

/*
 * Return the 'unique' identifier of the map.  This identifier
 * contains information about the map tag (E3M3), the WAD that
 * contains the map (DOOM.IWAD), and the game mode (doom-ultimate).
 *
 * The entire ID string will be in lowercase letters.
 */
const char *R_GetUniqueLevelID(void)
{
    static char uid[256];
    filename_t base;
    int lump = W_GetNumForName((char*)R_GetCurrentLevelID());

    M_ExtractFileBase(W_LumpSourceFile(lump), base);

    sprintf(uid, "%s|%s|%s|%s",
            R_GetCurrentLevelID(),
            base, (W_IsFromIWAD(lump) ? "iwad" : "pwad"),
            (char *) gx.GetVariable(DD_GAME_MODE));

    strlwr(uid);
    return uid;
}

/*
 * Sector light color may be affected by the sky light color.
 */
const byte *R_GetSectorLightColor(sector_t *sector)
{
    sector_t *src;
    int     i;

    if(!rendSkyLight || noSkyColorGiven)
        return sector->rgb;     // The sector's real color.

    if(!R_IsSkySurface(&sector->SP_ceilsurface) && !R_IsSkySurface(&sector->SP_floorsurface))
    {
        // A dominant light source affects this sector?
        src = SECT_INFO(sector)->lightsource;
        if(src && src->lightlevel >= sector->lightlevel)
        {
            // The color shines here, too.
            return R_GetSectorLightColor(src);
        }

        // Return the sector's real color (balanced against sky's).
        if(skyColorBalance >= 1)
        {
            return sector->rgb;
        }
        else
        {
            for(i = 0; i < 3; i++)
                balancedRGB[i] = (byte) (sector->rgb[i] * skyColorBalance);
            return balancedRGB;
        }
    }
    // Return the sky color.
    return skyColorRGB;
}

/*
 * Calculate the size of the entire map.
 */
void R_GetMapSize(vertex_t *min, vertex_t *max)
{
/*    byte *ptr;
    int i;
    fixed_t x;
    fixed_t y;

    memcpy(min, vertexes, sizeof(min));
    memcpy(max, vertexes, sizeof(max));

    for(i = 1, ptr = vertexes + VTXSIZE; i < numvertexes; i++, ptr += VTXSIZE)
    {
        x = ((vertex_t *) ptr)->x;
        y = ((vertex_t *) ptr)->y;

        if(x < min->x)
            min->x = x;
        if(x > max->x)
            max->x = x;
        if(y < min->y)
            min->y = y;
        if(y > max->y)
            max->y = y;
            }*/

    min->x = FRACUNIT * mapBounds[BLEFT];
    min->y = FRACUNIT * mapBounds[BTOP];

    max->x = FRACUNIT * mapBounds[BRIGHT];
    max->y = FRACUNIT * mapBounds[BBOTTOM];
}
