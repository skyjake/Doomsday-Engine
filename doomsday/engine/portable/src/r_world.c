/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     rendSkyLight = 1;       // cvar

boolean firstFrameAfterLoad;
boolean levelSetup;

nodeindex_t     *linelinks;         // indices to roots

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean noSkyColorGiven;
static float skyColorRGB[4], balancedRGB[4];
static float skyColorBalance;

// CODE --------------------------------------------------------------------

/**
 * Create a new plane for the given sector. The plane will be initialized
 * with default values.
 *
 * Post: The sector's plane list will be replaced, the new plane will be
 *       linked to the end of the list.
 *
 * @param sec           Ptr to sector for which a new plane will be created.
 * @param type          Plane type to be created (PLN_FLOOR/PLN_CEILING).
 *
 * @return              Ptr to the newly created plane.
 */
plane_t *R_NewPlaneForSector(sector_t *sec, planetype_t type)
{
    uint            i;
    surface_t      *suf;
    plane_t        *plane, **newList;

    if(!sec)
        return NULL; // Do wha?

    if(sec->planecount >= 2)
        Con_Error("P_NewPlaneForSector: Cannot create plane for sector, "
                  "limit is %i per sector.\n", 2);

    // Allocate the new plane.
    plane = Z_Calloc(sizeof(plane_t), PU_LEVEL, 0);
    suf = &plane->surface;

    // Allocate a new plane list for this sector.
    newList = Z_Malloc(sizeof(plane_t*) * (++sec->planecount + 1), PU_LEVEL, 0);
    // Copy any existing plane ptrs.
    for(i = 0; i < sec->planecount - 1; ++i)
        newList[i] = sec->planes[i];
    // Add the new plane to the end of the list.
    newList[i++] = plane;
    newList[i] = NULL; // Terminate.

    // Link the new plane list to the sector.
    if(sec->planes)
        Z_Free(sec->planes); // Free the old list.
    sec->planes = newList;

    // Setup header for DMU.
    plane->header.type = DMU_PLANE;

    // Initalize the plane.
    for(i = 0; i < 3; ++i)
        plane->glowrgb[i] = 1;

    // The back pointer (temporary)
    plane->sector = sec;

    // Initialize the surface.
	suf->material.texture = suf->oldmaterial.texture = -1;
	suf->material.isflat = suf->oldmaterial.isflat = true;
    for(i = 0; i < 4; ++i)
        suf->rgba[i] = 1;
    suf->flags = 0;
    suf->offx = suf->offy = 0;

    // Set normal.
    suf->normal[VX] = 0;
    suf->normal[VY] = 0;
    suf->normal[VZ] = 1;

	return plane;
}

/**
 * Permanently destroys the specified plane of the given sector.
 * The sector's plane list is updated accordingly.
 *
 * @param id            The sector, plane id to be destroyed.
 * @param sec           Ptr to sector for which a plane will be destroyed.
 */
void R_DestroyPlaneOfSector(uint id, sector_t *sec)
{
    plane_t        *plane, **newList = NULL;

    if(!sec)
        return; // Do wha?

    if(id >= sec->planecount)
        Con_Error("P_DestroyPlaneOfSector: Plane id #%i is not valid for "
                  "sector #%u", id, (uint) GET_SECTOR_IDX(sec));

    plane = sec->planes[id];

    // Create a new plane list?
    if(sec->planecount > 1)
    {
        uint        i, n;

        newList = Z_Malloc(sizeof(plane_t**) * sec->planecount, PU_LEVEL, 0);

        // Copy ptrs to the planes.
        n = 0;
        for(i = 0; i < sec->planecount; ++i)
        {
            if(i == id)
                continue;
            newList[n++] = sec->planes[i];
        }
        newList[n] = NULL; // Terminate.
    }

    // Destroy the specified plane.
    Z_Free(plane);
    sec->planecount--;

    // Link the new list to the sector.
    Z_Free(sec->planes);
    sec->planes = newList;
}

#ifdef _MSC_VER
#  pragma optimize("g", off)
#endif

/**
 * Called whenever the sector changes.
 *
 * This routine handles plane hacks where all of the sector's lines are
 * twosided and missing upper or lower textures.
 *
 * \note This does not support sectors with disjoint groups of lines
 *       (e.g. a sector with a "control" sector such as the forcefields in
 *       ETERNAL.WAD MAP01).
 *
 * \todo Needs updating for $nplanes.
 */
