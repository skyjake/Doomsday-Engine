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

// $smoothplane: Maximum speed for a smoothed plane.
#define MAX_SMOOTH_PLANE_MOVE   (64)

// $smoothmatoffset: Maximum speed for a smoothed material offset.
#define MAX_SMOOTH_MATERIAL_MOVE (64)

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

void R_AddWatchedSurface(watchedsurfacelist_t *wsl, surface_t *suf)
{
    uint                i;

    if(!wsl || !suf)
        return;

    // Check whether we are already tracking this surface.
    for(i = 0; i < wsl->num; ++i)
        if(wsl->list[i] == suf)
            return; // Yes we are.

    wsl->num++;

    // Only allocate memory when it's needed.
    if(wsl->num > wsl->maxNum)
    {
        wsl->maxNum *= 2;

        // The first time, allocate 8 watched surface nodes.
        if(!wsl->maxNum)
            wsl->maxNum = 8;

        wsl->list =
            Z_Realloc(wsl->list, sizeof(surface_t*) * (wsl->maxNum + 1),
                      PU_LEVEL);
    }

    // Add the surface to the list.
    wsl->list[wsl->num-1] = suf;
    wsl->list[wsl->num] = NULL; // Terminate.
}

boolean R_RemoveWatchedSurface(watchedsurfacelist_t *wsl,
                               const surface_t *suf)
{
    uint            i;

    if(!wsl || !suf)
        return false;

    for(i = 0; i < wsl->num; ++i)
    {
        if(wsl->list[i] == suf)
        {
            if(i == wsl->num - 1)
                wsl->list[i] = NULL;
            else
                memmove(&wsl->list[i], &wsl->list[i+1],
                        sizeof(surface_t*) * (wsl->num - 1 - i));
            wsl->num--;
            return true;
        }
    }

    return false;
}

/**
 * $smoothmatoffset: Roll the surface material offset tracker buffers.
 */
void R_UpdateWatchedSurfaces(watchedsurfacelist_t *wsl)
{
    uint                i;

    if(!wsl)
        return;

    for(i = 0; i < wsl->num; ++i)
    {
        surface_t          *suf = wsl->list[i];

        // X Offset
        suf->oldOffset[0][0] = suf->oldOffset[1][0];
        suf->oldOffset[1][0] = suf->offset[0];
        if(suf->oldOffset[0][0] != suf->oldOffset[1][0])
            if(fabs(suf->oldOffset[0][0] - suf->oldOffset[1][0]) >=
               MAX_SMOOTH_MATERIAL_MOVE)
            {
                // Too fast: make an instantaneous jump.
                suf->oldOffset[0][0] = suf->oldOffset[1][0];
            }

        // Y Offset
        suf->oldOffset[0][1] = suf->oldOffset[1][1];
        suf->oldOffset[1][1] = suf->offset[1];
        if(suf->oldOffset[0][1] != suf->oldOffset[1][1])
            if(fabs(suf->oldOffset[0][1] - suf->oldOffset[1][1]) >=
               MAX_SMOOTH_MATERIAL_MOVE)
            {
                // Too fast: make an instantaneous jump.
                suf->oldOffset[0][1] = suf->oldOffset[1][1];
            }
    }
}

/**
 * $smoothmatoffset: interpolate the visual offset.
 */
void R_InterpolateWatchedSurfaces(watchedsurfacelist_t *wsl,
                                  boolean resetNextViewer)
{
    uint                i;
    surface_t          *suf;

    if(!wsl)
        return;

    if(resetNextViewer)
    {
        // Reset the material offset trackers.
        for(i = 0; i < wsl->num; ++i)
        {
            suf = wsl->list[i];

            // X Offset.
            suf->visOffsetDelta[0] = 0;
            suf->oldOffset[0][0] = suf->oldOffset[1][0] = suf->offset[0];

            // X Offset.
            suf->visOffsetDelta[1] = 0;
            suf->oldOffset[0][1] = suf->oldOffset[1][1] = suf->offset[1];

            Surface_Update(suf);

            if(R_RemoveWatchedSurface(wsl, suf))
                i = (i > 0? i-1 : 0);
        }
    }
    // While the game is paused there is no need to calculate any
    // visual material offsets.
    else //if(!clientPaused)
    {
        // Set the visible material offsets.
        for(i = 0; i < wsl->num; ++i)
        {
            suf = wsl->list[i];

            // X Offset.
            suf->visOffsetDelta[0] =
                suf->oldOffset[0][0] * (1 - frameTimePos) +
                        suf->offset[0] * frameTimePos - suf->offset[0];

            // Y Offset.
            suf->visOffsetDelta[1] =
                suf->oldOffset[0][1] * (1 - frameTimePos) +
                        suf->offset[1] * frameTimePos - suf->offset[1];

            // Visible material offset.
            suf->visOffset[0] = suf->offset[0] + suf->visOffsetDelta[0];
            suf->visOffset[1] = suf->offset[1] + suf->visOffsetDelta[1];

            Surface_Update(suf);

            // Has this material reached its destination?
            if(suf->visOffset[0] == suf->offset[0] &&
               suf->visOffset[1] == suf->offset[1])
                if(R_RemoveWatchedSurface(wsl, suf))
                    i = (i > 0? i-1 : 0);
        }
    }
}

void R_AddWatchedPlane(watchedplanelist_t *wpl, plane_t *pln)
{
    uint                i;

    if(!wpl || !pln)
        return;

    // Check whether we are already tracking this plane.
    for(i = 0; i < wpl->num; ++i)
        if(wpl->list[i] == pln)
            return; // Yes we are.

    wpl->num++;

    // Only allocate memory when it's needed.
    if(wpl->num > wpl->maxNum)
    {
        wpl->maxNum *= 2;

        // The first time, allocate 8 watched plane nodes.
        if(!wpl->maxNum)
            wpl->maxNum = 8;

        wpl->list =
            Z_Realloc(wpl->list, sizeof(plane_t*) * (wpl->maxNum + 1),
                      PU_LEVEL);
    }

    // Add the plane to the list.
    wpl->list[wpl->num-1] = pln;
    wpl->list[wpl->num] = NULL; // Terminate.
}

