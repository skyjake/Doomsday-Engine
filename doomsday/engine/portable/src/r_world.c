/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
#define MAX_SMOOTH_MATERIAL_MOVE (8)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int rendSkyLight = 1; // cvar.
float rendLightWallAngle = 1.2f; // Intensity of angle-based wall lighting.
byte rendLightWallAngleSmooth = true;

boolean firstFrameAfterLoad;
boolean ddMapSetup;

nodeindex_t* linelinks; // Indices to roots.

skyfix_t skyFix[2];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean noSkyColorGiven;
static float skyColorRGB[4], balancedRGB[4];
static float skyColorBalance;

static surfacelistnode_t* unusedSurfaceListNodes = NULL;

// CODE --------------------------------------------------------------------

/**
 * Allocate a new surface list node.
 */
static surfacelistnode_t* allocListNode(void)
{
    surfacelistnode_t*      node = Z_Calloc(sizeof(*node), PU_STATIC, 0);
    return node;
}

/**
 * Free all memory acquired for the given surface list node.
 */
static void freeListNode(surfacelistnode_t* node)
{
    if(node)
        Z_Free(node);
}

surfacelistnode_t* R_SurfaceListNodeCreate(void)
{
    surfacelistnode_t*      node;

    // Is there a free node in the unused list?
    if(unusedSurfaceListNodes)
    {
        node = unusedSurfaceListNodes;
        unusedSurfaceListNodes = node->next;
    }
    else
    {
        node = allocListNode();
    }

    node->data = NULL;
    node->next = NULL;

    return node;
}

void R_SurfaceListNodeDestroy(surfacelistnode_t* node)
{
    // Move it to the list of unused nodes.
    node->data = NULL;
    node->next = unusedSurfaceListNodes;
    unusedSurfaceListNodes = node;
}

/**
 * Adds the surface to the given surface list.
 *
 * @param sl            The surface list to add the surface to.
 * @param suf           The surface to add to the list.
 */
void R_SurfaceListAdd(surfacelist_t* sl, surface_t* suf)
{
    surfacelistnode_t*  node;

    if(!sl || !suf)
        return;

    // Check whether this surface is already in the list.
    node = sl->head;
    while(node)
    {
        if((surface_t*) node->data == suf)
            return; // Yep.
        node = node->next;
    }

    // Not found, add it to the list.
    node = R_SurfaceListNodeCreate();
    node->data = suf;
    node->next = sl->head;

    sl->head = node;
    sl->num++;
}

boolean R_SurfaceListRemove(surfacelist_t* sl, const surface_t *suf)
{
    surfacelistnode_t*  last, *n;

    if(!sl || !suf)
        return false;

    last = sl->head;
    if(last)
    {
        n = last->next;
        while(n)
        {
            if((surface_t*) n->data == suf)
            {
                last->next = n->next;
                R_SurfaceListNodeDestroy(n);
                sl->num--;
                return true;
            }

            last = n;
            n = n->next;
        }
    }

    return false;
}

/**
 * Iterate the list of surfaces making a callback for each.
 *
 * @param sl            The surface list to iterate.
 * @param callback      The callback to make. Iteration will continue
 *                      until a callback returns a zero value.
 * @param context       Is passed to the callback function.
 */
boolean R_SurfaceListIterate(surfacelist_t* sl,
                             boolean (*callback) (surface_t* suf, void*),
                             void* context)
{
    boolean             result = true;
    surfacelistnode_t*  n, *np;

    if(sl)
    {
        n = sl->head;
        while(n)
        {
            np = n->next;
            if((result = callback((surface_t*) n->data, context)) == 0)
                break;
            n = np;
        }
    }

    return result;
}

boolean updateMovingSurface(surface_t* suf, void* context)
{
    // X Offset
    suf->oldOffset[0][0] = suf->oldOffset[0][1];
    suf->oldOffset[0][1] = suf->offset[0];
    if(suf->oldOffset[0][0] != suf->oldOffset[0][1])
        if(fabs(suf->oldOffset[0][0] - suf->oldOffset[0][1]) >=
           MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            suf->oldOffset[0][0] = suf->oldOffset[0][1];
        }

    // Y Offset
    suf->oldOffset[1][0] = suf->oldOffset[1][1];
    suf->oldOffset[1][1] = suf->offset[1];
    if(suf->oldOffset[1][0] != suf->oldOffset[1][1])
        if(fabs(suf->oldOffset[1][0] - suf->oldOffset[1][1]) >=
           MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            suf->oldOffset[1][0] = suf->oldOffset[1][1];
        }

    return true;
}