static void R_SetSectorLinks(sector_t *sec)
{
    uint        i;
    boolean     hackfloor, hackceil;
    sector_t   *floorLinkCandidate = 0, *ceilLinkCandidate = 0;

    // Must have a valid sector!
    if(!sec || !sec->linecount || sec->permanentlink)
        return;                 // Can't touch permanent links.

    hackfloor = (!R_IsSkySurface(&sec->SP_floorsurface));
    hackceil = (!R_IsSkySurface(&sec->SP_ceilsurface));
    if(!(hackfloor || hackceil))
        return;

    for(i = 0; i < sec->subsgroupcount; ++i)
    {
        subsector_t   **ssecp;
        ssecgroup_t    *ssgrp;

        if(!hackfloor && !hackceil)
            break;

        ssgrp = &sec->subsgroups[i];

        ssecp = sec->subsectors;
        while(*ssecp)
        {
            subsector_t    *ssec = *ssecp;

            if(!hackfloor && !hackceil)
                break;

            // Must be in the same group.
            if(ssec->group == i)
            {
                seg_t         **segp = ssec->segs;

                while(*segp)
                {
                    seg_t          *seg = *segp;
                    line_t         *lin;

                    if(!hackfloor && !hackceil)
                        break;

                    lin = seg->linedef;

                    if(lin && // minisegs don't count.
                       lin->L_frontside && lin->L_backside) // Must be twosided.
                    {
                        sector_t       *back;
                        side_t         *sid, *frontsid, *backsid;

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
                                floorLinkCandidate = back;
                        }

                        if(back->SP_ceilheight == sec->SP_ceilheight)
                        {
                            hackceil = false;
                        }
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
                                ceilLinkCandidate = back;
                        }
                    }

                    segp++;
                }
            }

            ssecp++;
        }

        if(hackfloor)
        {
            if(floorLinkCandidate == sec->containsector)
                ssgrp->linked[PLN_FLOOR] = floorLinkCandidate;
        }

        if(hackceil)
        {
            if(ceilLinkCandidate == sec->containsector)
                ssgrp->linked[PLN_CEILING] = ceilLinkCandidate;
        }
    }
}

#ifdef _MSC_VER
#  pragma optimize("", on)
#endif

/**
 * Initialize the skyfix. In practice all this does is to check for things
 * intersecting ceilings and if so: raises the sky fix for the sector a
 * bit to accommodate them.
 */
void R_InitSkyFix(void)
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
                    Con_Printf("S%li: (mo)skyfix to %g (ceil=%g)\n",
                               (long) GET_SECTOR_IDX(sec), *fix,
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
        Con_Printf("S%li: skyfix to %g (%s=%g)\n",
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
            else if(line->flags & LINEF_SELFREF)
            {
                // Its a selfreferencing linedef, these will ALWAYS return
                // the same height on the front and back so we need to find
                // the neighbouring lines either side of this and compare
                // the front and back sectors of those instead.
                uint        j;
                vertex_t   *vtx[2];

                vtx[0] = line->L_v1;
                vtx[1] = line->L_v2;
                // Walk around each vertex in each direction.
                for(j = 0; j < 2; ++j)
                    if(vtx[j]->numlineowners > 1)
                        spreadSkyFixForNeighbors(vtx[j], line,
                                                 doFix[PLN_FLOOR],
                                                 doFix[PLN_CEILING],
                                                 &adjusted[PLN_FLOOR],
                                                 &adjusted[PLN_CEILING]);
            }
        }
    }
    while(adjusted[PLN_FLOOR] || adjusted[PLN_CEILING]);
}

/**
 * @return          Ptr to the lineowner for this line for this vertex else
 *                  @c NULL.
 */
lineowner_t* R_GetVtxLineOwner(vertex_t *v, line_t *line)
{
    if(v == line->L_v1)
        return line->L_vo1;

    if(v == line->L_v2)
        return line->L_vo2;

    return NULL;
}

void R_SetupFog(float start, float end, float density, float *rgb)
{
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

void R_SetupFogDefaults(void)
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY,"fog off", true, false);
}

