/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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

static void    R_PrepareSubsector(subsector_t *sub);
static void    R_FindLineNeighbors(sector_t *sector, line_t *line, int side,
                                   struct line_s **neighbors, int alignment);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     rendSkyLight = 1;       // cvar

char    currentLevelId[64];

boolean firstFrameAfterLoad;
boolean levelSetup;

nodeindex_t     *linelinks;         // indices to roots

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static vertex_t *rootVtx; // used when sorting vertex line owners.

static boolean noSkyColorGiven;
static float skyColorRGB[4], balancedRGB[4];
static float skyColorBalance;
static float mapBounds[4];

// CODE --------------------------------------------------------------------

#ifdef MSVC
#  pragma optimize("g", off)
#endif

/**
 * Called whenever the sector changes.
 *
 * This routine handles plane hacks where all of the sector's
 * lines are twosided and missing upper or lower textures.
 *
 * NOTE: This does not support sectors with disjoint groups of
 *       lines (eg a sector with a "control" sector such as the
 *       forcefields in ETERNAL.WAD MAP01).
 */
static void R_SetSectorLinks(sector_t *sec)
{
    uint        i, j, k;
    sector_t   *back;
    line_t     *lin;
    boolean     hackfloor, hackceil;
    side_t     *sid, *frontsid, *backsid;
    sector_t   *floorlink_candidate = 0, *ceillink_candidate = 0;
    seg_t      *seg;
    subsector_t *sub;
    ssecgroup_t *ssgrp;

    // Must have a valid sector!
    if(!sec || !sec->linecount || sec->permanentlink)
        return;                 // Can't touch permanent links.

    hackfloor = (!R_IsSkySurface(&sec->SP_floorsurface));
    hackceil = (!R_IsSkySurface(&sec->SP_ceilsurface));
    if(hackfloor || hackceil)
    for(i = 0; i < sec->subsgroupcount; ++i)
    {
        if(!hackfloor && !hackceil)
            break;

        ssgrp = &sec->subsgroups[i];
        for(k = 0; k < sec->subscount; ++k)
        {
            if(!hackfloor && !hackceil)
                break;

            sub = sec->subsectors[k];
            // Must be in the same group.
            if(sub->group != i)
                continue;

            for(j = 0, seg = sub->firstseg; j < sub->segcount; ++j, seg++)
            {
                if(!hackfloor && !hackceil)
                    break;

                lin = seg->linedef;

                if(!lin)
                    continue; // minisegs don't count.

                // We are only interested in two-sided lines.
                if(!(lin->L_frontside && lin->L_backside))
                    continue;

                // Check the vertex line owners for both verts.
                // We are only interested in lines that do NOT share either vertex
                // with a one-sided line (ie, its not "anchored").
                if(lin->L_v1->anchored || lin->L_v2->anchored)
                    return;

                // Check which way the line is facing.
                sid = lin->L_frontside;
                if(sid->sector == sec)
                {
                    frontsid = sid;
                    backsid = lin->L_backside;
                }
                else
                {
                    frontsid = lin->L_backside;
                    backsid = sid;
                }
                back = backsid->sector;
                if(back == sec)
                    return;

                // Check that there is something on the other side.
                if(back->SP_ceilheight == back->SP_floorheight)
                    return;
                // Check the conditions that prevent the invis plane.
                if(back->SP_floorheight == sec->SP_floorheight)
                {
                    hackfloor = false;
                }
                else
                {
                    if(back->SP_floorheight > sec->SP_floorheight)
                        sid = frontsid;
                    else
                        sid = backsid;

                    if((sid->SW_bottomtexture && !(sid->SW_bottomflags & SUF_TEXFIX)) ||
                       (sid->SW_middletexture && !(sid->SW_middleflags & SUF_TEXFIX)))
                        hackfloor = false;
                    else
                        floorlink_candidate = back;
                }

                if(back->SP_ceilheight == sec->SP_ceilheight)
                    hackceil = false;
                else
                {
                    if(back->SP_ceilheight < sec->SP_ceilheight)
                        sid = frontsid;
                    else
                        sid = backsid;

                    if((sid->SW_toptexture && !(sid->SW_topflags & SUF_TEXFIX)) ||
                       (sid->SW_middletexture && !(sid->SW_middleflags & SUF_TEXFIX)))
                        hackceil = false;
                    else
                        ceillink_candidate = back;
                }
            }
        }
        if(hackfloor)
        {
            if(floorlink_candidate == sec->containsector)
                ssgrp->linked[PLN_FLOOR] = floorlink_candidate;

            /*      if(floorlink_candidate)
               Con_Printf("LF:%i->%i\n",
               i, GET_SECTOR_IDX(floorlink_candidate)); */
        }
        if(hackceil)
        {
            if(ceillink_candidate == sec->containsector)
                ssgrp->linked[PLN_CEILING] = ceillink_candidate;

            /*      if(ceillink_candidate)
               Con_Printf("LC:%i->%i\n",
               i, GET_SECTOR_IDX(ceillink_candidate)); */
        }
    }
}

#ifdef MSVC
#  pragma optimize("", on)
#endif

/**
 * Initialize the skyfix. In practice all this does is to check for things
 * intersecting ceilings and if so: raises the sky fix for the sector a
 * bit to accommodate them.
 */
static void R_InitSkyFix(void)
{
    float       f, b;
    float      *fix;
    uint        i;
    mobj_t     *it;
    sector_t   *sec;

    for(i = 0; i < numsectors; ++i)
    {
        sec = SECTOR_PTR(i);

        // Must have a sky ceiling.
        if(!R_IsSkySurface(&sec->SP_ceilsurface))
            continue;

        fix = &sec->skyfix[PLN_CEILING].offset;

        // Check that all the things in the sector fit in.
        for(it = sec->thinglist; it; it = it->snext)
        {
            b = it->height;
            f = sec->SP_ceilheight + *fix -
                sec->SP_floorheight;

            if(b > f)
            {   // Must increase skyfix.
                *fix += b - f;

                if(verbose)
                {
                    Con_Printf("S%i: (mo)skyfix to %g (ceil=%g)\n",
                               GET_SECTOR_IDX(sec), *fix,
                               sec->SP_ceilheight + *fix);
                }
            }
        }
    }
}

static boolean doSkyFix(sector_t *front, sector_t *back, uint pln)
{
    float       f, b;
    float       height = 0;
    boolean     adjusted = false;
    float      *fix = NULL;
    sector_t   *adjustSec = NULL;

    // Both the front and back surfaces must be sky on this plane.
    if(!R_IsSkySurface(&front->planes[pln]->surface) ||
       !R_IsSkySurface(&back->planes[pln]->surface))
        return false;

    f = front->planes[pln]->height + front->skyfix[pln].offset;
    b = back->planes[pln]->height + back->skyfix[pln].offset;

    if(f == b)
        return false;

    if(pln == PLN_CEILING)
    {
        if(f < b)
        {
            height = b - front->planes[pln]->height;
            fix = &front->skyfix[pln].offset;
            adjustSec = front;
        }
        else if(f > b)
        {
            height = f - back->planes[pln]->height;
            fix = &back->skyfix[pln].offset;
            adjustSec = back;
        }

        if(height > *fix)
        {   // Must increase skyfix.
           *fix = height;
            adjusted = true;
        }
    }
    else // its the floor.
    {
        if(f > b)
        {
            height = b - front->planes[pln]->height;
            fix = &front->skyfix[pln].offset;
            adjustSec = front;
        }
        else if(f < b)
        {
            height = f - back->planes[pln]->height;
            fix = &back->skyfix[pln].offset;
            adjustSec = back;
        }

        if(height < *fix)
        {   // Must increase skyfix.
           *fix = height;
            adjusted = true;
        }
    }

    if(verbose && adjusted)
    {
        Con_Printf("S%i: skyfix to %g (%s=%g)\n",
                   GET_SECTOR_IDX(adjustSec), *fix,
                   (pln == PLN_CEILING? "ceil" : "floor"),
                   adjustSec->planes[pln]->height + *fix);
    }

    return adjusted;
}