boolean R_RemoveWatchedPlane(watchedplanelist_t *wpl, const plane_t *pln)
{
    uint            i;

    if(!wpl || !pln)
        return false;

    for(i = 0; i < wpl->num; ++i)
    {
        if(wpl->list[i] == pln)
        {
            if(i == wpl->num - 1)
                wpl->list[i] = NULL;
            else
                memmove(&wpl->list[i], &wpl->list[i+1],
                        sizeof(plane_t*) * (wpl->num - 1 - i));
            wpl->num--;
            return true;
        }
    }

    return false;
}

/**
 * $smoothplane: Roll the height tracker buffers.
 */
void R_UpdateWatchedPlanes(watchedplanelist_t *wpl)
{
    uint                i;

    if(!wpl)
        return;

    for(i = 0; i < wpl->num; ++i)
    {
        plane_t        *pln = wpl->list[i];

        pln->oldHeight[0] = pln->oldHeight[1];
        pln->oldHeight[1] = pln->height;

        if(pln->oldHeight[0] != pln->oldHeight[1])
            if(fabs(pln->oldHeight[0] - pln->oldHeight[1]) >=
               MAX_SMOOTH_PLANE_MOVE)
            {
                // Too fast: make an instantaneous jump.
                pln->oldHeight[0] = pln->oldHeight[1];
            }
    }
}

/**
 * $smoothplane: interpolate the visual offset.
 */
void R_InterpolateWatchedPlanes(watchedplanelist_t *wpl,
                                boolean resetNextViewer)
{
    uint                i;
    plane_t            *pln;

    if(!wpl)
        return;

    if(resetNextViewer)
    {
        // $smoothplane: Reset the plane height trackers.
        for(i = 0; i < wpl->num; ++i)
        {
            pln = wpl->list[i];

            pln->visHeightDelta = 0;
            pln->oldHeight[0] = pln->oldHeight[1] = pln->height;

            if(pln->type == PLN_FLOOR || pln->type == PLN_CEILING)
            {
                R_MarkDependantSurfacesForDecorationUpdate(pln);
            }

            if(R_RemoveWatchedPlane(wpl, pln))
                i = (i > 0? i-1 : 0);
        }
    }
    // While the game is paused there is no need to calculate any
    // visual plane offsets $smoothplane.
    else //if(!clientPaused)
    {
        // $smoothplane: Set the visible offsets.
        for(i = 0; i < wpl->num; ++i)
        {
            pln = wpl->list[i];

            pln->visHeightDelta = pln->oldHeight[0] * (1 - frameTimePos) +
                        pln->height * frameTimePos -
                        pln->height;

            // Visible plane height.
            pln->visHeight = pln->height + pln->visHeightDelta;

            if(pln->type == PLN_FLOOR || pln->type == PLN_CEILING)
            {
                R_MarkDependantSurfacesForDecorationUpdate(pln);
            }

            // Has this plane reached its destination?
            if(pln->visHeight == pln->height)
                if(R_RemoveWatchedPlane(wpl, pln))
                    i = (i > 0? i-1 : 0);
        }
    }
}

/**
 * Called when a floor or ceiling height changes to update the plotted
 * decoration origins for surfaces whose material offset is dependant upon
 * the given plane.
 */
void R_MarkDependantSurfacesForDecorationUpdate(plane_t *pln)
{
    linedef_t         **linep;

    // Mark the decor lights on the sides of this plane as requiring
    // an update.
    linep = pln->sector->lineDefs;
    while(*linep)
    {
        linedef_t          *li = *linep;

        if(!li->L_backside)
        {
            if(pln->type != PLN_MID)
                Surface_Update(&li->L_frontside->SW_surface(SEG_MIDDLE));
        }
        else if(li->L_backsector != li->L_frontsector)
        {
            byte                side =
                (li->L_frontsector == pln->sector? FRONT : BACK);

            Surface_Update(&li->L_side(side)->SW_surface(SEG_BOTTOM));
            Surface_Update(&li->L_side(side)->SW_surface(SEG_TOP));

            if(pln->type == PLN_FLOOR)
                Surface_Update(&li->L_side(side^1)->SW_surface(SEG_BOTTOM));
            else
                Surface_Update(&li->L_side(side^1)->SW_surface(SEG_TOP));
        }

        *linep++;
    }
}

/**
 * Create a new plane for the given sector. The plane will be initialized
 * with default values.
 *
 * Post: The sector's plane list will be replaced, the new plane will be
 *       linked to the end of the list.
 *
 * @param sec       Sector for which a new plane will be created.
 *
 * @return          Ptr to the newly created plane.
 */