void R_SetupSky(ded_mapinfo_t *mapinfo)
{
    int         i, k;
    int         skyTex;

    if(!mapinfo)
    {   // Go with the defaults.
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
    byte        edge;

    edge = (sector == line->L_frontsector? 0:1);
    verts[0] = line->L_v(edge);
    verts[1] = line->L_v(edge^1);
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
        return other;

    if(!(other->flags & LINEF_SELFREF) &&
       (other->L_frontsector->SP_floorvisheight >= sector->SP_ceilvisheight ||
        other->L_frontsector->SP_ceilvisheight <= sector->SP_floorvisheight ||
        other->L_backsector->SP_floorvisheight >= sector->SP_ceilvisheight ||
        other->L_backsector->SP_ceilvisheight <= sector->SP_floorvisheight ||
        other->L_backsector->SP_ceilvisheight <= other->L_backsector->SP_floorvisheight))
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

    if(!(other->flags & LINEF_SELFREF))
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

void R_InitLinks(gamemap_t *map)
{
    uint        i;
    uint        starttime;

    Con_Message("R_InitLinks: Initializing\n");

    // Initialize node piles and line rings.
    NP_Init(&map->thingnodes, 256);  // Allocate a small pile.
    NP_Init(&map->linenodes, map->numlines + 1000);

    // Allocate the rings.
    starttime = Sys_GetRealTime();
    map->linelinks =
        Z_Malloc(sizeof(*map->linelinks) * map->numlines, PU_LEVELSTATIC, 0);
    for(i = 0; i < map->numlines; ++i)
        map->linelinks[i] = NP_New(&map->linenodes, NP_ROOT_NODE);
    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitLinks: Allocating line link rings. Done in %.2f seconds.\n",
             (Sys_GetRealTime() - starttime) / 1000.0f));
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

            if(line->mapflags & 0x0100) // The old ML_MAPPED flag
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
                line->mapflags &= ~0x0100; // remove the flag.
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
        {
        gamemap_t  *map = P_GetCurrentMap();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

        if(mapInfo && mapInfo->execute)
            Con_Execute(CMDS_DED, mapInfo->execute, true, false);
        }

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
    {
        gamemap_t  *map = P_GetCurrentMap();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(P_GetMapID(map));

        // Shouldn't do anything time-consuming, as we are no longer in busy mode.
        if(!mapInfo || !(mapInfo->flags & MIF_FOG))
            R_SetupFogDefaults();
        else
            R_SetupFog(mapInfo->fog_start, mapInfo->fog_end,
                       mapInfo->fog_density, mapInfo->fog_color);
        break;
    }
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
    texFlags =
        R_GraphicResourceFlags((suf->material.isflat? RC_FLAT : RC_TEXTURE),
                               suf->material.texture);
    oldTexFlags =
        R_GraphicResourceFlags((suf->oldmaterial.isflat? RC_FLAT : RC_TEXTURE),
                               suf->oldmaterial.texture);

    if(forceUpdate ||
       suf->material.isflat != suf->oldmaterial.isflat)
    {
        suf->oldmaterial.isflat = suf->material.isflat;
    }


    // \fixme Update glowing status?
    // The order of these tests is important.
    if(forceUpdate || (suf->material.texture != suf->oldmaterial.texture))
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
        else if(suf->oldmaterial.texture && (oldTexFlags & TXF_GLOW))
        {
            // The old texture was glowing but the new one is not.
            suf->flags &= ~SUF_GLOW;
        }

        suf->oldmaterial.texture = suf->material.texture;

        // No longer a missing texture fix?
        if(suf->material.texture && (oldTexFlags & SUF_TEXFIX))
            suf->flags &= ~SUF_TEXFIX;

        // Update the surface's blended texture.
        if(suf->material.isflat)
            suf->material.xlat = &flattranslation[suf->material.texture];
        else
            suf->material.xlat = &texturetranslation[suf->material.texture];

        if(suf->material.xlat && suf->material.texture)
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
        // \todo when surface colours are intergrated with the
        // bias lighting model we will need to recalculate the
        // vertex colours when they are changed.
        memcpy(suf->oldrgba, suf->rgba, sizeof(suf->oldrgba));
    }
}

void R_UpdateSector(sector_t* sec, boolean forceUpdate)
{
    uint        i, j;
    plane_t    *plane;
    boolean     updateReverb = false;

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

        // \fixme Now update the glow properties.
        if(plane->surface.flags & SUF_GLOW)
        {
            plane->glow = 4; // Default height factor is 4

            if(plane->PS_isflat)
                GL_GetFlatColor(plane->PS_texture, plane->glowrgb);
            else
                GL_GetTextureColor(plane->PS_texture, plane->glowrgb);
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
            updateReverb = true;
        }
    }

    if(updateReverb)
    {
        S_CalcSectorReverb(sec);
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