/**
 * $smoothmatoffset: Roll the surface material offset tracker buffers.
 */
void R_UpdateMovingSurfaces(void)
{
    if(!movingSurfaceList)
        return;

    R_SurfaceListIterate(movingSurfaceList, updateMovingSurface, NULL);
}

boolean resetMovingSurface(surface_t* suf, void* context)
{
    // X Offset.
    suf->visOffsetDelta[0] = 0;
    suf->oldOffset[0][0] = suf->oldOffset[0][1] = suf->offset[0];

    // Y Offset.
    suf->visOffsetDelta[1] = 0;
    suf->oldOffset[1][0] = suf->oldOffset[1][1] = suf->offset[1];

    Surface_Update(suf);
    R_SurfaceListRemove(movingSurfaceList, suf);

    return true;
}

boolean interpMovingSurface(surface_t* suf, void* context)
{
    // X Offset.
    suf->visOffsetDelta[0] =
        suf->oldOffset[0][0] * (1 - frameTimePos) +
                suf->offset[0] * frameTimePos - suf->offset[0];

    // Y Offset.
    suf->visOffsetDelta[1] =
        suf->oldOffset[1][0] * (1 - frameTimePos) +
                suf->offset[1] * frameTimePos - suf->offset[1];

    // Visible material offset.
    suf->visOffset[0] = suf->offset[0] + suf->visOffsetDelta[0];
    suf->visOffset[1] = suf->offset[1] + suf->visOffsetDelta[1];

    Surface_Update(suf);

    // Has this material reached its destination?
    if(suf->visOffset[0] == suf->offset[0] &&
       suf->visOffset[1] == suf->offset[1])
        R_SurfaceListRemove(movingSurfaceList, suf);

    return true;
}

/**
 * $smoothmatoffset: interpolate the visual offset.
 */