plane_t *R_NewPlaneForSector(sector_t *sec)
{
    surface_t      *suf;
    plane_t        *plane;

    if(!sec)
        return NULL; // Do wha?

    //if(sec->planeCount >= 2)
    //    Con_Error("P_NewPlaneForSector: Cannot create plane for sector, "
    //              "limit is %i per sector.\n", 2);

    // Allocate the new plane.
    plane = Z_Malloc(sizeof(plane_t), PU_LEVEL, 0);

    // Resize this sector's plane list.
    sec->planes =
        Z_Realloc(sec->planes, sizeof(plane_t*) * (++sec->planeCount + 1),
                  PU_LEVEL);
    // Add the new plane to the end of the list.
    sec->planes[sec->planeCount-1] = plane;
    sec->planes[sec->planeCount] = NULL; // Terminate.

    // Setup header for DMU.
    plane->header.type = DMU_PLANE;

    // Initalize the plane.
    plane->glowRGB[CR] = plane->glowRGB[CG] = plane->glowRGB[CB] = 1;
    plane->glow = 0;
    plane->sector = sec;
    plane->height = plane->oldHeight[0] = plane->oldHeight[1] = 0;
    plane->visHeight = plane->visHeightDelta = 0;
    plane->soundOrg.pos[VX] = sec->soundOrg.pos[VX];
    plane->soundOrg.pos[VY] = sec->soundOrg.pos[VY];
    plane->soundOrg.pos[VZ] = sec->soundOrg.pos[VZ];
    memset(&plane->soundOrg.thinker, 0, sizeof(plane->soundOrg.thinker));
    plane->speed = 0;
    plane->target = 0;
    plane->type = PLN_FLOOR;

    suf = &plane->surface;

    // Setup header for DMU.
    suf->header.type = DMU_SURFACE;

    // Initialize the surface.
    suf->flags = suf->oldFlags = 0;
    suf->decorations = NULL;
    suf->numDecorations = 0;
    suf->frameFlags = 0;
    suf->normal[VX] = suf->oldNormal[VX] = 0;
    suf->normal[VY] = suf->oldNormal[VY] = 0;
    suf->normal[VZ] = suf->oldNormal[VZ] = 1;
    // \todo The initial material should be the "unknown" material.
    Surface_SetMaterial(suf, NULL);
    Surface_SetMaterialOffsetXY(suf, 0, 0);
    Surface_SetColorRGBA(suf, 1, 1, 1, 1);
    Surface_SetBlendMode(suf, BM_NORMAL);

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
    plane_t            *plane, **newList = NULL;

    if(!sec)
        return; // Do wha?

    if(id >= sec->planeCount)
        Con_Error("P_DestroyPlaneOfSector: Plane id #%i is not valid for "
                  "sector #%u", id, (uint) GET_SECTOR_IDX(sec));

    plane = sec->planes[id];

    // Create a new plane list?
    if(sec->planeCount > 1)
    {
        uint                i, n;

        newList = Z_Malloc(sizeof(plane_t**) * sec->planeCount, PU_LEVEL, 0);

        // Copy ptrs to the planes.
        n = 0;
        for(i = 0; i < sec->planeCount; ++i)
        {
            if(i == id)
                continue;
            newList[n++] = sec->planes[i];
        }
        newList[n] = NULL; // Terminate.
    }

    // If this plane is currently being watched, remove it.
    R_RemoveWatchedPlane(watchedPlaneList, plane);
    // If this plane's surafce is currently being watched, remove it.
    R_RemoveWatchedSurface(watchedSurfaceList, &plane->surface);

    // Destroy the specified plane.
    Z_Free(plane);
    sec->planeCount--;

    // Link the new list to the sector.
    Z_Free(sec->planes);
    sec->planes = newList;
}

surfacedecor_t* R_CreateSurfaceDecoration(decortype_t type, surface_t *suf,
                                          const float *pos)
{
    uint                i;
    surfacedecor_t     *d, *s, *decorations;

    if(!suf)
        return NULL;

    decorations =
        Z_Malloc(sizeof(*decorations) * (++suf->numDecorations),
                 PU_LEVEL, 0);

    if(suf->numDecorations > 1)
    {   // Copy the existing decorations.
        for(i = 0; i < suf->numDecorations - 1; ++i)
        {
            d = &decorations[i];
            s = &suf->decorations[i];

            memcpy(d, s, sizeof(*d));
        }

        Z_Free(suf->decorations);
    }

    // Add the new decoration.
    d = &decorations[suf->numDecorations - 1];
    d->type = type;
    d->pos[VX] = pos[VX];
    d->pos[VY] = pos[VY];
    d->pos[VZ] = pos[VZ];

    suf->decorations = decorations;

    return d;
}