static void spreadSkyFixForNeighbors(vertex_t *vtx, line_t *refLine,
                                     boolean fixFloors, boolean fixCeilings,
                                     boolean *adjustedFloor,
                                     boolean *adjustedCeil)
{
    uint        pln;
    lineowner_t *base, *lOwner, *rOwner;
    boolean     doFix[2];
    boolean    *adjusted[2];

    doFix[PLN_FLOOR]   = fixFloors;
    doFix[PLN_CEILING] = fixCeilings;
    adjusted[PLN_FLOOR] = adjustedFloor;
    adjusted[PLN_CEILING] = adjustedCeil;

    // Find the reference line in the owner list.
    base = R_GetVtxLineOwner(vtx, refLine);

    // Spread will begin from the next line anti-clockwise.
    lOwner = base->LO_prev;

    // Spread clockwise around this vertex from the reference plus one
    // until we reach the reference again OR a single sided line.
    rOwner = base->LO_next;
    do
    {
        if(rOwner != lOwner)
        {
            for(pln = 0; pln < 2; ++pln)
            {
                if(!doFix[pln])
                    continue;

                if(doSkyFix(rOwner->line->L_frontsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(lOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_frontsector,
                            lOwner->line->L_backsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_backsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->line->L_backside && lOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_backsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;
            }
        }

        if(!rOwner->line->L_backside)
            break;

        rOwner = rOwner->LO_next;
    } while(rOwner != base);

    // Spread will begin from the next line clockwise.
    rOwner = base->LO_next;

    // Spread anti-clockwise around this vertex from the reference minus one
    // until we reach the reference again OR a single sided line.
    lOwner = base->LO_prev;
    do
    {
        if(rOwner != lOwner)
        {
            for(pln = 0; pln < 2; ++pln)
            {
                if(!doFix[pln])
                    continue;

                if(doSkyFix(rOwner->line->L_frontsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(lOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_frontsector,
                            lOwner->line->L_backsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_backsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->line->L_backside && lOwner->line->L_backside)
                if(doSkyFix(rOwner->line->L_backsector,
                            lOwner->line->L_frontsector, pln))
                    *adjusted[pln] = true;
            }
        }

        if(!lOwner->line->L_backside)
            break;

        lOwner = lOwner->LO_prev;
    } while(lOwner != base);
}

/**
 * Fixing the sky means that for adjacent sky sectors the lower sky
 * ceiling is lifted to match the upper sky. The raising only affects
 * rendering, it has no bearing on gameplay.
 */
void R_SkyFix(boolean fixFloors, boolean fixCeilings)
{
    uint        i, pln;
    boolean     adjusted[2], doFix[2];

    if(!fixFloors && !fixCeilings)
        return; // Why are we here?

    doFix[PLN_FLOOR]   = fixFloors;
    doFix[PLN_CEILING] = fixCeilings;

    // We'll do this as long as we must to be sure all the sectors are fixed.
    // Do both floors and ceilings at the same time.
    do
    {
        adjusted[PLN_FLOOR] = adjusted[PLN_CEILING] = false;

        // We need to check all the linedefs.
        for(i = 0; i < numlines; ++i)
        {
            line_t     *line = LINE_PTR(i);
            sector_t   *front = (line->L_frontside? line->L_frontsector : NULL);
            sector_t   *back = (line->L_backside? line->L_backsector : NULL);

            // The conditions: must have two sides.
            if(!front || !back)
                continue;

            if(front != back) // its a normal two sided line
            {
                // Perform the skyfix as usual using the front and back sectors
                // of THIS line for comparing.
                for(pln = 0; pln < 2; ++pln)
                {
                    if(doFix[pln])
                        if(doSkyFix(front, back, pln))
                            adjusted[pln] = true;
                }
            }
            else if(line->selfrefhackroot)
            {
                // Its a selfreferencing hack line. These will ALWAYS return
                // the same height on the front and back so we need to find the
                // neighbouring lines either side of this and compare the front
                // and back sectors of those instead.
                uint        j;

                // Walk around each vertex in each direction.
                for(j = 0; j < 2; ++j)
                    if(line->v[j]->numlineowners > 1)
                        spreadSkyFixForNeighbors(line->v[j], line,
                                                 doFix[PLN_FLOOR],
                                                 doFix[PLN_CEILING],
                                                 &adjusted[PLN_FLOOR],
                                                 &adjusted[PLN_CEILING]);
            }
        }
    }
    while(adjusted[PLN_FLOOR] || adjusted[PLN_CEILING]);
}

static void R_PrepareSubsector(subsector_t *sub)
{
    uint        j, num = sub->numverts;
    fvertex_t  *vtx = sub->verts;

    // Find the center point. First calculate the bounding box.
    sub->bbox[0].pos[VX] = sub->bbox[1].pos[VX] = sub->midpoint.pos[VX] = vtx->pos[VX];
    sub->bbox[0].pos[VY] = sub->bbox[1].pos[VY] = sub->midpoint.pos[VY] = vtx->pos[VY];

    for(j = 1, vtx++; j < num; ++j, vtx++)
    {
        if(vtx->pos[VX] < sub->bbox[0].pos[VX])
            sub->bbox[0].pos[VX] = vtx->pos[VX];
        if(vtx->pos[VY] < sub->bbox[0].pos[VY])
            sub->bbox[0].pos[VY] = vtx->pos[VY];
        if(vtx->pos[VX] > sub->bbox[1].pos[VX])
            sub->bbox[1].pos[VX] = vtx->pos[VX];
        if(vtx->pos[VY] > sub->bbox[1].pos[VY])
            sub->bbox[1].pos[VY] = vtx->pos[VY];

        sub->midpoint.pos[VX] += vtx->pos[VX];
        sub->midpoint.pos[VY] += vtx->pos[VY];
    }
    sub->midpoint.pos[VX] /= num;
    sub->midpoint.pos[VY] /= num;
}

static void R_PolygonizeWithoutCarving(void)
{
    uint        startTime = Sys_GetRealTime();

    uint        i, j, num;
    fvertex_t  *vtx;
    subsector_t *sub;
    seg_t      *seg;

    for(i = 0; i < numsubsectors; ++i)
    {
        sub = SUBSECTOR_PTR(i);
        num = sub->numverts = sub->segcount;
        vtx = sub->verts =
            Z_Malloc(sizeof(fvertex_t) * sub->segcount, PU_LEVELSTATIC, 0);

        for(j = 0, seg = sub->firstseg; j < num; ++j, seg++, vtx++)
        {
            vtx->pos[VX] = seg->SG_v1->pos[VX];
            vtx->pos[VY] = seg->SG_v1->pos[VY];
        }

        R_PrepareSubsector(sub);
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_PolygonizeWithoutCarving: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

/**
 * Returns a pointer to the list of points. It must be used.
 */
static fvertex_t *edgeClipper(uint *numpoints, fvertex_t *points,
                              uint numclippers, fdivline_t *clippers)
{
    unsigned char *sidelist;
    uint        i, k, num = *numpoints, sidelistSize = 64;

    sidelist = M_Malloc(sizeof(unsigned char) * sidelistSize);

    // We'll clip the polygon with each of the divlines. The left side of
    // each divline is discarded.
    for(i = 0; i < numclippers; ++i)
    {
        fdivline_t *curclip = clippers + i;

        // First we'll determine the side of each vertex. Points are allowed
        // to be on the line.
        for(k = 0; k < num; ++k)
        {
            sidelist[k] = P_FloatPointOnLineSide(points + k, curclip);
        }

        for(k = 0; k < num; ++k)
        {
            uint    startIdx = k, endIdx = k + 1;

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
                    (fvertex_t *) M_Realloc(points, (++num) * sizeof(fvertex_t));
                if(num >= sidelistSize)
                {
                    sidelistSize *= 2;
                    sidelist = M_Realloc(sidelist, sizeof(unsigned char) * sidelistSize);
                }

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
        for(k = 0; k < num; ++k)
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
    for(i = 0; i < num; ++i)
    {
        long    previdx = i - 1;

        if(previdx < 0)
            previdx = num - 1;
        if(points[i].pos[VX] == points[previdx].pos[VX] &&
           points[i].pos[VY] == points[previdx].pos[VY])
        {
            // This point (i) must be removed.
            memmove(points + i, points + i + 1,
                    sizeof(fvertex_t) * (num - i - 1));
            num--;
            i--;
        }
    }
    *numpoints = num;

    M_Free(sidelist);
    return points;
}

static void R_ConvexClipper(subsector_t *ssec, uint num, fdivline_t *list)
{
    uint        i, numedgepoints;
    uint        numclippers = num + ssec->segcount;
    fvertex_t  *edgepoints;
    fdivline_t *clippers, *clip;

    clippers = M_Malloc(numclippers * sizeof(fdivline_t));

    // Reverse the order.
    for(i = 0, clip = clippers; i < numclippers; clip++, ++i)
    {
        if(i < num)
        {
            clip->pos[VX] = list[num - i - 1].pos[VX];
            clip->pos[VY] = list[num - i - 1].pos[VY];
            clip->dx = list[num - i - 1].dx;
            clip->dy = list[num - i - 1].dy;
        }
        else
        {
            seg_t  *seg = (ssec->firstseg + i - num);

            clip->pos[VX] = seg->SG_v1->pos[VX];
            clip->pos[VY] = seg->SG_v1->pos[VY];
            clip->dx = seg->SG_v2->pos[VX] - seg->SG_v1->pos[VX];
            clip->dy = seg->SG_v2->pos[VY] - seg->SG_v1->pos[VY];
        }
    }

    // Setup the 'worldwide' polygon.
    numedgepoints = 4;
    edgepoints = M_Malloc(numedgepoints * sizeof(fvertex_t));

    edgepoints[0].pos[VX] = -32768;
    edgepoints[0].pos[VY] = 32768;

    edgepoints[1].pos[VX] = 32768;
    edgepoints[1].pos[VY] = 32768;

    edgepoints[2].pos[VX] = 32768;
    edgepoints[2].pos[VY] = -32768;

    edgepoints[3].pos[VX] = -32768;
    edgepoints[3].pos[VY] = -32768;

    // Do some clipping, <snip> <snip>
    edgepoints = edgeClipper(&numedgepoints, edgepoints, numclippers, clippers);

    if(!numedgepoints)
    {
        int idx = GET_SUBSECTOR_IDX(ssec);

        printf("All clipped away: subsector %i\n", idx);
        ssec->numverts = 0;
        ssec->verts = 0;
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
    M_Free(edgepoints);
    M_Free(clippers);
}

/**
 * Recursively polygonizes all ceilings and floors.
 */
void R_CreateFloorsAndCeilings(uint bspnode, uint numdivlines,
                               fdivline_t *divlines)
{
    node_t     *nod;
    fdivline_t *childlist, *dl;
    uint        childlistsize = numdivlines + 1;

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
    childlist = M_Malloc(childlistsize * sizeof(fdivline_t));

    // Copy the previous lines, from the parent nodes.
    if(divlines)
        memcpy(childlist, divlines, numdivlines * sizeof(fdivline_t));

    dl = childlist + numdivlines;
    dl->pos[VX] = nod->x;
    dl->pos[VY] = nod->y;
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
    M_Free(childlist);
}

static float TriangleArea(fvertex_t *o, fvertex_t *s, fvertex_t *t)
{
    fvertex_t a, b;
    float   area;

    a.pos[VX] = s->pos[VX] - o->pos[VX];
    a.pos[VY] = s->pos[VY] - o->pos[VY];

    b.pos[VX] = t->pos[VX] - o->pos[VX];
    b.pos[VY] = t->pos[VY] - o->pos[VY];

    area = (a.pos[VX] * b.pos[VY] - b.pos[VX] * a.pos[VY]) / 2;

    if(area < 0)
        return -area;
    else
        return area;
}

/**
 * Returns true if 'base' is a good tri-fan base.
 */
static boolean R_TestTriFan(subsector_t *sub, uint base, uint num)
{
#define TRIFAN_LIMIT    0.1
    uint        i, a, b;
    fvertex_t  *verts = sub->verts;

    if(num == 3)
        return true;            // They're all valid.

    // Higher vertex counts need checking.
    for(i = 0; i < num - 2; ++i)
    {
        a = base + 1 + i;
        b = a + 1;

        if(a >= num) a -= num;
        if(b >= num) b -= num;

        if(TriangleArea(&verts[base], &verts[a], &verts[b]) <= TRIFAN_LIMIT)
            return false;
    }

    // Whole triangle fan checked out OK, must be good.
    return true;
#undef TRIFAN_LIMIT
}

static void R_SubsectorPlanes(void)
{
    uint        i, k, num, bufSize = 64;
    subsector_t *sub;
    fvertex_t  *verts;
    fvertex_t  *vbuf;
    size_t      size = sizeof(fvertex_t);
    boolean     valid;

    vbuf = M_Malloc(size * bufSize);

    for(i = 0, sub = subsectors; i < numsubsectors; sub++, ++i)
    {
        num = sub->numverts;
        verts = sub->verts;

        if(num >= bufSize - 2)
        {
            bufSize = num;
            vbuf = M_Realloc(vbuf, size * (bufSize));
        }

        // We need to find a good tri-fan base vertex.
        // (One that doesn't generate zero-area triangles).
        // We'll test each one and pick the first good one.
        valid = false;
        for(k = 0; k < num; ++k)
        {
            if(R_TestTriFan(sub, k, num))
            {
                // Yes! This'll do nicely. Change the order of the
                // vertices so that k comes first.
                if(k)           // Need to change?
                {
                    memcpy(vbuf, verts, size * num);
                    memcpy(verts, &vbuf[k], size * (num - k));
                    memcpy(&verts[num - k], vbuf, size * k);
                }
                valid = true;
                break;
            }
        }

        if(!valid)
        {
            // There was no match. Bugger. We need to use the subsector
            // midpoint as the base. It's always valid.
            sub->flags |= DDSUBF_MIDPOINT;
            //Con_Message("Using midpoint for subsctr %i.\n", i);
        }
    }

    M_Free(vbuf);
}

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int C_DECL lineAngleSorter(const void *a, const void *b)
{
    uint        i;
    fixed_t     dx, dy;
    binangle_t  angles[2];
    lineowner_t *own[2];
    line_t     *line;

    own[0] = (lineowner_t *)a;
    own[1] = (lineowner_t *)b;
    for(i = 0; i < 2; ++i)
    {
        if(own[i]->LO_prev) // We have a cached result.
        {
            angles[i] = own[i]->angle;
        }
        else
        {
            line = own[i]->line;

            if(line->L_v1 == rootVtx)
            {
                dx = line->L_v2->pos[VX] - rootVtx->pos[VX];
                dy = line->L_v2->pos[VY] - rootVtx->pos[VY];
            }
            else
            {
                dx = line->L_v1->pos[VX] - rootVtx->pos[VX];
                dy = line->L_v1->pos[VY] - rootVtx->pos[VY];
            }

            own[i]->angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->LO_prev = (lineowner_t*) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return          Ptr to the newly merged list.
 */
static lineowner_t *mergeLineOwners(lineowner_t *left, lineowner_t *right,
                                    int (C_DECL *compare) (const void *a,
                                                           const void *b))
{
    lineowner_t tmp, *np;

    np = &tmp;
    tmp.LO_next = np;
    while(left != NULL && right != NULL)
    {
        if(compare(left, right) <= 0)
        {
            np->LO_next = left;
            np = left;

            left = left->LO_next;
        }
        else
        {
            np->LO_next = right;
            np = right;

            right = right->LO_next;
        }
    }

    // At least one of these lists is now empty.
    if(left)
        np->LO_next = left;
    if(right)
        np->LO_next = right;

    // Is the list empty?
    if(tmp.LO_next == &tmp)
        return NULL;

    return tmp.LO_next;
}

static lineowner_t *splitLineOwners(lineowner_t *list)
{
    lineowner_t *lista, *listb, *listc;

    if(!list)
        return NULL;

    lista = listb = listc = list;
    do
    {
        listc = listb;
        listb = listb->LO_next;
        lista = lista->LO_next;
        if(lista != NULL)
            lista = lista->LO_next;
    } while(lista);

    listc->LO_next = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static lineowner_t *sortLineOwners(lineowner_t *list,
                                   int (C_DECL *compare) (const void *a,
                                                          const void *b))
{
    lineowner_t *p;

    if(list && list->LO_next)
    {
        p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void R_SetVertexLineOwner(vertex_t *vtx, line_t *lineptr,
                                 lineowner_t **storage)
{
    lineowner_t *p, *newOwner;

    if(!lineptr)
        return;

    // If this is a one-sided line then this is an "anchored" vertex.
    if(!(lineptr->L_frontside && lineptr->L_backside))
        vtx->anchored = true;

    // Has this line already been registered with this vertex?
    if(vtx->numlineowners != 0)
    {
        p = vtx->lineowners;
        while(p)
        {
            if(p->line == lineptr)
                return;             // Yes, we can exit.

            p = p->LO_next;
        }
    }

    //Add a new owner.
    vtx->numlineowners++;

    newOwner = (*storage)++;
    newOwner->line = lineptr;
    newOwner->LO_prev = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->LO_next = vtx->lineowners;
    vtx->lineowners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == lineptr->L_v1)
        lineptr->L_vo1 = newOwner;
    else
        lineptr->L_vo2 = newOwner;
}

static void R_SetVertexSectorOwner(ownerlist_t *ownerList, sector_t *secptr)
{
    uint        i;
    ownernode_t *node;

    if(!secptr)
        return;

    // Has this sector been already registered?
    if(ownerList->count)
    {
        for(i = 0, node = ownerList->head; i < ownerList->count; ++i,
            node = node->next)
            if((sector_t*) node->data == secptr)
                return;             // Yes, we can exit.
    }

    // Add a new owner.
    ownerList->count++;

    node = M_Malloc(sizeof(ownernode_t));
    node->data = secptr;
    node->next = ownerList->head;
    ownerList->head = node;
}

/**
 * Generates an array of sector references for each vertex. The list
 * includes all the sectors the vertex belongs to.
 *
 * Generates an array of line references for each vertex. The list includes
 * all the lines the vertex belongs to sorted by angle, (the list is
 * arranged in clockwise order, east = 0).
 */
static void R_BuildVertexOwners(void)
{
    uint            startTime = Sys_GetRealTime();

    uint            i, k, p;
    sector_t       *sec;
    line_t         *line;
    lineowner_t    *lineOwners, *allocator;
    ownerlist_t    *vtxSecOwnerLists;

    // Allocate memory for vertex sector owner processing.
    vtxSecOwnerLists = M_Calloc(sizeof(ownerlist_t) * numvertexes);

    // We know how many vertex line owners we need (numlines * 2).
    lineOwners =
        Z_Malloc(sizeof(lineowner_t) * numlines * 2, PU_LEVELSTATIC, 0);
    allocator = lineOwners;

    for(i = 0, sec = sectors; i < numsectors; ++i, sec++)
    {
        // Traversing the line list will do fine.
        for(k = 0; k < sec->linecount; ++k)
        {
            line = sec->Lines[k];

            for(p = 0; p < 2; ++p)
            {
                uint idx = GET_VERTEX_IDX(line->v[p]);

                if(line->L_frontside)
                    R_SetVertexSectorOwner(&vtxSecOwnerLists[idx],
                                           line->L_frontsector);
                if(line->L_backside)
                    R_SetVertexSectorOwner(&vtxSecOwnerLists[idx],
                                           line->L_backsector);

                R_SetVertexLineOwner(line->v[p], line, &allocator);
            }
        }
    }

    // Now "harden" the sector owner linked lists into arrays and free as
    // we go. Also need to sort line owners and then finish the rings.
    for(i = 0; i < numvertexes; ++i)
    {
        vertex_t   *v = VERTEX_PTR(i);

        // Sector owners:
        v->numsecowners = vtxSecOwnerLists[i].count;
        if(v->numsecowners != 0)
        {
            uint       *ptr;
            ownernode_t *node, *p;

            v->secowners =
                Z_Malloc(v->numsecowners * sizeof(uint), PU_LEVELSTATIC, 0);
            for(k = 0, ptr = v->secowners, node = vtxSecOwnerLists[i].head;
                k < v->numsecowners; ++k, ptr++)
            {
                p = node->next;
                *ptr = GET_SECTOR_IDX((sector_t*) node->data);
                M_Free(node);
                node = p;
            }
        }

        // Line owners:
        if(v->numlineowners != 0)
        {
            lineowner_t *p, *last;
            binangle_t  lastAngle = 0;

            // Sort them so that they are ordered clockwise based on angle.
            rootVtx = v;
            v->lineowners =
                sortLineOwners(v->lineowners, lineAngleSorter);

            // Finish the linking job and convert to relative angles.
            // They are only singly linked atm, we need them to be doubly
            // and circularly linked.
            last = v->lineowners;
            p = last->LO_next;
            while(p)
            {
                p->LO_prev = last;

                // Convert to a relative angle between last and this.
                last->angle = last->angle - p->angle;
                lastAngle += last->angle;

                last = p;
                p = p->LO_next;
            }
            last->LO_next = v->lineowners;
            v->lineowners->LO_prev = last;

            // Set the angle of the last owner.
            last->angle = BANG_360 - lastAngle;

#if _DEBUG
{
// For checking the line owner link rings are formed correctly.
lineowner_t *base;
uint        idx;

if(verbose >= 2)
    Con_Message("Vertex #%i: line owners #%i\n", i, v->numlineowners);

p = base = v->lineowners;
idx = 0;
do
{
    if(verbose >= 2)
        Con_Message("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f\n", idx,
                    GET_LINE_IDX(p->LO_prev->line),
                    GET_LINE_IDX(p->line),
                    GET_LINE_IDX(p->LO_next->line), BANG2DEG(p->angle));

    if(p->LO_prev->LO_next != p || p->LO_next->LO_prev != p)
       Con_Error("Invalid line owner link ring!");

    p = p->LO_next;
    idx++;
} while(p != base);
}
#endif
        }
    }
    M_Free(vtxSecOwnerLists);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_BuildVertexOwners: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

/**
 * @return          Ptr to the lineowner for this line for this vertex else
 *                  <code>NULL</code>.
 */
lineowner_t* R_GetVtxLineOwner(vertex_t *vtx, line_t *line)
{
    if(vtx == line->L_v1)
        return line->L_vo1;
    else if(vtx == line->L_v2)
        return line->L_vo2;

    return NULL;
}

/*boolean DD_SubContainTest(sector_t *innersec, sector_t *outersec)
   {
   uint i, k;
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

/**
 * The test is done on subsectors.
 */
static sector_t *R_GetContainingSectorOf(sector_t *sec)
{
    uint        i;
    float       cdiff = -1, diff;
    float       inner[4], outer[4];
    sector_t   *other, *closest = NULL;

    memcpy(inner, sec->bounds, sizeof(inner));

    // Try all sectors that fit in the bounding box.
    for(i = 0, other = sectors; i < numsectors; other++, ++i)
    {
        if(!other->linecount || other->unclosed)
            continue;
        if(other == sec)
            continue;           // Don't try on self!

        memcpy(outer, other->bounds, sizeof(outer));
        if(inner[BLEFT]  >= outer[BLEFT] &&
           inner[BRIGHT] <= outer[BRIGHT] &&
           inner[BTOP]   >= outer[BTOP] &&
           inner[BBOTTOM]<= outer[BBOTTOM])
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

static void R_BuildSectorLinks(void)
{
    uint        i, k;
    sector_t   *sec, *other;
    line_t     *lin;
    boolean     dohack, unclosed;

    // Calculate bounding boxes for all sectors.
    // Check for unclosed sectors.
    for(i = 0, sec = sectors; i < numsectors; ++i, sec++)
    {
        P_SectorBoundingBox(sec, sec->bounds);

        if(i == 0)
        {
            // The first sector is used as is.
            memcpy(mapBounds, sec->bounds, sizeof(mapBounds));
        }
        else
        {
            // Expand the bounding box.
            M_JoinBoxes(mapBounds, sec->bounds);
        }

        // Detect unclosed sectors.
        unclosed = false;
        if(sec->linecount < 3)
            unclosed = true;
        else
        {
            // TODO:
            // Add algorithm to check for unclosed sectors here.
            // Perhaps have a look at glBSP.
        }

        if(unclosed)
            sec->unclosed = true;
    }

    for(i = 0, sec = sectors; i < numsectors; sec++, ++i)
    {
        if(!sec->linecount)
            continue;

        // Is this sector completely contained by another?
        sec->containsector = R_GetContainingSectorOf(sec);

        dohack = true;
        for(k = 0; k < sec->linecount; ++k)
        {
            lin = sec->Lines[k];
            if(!lin->L_frontside || !lin->L_backside ||
                lin->L_frontsector != lin->L_backsector)
            {
                dohack = false;
                break;
            }
        }

        if(dohack)
        {
            // Link all planes permanently.
            sec->permanentlink = true;
            // Only floor and ceiling can be linked, not all planes inbetween.
            for(k = 0; k < sec->subsgroupcount; ++k)
            {
                ssecgroup_t *ssgrp = &sec->subsgroups[k];
                uint        p;

                for(p = 0; p < sec->planecount; ++p)
                    ssgrp->linked[p] = sec->containsector;
            }

            Con_Printf("Linking S%i planes permanently to S%i\n", i,
                       GET_SECTOR_IDX(sec->containsector));
        }

        // Is this sector large enough to be a dominant light source?
        if(sec->lightsource == NULL &&
           (R_IsSkySurface(&sec->SP_ceilsurface) ||
            R_IsSkySurface(&sec->SP_floorsurface)) &&
           sec->bounds[BRIGHT] - sec->bounds[BLEFT] > DOMINANT_SIZE &&
           sec->bounds[BBOTTOM] - sec->bounds[BTOP] > DOMINANT_SIZE)
        {
            // All sectors touching this one will be affected.
            for(k = 0; k < sec->linecount; ++k)
            {
                lin = sec->Lines[k];
                other = lin->L_frontsector;
                if(!other || other == sec)
                {
                    if(lin->L_backside)
                    {
                        other = lin->L_backsector;
                        if(!other || other == sec)
                            continue;
                    }
                }
                other->lightsource = sec;
            }
        }
    }
}

static void R_InitPlaneIllumination(subsector_t *sub, uint planeid)
{
    uint        i, j, num;
    subplaneinfo_t *plane = sub->planes[planeid];

    num = sub->numvertices;

    plane->illumination =
        Z_Calloc(num * sizeof(vertexillum_t), PU_LEVELSTATIC, NULL);

    for(i = 0; i < num; ++i)
    {
        plane->illumination[i].flags |= VIF_STILL_UNSEEN;

        for(j = 0; j < MAX_BIAS_AFFECTED; ++j)
            plane->illumination[i].casted[j].source = -1;
    }
}

static void R_InitSubsectorPoly(subsector_t *subsector)
{
    uint        numvrts, i;
    fvertex_t  *vrts, *vtx, *pv;

    // Take the subsector's vertices.
    numvrts = subsector->numverts;
    vrts = subsector->verts;

    // Copy the vertices to the poly.
    if(subsector->flags & DDSUBF_MIDPOINT)
    {
        // Triangle fan base is the midpoint of the subsector.
        subsector->numvertices = 2 + numvrts;
        subsector->vertices =
            Z_Malloc(sizeof(fvertex_t) * subsector->numvertices, PU_LEVELSTATIC, 0);

        memcpy(subsector->vertices, &subsector->midpoint, sizeof(fvertex_t));

        vtx = vrts;
        pv = subsector->vertices + 1;
    }
    else
    {
        subsector->numvertices = numvrts;
        subsector->vertices =
            Z_Malloc(sizeof(fvertex_t) * subsector->numvertices, PU_LEVELSTATIC, 0);

        // The first vertex is always the same: vertex zero.
        pv = subsector->vertices;
        memcpy(pv, &vrts[0], sizeof(*pv));

        vtx = vrts + 1;
        pv++;
        numvrts--;
    }

    // Add the rest of the vertices.
    for(i = 0; i < numvrts; ++i, vtx++, pv++)
        memcpy(pv, vtx, sizeof(*vtx));

    if(subsector->flags & DDSUBF_MIDPOINT)
    {
        // Re-add the first vertex so the triangle fan wraps around.
        memcpy(pv, &subsector->vertices[1], sizeof(*pv));
    }
}

static void R_BuildSubsectorPolys(void)
{
    uint        i, k;
    subsector_t *sub;

#ifdef _DEBUG
    Con_Printf("R_BuildSubsectorPolys\n");
#endif

    for(i = 0; i < numsubsectors; ++i)
    {
        sub = SUBSECTOR_PTR(i);

        R_InitSubsectorPoly(sub);

        sub->planes =
            Z_Malloc(sub->sector->planecount * sizeof(subplaneinfo_t*),
                     PU_LEVEL, NULL);
        for(k = 0; k < sub->sector->planecount; ++k)
        {
            sub->planes[k] =
                Z_Calloc(sizeof(subplaneinfo_t), PU_LEVEL, NULL);

            // Initialize the illumination for the subsector.
            R_InitPlaneIllumination(sub, k);
        }
        // FIXME: $nplanes
        // Initialize the plane types.
        sub->planes[PLN_FLOOR]->type = PLN_FLOOR;
        sub->planes[PLN_CEILING]->type = PLN_CEILING;
    }

#ifdef _DEBUG
    Z_CheckHeap();
#endif
}

/**
 * Determines if the given sector is made up of any self-referencing
 * "root"-lines and marks them as such.
 *
 * @return              The number of "root"-lines found ELSE 0.
 */
static uint MarkSecSelfRefRootLines(sector_t *sec)
{
    uint        i;
    uint        numroots;
    vertex_t   *vtx[2];
    line_t     *lin;

    numroots = 0;
    for(i = 0; i < sec->linecount; ++i)
    {
        lin = sec->Lines[i];

        if(lin->L_frontside && lin->L_backside &&
           lin->L_frontsector == lin->L_backsector &&
           lin->L_backsector == sec)
        {
            // The line properties indicate that this might be a
            // self-referencing, hack sector.

            // Make sure this line isn't isolated
            // (ie both vertexes arn't endpoints).
            vtx[0] = lin->L_v1;
            vtx[1] = lin->L_v2;
            if(!(vtx[0]->numlineowners == 1 && vtx[1]->numlineowners == 1))
            {
                // Also, this line could split a sector and both ends
                // COULD be vertexes that make up the sector outline.
                // So, check all line owners of each vertex.

                // Test simple case - single line dividing a sector
                if(!(vtx[0]->numsecowners == 1 && vtx[1]->numsecowners == 1))
                {
                    lineowner_t *base;
                    line_t      *lOwner, *rOwner;
                    boolean ok = false;
                    boolean ok2 = false;

                    // Ok, need to check the logical neighbors.

                    //Con_Message("Hack root candidate is %i\n", GET_LINE_IDX(lin));
                    if(vtx[0]->numlineowners > 1)
                    {
                        base = R_GetVtxLineOwner(vtx[0], lin);
                        lOwner = base->LO_prev->line;
                        rOwner = base->LO_next->line;

                        if(rOwner == lOwner)
                            ok = true;
                        else
                        {
                            if(rOwner->L_frontsector != lOwner->L_frontsector &&
                               ((!rOwner->L_backside || !lOwner->L_backside) ||
                                (rOwner->L_backside && lOwner->L_backside &&
                                 rOwner->L_backsector != lOwner->L_backsector)) &&
                               (rOwner->L_frontsector == sec || lOwner->L_frontsector == sec ||
                                (rOwner->L_backside && rOwner->L_backsector == sec) ||
                                (lOwner->L_backside && lOwner->L_backsector == sec)))
                                ok = true;
                        }
                    }

                    if(ok && vtx[1]->numlineowners > 1)
                    {
                        base = R_GetVtxLineOwner(vtx[1], lin);
                        lOwner = base->LO_prev->line;
                        rOwner = base->LO_next->line;

                        if(rOwner == lOwner)
                            ok2 = true;
                        else
                        {
                            if(rOwner->L_frontsector != lOwner->L_frontsector &&
                               ((!rOwner->L_backside || !lOwner->L_backside) ||
                                (rOwner->L_backside && lOwner->L_backside &&
                                 rOwner->L_backsector != lOwner->L_backsector)) &&
                               (rOwner->L_frontsector == sec || lOwner->L_frontsector == sec ||
                                (rOwner->L_backside && rOwner->L_backsector == sec) ||
                                (lOwner->L_backside && lOwner->L_backsector == sec)))
                                ok2 = true;
                        }
                    }

                    if(ok && ok2)
                    {
                        lin->selfrefhackroot = true;
                        VERBOSE2(Con_Message("L%i selfref root to S%i\n",
                                    GET_LINE_IDX(lin), GET_SECTOR_IDX(sec)));
                        ++numroots;
                    }
                }
            }
        }
    }

    return numroots;
}

/**
 * Scans all sectors for any supported DOOM.exe renderer hacks.
 * Updates sectorinfo accordingly.
 *
 * This algorithm detects self-referencing sector hacks used for
 * fake 3D structures.
 *
 * NOTE: Self-referencing sectors where all lines in the sector
 *       are self-referencing are NOT handled by this algorthim.
 *       Those kind of hacks are detected in R_BuildSectorLinks().
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
    uint        startTime = Sys_GetRealTime();

    uint        i, k, l, m;
    sector_t   *sec;
    line_t     *lin;
    line_t    **collectedLines;
    uint        maxNumLines = 20; // Initial size of the "run" (line list).

    // Allocate some memory for the line "run" (line list).
    collectedLines = M_Malloc(maxNumLines * sizeof(line_t*));

    for(i = 0, sec = sectors; i < numsectors; sec++, ++i)
    {
        if(!sec->linecount)
            continue;

        // Detect self-referencing "root" lines.
        if(MarkSecSelfRefRootLines(sec) != 0)
        {
            uint        count;
            line_t     *line, *next;
            vertex_t   *vtx;
            boolean     scanOwners, rescanVtx, addToCollection, found;

            // Mark this sector as a self-referencing hack.
            sec->selfRefHack = true;

            // We'll use validcount to ensure we only attempt to find
            // each hack group once.
            ++validcount;

            // Now look for lines connected to this root line (and any
            // subsequent lines that connect to those) that match the
            // requirements for a selfreferencing hack.

            // Once all lines have been collected - promote them to
            // self-referencing hack lines for this sector.
            for(k = 0; k < sec->linecount; ++k)
            {
                lin = sec->Lines[k];

                if(!lin->selfrefhackroot)
                    continue;

                if(lin->validcount == validcount)
                    continue; // We've already found this hack group.

                for(l = 0; l < 2; ++l)
                {
                    lineowner_t *p, *base;

                    // Clear the collectedLines list.
                    memset(collectedLines, 0, sizeof(collectedLines));
                    count = 0;

                    line = lin;
                    scanOwners = true;

                    vtx = lin->v[l];

                    p = base = vtx->lineowners;
                    do
                    {
                        next = p->line;

                        rescanVtx = false;
                        if(next != line && next != lin)
                        {
                            line = next;

                            if(line->L_frontsector == sec &&
                               (line->L_backside && line->L_backsector == sec))
                            {
                                // Is this line already in the current run? (self collision)
                                addToCollection = true;
                                for(m = 0; m < count && addToCollection; ++m)
                                {
                                    if(line == collectedLines[m])
                                    {
                                        int         n, q;
                                        uint        o;
                                        line_t     *lcand, *cline;
                                        vertex_t   *vcand;
                                        lineowner_t *p2, *base2;

                                        // Pick another candidate to continue line collection.
                                        pickNewCandiate:;

                                        // Logic: Work backwards through the collected line
                                        // list, checking the number of line owners of each
                                        // vertex. If we find one with >2 line owners - check
                                        // the lines to see if it would be a good place to
                                        // recommence line collection (a self-referencing line,
                                        // same sector).
                                        lcand = NULL;
                                        vcand = NULL;
                                        found = false;
                                        n = count - 1;
                                        while(n >= 0 && !found)
                                        {
                                            cline = collectedLines[n];

                                            o = 0;
                                            while(o < 2 && !found)
                                            {
                                                vcand = cline->v[o];

                                                if(vcand->numlineowners > 2)
                                                {
                                                    p2 = base2 = vcand->lineowners;
                                                    do
                                                    {
                                                        lcand = p->line;

                                                        if(lcand &&
                                                           lcand->L_frontside && lcand->L_backside &&
                                                           lcand->L_frontsector == lcand->L_backsector &&
                                                           lcand->L_frontsector == sec)
                                                        {
                                                            // Have we already collected it?
                                                            found = true;
                                                            q = count - 1;
                                                            while(q >= 0 && found)
                                                            {
                                                                if(lcand == collectedLines[q])
                                                                    found = false; // no good

                                                                if(found)
                                                                    q--;
                                                            }
                                                        }

                                                        if(!found)
                                                            p2 = p2->LO_next;
                                                    } while(!found && p2 != base2);
                                                }

                                                if(!found)
                                                    o++;
                                            }

                                            if(!found)
                                                n--;
                                        }

                                        if(found)
                                        {
                                            // Found a suitable one.
                                            line = lcand;
                                            vtx = vcand;
                                            addToCollection = true;
                                        }
                                        else
                                        {
                                            // We've found all the lines involved in
                                            // this self-ref hack. Promote all lines in the collection
                                            // to self-referencing root lines.
                                            for(o = 0; o < count; ++o)
                                            {
                                                collectedLines[o]->selfrefhackroot = true;
                                                VERBOSE2(Con_Message("  L%i selfref root to S%i\n",
                                                                     GET_LINE_IDX(collectedLines[o]),
                                                                     GET_SECTOR_IDX(sec)));

                                                // Use validcount to mark them as done.
                                                collectedLines[o]->validcount = validcount;
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

                                    if(line->selfrefhackroot)
                                        goto pickNewCandiate;
                                }

                                if(scanOwners)
                                {
                                    // Get the vertex info for the other vertex.
                                    if(vtx == line->L_v1)
                                        vtx = line->L_v2;
                                    else
                                        vtx = line->L_v1;

                                    rescanVtx = true; // Start from the begining with this vertex.
                                }
                            }
                        }

                        if(!rescanVtx)
                            p = p->LO_next;
                    } while(p != base && scanOwners);
                }
            }
        }
    }

    // Free temporary storage.
    M_Free(collectedLines);

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_RationalizeSectors: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

/**
 * Mapinfo must be available.
 */
void R_SetupFog(void)
{
    int flags = 0;
    
    if(!mapinfo)
    {
        // Go with the defaults.
        Con_Execute(CMDS_DDAY,"fog off", true, false);
        return;
    }
    
    // Check the flags.
    flags = mapinfo->flags;
    if(flags & MIF_FOG)
    {
        // Setup fog.
        Con_Execute(CMDS_DDAY, "fog on", true, false);
        Con_Executef(CMDS_DDAY, true, "fog start %f", mapinfo->fog_start);
        Con_Executef(CMDS_DDAY, true, "fog end %f", mapinfo->fog_end);
        Con_Executef(CMDS_DDAY, true, "fog density %f", mapinfo->fog_density);
        Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                     mapinfo->fog_color[0] * 255, mapinfo->fog_color[1] * 255,
                     mapinfo->fog_color[2] * 255);
    }
    else
    {
        Con_Execute(CMDS_DDAY, "fog off", true, false);
    }
}

/**
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
    for(i = 0; i < 2; ++i)
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
    for(i = 0; i < 3; ++i)
    {
        skyColorRGB[i] = mapinfo->sky_color[i];
        if(mapinfo->sky_color[i] > 0)
            noSkyColorGiven = false;
    }

    // Calculate a balancing factor, so the light in the non-skylit
    // sectors won't appear too bright.
    if(mapinfo->sky_color[0] > 0 || mapinfo->sky_color[1] > 0 ||
        mapinfo->sky_color[2] > 0)
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

/**
 * Returns pointers to the line's vertices in such a fashion that verts[0]
 * is the leftmost vertex and verts[1] is the rightmost vertex, when the
 * line lies at the edge of `sector.'
 */
void R_OrderVertices(line_t *line, const sector_t *sector, vertex_t *verts[2])
{
    if(sector == line->L_frontsector)
    {
        verts[0] = line->v[0];
        verts[1] = line->v[1];
    }
    else
    {
        verts[0] = line->v[1];
        verts[1] = line->v[0];
    }
}

/**
 * A neighbour is a line that shares a vertex with 'line', and faces the
 * specified sector.
 */
line_t *R_FindLineNeighbor(sector_t *sector, line_t *line, lineowner_t *own,
                           boolean antiClockwise, binangle_t *diff)
{
    lineowner_t *cown = own->link[!antiClockwise];
    line_t     *other = cown->line;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? own->LO_prev->angle : own->angle);

    if(!other->L_backside || other->L_frontsector != other->L_backsector)
    {
        if(sector) // Must one of the sectors match?
        {
            if(other->L_frontsector == sector ||
               (other->L_backside && other->L_backsector == sector))
                return other;
        }
        else
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineNeighbor(sector, line, cown, antiClockwise, diff);
}

line_t *R_FindSolidLineNeighbor(sector_t *sector, line_t *line, lineowner_t *own,
                                boolean antiClockwise, binangle_t *diff)
{
    lineowner_t *cown = own->link[!antiClockwise];
    line_t     *other = cown->line;
    int         side;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? own->LO_prev->angle : own->angle);

    if(!other->L_frontside || !other->L_backside)
    {
        return other;
    }

    if(other->L_frontsector != other->L_backsector &&
       (other->L_frontsector->SP_floorvisheight >= sector->SP_ceilvisheight ||
        other->L_frontsector->SP_ceilvisheight <= sector->SP_floorvisheight ||
        other->L_backsector->SP_floorvisheight >= sector->SP_ceilvisheight ||
        other->L_backsector->SP_ceilvisheight <= sector->SP_floorvisheight))
        return other;

    // Both front and back MUST be open by this point.

    // Check for mid texture which fills the gap between floor and ceiling.
    // We should not give away the location of false walls (secrets).
    side = (other->L_frontsector == sector? 0 : 1);
    if(other->sides[side]->SW_middletexture != 0)
    {
        float oFCeil  = other->L_frontsector->SP_ceilvisheight;
        float oFFloor = other->L_frontsector->SP_floorvisheight;
        float oBCeil  = other->L_backsector->SP_ceilvisheight;
        float oBFloor = other->L_backsector->SP_floorvisheight;

        if((side == 0 &&
            ((oBCeil > sector->SP_floorvisheight &&
                  oBFloor <= sector->SP_floorvisheight) ||
             (oBFloor < sector->SP_ceilvisheight &&
                  oBCeil >= sector->SP_ceilvisheight) ||
             (oBFloor < sector->SP_ceilvisheight &&
                  oBCeil > sector->SP_floorvisheight))) ||
           ( /* side must be 1 */
            ((oFCeil > sector->SP_floorvisheight &&
                  oFFloor <= sector->SP_floorvisheight) ||
             (oFFloor < sector->SP_ceilvisheight &&
                  oFCeil >= sector->SP_ceilvisheight) ||
             (oFFloor < sector->SP_ceilvisheight &&
                  oFCeil > sector->SP_floorvisheight)))  )
        {

            if(!Rend_DoesMidTextureFillGap(other, side))
                return 0;
        }
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

/**
 * Find a backneighbour for the given line.
 * They are the neighbouring line in the backsector of the imediate line
 * neighbor.
 */
line_t *R_FindLineBackNeighbor(sector_t *sector, line_t *line, lineowner_t *own,
                               boolean antiClockwise, binangle_t *diff)
{
    lineowner_t *cown = own->link[!antiClockwise];
    line_t *other = cown->line;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? own->LO_prev->angle : own->angle);

    if(!other->L_backside || other->L_frontsector != other->L_backsector)
    {
        if(!(other->L_frontsector == sector ||
             (other->L_backside && other->L_backsector == sector)))
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineBackNeighbor(sector, line, cown, antiClockwise,
                                   diff);
}

/**
 * A side's alignneighbor is a line that shares a vertex with 'line' and
 * whos orientation is aligned with it (thus, making it unnecessary to have
 * a shadow between them. In practice, they would be considered a single,
 * long sidedef by the shadow generator).
 */
line_t *R_FindLineAlignNeighbor(sector_t *sec, line_t *line,
                                lineowner_t *own, boolean antiClockwise,
                                int alignment)
{
#define SEP 10

    lineowner_t *cown = own->link[!antiClockwise];
    line_t *other = cown->line;
    binangle_t diff;

    if(other == line)
        return NULL;

    if(!(other->L_backside &&
         other->L_backsector == other->L_frontsector))
    {
        diff = line->angle - other->angle;

        if(alignment < 0)
            diff -= BANG_180;
        if(other->L_frontsector != sec)
            diff -= BANG_180;
        if(diff < SEP || diff > BANG_360 - SEP)
            return other;
    }

    // Can't step over non-twosided lines.
    if(!other->L_backside || !other->L_frontside)
        return NULL;

    // Not suitable, try the next.
    return R_FindLineAlignNeighbor(sec, line, cown, antiClockwise, alignment);

#undef SEP
}

/**
 * Find line neighbours, which will be used in the FakeRadio calculations.
 */
#if 0
void R_InitLineNeighbors(void)
{
    uint        startTime = Sys_GetRealTime();

    uint        i, k, j, m, sid;
    line_t     *line, *other;
    sector_t   *sector;
    side_t     *side;
    vertex_t   *vtx;

    // Find neighbours. We'll do this sector by sector.
    for(k = 0; k < numsectors; ++k)
    {
        sector = SECTOR_PTR(k);
        for(i = 0; i < sector->linecount; ++i)
        {
            line = sector->Lines[i];

            // Which side is this?
            sid = (line->L_frontsector == sector? FRONT:BACK);
            side = line->sides[sid];

            for(j = 0; j < 2; ++j)
            {
                side->neighbor[j] =
                    R_FindLineNeighbor(sector, line, line->vo[sid^j], j, NULL);

                // Figure out the sectors in the proximity.
                // Neighbour must be two-sided.
                if(side->neighbor[j] && side->neighbor[j]->L_frontsector &&
                   side->neighbor[j]->L_backsector)
                {
                    int         nSide =
                        (side->neighbor[j]->L_frontsector == sector ? BACK:FRONT);

                    side->proxsector[j] = side->neighbor[j]->sec[nSide];
                }
                else
                {
                    side->proxsector[j] = NULL;
                }

                // Look for aligned neighbours.  They are side-specific.
                side->alignneighbor[j] =
                    R_FindLineAlignNeighbor(sector, line, line->vo[sid^j], j,
                                    (side == line->L_frontside? 1:-1));
            }

            // Attempt to find "pretend" neighbours for this line, if "real"
            // ones have not been found.
            // NOTE: selfrefhackroot lines don't have neighbours.
            //       They can only be "pretend" neighbours.

            // Pretend neighbours are selfrefhackroot lines but BOTH vertices
            // are owned by this sector and ONE of the vertexes is owned by
            // this line.
            if((!side->neighbor[0] || !side->neighbor[1]) &&
               !line->selfrefhackroot)
            {

                uint        v;
                boolean     done, ok = false;
                lineowner_t *own;

                for(j = 0; j < 2; ++j)
                {
                    if(!side->neighbor[j])
                    {
                        own = line->vo[sid^j];
                        done = false;
                        do
                        {
                            other = own->line;
                            if(other->selfrefhackroot &&
                               (other->L_v1 == line->L_v1 || other->L_v1 == line->L_v2 ||
                                other->L_v2 == line->L_v1 || other->L_v2 == line->L_v2))
                            {
                                for(v = 0; v < 2; ++v)
                                {
                                    ok = false;
                                    vtx = other->v[v];
                                    for(m = 0; m < vtx->numsecowners && !ok; ++m)
                                        if(vtx->secowners[m] == k) // k == sector id
                                            ok = true;
                                    if(!ok)
                                        break;
                                }

                                if(ok)
                                {
                                    side->neighbor[j] = other;
                                    side->pretendneighbor[j] = true;
                                    done = true;
                                }
                            }

                            own = own->LO_next;
                        } while(own->line != line && !done);
                    }
                }
            }
        }
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitLineNeighbors: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

#if _DEBUG
if(verbose >= 1)
{
    for(i = 0; i < numlines; ++i)
    {
        line = LINE_PTR(i);
        for(k = 0; k < 2; ++k)
        {
            if(!line->sides[k])
                continue;

            side = line->sides[k];

            if(side->alignneighbor[0] || side->alignneighbor[1])
                Con_Printf("Line %i/%i: l=%i r=%i\n", i, k,
                           (side->alignneighbor[0] ?
                            GET_LINE_IDX(side->alignneighbor[0]) : -1),
                           (side->alignneighbor[1] ?
                            GET_LINE_IDX(side->alignneighbor[1]) : -1));

            if(side->neighbor[k] && side->pretendneighbor[k])
                Con_Printf("  has a pretend neighbor, line %i\n",
                           GET_LINE_IDX(side->neighbor[k]));
        }
    }
}
#endif
}
#endif

void R_InitLinks(void)
{
    uint        i;

    Con_Message("R_InitLinks: Initializing\n");

    // Init polyobj blockmap.
    P_InitPolyBlockMap();

    // Initialize node piles and line rings.
    NP_Init(&thingnodes, 256);  // Allocate a small pile.
    NP_Init(&linenodes, numlines + 1000);

    // Allocate the rings.
    linelinks = Z_Malloc(sizeof(*linelinks) * numlines, PU_LEVELSTATIC, 0);
    for(i = 0; i < numlines; ++i)
        linelinks[i] = NP_New(&linenodes, NP_ROOT_NODE);
}

/**
 * This routine is called from P_AttemptMapLoad() to polygonize the current
 * level. Creates floors and ceilings and fixes the adjacent sky sector
 * heights.  Creates a big enough dlBlockLinks.
 */
void R_InitLevel(char *level_id)
{
    uint        startTime;
    uint        i;

    // Must be called before any mobjs are spawned.
    R_InitLinks();

    if(isServer)
    {
        // Whenever the map changes, remote players must tell us when
        // they're ready to begin receiving frames.
        for(i = 0; i < MAXPLAYERS; ++i)
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

    //Con_InitProgress("Setting up level...", 100);
    strcpy(currentLevelId, level_id);

    //Con_Progress(10, 0);

    // Polygonize.
    if(P_GLNodeDataPresent() && bspBuild)
        R_PolygonizeWithoutCarving();
    else
        R_CreateFloorsAndCeilings(numnodes - 1, 0, NULL);

    //Con_Progress(10, 0);

    // Init Particle Generator links.
    PG_InitForLevel();

    // Make sure subsector floors and ceilings will be rendered
    // correctly.
    R_SubsectorPlanes();

    // The map bounding box will be updated during sector info
    // initialization.
    memset(mapBounds, 0, sizeof(mapBounds));
    R_BuildSectorLinks();

    R_BuildSubsectorPolys();

    // Compose the vertex owner arrays.
    R_BuildVertexOwners();

    // Init blockmap for searching subsectors.
    P_InitSubsectorBlockMap();

    R_RationalizeSectors();
//    R_InitLineNeighbors();  // Must follow R_RationalizeSectors.
    R_InitSectorShadows();

    //Con_Progress(10, 0);

    startTime = Sys_GetRealTime();
    R_InitSkyFix();
    R_SkyFix(true, true); // fix floors and ceilings.
    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitLevel: Initial SkyFix Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

    S_CalcSectorReverbs();

    DL_InitForMap();

    Cl_Reset();
    RL_DeleteLists();

    // See what mapinfo says about this level.
    mapinfo = Def_GetMapInfo(level_id);
    if(!mapinfo)
        mapinfo = Def_GetMapInfo("*");

    // Setup accordingly.
    R_SetupSky();

    if(mapinfo)
    {
        mapgravity = mapinfo->gravity * FRACUNIT;
        mapambient = mapinfo->ambient * 255;
    }
    else
    {
        // No map info found, so set some basic stuff.
        mapgravity = FRACUNIT;
        mapambient = 0;
    }

    // Invalidate old cmds and init player values.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        players[i].invoid = false;
        if(isServer && players[i].ingame)
            clients[i].runTime = SECONDS_TO_TICKS(gameTime);
    }

    // Spawn all type-triggered particle generators.
    // Let's hope there aren't too many...
    startTime = Sys_GetRealTime();
    P_SpawnTypeParticleGens();
    P_SpawnMapParticleGens(level_id);
    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitLevel: Spawn Generators Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

    // Make sure that the next frame doesn't use a filtered viewer.
    R_ResetViewer();

    // Texture animations should begin from their first step.
    R_ResetAnimGroups();

    // Tell shadow bias to initialize the bias light sources.
    SB_InitForLevel(R_GetUniqueLevelID());

    // Initialize the lighting grid.
    LG_Init();

    R_InitRendPolyPool();

    //Con_Progress(10, 0);        // 50%.
}

/**
 * Called by the game at various points in the level setup process.
 */
void R_SetupLevel(int mode, int flags)
{
    uint        i;

    switch(mode)
    {
    case DDSLM_INITIALIZE:
        // Switch to fast malloc mode in the zone. This is intended for large
        // numbers of mallocs with no frees in between.
        Z_EnableFastMalloc(false);

        // A new level is about to be setup.
        levelSetup = true;
        return;

    case DDSLM_AFTER_LOADING:
    {
        side_t *side;

        // Loading a game usually destroys all thinkers. Until a proper
        // savegame system handled by the engine is introduced we'll have
        // to resort to re-initializing the most important stuff.
        P_SpawnTypeParticleGens();

        // Update everything again. Its possible that after loading we
        // now have more HOMs to fix, etc..

        R_SkyFix(true, true); // fix floors and ceilings.

        // Update all sectors. Set intial values of various tracked
        // and interpolated properties (lighting, smoothed planes etc).
        for(i = 0; i < numsectors; ++i)
            R_UpdateSector(SECTOR_PTR(i), false);

        // Do the same for side surfaces.
        for(i = 0; i < numsides; ++i)
        {
            side = SIDE_PTR(i);
            R_UpdateSurface(&side->SW_topsurface, false);
            R_UpdateSurface(&side->SW_middlesurface, false);
            R_UpdateSurface(&side->SW_bottomsurface, false);
        }

        // We don't render fakeradio on polyobjects...
        PO_SetupPolyobjs();
        return;
    }
    case DDSLM_FINALIZE:
    {
        int     j, k;
        line_t *line;

        // Init server data.
        Sv_InitPools();

        // Recalculate the light range mod matrix.
        Rend_CalcLightRangeModMatrix(NULL);

        // Update all sectors. Set intial values of various tracked
        // and interpolated properties (lighting, smoothed planes etc).
        for(i = 0; i < numsectors; ++i)
            R_UpdateSector(SECTOR_PTR(i), true);

        for(i = 0; i < numlines; ++i)
        {
            line = LINE_PTR(i);

            if(line->flags & 0x0100) // The old ML_MAPPED flag
            {
                // This line wants to be seen in the map from the begining.
                memset(&line->mapped, 1, sizeof(&line->mapped));

                // Send a status report.
                if(gx.HandleMapObjectStatusReport)
                {
                    int  pid;

                    for(k = 0; k < DDMAXPLAYERS; ++k)
                    {
                        pid = k;
                        gx.HandleMapObjectStatusReport(DMUSC_LINE_FIRSTRENDERED,
                                                      i, DMU_LINE, &pid);
                    }
                }
                line->flags &= ~0x0100; // remove the flag.
            }

            // Update side surfaces.
            for(j = 0; j < 2; ++j)
            {
                if(!line->sides[j])
                    continue;

                R_UpdateSurface(&line->sides[j]->SW_topsurface, true);
                R_UpdateSurface(&line->sides[j]->SW_middlesurface, true);
                R_UpdateSurface(&line->sides[j]->SW_bottomsurface, true);
            }
        }

        // We don't render fakeradio on polyobjects...
        PO_SetupPolyobjs();

        // Run any commands specified in Map Info.
        if(mapinfo && mapinfo->execute)
            Con_Execute(CMDS_DED, mapinfo->execute, true, false);

        // The level setup has been completed.  Run the special level
        // setup command, which the user may alias to do something
        // useful.
        if(levelid && levelid[0])
        {
            char    cmd[80];

            sprintf(cmd, "init-%s", levelid);
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
        for(i = 0; i < MAXPLAYERS; ++i)
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
    case DDSLM_AFTER_BUSY:
        // Shouldn't do anything time-consuming, as we are no longer in busy mode.
        R_SetupFog();
        break;
    default:
        Con_Error("R_SetupLevel: Unknown setup mode %i", mode);
    }
}

void R_ClearSectorFlags(void)
{
    uint        i;
    sector_t   *sec;

    for(i = 0; i < numsectors; ++i)
    {
        sec = SECTOR_PTR(i);
        // Clear all flags that can be cleared before each frame.
        sec->frameflags &= ~SIF_FRAME_CLEAR;
    }
}

sector_t *R_GetLinkedSector(subsector_t *startssec, uint plane)
{
    ssecgroup_t *ssgrp = &startssec->sector->subsgroups[startssec->group];

    if(!devNoLinkedSurfaces && ssgrp->linked[plane])
        return ssgrp->linked[plane];
    else
        return startssec->sector;
}

void R_UpdateAllSurfaces(boolean forceUpdate)
{
    uint        i, j;

    // First, all planes of all sectors.
    for(i = 0; i < numsectors; ++i)
    {
        sector_t *sec = SECTOR_PTR(i);

        for(j = 0; j < sec->planecount; ++j)
            R_UpdateSurface(&sec->planes[j]->surface, forceUpdate);
    }

    // Then all sections of all sides.
    for(i = 0; i < numsides; ++i)
    {
        side_t *side = SIDE_PTR(i);

        R_UpdateSurface(&side->SW_topsurface, forceUpdate);
        R_UpdateSurface(&side->SW_middlesurface, forceUpdate);
        R_UpdateSurface(&side->SW_bottomsurface, forceUpdate);
    }
}

void R_UpdateSurface(surface_t *suf, boolean forceUpdate)
{
    int         texFlags, oldTexFlags;

    // Any change to the texture or glow properties?
    // TODO: Implement Decoration{ Glow{}} definitions.
    texFlags =
        R_GraphicResourceFlags((suf->isflat? RC_FLAT : RC_TEXTURE),
                               suf->texture);
    oldTexFlags =
        R_GraphicResourceFlags((suf->oldisflat? RC_FLAT : RC_TEXTURE),
                               suf->oldtexture);

    if(forceUpdate ||
       suf->isflat != suf->oldisflat)
    {
        suf->oldisflat = suf->isflat;
    }

    // FIXME >
    // Update glowing status?
    // The order of these tests is important.
    if(forceUpdate || (suf->texture != suf->oldtexture))
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
        if(texFlags & TXF_GLOW)
        {
            // The new texture is glowing.
            suf->flags |= SUF_GLOW;
        }
        else if(suf->oldtexture && (oldTexFlags & TXF_GLOW))
        {
            // The old texture was glowing but the new one is not.
            suf->flags &= ~SUF_GLOW;
        }

        suf->oldtexture = suf->texture;

        // No longer a missing texture fix?
        if(suf->texture && (oldTexFlags & SUF_TEXFIX))
            suf->flags &= ~SUF_TEXFIX;

        // Update the surface's blended texture.
        if(suf->isflat)
            suf->xlat = &flattranslation[suf->texture];
        else
            suf->xlat = &texturetranslation[suf->texture];

        if(suf->xlat && suf->texture)
            suf->flags |= SUF_BLEND;
        else
            suf->flags &= ~SUF_BLEND;
    }
    else if((texFlags & TXF_GLOW) != (oldTexFlags & TXF_GLOW))
    {
        // The glow property of the current flat has been changed
        // since last update.

        // NOTE:
        // This approach is hardly optimal but since flats will
        // rarely/if ever have this property changed during normal
        // gameplay (the RENDER_GLOWFLATS text string is depreciated and
        // the only time this property might change is after a console
        // RESET) so it doesn't matter.
        if(!(texFlags & TXF_GLOW) && (oldTexFlags & TXF_GLOW))
        {
            // The current flat is no longer glowing
            suf->flags &= ~SUF_GLOW;
        }
        else if((texFlags & TXF_GLOW) && !(oldTexFlags & TXF_GLOW))
        {
            // The current flat is now glowing
            suf->flags |= SUF_GLOW;
        }
    }
    // < FIXME

    if(forceUpdate ||
       suf->flags != suf->oldflags)
    {
        suf->oldflags = suf->flags;
    }

    if(forceUpdate ||
       (suf->texmove[0] != suf->oldtexmove[0] ||
        suf->texmove[1] != suf->oldtexmove[1]))
    {
        suf->oldtexmove[0] = suf->texmove[0];
        suf->oldtexmove[1] = suf->texmove[1];
    }

    if(forceUpdate ||
       (suf->offx != suf->oldoffx))
    {
        suf->oldoffx = suf->offx;
    }

    if(forceUpdate ||
       (suf->offy != suf->oldoffy))
    {
        suf->oldoffy = suf->offy;
    }

    // Surface color change?
    if(forceUpdate ||
       (suf->rgba[0] != suf->oldrgba[0] ||
        suf->rgba[1] != suf->oldrgba[1] ||
        suf->rgba[2] != suf->oldrgba[2] ||
        suf->rgba[3] != suf->oldrgba[3]))
    {
        // TODO: when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        memcpy(suf->oldrgba, suf->rgba, sizeof(suf->oldrgba));
    }
}

void R_UpdateSector(sector_t* sec, boolean forceUpdate)
{
    uint        i, j;
    plane_t    *plane;

    // Check if there are any lightlevel or color changes.
    if(forceUpdate ||
       (sec->lightlevel != sec->oldlightlevel ||
        sec->rgb[0] != sec->oldrgb[0] ||
        sec->rgb[1] != sec->oldrgb[1] ||
        sec->rgb[2] != sec->oldrgb[2]))
    {
        sec->frameflags |= SIF_LIGHT_CHANGED;
        sec->oldlightlevel = sec->lightlevel;
        memcpy(sec->oldrgb, sec->rgb, sizeof(sec->oldrgb));

        LG_SectorChanged(sec);
    }
    else
    {
        sec->frameflags &= ~SIF_LIGHT_CHANGED;
    }

    // For each plane.
    for(i = 0; i < sec->planecount; ++i)
    {
        plane = sec->planes[i];

        // Surface changes?
        R_UpdateSurface(&plane->surface, forceUpdate);

        // FIXME >
        // Now update the glow properties.
        if(plane->surface.flags & SUF_GLOW)
        {
            plane->glow = 4; // Default height factor is 4

            if(plane->surface.isflat)
                GL_GetFlatColor(plane->surface.texture, plane->glowrgb);
            else
                GL_GetTextureColor(plane->surface.texture, plane->glowrgb);
        }
        else
        {
            plane->glow = 0;
            for(j = 0; j < 3; ++j)
                plane->glowrgb[j] = 0;
        }
        // < FIXME

        // Geometry change?
        if(forceUpdate ||
           plane->height != plane->oldheight[1])
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

                if((player->flags & DDPF_CAMERA) &&
                   player->mo->subsector->sector == sec &&
                   (FIX2FLT(player->mo->pos[VZ]) > sec->SP_ceilheight ||
                    FIX2FLT(player->mo->pos[VZ]) < sec->SP_floorheight))
                {
                    player->invoid = true;
                }
            }

            P_PlaneChanged(sec, i);
        }
    }

    if(!sec->permanentlink) // Assign new links
    {
        // Only floor and ceiling can be linked, not all inbetween
        for(i = 0; i < sec->subsgroupcount; ++i)
        {
            ssecgroup_t *ssgrp = &sec->subsgroups[i];

            ssgrp->linked[PLN_FLOOR] = NULL;
            ssgrp->linked[PLN_CEILING] = NULL;
        }
        R_SetSectorLinks(sec);
    }
}

/**
 * All links will be updated every frame (sectorheights may change at
 * any time without notice).
 */
void R_UpdatePlanes(void)
{
    // Nothing to do.
}

/**
 * This ID is the name of the lump tag that marks the beginning of map
 * data, e.g. "MAP03" or "E2M8".
 */
const char *R_GetCurrentLevelID(void)
{
    return currentLevelId;
}

/**
 * Return the 'unique' identifier of the map.  This identifier
 * contains information about the map tag (E3M3), the WAD that
 * contains the map (DOOM.IWAD), and the game mode (doom-ultimate).
 *
 * The entire ID string will be in lowercase letters.
 */
const char *R_GetUniqueLevelID(void)
{
    static char uid[256];
    filename_t  base;
    int         lump = W_GetNumForName((char*)R_GetCurrentLevelID());

    M_ExtractFileBase(W_LumpSourceFile(lump), base);

    sprintf(uid, "%s|%s|%s|%s",
            R_GetCurrentLevelID(),
            base, (W_IsFromIWAD(lump) ? "iwad" : "pwad"),
            (char *) gx.GetVariable(DD_GAME_MODE));

    strlwr(uid);
    return uid;
}

/**
 * Sector light color may be affected by the sky light color.
 */
const float *R_GetSectorLightColor(sector_t *sector)
{
    sector_t   *src;
    uint        i;

    if(!rendSkyLight || noSkyColorGiven)
        return sector->rgb;     // The sector's real color.

    if(!R_IsSkySurface(&sector->SP_ceilsurface) &&
       !R_IsSkySurface(&sector->SP_floorsurface))
    {
        // A dominant light source affects this sector?
        src = sector->lightsource;
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
            for(i = 0; i < 3; ++i)
                balancedRGB[i] = sector->rgb[i] * skyColorBalance;
            return balancedRGB;
        }
    }
    // Return the sky color.
    return skyColorRGB;
}

/**
 * Calculate the size of the entire map.
 */
void R_GetMapSize(fixed_t *min, fixed_t *max)
{
    min[VX] = FRACUNIT * mapBounds[BLEFT];
    min[VY] = FRACUNIT * mapBounds[BTOP];

    max[VX] = FRACUNIT * mapBounds[BRIGHT];
    max[VY] = FRACUNIT * mapBounds[BBOTTOM];
}