void R_InterpolateMovingSurfaces(boolean resetNextViewer)
{
    if(!movingSurfaceList)
        return;

    if(resetNextViewer)
    {
        // Reset the material offset trackers.
        R_SurfaceListIterate(movingSurfaceList, resetMovingSurface, NULL);
    }
    // While the game is paused there is no need to calculate any
    // visual material offsets.
    else //if(!clientPaused)
    {
        // Set the visible material offsets.
        R_SurfaceListIterate(movingSurfaceList, interpMovingSurface, NULL);
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
                      PU_MAP);
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
void R_MarkDependantSurfacesForDecorationUpdate(plane_t* pln)
{
    linedef_t**         linep;

    if(!pln || !pln->sector->lineDefs)
        return;

    // Mark the decor lights on the sides of this plane as requiring
    // an update.
    linep = pln->sector->lineDefs;

    while(*linep)
    {
        linedef_t*          li = *linep;

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
 * @param sec           Sector for which a new plane will be created.
 *
 * @return              Ptr to the newly created plane.
 */
plane_t* R_NewPlaneForSector(sector_t* sec)
{
    surface_t*          suf;
    plane_t*            plane;
    subsector_t**       ssecPtr;

    if(!sec)
        return NULL; // Do wha?

    //if(sec->planeCount >= 2)
    //    Con_Error("P_NewPlaneForSector: Cannot create plane for sector, "
    //              "limit is %i per sector.\n", 2);

    // Allocate the new plane.
    plane = Z_Malloc(sizeof(plane_t), PU_MAP, 0);

    // Resize this sector's plane list.
    sec->planes =
        Z_Realloc(sec->planes, sizeof(plane_t*) * (++sec->planeCount + 1),
                  PU_MAP);
    // Add the new plane to the end of the list.
    sec->planes[sec->planeCount-1] = plane;
    sec->planes[sec->planeCount] = NULL; // Terminate.

    // Setup header for DMU.
    plane->header.type = DMU_PLANE;

    // Initalize the plane.
    plane->surface.owner = (void*) plane;
    plane->sector = sec;
    plane->height = plane->oldHeight[0] = plane->oldHeight[1] = 0;
    plane->visHeight = plane->visHeightDelta = 0;
    plane->soundOrg.pos[VX] = sec->soundOrg.pos[VX];
    plane->soundOrg.pos[VY] = sec->soundOrg.pos[VY];
    plane->soundOrg.pos[VZ] = sec->soundOrg.pos[VZ];
    memset(&plane->soundOrg.thinker, 0, sizeof(plane->soundOrg.thinker));
    plane->speed = 0;
    plane->target = 0;
    plane->type = PLN_MID;
    plane->planeID = sec->planeCount-1;

    // Initialize the surface.
    memset(&plane->surface, 0, sizeof(plane->surface));
    suf = &plane->surface;
    suf->header.type = DMU_SURFACE; // Setup header for DMU.
    suf->normal[VZ] = suf->oldNormal[VZ] = 1;
    // \todo The initial material should be the "unknown" material.
    Surface_SetMaterial(suf, NULL);
    Surface_SetMaterialOffsetXY(suf, 0, 0);
    Surface_SetColorRGBA(suf, 1, 1, 1, 1);
    Surface_SetBlendMode(suf, BM_NORMAL);

    /**
     * Resize the biassurface lists for the subsector planes.
     * If we are in map setup mode, don't create the biassurfaces now,
     * as planes are created before the bias system is available.
     */

    ssecPtr = sec->ssectors;
    while(*ssecPtr)
    {
        subsector_t*        ssec = *ssecPtr;
        biassurface_t**     newList;
        uint                n = 0;

        newList = Z_Calloc(sec->planeCount * sizeof(biassurface_t*),
                           PU_MAP, NULL);
        // Copy the existing list?
        if(ssec->bsuf)
        {
            for(; n < sec->planeCount - 1; ++n)
            {
                newList[n] = ssec->bsuf[n];
            }
            Z_Free(ssec->bsuf);
        }

        if(!ddMapSetup)
        {
            uint                i;
            biassurface_t*      bsuf = SB_CreateSurface();

            bsuf->size = ssec->numVertices;
            bsuf->illum = Z_Calloc(sizeof(vertexillum_t) * bsuf->size, PU_MAP, 0);

            for(i = 0; i < bsuf->size; ++i)
                SB_InitVertexIllum(&bsuf->illum[i]);

            newList[n] = bsuf;
        }

        ssec->bsuf = newList;

        *ssecPtr++;
    }

    return plane;
}

/**
 * Permanently destroys the specified plane of the given sector.
 * The sector's plane list is updated accordingly.
 *
 * @param id            The sector, plane id to be destroyed.
 * @param sec           Ptr to sector for which a plane will be destroyed.
 */
void R_DestroyPlaneOfSector(uint id, sector_t* sec)
{
    uint                i;
    plane_t*            plane, **newList = NULL;
    subsector_t**       ssecPtr;

    if(!sec)
        return; // Do wha?

    if(id >= sec->planeCount)
        Con_Error("P_DestroyPlaneOfSector: Plane id #%i is not valid for "
                  "sector #%u", id, (uint) GET_SECTOR_IDX(sec));

    plane = sec->planes[id];

    // Create a new plane list?
    if(sec->planeCount > 1)
    {
        uint                n;

        newList = Z_Malloc(sizeof(plane_t**) * sec->planeCount, PU_MAP, 0);

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
    // If this plane's surface is in the moving list, remove it.
    R_SurfaceListRemove(movingSurfaceList, &plane->surface);
    // If this plane's surface is in the deocrated list, remove it.
    R_SurfaceListRemove(decoratedSurfaceList, &plane->surface);
    // If this plane's surface is in the glowing list, remove it.
    R_SurfaceListRemove(glowingSurfaceList, &plane->surface);

    // Destroy the biassurfaces for this plane.
    ssecPtr = sec->ssectors;
    while(*ssecPtr)
    {
        subsector_t*        ssec = *ssecPtr;

        SB_DestroySurface(ssec->bsuf[id]);
        if(id < sec->planeCount)
            memmove(ssec->bsuf + id, ssec->bsuf + id + 1, sizeof(biassurface_t*));
        *ssecPtr++;
    }

    // Destroy the specified plane.
    Z_Free(plane);
    sec->planeCount--;

    // Link the new list to the sector.
    Z_Free(sec->planes);
    sec->planes = newList;
}

surfacedecor_t* R_CreateSurfaceDecoration(decortype_t type, surface_t *suf)
{
    uint                i;
    surfacedecor_t     *d, *s, *decorations;

    if(!suf)
        return NULL;

    decorations =
        Z_Malloc(sizeof(*decorations) * (++suf->numDecorations),
                 PU_MAP, 0);

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

void R_UpdateSkyFixForSec(const sector_t* sec)
{
    boolean             skyFloor, skyCeil;

    if(!sec)
        return; // Wha?

    skyFloor = R_IsSkySurface(&sec->SP_floorsurface);
    skyCeil = R_IsSkySurface(&sec->SP_ceilsurface);

    if(!skyFloor && !skyCeil)
        return;

    if(skyCeil)
    {
        mobj_t*             mo;

        // Adjust for the plane height.
        if(sec->SP_ceilvisheight > skyFix[PLN_CEILING].height)
        {   // Must raise the skyfix ceiling.
            skyFix[PLN_CEILING].height = sec->SP_ceilvisheight;
        }

        // Check that all the mobjs in the sector fit in.
        for(mo = sec->mobjList; mo; mo = mo->sNext)
        {
            float               extent = mo->pos[VZ] + mo->height;

            if(extent > skyFix[PLN_CEILING].height)
            {   // Must raise the skyfix ceiling.
                skyFix[PLN_CEILING].height = extent;
            }
        }
    }

    if(skyFloor)
    {
        // Adjust for the plane height.
        if(sec->SP_floorvisheight < skyFix[PLN_FLOOR].height)
        {   // Must lower the skyfix floor.
            skyFix[PLN_FLOOR].height = sec->SP_floorvisheight;
        }
    }

    // Update for middle textures on two sided linedefs which intersect the
    // floor and/or ceiling of their front and/or back sectors.
    if(sec->lineDefs)
    {
        linedef_t**         linePtr = sec->lineDefs;

        while(*linePtr)
        {
            linedef_t*          li = *linePtr;

            // Must be twosided.
            if(li->L_frontside && li->L_backside)
            {
                sidedef_t*          si = li->L_frontsector == sec?
                    li->L_frontside : li->L_backside;

                if(si->SW_middlematerial)
                {
                    if(skyCeil)
                    {
                        float               top =
                            sec->SP_ceilvisheight +
                                si->SW_middlevisoffset[VY];

                        if(top > skyFix[PLN_CEILING].height)
                        {   // Must raise the skyfix ceiling.
                            skyFix[PLN_CEILING].height = top;
                        }
                    }

                    if(skyFloor)
                    {
                        float               bottom =
                            sec->SP_floorvisheight +
                                si->SW_middlevisoffset[VY] -
                                    si->SW_middlematerial->height;

                        if(bottom < skyFix[PLN_FLOOR].height)
                        {   // Must lower the skyfix floor.
                            skyFix[PLN_FLOOR].height = bottom;
                        }
                    }
                }
            }
            *linePtr++;
        }
    }
}

/**
 * Fixing the sky means that for adjacent sky sectors the lower sky
 * ceiling is lifted to match the upper sky. The raising only affects
 * rendering, it has no bearing on gameplay.
 */
void R_InitSkyFix(void)
{
    uint                i;

    skyFix[PLN_FLOOR].height = DDMAXFLOAT;
    skyFix[PLN_CEILING].height = DDMINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    for(i = 0; i < numSectors; ++i)
    {
        R_UpdateSkyFixForSec(SECTOR_PTR(i));
    }
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

void R_SetupSky(ded_sky_t* sky)
{
    int                 i;
    int                 skyTex;
    int                 ival = 0;
    float               fval = 0;

    if(!sky)
    {   // Go with the defaults.
        fval = .666667f;
        Rend_SkyParams(DD_SKY, DD_HEIGHT, &fval);
        Rend_SkyParams(DD_SKY, DD_HORIZON, &ival);
        Rend_SkyParams(0, DD_ENABLE, NULL);
        ival = P_MaterialNumForName("SKY1", MN_TEXTURES);
        Rend_SkyParams(0, DD_MATERIAL, &ival);
        ival = DD_NO;
        Rend_SkyParams(0, DD_MASK, &ival);
        fval = 0;
        Rend_SkyParams(0, DD_OFFSET, &fval);
        Rend_SkyParams(1, DD_DISABLE, NULL);

        // There is no sky color.
        noSkyColorGiven = true;
        return;
    }

    Rend_SkyParams(DD_SKY, DD_HEIGHT, &sky->height);
    Rend_SkyParams(DD_SKY, DD_HORIZON, &sky->horizonOffset);
    for(i = 0; i < 2; ++i)
    {
        ded_skylayer_t*     layer = &sky->layers[i];

        if(layer->flags & SLF_ENABLED)
        {
            skyTex = P_MaterialNumForName(layer->material.name,
                                          layer->material.mnamespace);
            if(!skyTex)
            {
                Con_Message("R_SetupSky: Invalid/missing texture \"%s\"\n",
                            layer->material.name);
                skyTex = P_MaterialNumForName("SKY1", MN_TEXTURES);
            }

            Rend_SkyParams(i, DD_ENABLE, NULL);
            Rend_SkyParams(i, DD_MATERIAL, &skyTex);
            ival = ((layer->flags & SLF_MASKED)? DD_YES : DD_NO);
            Rend_SkyParams(i, DD_MASK, &ival);
            Rend_SkyParams(i, DD_OFFSET, &layer->offset);
            Rend_SkyParams(i, DD_COLOR_LIMIT, &layer->colorLimit);
        }
        else
        {
            Rend_SkyParams(i, DD_DISABLE, NULL);
        }
    }

    // Any sky models to setup? Models will override the normal
    // sphere.
    R_SetupSkyModels(sky);

    // How about the sky color?
    noSkyColorGiven = true;
    for(i = 0; i < 3; ++i)
    {
        skyColorRGB[i] = sky->color[i];
        if(sky->color[i] > 0)
            noSkyColorGiven = false;
    }

    // Calculate a balancing factor, so the light in the non-skylit
    // sectors won't appear too bright.
    if(sky->color[0] > 0 || sky->color[1] > 0 ||
        sky->color[2] > 0)
    {
        skyColorBalance =
            (0 +
             (sky->color[0] * 2 + sky->color[1] * 3 +
              sky->color[2] * 2) / 7) / 1;
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

linedef_t* R_FindSolidLineNeighbor(const sector_t* sector,
                                   const linedef_t* line,
                                   const lineowner_t* own,
                                   boolean antiClockwise, binangle_t* diff)
{
    lineowner_t*        cown = own->link[!antiClockwise];
    linedef_t*          other = cown->lineDef;
    int                 side;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

    if(!(other->buildData.windowEffect && other->L_frontsector != sector))
    {
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

    if(!other->L_backside || other->L_frontsector != other->L_backsector ||
       other->buildData.windowEffect)
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
    if((!other->L_backside || !other->L_frontside))
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
        Z_Malloc(sizeof(*map->lineLinks) * map->numLineDefs, PU_MAPSTATIC, 0);
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
        Z_Malloc(sizeof(fvertex_t*) * (ssec->numVertices + 1),PU_MAP, 0);

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

/**
 * The test is done on subsectors.
 */
#if 0 /* Currently unused. */
static sector_t *getContainingSectorOf(gamemap_t* map, sector_t* sec)
{
    uint                i;
    float               cdiff = -1, diff;
    float               inner[4], outer[4];
    sector_t*           other, *closest = NULL;

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
#endif

void R_BuildSectorLinks(gamemap_t *map)
{
#define DOMINANT_SIZE   1000

    uint                i;

    for(i = 0; i < map->numSectors; ++i)
    {
        sector_t           *sec = &map->sectors[i];

        if(!sec->lineDefCount)
            continue;

        // Is this sector large enough to be a dominant light source?
        if(sec->lightSource == NULL && sec->planeCount > 0 &&
           sec->bBox[BOXRIGHT] - sec->bBox[BOXLEFT]   > DOMINANT_SIZE &&
           sec->bBox[BOXTOP]   - sec->bBox[BOXBOTTOM] > DOMINANT_SIZE)
        {
            if(R_SectorContainsSkySurfaces(sec))
            {
                uint                k;

                // All sectors touching this one will be affected.
                for(k = 0; k < sec->lineDefCount; ++k)
                {
                    linedef_t*          lin = sec->lineDefs[k];
                    sector_t           *other;

                    other = lin->L_frontsector;
                    if(other == sec)
                    {
                        if(lin->L_backside)
                            other = lin->L_backsector;
                    }

                    if(other && other != sec)
                        other->lightSource = sec;
                }
            }
        }
    }

#undef DOMINANT_SIZE
}

static __inline void initSurfaceMaterialOffset(surface_t *suf)
{
    if(!suf)
        return;

    suf->visOffset[VX] = suf->oldOffset[0][VX] =
        suf->oldOffset[1][VX] = suf->offset[VX];
    suf->visOffset[VY] = suf->oldOffset[0][VY] =
        suf->oldOffset[1][VY] = suf->offset[VY];
}

/**
 * Called by the game at various points in the map setup process.
 */
void R_SetupMap(int mode, int flags)
{
    uint i;

    switch(mode)
    {
    case DDSMM_INITIALIZE:
        // Switch to fast malloc mode in the zone. This is intended for large
        // numbers of mallocs with no frees in between.
        Z_EnableFastMalloc(false);

        // A new map is about to be setup.
        ddMapSetup = true;

        // Reset Material precache status.
        for(i = 0; i < Materials_Count(); ++i)
            P_MaterialPrecache(P_ToMaterial(i+1), false);
        return;

    case DDSMM_AFTER_LOADING:
    {
        // Update everything again. Its possible that after loading we
        // now have more HOMs to fix, etc..

        R_InitSkyFix();

        // Set intial values of various tracked and interpolated properties
        // (lighting, smoothed planes etc).
        for(i = 0; i < numSectors; ++i)
        {
            uint                j;
            sector_t           *sec = SECTOR_PTR(i);

            R_UpdateSector(sec, false);
            for(j = 0; j < sec->planeCount; ++j)
            {
                plane_t            *pln = sec->SP_plane(j);

                pln->visHeight = pln->oldHeight[0] = pln->oldHeight[1] =
                    pln->height;

                initSurfaceMaterialOffset(&pln->surface);
            }
        }

        for(i = 0; i < numSideDefs; ++i)
        {
            sidedef_t          *si = SIDE_PTR(i);

            initSurfaceMaterialOffset(&si->SW_topsurface);
            initSurfaceMaterialOffset(&si->SW_middlesurface);
            initSurfaceMaterialOffset(&si->SW_bottomsurface);
        }

        P_MapInitPolyobjs();
        return;
    }
    case DDSMM_FINALIZE:
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
            uint                l;
            sector_t           *sec = SECTOR_PTR(i);

            R_UpdateSector(sec, true);
            for(l = 0; l < sec->planeCount; ++l)
            {
                plane_t            *pln = sec->SP_plane(l);

                pln->visHeight = pln->oldHeight[0] = pln->oldHeight[1] =
                    pln->height;

                initSurfaceMaterialOffset(&pln->surface);
            }
        }

        for(i = 0; i < numSideDefs; ++i)
        {
            sidedef_t*          si = SIDE_PTR(i);

            initSurfaceMaterialOffset(&si->SW_topsurface);
            initSurfaceMaterialOffset(&si->SW_middlesurface);
            initSurfaceMaterialOffset(&si->SW_bottomsurface);
        }

        P_MapInitPolyobjs();

        // Run any commands specified in Map Info.
        {
        ded_mapinfo_t*      mapInfo = Def_GetMapInfo(P_GetMapID(map));

        if(mapInfo && mapInfo->execute)
            Con_Execute(CMDS_DED, mapInfo->execute, true, false);
        }

        // The map setup has been completed. Run the special map setup
        // command, which the user may alias to do something useful.
        if(mapID && mapID[0])
        {
            char                cmd[80];

            sprintf(cmd, "init-%s", mapID);
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
            player_t* plr = &ddPlayers[i];
            ddplayer_t* ddpl = &plr->shared;

            clients[i].numTics = 0;

            // Determine if the player is in the void.
            ddpl->inVoid = true;
            if(ddpl->mo)
            {
                subsector_t* ssec = R_PointInSubsector(ddpl->mo->pos[VX], ddpl->mo->pos[VY]);

                //// \fixme $nplanes
                if(ssec && ddpl->mo->pos[VZ] >= ssec->sector->SP_floorvisheight && ddpl->mo->pos[VZ] < ssec->sector->SP_ceilvisheight - 4)
                   ddpl->inVoid = false;
            }
        }

        // Reset the map tick timer.
        ddMapTime = 0;

        // We've finished setting up the map.
        ddMapSetup = false;

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;

        // Switch back to normal malloc mode in the zone. Z_Malloc will look
        // for free blocks in the entire zone and purge purgable blocks.
        Z_EnableFastMalloc(false);
        return;
    }
    case DDSMM_AFTER_BUSY:
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
        Con_Error("R_SetupMap: Unknown setup mode %i", mode);
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

/**
 * Is the specified plane glowing (it glows or is a sky mask surface)?
 *
 * @return              @c true, if the specified plane is non-glowing,
 *                      i.e. not glowing or a sky.
 */
boolean R_IsGlowingPlane(const plane_t* pln)
{
    material_t* mat = pln->surface.material;
    material_snapshot_t ms;
    Materials_Prepare(&ms, mat, true, 0);
    return ((mat && (mat->flags & MATF_NO_DRAW)) || ms.glowing > 0 ||
            R_IsSkySurface(&pln->surface));
}

/**
 * Does the specified sector contain any sky surfaces?
 *
 * @return              @c true, if one or more surfaces in the given sector
 *                      use the special sky mask material.
 */
boolean R_SectorContainsSkySurfaces(const sector_t* sec)
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

/**
 * Given a sidedef section, look at the neighbouring surfaces and pick the
 * best choice of material used on those surfaces to be applied to "this"
 * surface.
 *
 * Material on back neighbour plane has priority.
 * Non-animated materials are preferred.
 * Sky materials are ignored.
 */
static material_t* chooseFixMaterial(sidedef_t* s, segsection_t section)
{
    material_t* choice1 = NULL, *choice2 = NULL;

    if(section == SEG_BOTTOM || section == SEG_TOP)
    {
        byte sid = (s->line->L_frontside == s? 0 : 1);
        sector_t* frontSec = s->line->L_sector(sid);
        sector_t* backSec = s->line->L_sector(sid^1);
        surface_t* suf;

        if(backSec)
        {
            suf = &backSec->SP_plane(section == SEG_BOTTOM? PLN_FLOOR : PLN_CEILING)->surface;
            if(suf->material && !R_IsSkySurface(suf))
                choice1 = suf->material;
        }

        suf = &frontSec->SP_plane(section == SEG_BOTTOM? PLN_FLOOR : PLN_CEILING)->surface;
        if(suf->material && !R_IsSkySurface(suf))
            choice2 = suf->material;
    }

    if(choice1 && !choice1->inAnimGroup)
        return choice1;
    if(choice2 && !choice2->inAnimGroup)
        return choice2;
    if(choice1)
        return choice1;
    if(choice2)
        return choice2;
    return NULL;
}

static void updateSidedefSection(sidedef_t* s, segsection_t section)
{
    surface_t*          suf;

    if(section == SEG_MIDDLE)
        return; // Not applicable.

    suf = &s->sections[section];
    if(!suf->material /*&&
       !R_IsSkySurface(&s->sector->
            SP_plane(section == SEG_BOTTOM? PLN_FLOOR : PLN_CEILING)->
                surface)*/)
    {
        Surface_SetMaterial(suf, chooseFixMaterial(s, section));
        suf->inFlags |= SUIF_MATERIAL_FIX;
    }
}

void R_UpdateLinedefsOfSector(sector_t* sec)
{
    uint                i;

    for(i = 0; i < sec->lineDefCount; ++i)
    {
        linedef_t*          li = sec->lineDefs[i];
        sidedef_t*          front, *back;
        sector_t*           frontSec, *backSec;

        if(!li->L_frontside || !li->L_backside)
            continue;
        if(LINE_SELFREF(li))
            continue;

        front = li->L_frontside;
        back  = li->L_backside;
        frontSec = front->sector;
        backSec = li->L_backside? back->sector : NULL;

        /**
         * Do as in the original Doom if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless it is skymasked),
         * or if there is a midtexture use that instead.
         */

        // Check for missing lowers.
        if(frontSec->SP_floorheight < backSec->SP_floorheight)
            updateSidedefSection(front, SEG_BOTTOM);
        else if(frontSec->SP_floorheight > backSec->SP_floorheight)
            updateSidedefSection(back, SEG_BOTTOM);

        // Check for missing uppers.
        if(backSec->SP_ceilheight < frontSec->SP_ceilheight)
            updateSidedefSection(front, SEG_TOP);
        else if(backSec->SP_ceilheight > frontSec->SP_ceilheight)
            updateSidedefSection(back, SEG_TOP);
    }
}

boolean R_UpdatePlane(plane_t* pln, boolean forceUpdate)
{
    sector_t* sec = pln->sector;
    boolean changed = false;

    // Geometry change?
    if(forceUpdate || pln->height != pln->oldHeight[1])
    {
        sidedef_t* front = NULL, *back = NULL;
        subsector_t** ssecp;
        uint i;

        // Check if there are any camera players in this sector. If their
        // height is now above the ceiling/below the floor they are now in
        // the void.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];
            ddplayer_t* ddpl = &plr->shared;

            if(!ddpl->inGame || !ddpl->mo || !ddpl->mo->subsector)
                continue;

            //// \fixme $nplanes
            if((ddpl->flags & DDPF_CAMERA) && ddpl->mo->subsector->sector == sec &&
               (ddpl->mo->pos[VZ] > sec->SP_ceilheight - 4 || ddpl->mo->pos[VZ] < sec->SP_floorheight))
            {
                ddpl->inVoid = true;
            }
        }

        // Update the z position of the degenmobj for this plane.
        pln->soundOrg.pos[VZ] = pln->height;

        // Inform the shadow bias of changed geometry.
        ssecp = sec->ssectors;
        while(*ssecp)
        {
            subsector_t* ssec = *ssecp;
            seg_t** segp = ssec->segs;

            while(*segp)
            {
                seg_t* seg = *segp;

                if(seg->lineDef)
                {
                    for(i = 0; i < 3; ++i)
                        SB_SurfaceMoved(seg->bsuf[i]);
                }

                *segp++;
            }

            SB_SurfaceMoved(ssec->bsuf[pln->planeID]);

            *ssecp++;
        }

        // We need the decorations updated.
        Surface_Update(&pln->surface);

        changed = true;
    }

    return changed;
}

#if 0
/**
 * Stub.
 */
boolean R_UpdateSubSector(subsector_t* ssec, boolean forceUpdate)
{
    return false; // Not changed.
}
#endif

boolean R_UpdateSector(sector_t* sec, boolean forceUpdate)
{
    uint                i;
    boolean             changed = false, planeChanged = false;

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

        changed = true;
    }
    else
    {
        sec->frameFlags &= ~SIF_LIGHT_CHANGED;
    }

    // For each plane.
    for(i = 0; i < sec->planeCount; ++i)
    {
        if(R_UpdatePlane(sec->planes[i], forceUpdate))
        {
            planeChanged = true;
        }
    }

    if(planeChanged)
    {
        sec->soundOrg.pos[VZ] = (sec->SP_ceilheight - sec->SP_floorheight) / 2;
        R_UpdateLinedefsOfSector(sec);
        S_CalcSectorReverb(sec);

        changed = true;
    }

    return planeChanged;
}

/**
 * Stub.
 */
boolean R_UpdateLinedef(linedef_t* line, boolean forceUpdate)
{
    return false; // Not changed.
}

/**
 * Stub.
 */
boolean R_UpdateSidedef(sidedef_t* side, boolean forceUpdate)
{
    return false; // Not changed.
}

/**
 * Stub.
 */
boolean R_UpdateSurface(surface_t* suf, boolean forceUpdate)
{
    return false; // Not changed.
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
 * The DOOM lighting model applies distance attenuation to sector light
 * levels.
 *
 * @param distToViewer  Distance from the viewer to this object.
 * @param lightLevel    Sector lightLevel at this object's origin.
 * @return              The specified lightLevel plus any attentuation.
 */
float R_DistAttenuateLightLevel(float distToViewer, float lightLevel)
{
    if(distToViewer > 0 && rendLightDistanceAttentuation > 0)
    {
        float               real, minimum;

        real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttentuation *
                (1 - lightLevel);

        minimum = lightLevel * lightLevel + (lightLevel - .63f) * .5f;
        if(real < minimum)
            real = minimum; // Clamp it.

        return real;
    }

    return lightLevel;
}

/**
 * The DOOM lighting model applies a light level delta to everything when
 * e.g. the player shoots.
 *
 * @return              Calculated delta.
 */
float R_ExtraLightDelta(void)
{
    return extraLightDelta;
}

/**
 * @return              @c > 0, if the sector lightlevel passes the
 *                      limit condition.
 */
float R_CheckSectorLight(float lightlevel, float min, float max)
{
    // Has a limit been set?
    if(min == max)
        return 1;

    // Apply adaptation
    Rend_ApplyLightAdaptation(&lightlevel);

    return MINMAX_OF(0, (lightlevel - min) / (float) (max - min), 1);
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
        sector_t           *src;

        // A dominant light source affects this sector?
        src = sector->lightSource;
        if(src && src->lightLevel >= sector->lightLevel)
        {
            // The color shines here, too.
            return R_GetSectorLightColor(src);
        }

        return sector->rgb;
/*
        // Return the sector's real color (balanced against sky's).
        if(skyColorBalance >= 1)
        {
            return sector->rgb;
        }
        else
        {
            int                 c;

            for(c = 0; c < 3; ++c)
                balancedRGB[c] = sector->rgb[c] * skyColorBalance;
            return balancedRGB;
        }
*/
    }

    // Return the sky color.
    return skyColorRGB;
}