void R_ClearSurfaceDecorations(surface_t *suf)
{
    if(!suf)
        return;

    if(suf->decorations)
        Z_Free(suf->decorations);
    suf->decorations = NULL;
    suf->numDecorations = 0;
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
    uint                i;
    boolean             hackfloor, hackceil;
    sector_t           *floorLinkCandidate = 0, *ceilLinkCandidate = 0;

    // Must have a valid sector!
    if(!sec || !sec->lineDefCount || (sec->flags & SECF_PERMANENTLINK))
        return; // Can't touch permanent links.

    hackfloor = (!R_IsSkySurface(&sec->SP_floorsurface));
    hackceil  = (!R_IsSkySurface(&sec->SP_ceilsurface));
    if(!(hackfloor || hackceil))
        return;

    for(i = 0; i < sec->subsGroupCount; ++i)
    {
        subsector_t       **ssecp;
        ssecgroup_t        *ssgrp;

        if(!hackfloor && !hackceil)
            break;

        ssgrp = &sec->subsGroups[i];

        ssecp = sec->ssectors;
        while(*ssecp)
        {
            subsector_t        *ssec = *ssecp;

            if(!hackfloor && !hackceil)
                break;

            // Must be in the same group.
            if(ssec->group == i)
            {
                seg_t             **segp = ssec->segs;

                while(*segp)
                {
                    seg_t              *seg = *segp;
                    linedef_t          *lin;

                    if(!hackfloor && !hackceil)
                        break;

                    lin = seg->lineDef;

                    if(lin && // minisegs don't count.
                       lin->L_frontside && lin->L_backside) // Must be twosided.
                    {
                        sector_t           *back;
                        sidedef_t          *sid, *frontsid, *backsid;

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

                            if((sid->SW_bottommaterial && !(sid->SW_bottomflags & SUF_TEXFIX)) ||
                               (sid->SW_middlematerial && !(sid->SW_middleflags & SUF_TEXFIX)))
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

                            if((sid->SW_topmaterial && !(sid->SW_topflags & SUF_TEXFIX)) ||
                               (sid->SW_middlematerial && !(sid->SW_middleflags & SUF_TEXFIX)))
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
            if(floorLinkCandidate == sec->containSector)
                ssgrp->linked[PLN_FLOOR] = floorLinkCandidate;
        }

        if(hackceil)
        {
            if(ceilLinkCandidate == sec->containSector)
                ssgrp->linked[PLN_CEILING] = ceilLinkCandidate;
        }
    }
}

#ifdef _MSC_VER
#  pragma optimize("", on)
#endif

/**
 * Initialize the skyfix. In practice all this does is to check for mobjs
 * intersecting ceilings and if so: raises the sky fix for the sector a
 * bit to accommodate them.
 */
void R_InitSkyFix(void)
{
    float               f, b;
    float              *fix;
    uint                i;
    mobj_t             *it;
    sector_t           *sec;

    for(i = 0; i < numSectors; ++i)
    {
        sec = SECTOR_PTR(i);

        // Must have a sky ceiling.
        if(!R_IsSkySurface(&sec->SP_ceilsurface))
            continue;

        fix = &sec->skyFix[PLN_CEILING].offset;

        // Check that all the mobjs in the sector fit in.
        for(it = sec->mobjList; it; it = it->sNext)
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
    float               f, b;
    float               height = 0;
    boolean             adjusted = false;
    float              *fix = NULL;
    sector_t           *adjustSec = NULL;

    // Both the front and back surfaces must be sky on this plane.
    if(!R_IsSkySurface(&front->planes[pln]->surface) ||
       !R_IsSkySurface(&back->planes[pln]->surface))
        return false;

    f = front->planes[pln]->height + front->skyFix[pln].offset;
    b = back->planes[pln]->height + back->skyFix[pln].offset;

    if(f == b)
        return false;

    if(pln == PLN_CEILING)
    {
        if(f < b)
        {
            height = b - front->planes[pln]->height;
            fix = &front->skyFix[pln].offset;
            adjustSec = front;
        }
        else if(f > b)
        {
            height = f - back->planes[pln]->height;
            fix = &back->skyFix[pln].offset;
            adjustSec = back;
        }

        if(height > *fix)
        {   // Must increase skyfix.
           *fix = height;
            adjusted = true;
        }
    }
    else // Its the floor.
    {
        if(f > b)
        {
            height = b - front->planes[pln]->height;
            fix = &front->skyFix[pln].offset;
            adjustSec = front;
        }
        else if(f < b)
        {
            height = f - back->planes[pln]->height;
            fix = &back->skyFix[pln].offset;
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
        Con_Printf("S%ui: skyfix to %g (%s=%g)\n",
                   (uint) GET_SECTOR_IDX(adjustSec), *fix,
                   (pln == PLN_CEILING? "ceil" : "floor"),
                   adjustSec->planes[pln]->height + (*fix));
    }

    return adjusted;
}

static void spreadSkyFixForNeighbors(vertex_t *vtx, linedef_t *refLine,
                                     boolean fixFloors, boolean fixCeilings,
                                     boolean *adjustedFloor,
                                     boolean *adjustedCeil)
{
    uint                pln;
    lineowner_t        *base, *lOwner, *rOwner;
    boolean             doFix[2];
    boolean            *adjusted[2];

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

                if(doSkyFix(rOwner->lineDef->L_frontsector,
                            lOwner->lineDef->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(lOwner->lineDef->L_backside)
                if(doSkyFix(rOwner->lineDef->L_frontsector,
                            lOwner->lineDef->L_backsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->lineDef->L_backside)
                if(doSkyFix(rOwner->lineDef->L_backsector,
                            lOwner->lineDef->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->lineDef->L_backside && lOwner->lineDef->L_backside)
                if(doSkyFix(rOwner->lineDef->L_backsector,
                            lOwner->lineDef->L_frontsector, pln))
                    *adjusted[pln] = true;
            }
        }

        if(!rOwner->lineDef->L_backside)
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

                if(doSkyFix(rOwner->lineDef->L_frontsector,
                            lOwner->lineDef->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(lOwner->lineDef->L_backside)
                if(doSkyFix(rOwner->lineDef->L_frontsector,
                            lOwner->lineDef->L_backsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->lineDef->L_backside)
                if(doSkyFix(rOwner->lineDef->L_backsector,
                            lOwner->lineDef->L_frontsector, pln))
                    *adjusted[pln] = true;

                if(rOwner->lineDef->L_backside && lOwner->lineDef->L_backside)
                if(doSkyFix(rOwner->lineDef->L_backsector,
                            lOwner->lineDef->L_frontsector, pln))
                    *adjusted[pln] = true;
            }
        }

        if(!lOwner->lineDef->L_backside)
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
        for(i = 0; i < numLineDefs; ++i)
        {
            linedef_t          *line = LINE_PTR(i);

            // The conditions: must have two sides.
            if(!line->L_frontside || !line->L_backside)
                continue;

            if(line->L_frontsector != line->L_backsector)
            {
                // Perform the skyfix as usual using the front and back
                // sectors of THIS line for comparing.
                for(pln = 0; pln < 2; ++pln)
                {
                    if(doFix[pln])
                        if(doSkyFix(line->L_frontsector, line->L_backsector,
                                    pln))
                            adjusted[pln] = true;
                }
            }
            else if(LINE_SELFREF(line))
            {
                // Its a selfreferencing linedef, these will ALWAYS return
                // the same height on the front and back so we need to find
                // the neighbouring lines either side of this and compare
                // the front and back sectors of those instead.
                uint                j;
                vertex_t           *vtx[2];

                vtx[0] = line->L_v1;
                vtx[1] = line->L_v2;
                // Walk around each vertex in each direction.
                for(j = 0; j < 2; ++j)
                    if(vtx[j]->numLineOwners > 1)
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
 * @return              Ptr to the lineowner for this line for this vertex
 *                      else @c NULL.
 */
lineowner_t* R_GetVtxLineOwner(const vertex_t *v, const linedef_t *line)
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
        Rend_SkyParams(0, DD_MATERIAL, R_MaterialNumForName("SKY1", MAT_TEXTURE));
        Rend_SkyParams(0, DD_MASK, DD_NO);
        Rend_SkyParams(0, DD_OFFSET, 0);
        Rend_SkyParams(1, DD_DISABLE, 0);

        // There is no sky color.
        noSkyColorGiven = true;
        return;
    }

    Rend_SkyParams(DD_SKY, DD_HEIGHT, mapinfo->skyHeight);
    Rend_SkyParams(DD_SKY, DD_HORIZON, mapinfo->horizonOffset);
    for(i = 0; i < 2; ++i)
    {
        k = mapinfo->skyLayers[i].flags;
        if(k & SLF_ENABLED)
        {
            skyTex = R_MaterialNumForName(mapinfo->skyLayers[i].texture, MAT_TEXTURE);
            if(skyTex == -1)
            {
                Con_Message("R_SetupSky: Invalid/missing texture \"%s\"\n",
                            mapinfo->skyLayers[i].texture);
                skyTex = R_MaterialNumForName("SKY1", MAT_TEXTURE);
            }

            Rend_SkyParams(i, DD_ENABLE, 0);
            Rend_SkyParams(i, DD_MATERIAL, skyTex);
            Rend_SkyParams(i, DD_MASK, k & SLF_MASKED ? DD_YES : DD_NO);
            Rend_SkyParams(i, DD_OFFSET, mapinfo->skyLayers[i].offset);
            Rend_SkyParams(i, DD_COLOR_LIMIT,
                           mapinfo->skyLayers[i].colorLimit);
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
        skyColorRGB[i] = mapinfo->skyColor[i];
        if(mapinfo->skyColor[i] > 0)
            noSkyColorGiven = false;
    }

    // Calculate a balancing factor, so the light in the non-skylit
    // sectors won't appear too bright.
    if(mapinfo->skyColor[0] > 0 || mapinfo->skyColor[1] > 0 ||
        mapinfo->skyColor[2] > 0)
    {
        skyColorBalance =
            (0 +
             (mapinfo->skyColor[0] * 2 + mapinfo->skyColor[1] * 3 +
              mapinfo->skyColor[2] * 2) / 7) / 1;
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
void R_OrderVertices(const linedef_t *line, const sector_t *sector, vertex_t *verts[2])
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
linedef_t *R_FindLineNeighbor(const sector_t *sector, const linedef_t *line,
                              const lineowner_t *own, boolean antiClockwise,
                              binangle_t *diff)
{
    lineowner_t            *cown = own->link[!antiClockwise];
    linedef_t              *other = cown->lineDef;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

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

linedef_t *R_FindSolidLineNeighbor(const sector_t *sector,
                                   const linedef_t *line,
                                   const lineowner_t *own,
                                   boolean antiClockwise, binangle_t *diff)
{
    lineowner_t            *cown = own->link[!antiClockwise];
    linedef_t              *other = cown->lineDef;
    int                     side;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

    if(!other->L_frontside || !other->L_backside)
        return other;

    if(!LINE_SELFREF(other) &&
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
    if(other->sideDefs[side]->SW_middlematerial)
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
linedef_t *R_FindLineBackNeighbor(const sector_t *sector,
                                  const linedef_t *line,
                                  const lineowner_t *own,
                                  boolean antiClockwise,
                                  binangle_t *diff)
{
    lineowner_t        *cown = own->link[!antiClockwise];
    linedef_t          *other = cown->lineDef;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

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
linedef_t *R_FindLineAlignNeighbor(const sector_t *sec,
                                   const linedef_t *line,
                                   const lineowner_t *own,
                                   boolean antiClockwise,
                                   int alignment)
{
#define SEP 10

    lineowner_t        *cown = own->link[!antiClockwise];
    linedef_t          *other = cown->lineDef;
    binangle_t          diff;

    if(other == line)
        return NULL;

    if(!LINE_SELFREF(other))
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
    uint                i;
    uint                starttime;

    Con_Message("R_InitLinks: Initializing\n");

    // Initialize node piles and line rings.
    NP_Init(&map->mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&map->lineNodes, map->numLineDefs + 1000);

    // Allocate the rings.
    starttime = Sys_GetRealTime();
    map->lineLinks =
        Z_Malloc(sizeof(*map->lineLinks) * map->numLineDefs, PU_LEVELSTATIC, 0);
    for(i = 0; i < map->numLineDefs; ++i)
        map->lineLinks[i] = NP_New(&map->lineNodes, NP_ROOT_NODE);
    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_InitLinks: Allocating line link rings. Done in %.2f seconds.\n",
             (Sys_GetRealTime() - starttime) / 1000.0f));
}

/**
 * Create a list of vertices for the subsector which are suitable for
 * use as the points of single a trifan.
 *
 * We are assured by the node building process that subsector->segs has
 * been ordered by angle, clockwise starting from the smallest angle.
 * So, most of the time, the points can be created directly from the
 * seg vertexes.
 *
 * However, we do not want any overlapping tris so check the area of
 * each triangle is > 0, if not; try the next vertice in the list until
 * we find a good one to use as the center of the trifan. If a suitable
 * point cannot be found use the center of subsector instead (it will
 * always be valid as subsectors are convex).
 */
static void triangulateSubSector(subsector_t *ssec)
{
    uint                baseIDX = 0, i, n;
    boolean             found = false;

    // We need to find a good tri-fan base vertex, (one that doesn't
    // generate zero-area triangles).
    if(ssec->segCount <= 3)
    {   // Always valid.
        found = true;
    }
    else
    {   // Higher vertex counts need checking, we'll test each one and pick
        // the first good one.
#define TRIFAN_LIMIT    0.1

        fvertex_t          *base, *a, *b;

        baseIDX = 0;
        do
        {
            seg_t              *seg = ssec->segs[baseIDX];

            base = &seg->SG_v1->v;
            i = 0;
            do
            {
                seg_t              *seg2 = ssec->segs[i];

                if(!(baseIDX > 0 && (i == baseIDX || i == baseIDX - 1)))
                {
                    a = &seg2->SG_v1->v;
                    b = &seg2->SG_v2->v;

                    if(TRIFAN_LIMIT >=
                       M_TriangleArea(base->pos, a->pos, b->pos))
                    {
                        base = NULL;
                    }
                }

                i++;
            } while(base && i < ssec->segCount);

            if(!base)
                baseIDX++;
        } while(!base && baseIDX < ssec->segCount);

        if(base)
            found = true;
#undef TRIFAN_LIMIT
    }

    ssec->numVertices = ssec->segCount;
    if(!found)
        ssec->numVertices += 2;
    ssec->vertices =
        Z_Malloc(sizeof(fvertex_t*) * (ssec->numVertices + 1),PU_LEVEL, 0);

    // We can now create the subsector fvertex array.
    // NOTE: The same polygon is used for all planes of this subsector.
    n = 0;
    if(!found)
        ssec->vertices[n++] = &ssec->midPoint;
    for(i = 0; i < ssec->segCount; ++i)
    {
        uint                idx;
        seg_t              *seg;

        idx = baseIDX + i;
        if(idx >= ssec->segCount)
            idx = idx - ssec->segCount;
        seg = ssec->segs[idx];
        ssec->vertices[n++] = &seg->SG_v1->v;
    }
    if(!found)
        ssec->vertices[n++] = &ssec->segs[0]->SG_v1->v;
    ssec->vertices[n] = NULL; // terminate.

    if(!found)
        ssec->flags |= SUBF_MIDPOINT;
}

/**
 * Polygonizes all subsectors in the map.
 */
void R_PolygonizeMap(gamemap_t *map)
{
    uint                i;
    uint                startTime;

    startTime = Sys_GetRealTime();

    // Polygonize each subsector.
    for(i = 0; i < map->numSSectors; ++i)
    {
        subsector_t        *sub = &map->ssectors[i];
        triangulateSubSector(sub);
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("R_PolygonizeMap: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));

#ifdef _DEBUG
    Z_CheckHeap();
#endif
}

static void initPlaneIllumination(subsector_t *ssec, uint planeID)
{
    uint        i, j, num;
    subplaneinfo_t *plane = ssec->planes[planeID];

    num = ssec->numVertices;

    plane->illumination =
        Z_Calloc(num * sizeof(vertexillum_t), PU_LEVELSTATIC, NULL);

    for(i = 0; i < num; ++i)
    {
        plane->illumination[i].flags |= VIF_STILL_UNSEEN;

        for(j = 0; j < MAX_BIAS_AFFECTED; ++j)
            plane->illumination[i].casted[j].source = -1;
    }
}

static void initSSecPlanes(subsector_t *ssec)
{
    uint                i;

    // Allocate the subsector plane info array.
    ssec->planes =
        Z_Malloc(ssec->sector->planeCount * sizeof(subplaneinfo_t*),
                 PU_LEVEL, NULL);
    for(i = 0; i < ssec->sector->planeCount; ++i)
    {
        ssec->planes[i] =
            Z_Calloc(sizeof(subplaneinfo_t), PU_LEVEL, NULL);
        ssec->planes[i]->planeID = i;
        ssec->planes[i]->type = PLN_MID;

        // Initialize the illumination for the subsector.
        initPlaneIllumination(ssec, i);
    }

    // \fixme $nplanes
    // Initialize the plane types.
    ssec->planes[PLN_FLOOR]->type = PLN_FLOOR;
    ssec->planes[PLN_CEILING]->type = PLN_CEILING;
}

static void prepareSubsectorForBias(subsector_t *ssec)
{
    seg_t             **segPtr;

    initSSecPlanes(ssec);

    segPtr = ssec->segs;
    while(*segPtr)
    {
        uint                i;
        seg_t              *seg = *segPtr;

        for(i = 0; i < 4; ++i)
        {
            uint            j;

            for(j = 0; j < 3; ++j)
            {
                uint            n;

                seg->illum[j][i].flags = VIF_STILL_UNSEEN;
                for(n = 0; n < MAX_BIAS_AFFECTED; ++n)
                {
                    seg->illum[j][i].casted[n].source = -1;
                }
            }
        }

        *segPtr++;
    }
}

void R_PrepareForBias(gamemap_t *map)
{
    uint                i;
    uint                starttime = Sys_GetRealTime();

    Con_Message("prepareForBias: Processing...\n");

    for(i = 0; i < map->numSSectors; ++i)
    {
        subsector_t    *ssec = &map->ssectors[i];
        prepareSubsectorForBias(ssec);
    }

    // How much time did we spend?
    VERBOSE(Con_Message
            ("prepareForBias: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - starttime) / 1000.0f));
}

/**
 * The test is done on subsectors.
 */
static sector_t *getContainingSectorOf(gamemap_t *map, sector_t *sec)
{
    uint                i;
    float               cdiff = -1, diff;
    float               inner[4], outer[4];
    sector_t           *other, *closest = NULL;

    memcpy(inner, sec->bBox, sizeof(inner));

    // Try all sectors that fit in the bounding box.
    for(i = 0, other = map->sectors; i < map->numSectors; other++, ++i)
    {
        if(!other->lineDefCount || (other->flags & SECF_UNCLOSED))
            continue;

        if(other == sec)
            continue; // Don't try on self!

        memcpy(outer, other->bBox, sizeof(outer));
        if(inner[BOXLEFT]  >= outer[BOXLEFT] &&
           inner[BOXRIGHT] <= outer[BOXRIGHT] &&
           inner[BOXTOP]   <= outer[BOXTOP] &&
           inner[BOXBOTTOM]>= outer[BOXBOTTOM])
        {
            // Sec is totally and completely inside other!
            diff = M_BoundingBoxDiff(inner, outer);
            if(cdiff < 0 || diff <= cdiff)
            {
                closest = other;
                cdiff = diff;
            }
        }
    }
    return closest;
}

void R_BuildSectorLinks(gamemap_t *map)
{
#define DOMINANT_SIZE   1000

    uint                i;

    for(i = 0; i < map->numSectors; ++i)
    {
        uint                k;
        sector_t           *other;
        linedef_t          *lin;
        sector_t           *sec = &map->sectors[i];
        boolean             dohack;

        if(!sec->lineDefCount)
            continue;

        // Is this sector completely contained by another?
        sec->containSector = getContainingSectorOf(map, sec);

        dohack = true;
        for(k = 0; k < sec->lineDefCount; ++k)
        {
            lin = sec->lineDefs[k];
            if(!lin->L_frontside || !lin->L_backside ||
                lin->L_frontsector != lin->L_backsector)
            {
                dohack = false;
                break;
            }
        }

        if(dohack)
        {   // Link all planes permanently.
            sec->flags |= SECF_PERMANENTLINK;

            // Only floor and ceiling can be linked, not all planes inbetween.
            for(k = 0; k < sec->subsGroupCount; ++k)
            {
                ssecgroup_t *ssgrp = &sec->subsGroups[k];
                uint        p;

                for(p = 0; p < sec->planeCount; ++p)
                    ssgrp->linked[p] = sec->containSector;
            }

            Con_Printf("Linking S%i planes permanently to S%li\n", i,
                       (long) (sec->containSector - map->sectors));
        }

        // Is this sector large enough to be a dominant light source?
        if(sec->lightSource == NULL && sec->planeCount > 0 &&
           sec->bBox[BOXRIGHT] - sec->bBox[BOXLEFT]   > DOMINANT_SIZE &&
           sec->bBox[BOXTOP]   - sec->bBox[BOXBOTTOM] > DOMINANT_SIZE)
        {
            if(R_SectorContainsSkySurfaces(sec))
            {
                // All sectors touching this one will be affected.
                for(k = 0; k < sec->lineDefCount; ++k)
                {
                    lin = sec->lineDefs[k];
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

                    other->lightSource = sec;
                }
            }
        }
    }

#undef DOMINANT_SIZE
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
        // Loading a game usually destroys all thinkers. Until a proper
        // savegame system handled by the engine is introduced we'll have
        // to resort to re-initializing the most important stuff.
        P_SpawnTypeParticleGens();

        // Update everything again. Its possible that after loading we
        // now have more HOMs to fix, etc..

        R_SkyFix(true, true); // fix floors and ceilings.

        // Update all sectors. Set intial values of various tracked
        // and interpolated properties (lighting, smoothed planes etc).
        for(i = 0; i < numSectors; ++i)
        {
            uint            j;
            sector_t       *sec = SECTOR_PTR(i);

            R_UpdateSector(sec, false);
            for(j = 0; j < sec->planeCount; ++j)
            {
                sec->planes[j]->visHeight = sec->planes[j]->oldHeight[0] =
                    sec->planes[j]->oldHeight[1] = sec->planes[j]->height;
            }
        }

        // We don't render fakeradio on polyobjects...
        PO_InitForMap();
        return;
    }
    case DDSLM_FINALIZE:
    {
        gamemap_t          *map = P_GetCurrentMap();

        // We are now finished with the game data, map object db.
        P_DestroyGameMapObjDB(&map->gameObjData);

        // Init server data.
        Sv_InitPools();

        // Recalculate the light range mod matrix.
        Rend_CalcLightModRange(NULL);

        // Update all sectors. Set intial values of various tracked
        // and interpolated properties (lighting, smoothed planes etc).
        for(i = 0; i < numSectors; ++i)
        {
            uint            l;
            sector_t       *sec = SECTOR_PTR(i);

            R_UpdateSector(sec, true);
            for(l = 0; l < sec->planeCount; ++l)
            {
                sec->planes[l]->visHeight = sec->planes[l]->oldHeight[0] =
                    sec->planes[l]->oldHeight[1] = sec->planes[l]->height;
            }
        }

        // We don't render fakeradio on polyobjects...
        PO_InitForMap();

        // Run any commands specified in Map Info.
        {
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

        // Kill all local commands and determine the invoid status of players.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t           *plr = &ddPlayers[i];
            ddplayer_t         *ddpl = &plr->shared;

            clients[i].numTics = 0;

            // Determine if the player is in the void.
            ddpl->inVoid = true;
            if(ddpl->mo)
            {
                subsector_t        *ssec =
                    R_PointInSubsector(ddpl->mo->pos[VX],
                                       ddpl->mo->pos[VY]);

                //// \fixme $nplanes
                if(ssec &&
                   ddpl->mo->pos[VZ] > ssec->sector->SP_floorheight + 4 &&
                   ddpl->mo->pos[VZ] < ssec->sector->SP_ceilheight - 4)
                   ddpl->inVoid = false;
            }
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
            R_SetupFog(mapInfo->fogStart, mapInfo->fogEnd,
                       mapInfo->fogDensity, mapInfo->fogColor);
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

    for(i = 0; i < numSectors; ++i)
    {
        sec = SECTOR_PTR(i);
        // Clear all flags that can be cleared before each frame.
        sec->frameFlags &= ~SIF_FRAME_CLEAR;
    }
}

sector_t *R_GetLinkedSector(subsector_t *startssec, uint plane)
{
    ssecgroup_t        *ssgrp =
        &startssec->sector->subsGroups[startssec->group];

    if(!devNoLinkedSurfaces && ssgrp->linked[plane])
        return ssgrp->linked[plane];
    else
        return startssec->sector;
}

/**
 * Is the specified plane glowing (it glows or is a sky mask surface)?
 *
 * @return              @c true, if the specified plane is non-glowing,
 *                      i.e. not glowing or a sky.
 */
boolean R_IsGlowingPlane(const plane_t *pln)
{
    material_t         *mat = pln->surface.material;

    return (!(mat && mat->ofTypeID > 0) || pln->glow > 0 ||
            R_IsSkySurface(&pln->surface));
}

/**
 * Does the specified sector contain any sky surfaces?
 *
 * @return              @c true, if one or more surfaces in the given sector
 *                      use the special sky mask material.
 */
boolean R_SectorContainsSkySurfaces(const sector_t *sec)
{
    uint                i;
    boolean             sectorContainsSkySurfaces;

    // Does this sector feature any sky surfaces?
    sectorContainsSkySurfaces = false;
    i = 0;
    do
    {
        if(R_IsSkySurface(&sec->SP_planesurface(i)))
            sectorContainsSkySurfaces = true;
        else
            i++;
    } while(!sectorContainsSkySurfaces && i < sec->planeCount);

    return sectorContainsSkySurfaces;
}

void R_UpdateSector(sector_t* sec, boolean forceUpdate)
{
    uint                i, j;
    plane_t            *plane;
    boolean             updateReverb = false;
    boolean             updateDecorations = false;
    boolean             hasGlow;

    // Check if there are any lightlevel or color changes.
    if(forceUpdate ||
       (sec->lightLevel != sec->oldLightLevel ||
        sec->rgb[0] != sec->oldRGB[0] ||
        sec->rgb[1] != sec->oldRGB[1] ||
        sec->rgb[2] != sec->oldRGB[2]))
    {
        sec->frameFlags |= SIF_LIGHT_CHANGED;
        sec->oldLightLevel = sec->lightLevel;
        memcpy(sec->oldRGB, sec->rgb, sizeof(sec->oldRGB));

        LG_SectorChanged(sec);
    }
    else
    {
        sec->frameFlags &= ~SIF_LIGHT_CHANGED;
    }

    // For each plane.
    for(i = 0; i < sec->planeCount; ++i)
    {
        plane = sec->planes[i];

        // \fixme Now update the glow properties.
        hasGlow = false;
        if(plane->surface.flags & SUF_GLOW)
        {
            if(R_GetMaterialColor(plane->PS_material, plane->glowRGB))
            {
                hasGlow = true;
            }
        }

        if(hasGlow)
        {
            plane->glow = 4; // Default height factor is 4
        }
        else
        {
            plane->glowRGB[CR] = plane->glowRGB[CG] =
                plane->glowRGB[CB] = 0;
            plane->glow = 0;
        }
        // < FIXME

        // Geometry change?
        if(forceUpdate ||
           plane->height != plane->oldHeight[1])
        {
            // Check if there are any camera players in this sector. If their
            // height is now above the ceiling/below the floor they are now in
            // the void.
            for(j = 0; j < DDMAXPLAYERS; ++j)
            {
                player_t               *plr = &ddPlayers[j];
                ddplayer_t             *ddpl = &plr->shared;

                if(!ddpl->inGame || !ddpl->mo || !ddpl->mo->subsector)
                    continue;

                //// \fixme $nplanes
                if((ddpl->flags & DDPF_CAMERA) &&
                   ddpl->mo->subsector->sector == sec &&
                   (ddpl->mo->pos[VZ] > sec->SP_ceilheight ||
                    ddpl->mo->pos[VZ] < sec->SP_floorheight))
                {
                    ddpl->inVoid = true;
                }
            }

            P_PlaneChanged(sec, i);
            updateReverb = true;
            updateDecorations = true;
        }

        if(updateDecorations)
            Surface_Update(&plane->surface);
    }

    if(updateReverb)
    {
        S_CalcSectorReverb(sec);
    }

    if(!(sec->flags & SECF_PERMANENTLINK)) // Assign new links
    {
        // Only floor and ceiling can be linked, not all in between.
        for(i = 0; i < sec->subsGroupCount; ++i)
        {
            ssecgroup_t *ssgrp = &sec->subsGroups[i];

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
const float *R_GetSectorLightColor(const sector_t *sector)
{
    if(!rendSkyLight || noSkyColorGiven)
        return sector->rgb; // The sector's real color.

    if(!R_SectorContainsSkySurfaces(sector))
    {
        uint                c;
        sector_t           *src;

        // A dominant light source affects this sector?
        src = sector->lightSource;
        if(src && src->lightLevel >= sector->lightLevel)
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
            for(c = 0; c < 3; ++c)
                balancedRGB[c] = sector->rgb[c] * skyColorBalance;
            return balancedRGB;
        }
    }

    // Return the sky color.
    return skyColorRGB;
}
