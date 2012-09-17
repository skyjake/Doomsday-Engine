/**\file r_world.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * World Setup and Refresh.
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

#include "materialvariant.h"

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

float rendLightWallAngle = 1.2f; // Intensity of angle-based wall lighting.
byte rendLightWallAngleSmooth = true;

float rendSkyLight = .2f; // Intensity factor.
byte rendSkyLightAuto = true;

boolean firstFrameAfterLoad;
boolean ddMapSetup;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static surfacelistnode_t* unusedSurfaceListNodes = 0;

// CODE --------------------------------------------------------------------

/**
 * Allocate a new surface list node.
 */
static surfacelistnode_t* allocListNode(void)
{
    surfacelistnode_t*      node = Z_Calloc(sizeof(*node), PU_APPSTATIC, 0);
    return node;
}

#if 0
/**
 * Free all memory acquired for the given surface list node.
 *
 * @todo  This function is never called anywhere?
 */
static void freeListNode(surfacelistnode_t* node)
{
    if(node)
        Z_Free(node);
}
#endif

surfacelistnode_t* R_SurfaceListNodeCreate(void)
{
    surfacelistnode_t* node;

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

void R_SurfaceListAdd(surfacelist_t* sl, Surface* suf)
{
    surfacelistnode_t* node;

    if(!sl || !suf)
        return;

    // Check whether this surface is already in the list.
    node = sl->head;
    while(node)
    {
        if((Surface*) node->data == suf)
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

boolean R_SurfaceListRemove(surfacelist_t* sl, const Surface* suf)
{
    surfacelistnode_t* last, *n;

    if(!sl || !suf)
        return false;

    last = sl->head;
    if(last)
    {
        n = last->next;
        while(n)
        {
            if((Surface*) n->data == suf)
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

void R_SurfaceListClear(surfacelist_t* sl)
{
    if(sl)
    {
        surfacelistnode_t* node, *next;
        node = sl->head;
        while(node)
        {
            next = node->next;
            R_SurfaceListRemove(sl, (Surface*)node->data);
            node = next;
        }
    }
}

boolean R_SurfaceListIterate(surfacelist_t* sl, boolean (*callback) (Surface* suf, void*),
    void* context)
{
    boolean  result = true;
    surfacelistnode_t*  n, *np;

    if(sl)
    {
        n = sl->head;
        while(n)
        {
            np = n->next;
            if((result = callback((Surface*) n->data, context)) == 0)
                break;
            n = np;
        }
    }

    return result;
}

boolean updateSurfaceScroll(Surface* suf, void* context)
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
void R_UpdateSurfaceScroll(void)
{
    surfacelist_t* slist;
    if(!theMap) return;
    slist = GameMap_ScrollingSurfaces(theMap);
    if(!slist) return;

    R_SurfaceListIterate(slist, updateSurfaceScroll, NULL);
}

boolean resetSurfaceScroll(Surface* suf, void* context)
{
    // X Offset.
    suf->visOffsetDelta[0] = 0;
    suf->oldOffset[0][0] = suf->oldOffset[0][1] = suf->offset[0];

    // Y Offset.
    suf->visOffsetDelta[1] = 0;
    suf->oldOffset[1][0] = suf->oldOffset[1][1] = suf->offset[1];

    Surface_Update(suf);
    /// @todo Do not assume surface is from the CURRENT map.
    R_SurfaceListRemove(GameMap_ScrollingSurfaces(theMap), suf);

    return true;
}

boolean interpSurfaceScroll(Surface* suf, void* context)
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
    if(suf->visOffset[0] == suf->offset[0] && suf->visOffset[1] == suf->offset[1])
    {
        /// @todo Do not assume surface is from the CURRENT map.
        R_SurfaceListRemove(GameMap_ScrollingSurfaces(theMap), suf);
    }

    return true;
}

/**
 * $smoothmatoffset: interpolate the visual offset.
 */
void R_InterpolateSurfaceScroll(boolean resetNextViewer)
{
    surfacelist_t* slist;

    if(!theMap) return;
    slist = GameMap_ScrollingSurfaces(theMap);
    if(!slist) return;

    if(resetNextViewer)
    {
        // Reset the material offset trackers.
        R_SurfaceListIterate(slist, resetSurfaceScroll, NULL);
    }
    // While the game is paused there is no need to calculate any
    // visual material offsets.
    else //if(!clientPaused)
    {
        // Set the visible material offsets.
        R_SurfaceListIterate(slist, interpSurfaceScroll, NULL);
    }
}

void R_AddTrackedPlane(planelist_t* plist, Plane *pln)
{
    uint i;

    if(!plist || !pln) return;

    // Check whether we are already tracking this plane.
    for(i = 0; i < plist->num; ++i)
    {
        if(plist->array[i] == pln)
            return; // Yes we are.
    }

    plist->num++;

    // Only allocate memory when it's needed.
    if(plist->num > plist->maxNum)
    {
        plist->maxNum *= 2;

        // The first time, allocate 8 watched plane nodes.
        if(!plist->maxNum)
            plist->maxNum = 8;

        plist->array = Z_Realloc(plist->array, sizeof(Plane*) * (plist->maxNum + 1), PU_MAP);
    }

    // Add the plane to the list.
    plist->array[plist->num-1] = pln;
    plist->array[plist->num] = NULL; // Terminate.
}

boolean R_RemoveTrackedPlane(planelist_t *plist, const Plane *pln)
{
    uint            i;

    if(!plist || !pln)
        return false;

    for(i = 0; i < plist->num; ++i)
    {
        if(plist->array[i] == pln)
        {
            if(i == plist->num - 1)
                plist->array[i] = NULL;
            else
                memmove(&plist->array[i], &plist->array[i+1],
                        sizeof(Plane*) * (plist->num - 1 - i));
            plist->num--;
            return true;
        }
    }

    return false;
}

/**
 * $smoothplane: Roll the height tracker buffers.
 */
void R_UpdateTrackedPlanes(void)
{
    planelist_t* plist;
    uint i;

    if(!theMap) return;
    plist = GameMap_TrackedPlanes(theMap);
    if(!plist) return;

    for(i = 0; i < plist->num; ++i)
    {
        Plane* pln = plist->array[i];

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
void R_InterpolateTrackedPlanes(boolean resetNextViewer)
{
    planelist_t* plist;
    Plane* pln;
    uint i;

    if(!theMap) return;
    plist = GameMap_TrackedPlanes(theMap);
    if(!plist) return;

    if(resetNextViewer)
    {
        // $smoothplane: Reset the plane height trackers.
        for(i = 0; i < plist->num; ++i)
        {
            pln = plist->array[i];

            pln->visHeightDelta = 0;
            pln->visHeight = pln->oldHeight[0] = pln->oldHeight[1] = pln->height;

            if(pln->type == PLN_FLOOR || pln->type == PLN_CEILING)
            {
                R_MarkDependantSurfacesForDecorationUpdate(pln);
            }

            if(R_RemoveTrackedPlane(plist, pln))
                i = (i > 0? i-1 : 0);
        }
    }
    // While the game is paused there is no need to calculate any
    // visual plane offsets $smoothplane.
    else //if(!clientPaused)
    {
        // $smoothplane: Set the visible offsets.
        for(i = 0; i < plist->num; ++i)
        {
            pln = plist->array[i];

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
            if(pln->visHeight == pln->height) /// @todo  Can this fail? (float equality)
            {
                if(R_RemoveTrackedPlane(plist, pln))
                    i = (i > 0? i-1 : 0);
            }
        }
    }
}

/**
 * To be called when a floor or ceiling height changes to update the plotted
 * decoration origins for surfaces whose material offset is dependant upon
 * the given plane.
 */
void R_MarkDependantSurfacesForDecorationUpdate(Plane* pln)
{
    LineDef** linep;

    if(!pln || !pln->sector->lineDefs) return;

    // "Middle" planes have no dependent surfaces.
    if(pln->type == PLN_MID) return;

    // Mark the decor lights on the sides of this plane as requiring
    // an update.
    linep = pln->sector->lineDefs;

    while(*linep)
    {
        LineDef* li = *linep;

        Surface_Update(&li->L_frontsidedef->SW_surface(SS_MIDDLE));
        Surface_Update(&li->L_frontsidedef->SW_surface(SS_BOTTOM));
        Surface_Update(&li->L_frontsidedef->SW_surface(SS_TOP));

        if(li->L_backsidedef)
        {
            Surface_Update(&li->L_backsidedef->SW_surface(SS_MIDDLE));
            Surface_Update(&li->L_backsidedef->SW_surface(SS_BOTTOM));
            Surface_Update(&li->L_backsidedef->SW_surface(SS_TOP));
        }

        linep++;
    }
}

static boolean markSurfaceForDecorationUpdate(Surface* surface, void* paramaters)
{
    material_t* material = (material_t*) paramaters;
    if(material == surface->material)
    {
        Surface_Update(surface);
    }
    return 1; // Continue iteration.
}

void R_UpdateMapSurfacesOnMaterialChange(material_t* material)
{
    surfacelist_t* slist;

    if(!material || !theMap || ddMapSetup) return;
    slist = GameMap_DecoratedSurfaces(theMap);
    if(!slist) return;

    // Light decorations will need a refresh.
    R_SurfaceListIterate(slist, markSurfaceForDecorationUpdate, material);
}

/**
 * Create a new plane for the given sector. The plane will be initialized
 * with default values.
 *
 * Post: The sector's plane list will be replaced, the new plane will be
 *       linked to the end of the list.
 *
 * @param sec  Sector for which a new plane will be created.
 *
 * @return  Ptr to the newly created plane.
 */
Plane* R_NewPlaneForSector(Sector* sec)
{
    Surface* suf;
    Plane* plane;

    if(!sec)
        return NULL; // Do wha?

    // Allocate the new plane.
    plane = Z_Malloc(sizeof(Plane), PU_MAP, 0);

    // Resize this sector's plane list.
    sec->planes = Z_Realloc(sec->planes, sizeof(Plane*) * (++sec->planeCount + 1), PU_MAP);
    // Add the new plane to the end of the list.
    sec->planes[sec->planeCount-1] = plane;
    sec->planes[sec->planeCount] = NULL; // Terminate.

    // Setup header for DMU.
    plane->header.type = DMU_PLANE;

    // Initalize the plane.
    plane->sector = sec;
    plane->height = plane->oldHeight[0] = plane->oldHeight[1] = 0;
    plane->visHeight = plane->visHeightDelta = 0;
    memset(&plane->PS_base.thinker, 0, sizeof(plane->PS_base.thinker));
    plane->speed = 0;
    plane->target = 0;
    plane->type = PLN_MID;
    plane->planeID = sec->planeCount-1;

    // Initialize the surface.
    memset(&plane->surface, 0, sizeof(plane->surface));
    suf = &plane->surface;
    suf->header.type = DMU_SURFACE; // Setup header for DMU.
    suf->normal[VZ] = 1;
    V3f_BuildTangents(suf->tangent, suf->bitangent, suf->normal);

    suf->owner = (void*) plane;
    /// @todo The initial material should be the "unknown" material.
    Surface_SetMaterial(suf, NULL);
    Surface_SetMaterialOrigin(suf, 0, 0);
    Surface_SetColorAndAlpha(suf, 1, 1, 1, 1);
    Surface_SetBlendMode(suf, BM_NORMAL);
    Surface_UpdateBaseOrigin(suf);

    /**
     * Resize the biassurface lists for the BSP leaf planes.
     * If we are in map setup mode, don't create the biassurfaces now,
     * as planes are created before the bias system is available.
     */

    if(sec->bspLeafs && *sec->bspLeafs)
    {
        BspLeaf** ssecIter = sec->bspLeafs;
        do
        {
            BspLeaf* bspLeaf = *ssecIter;
            biassurface_t** newList;
            uint n = 0;

            newList = Z_Calloc(sec->planeCount * sizeof(biassurface_t*), PU_MAP, NULL);
            // Copy the existing list?
            if(bspLeaf->bsuf)
            {
                for(; n < sec->planeCount - 1; ++n)
                {
                    newList[n] = bspLeaf->bsuf[n];
                }
                Z_Free(bspLeaf->bsuf);
            }

            if(!ddMapSetup)
            {
                biassurface_t* bsuf = SB_CreateSurface();

                bsuf->size = Rend_NumFanVerticesForBspLeaf(bspLeaf);
                bsuf->illum = Z_Calloc(sizeof(vertexillum_t) * bsuf->size, PU_MAP, 0);

                { uint i;
                for(i = 0; i < bsuf->size; ++i)
                    SB_InitVertexIllum(&bsuf->illum[i]);
                }

                newList[n] = bsuf;
            }

            bspLeaf->bsuf = newList;

            ssecIter++;
        } while(*ssecIter);
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
void R_DestroyPlaneOfSector(uint id, Sector* sec)
{
    Plane* plane, **newList = NULL;
    surfacelist_t* slist;
    planelist_t* plist;
    uint i;

    if(!sec) return; // Do wha?

    if(id >= sec->planeCount)
        Con_Error("P_DestroyPlaneOfSector: Plane id #%i is not valid for "
                  "sector #%u", id, (uint) GET_SECTOR_IDX(sec));

    plane = sec->planes[id];

    // Create a new plane list?
    if(sec->planeCount > 1)
    {
        uint n;

        newList = Z_Malloc(sizeof(Plane**) * sec->planeCount, PU_MAP, 0);

        // Copy ptrs to the planes.
        n = 0;
        for(i = 0; i < sec->planeCount; ++i)
        {
            if(i == id) continue;
            newList[n++] = sec->planes[i];
        }
        newList[n] = NULL; // Terminate.
    }

    // If this plane is currently being watched, remove it.
    plist = GameMap_TrackedPlanes(theMap);
    if(plist) R_RemoveTrackedPlane(plist, plane);

    // If this plane's surface is in the moving list, remove it.
    slist = GameMap_ScrollingSurfaces(theMap);
    if(slist) R_SurfaceListRemove(slist, &plane->surface);

    // If this plane's surface is in the deocrated list, remove it.
    slist = GameMap_DecoratedSurfaces(theMap);
    if(slist) R_SurfaceListRemove(slist, &plane->surface);

    // If this plane's surface is in the glowing list, remove it.
    slist = GameMap_GlowingSurfaces(theMap);
    if(slist) R_SurfaceListRemove(slist, &plane->surface);

    // Destroy the biassurfaces for this plane.
    { BspLeaf** bspLeafIter;
    for(bspLeafIter = sec->bspLeafs; *bspLeafIter; bspLeafIter++)
    {
        BspLeaf* bspLeaf = *bspLeafIter;
        SB_DestroySurface(bspLeaf->bsuf[id]);
        if(id < sec->planeCount)
            memmove(bspLeaf->bsuf + id, bspLeaf->bsuf + id + 1, sizeof(biassurface_t*));
    }}

    // Destroy the specified plane.
    Z_Free(plane);
    sec->planeCount--;

    // Link the new list to the sector.
    Z_Free(sec->planes);
    sec->planes = newList;
}

surfacedecor_t* R_CreateSurfaceDecoration(Surface* suf)
{
    surfacedecor_t* d, *s, *decorations;
    uint i;

    if(!suf) return NULL;

    decorations = Z_Malloc(sizeof(*decorations) * (++suf->numDecorations), PU_MAP, 0);

    if(suf->numDecorations > 1)
    {
        // Copy the existing decorations.
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

    suf->decorations = decorations;

    return d;
}

void R_ClearSurfaceDecorations(Surface* suf)
{
    if(!suf) return;

    if(suf->decorations)
        Z_Free(suf->decorations);
    suf->decorations = NULL;
    suf->numDecorations = 0;
}

void GameMap_UpdateSkyFixForSector(GameMap* map, Sector* sec)
{
    boolean skyFloor, skyCeil;
    assert(map);

    if(!sec || 0 == sec->lineDefCount) return;

    skyFloor = Surface_IsSkyMasked(&sec->SP_floorsurface);
    skyCeil  = Surface_IsSkyMasked(&sec->SP_ceilsurface);

    if(!skyFloor && !skyCeil) return;

    if(skyCeil)
    {
        mobj_t* mo;

        // Adjust for the plane height.
        if(sec->SP_ceilvisheight > map->skyFix[PLN_CEILING].height)
        {
            // Must raise the skyfix ceiling.
            map->skyFix[PLN_CEILING].height = sec->SP_ceilvisheight;
        }

        // Check that all the mobjs in the sector fit in.
        for(mo = sec->mobjList; mo; mo = mo->sNext)
        {
            float extent = mo->origin[VZ] + mo->height;

            if(extent > map->skyFix[PLN_CEILING].height)
            {
                // Must raise the skyfix ceiling.
                map->skyFix[PLN_CEILING].height = extent;
            }
        }
    }

    if(skyFloor)
    {
        // Adjust for the plane height.
        if(sec->SP_floorvisheight < map->skyFix[PLN_FLOOR].height)
        {
            // Must lower the skyfix floor.
            map->skyFix[PLN_FLOOR].height = sec->SP_floorvisheight;
        }
    }

    // Update for middle textures on two sided linedefs which intersect the
    // floor and/or ceiling of their front and/or back sectors.
    if(sec->lineDefs && *sec->lineDefs)
    {
        LineDef** linePtr = sec->lineDefs;
        do
        {
            LineDef* li = *linePtr;

            // Must be twosided.
            if(li->L_frontsidedef && li->L_backsidedef)
            {
                SideDef* si = li->L_frontsector == sec? li->L_frontsidedef : li->L_backsidedef;

                if(si->SW_middlematerial)
                {
                    if(skyCeil)
                    {
                        float top = sec->SP_ceilvisheight + si->SW_middlevisoffset[VY];

                        if(top > map->skyFix[PLN_CEILING].height)
                        {
                            // Must raise the skyfix ceiling.
                            map->skyFix[PLN_CEILING].height = top;
                        }
                    }

                    if(skyFloor)
                    {
                        float bottom = sec->SP_floorvisheight +
                                si->SW_middlevisoffset[VY] - Material_Height(si->SW_middlematerial);

                        if(bottom < map->skyFix[PLN_FLOOR].height)
                        {
                            // Must lower the skyfix floor.
                            map->skyFix[PLN_FLOOR].height = bottom;
                        }
                    }
                }
            }
            linePtr++;
        } while(*linePtr);
    }
}

void GameMap_InitSkyFix(GameMap* map)
{
    uint i;
    assert(map);

    map->skyFix[PLN_FLOOR].height = DDMAXFLOAT;
    map->skyFix[PLN_CEILING].height = DDMINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    for(i = 0; i < map->numSectors; ++i)
    {
        GameMap_UpdateSkyFixForSector(map, map->sectors + i);
    }
}

/**
 * @return              Ptr to the lineowner for this line for this vertex
 *                      else @c NULL.
 */
lineowner_t* R_GetVtxLineOwner(const Vertex *v, const LineDef *line)
{
    if(v == line->L_v1)
        return line->L_vo1;

    if(v == line->L_v2)
        return line->L_vo2;

    return NULL;
}

/// @note Part of the Doomsday public API.
void R_SetupFog(float start, float end, float density, float *rgb)
{
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

/// @note Part of the Doomsday public API.
void R_SetupFogDefaults(void)
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY,"fog off", true, false);
}

/**
 * Returns pointers to the line's vertices in such a fashion that verts[0]
 * is the leftmost vertex and verts[1] is the rightmost vertex, when the
 * line lies at the edge of `sector.'
 */
void R_OrderVertices(const LineDef* line, const Sector* sector, Vertex* verts[2])
{
    byte edge = (sector == line->L_frontsector? 0:1);
    verts[0] = line->L_v(edge);
    verts[1] = line->L_v(edge^1);
}

boolean R_FindBottomTop2(SideDefSection section, int lineFlags,
    Sector* frontSec, Sector* backSec, SideDef* frontDef, SideDef* backDef,
    coord_t* low, coord_t* hi, float matOffset[2])
{
    const boolean unpegBottom = !!(lineFlags & DDLF_DONTPEGBOTTOM);
    const boolean unpegTop    = !!(lineFlags & DDLF_DONTPEGTOP);

    // Single sided?
    if(!frontSec || !backSec || !backDef/*front side of a "window"*/)
    {
        *low = frontSec->SP_floorvisheight;
        *hi  = frontSec->SP_ceilvisheight;

        if(matOffset)
        {
            Surface* suf = &frontDef->SW_middlesurface;
            matOffset[0] = suf->visOffset[0];
            matOffset[1] = suf->visOffset[1];
            if(unpegBottom)
            {
                matOffset[1] -= *hi - *low;
            }
        }
    }
    else
    {
        const boolean stretchMiddle = !!(frontDef->flags & SDF_MIDDLE_STRETCH);
        Plane* ffloor = frontSec->SP_plane(PLN_FLOOR);
        Plane* fceil  = frontSec->SP_plane(PLN_CEILING);
        Plane* bfloor = backSec->SP_plane(PLN_FLOOR);
        Plane* bceil  = backSec->SP_plane(PLN_CEILING);
        Surface* suf = &frontDef->SW_surface(section);

        switch(section)
        {
        case SS_TOP:
            // Can't go over front ceiling (would induce geometry flaws).
            if(bceil->visHeight < ffloor->visHeight)
                *low = ffloor->visHeight;
            else
                *low = bceil->visHeight;
            *hi = fceil->visHeight;

            if(matOffset)
            {
                matOffset[0] = suf->visOffset[0];
                matOffset[1] = suf->visOffset[1];
                if(!unpegTop)
                {
                    // Align with normal middle texture.
                    matOffset[1] -= fceil->visHeight - bceil->visHeight;
                }
            }
            break;

        case SS_BOTTOM: {
            const boolean raiseToBackFloor = (Surface_IsSkyMasked(&fceil->surface) && Surface_IsSkyMasked(&bceil->surface) && fceil->visHeight < bceil->visHeight);
            coord_t t = bfloor->visHeight;

            *low = ffloor->visHeight;
            // Can't go over the back ceiling, would induce polygon flaws.
            if(bfloor->visHeight > bceil->visHeight)
                t = bceil->visHeight;

            // Can't go over front ceiling, would induce polygon flaws.
            // In the special case of a sky masked upper we must extend the bottom
            // section up to the height of the back floor.
            if(t > fceil->visHeight && !raiseToBackFloor)
                t = fceil->visHeight;
            *hi = t;

            if(matOffset)
            {
                matOffset[0] = suf->visOffset[0];
                matOffset[1] = suf->visOffset[1];
                if(bfloor->visHeight > fceil->visHeight)
                {
                    matOffset[1] -= (raiseToBackFloor? t : fceil->visHeight) - bfloor->visHeight;
                }

                if(unpegBottom)
                {
                    // Align with normal middle texture.
                    matOffset[1] += (raiseToBackFloor? t : fceil->visHeight) - bfloor->visHeight;
                }
            }
            break; }

        case SS_MIDDLE:
            *low = MAX_OF(bfloor->visHeight, ffloor->visHeight);
            *hi  = MIN_OF(bceil->visHeight,  fceil->visHeight);

            if(matOffset)
            {
                matOffset[0] = suf->visOffset[0];
                matOffset[1] = 0;
            }

            if(suf->material && !stretchMiddle)
            {
                const boolean clipBottom = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && Surface_IsSkyMasked(&ffloor->surface) && Surface_IsSkyMasked(&bfloor->surface));
                const boolean clipTop    = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && Surface_IsSkyMasked(&fceil->surface)  && Surface_IsSkyMasked(&bceil->surface));

                const coord_t openBottom = *low;
                const coord_t openTop    = *hi;
                const int matHeight      = Material_Height(suf->material);
                const coord_t matYOffset = suf->visOffset[VY];

                if(openTop > openBottom)
                {
                    if(unpegBottom)
                    {
                        *low += matYOffset;
                        *hi   = *low + matHeight;
                    }
                    else
                    {
                        *hi += matYOffset;
                        *low = *hi - matHeight;
                    }

                    if(matOffset && *hi > openTop)
                    {
                        matOffset[1] = *hi - openTop;
                    }

                    // Clip it?
                    if(clipTop || clipBottom)
                    {
                        if(clipBottom && *low < openBottom)
                            *low = openBottom;

                        if(clipTop && *hi > openTop)
                            *hi = openTop;
                    }

                    if(matOffset && !clipTop)
                    {
                        matOffset[1] = 0;
                    }
                }
            }
            break;
        }
    }

    return /*is_visible=*/ *hi > *low;
}

boolean R_FindBottomTop(SideDefSection section, int lineFlags,
    Sector* frontSec, Sector* backSec, SideDef* frontDef, SideDef* backDef,
    coord_t* low, coord_t* hi)
{
    return R_FindBottomTop2(section, lineFlags, frontSec, backSec, frontDef, backDef,
                            low, hi, 0/*offset not needed*/);
}

coord_t R_OpenRange(Sector const* frontSec, Sector const* backSec, coord_t* retBottom, coord_t* retTop)
{
    coord_t bottom, top;

    DENG_ASSERT(frontSec);

    if(backSec && backSec->SP_ceilheight < frontSec->SP_ceilheight)
    {
        top = backSec->SP_ceilheight;
    }
    else
    {
        top = frontSec->SP_ceilheight;
    }

    if(backSec && backSec->SP_floorheight > frontSec->SP_floorheight)
    {
        bottom = backSec->SP_floorheight;
    }
    else
    {
        bottom = frontSec->SP_floorheight;
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

coord_t R_VisOpenRange(Sector const* frontSec, Sector const* backSec, coord_t* retBottom, coord_t* retTop)
{
    coord_t bottom, top;

    DENG_ASSERT(frontSec);

    if(backSec && backSec->SP_ceilvisheight < frontSec->SP_ceilvisheight)
    {
        top = backSec->SP_ceilvisheight;
    }
    else
    {
        top = frontSec->SP_ceilvisheight;
    }

    if(backSec && backSec->SP_floorvisheight > frontSec->SP_floorvisheight)
    {
        bottom = backSec->SP_floorvisheight;
    }
    else
    {
        bottom = frontSec->SP_floorvisheight;
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

boolean R_MiddleMaterialCoversOpening(int lineFlags, Sector* frontSec, Sector* backSec,
    SideDef* frontDef, SideDef* backDef, boolean ignoreOpacity)
{
    material_t* material;
    const materialsnapshot_t* ms;

    if(!frontSec || !frontDef) return false; // Never.

    material = frontDef->SW_middlematerial;
    if(!material) return false;

    // Ensure we have up to date info about the material.
    ms = Materials_Prepare(material, Rend_MapSurfaceDiffuseMaterialSpec(), true);
    if(ignoreOpacity || (ms->isOpaque && !frontDef->SW_middleblendmode && frontDef->SW_middlergba[3] >= 1))
    {
        coord_t openRange, openBottom, openTop;

        // Stretched middles always cover the opening.
        if(frontDef->flags & SDF_MIDDLE_STRETCH) return true;

        // Might the material cover the opening?
        openRange = R_VisOpenRange(frontSec, backSec, &openBottom, &openTop);
        if(ms->size.height >= openRange)
        {
            // Possibly; check the placement.
            coord_t bottom, top;
            if(R_FindBottomTop(SS_MIDDLE, lineFlags, frontSec, backSec, frontDef, backDef,
                               &bottom, &top))
            {
                return (top >= openTop && bottom <= openBottom);
            }
        }
    }

    return false;
}

boolean R_MiddleMaterialCoversLineOpening(LineDef* line, int side, boolean ignoreOpacity)
{
    DENG_ASSERT(line);
{
    Sector* frontSec  = line->L_sector(side);
    Sector* backSec   = line->L_sector(side^1);
    SideDef* frontDef = line->L_sidedef(side);
    SideDef* backDef  = line->L_sidedef(side^1);
    return R_MiddleMaterialCoversOpening(line->flags, frontSec, backSec, frontDef, backDef, ignoreOpacity);
}}

LineDef* R_FindLineNeighbor(const Sector* sector, const LineDef* line,
    const lineowner_t* own, boolean antiClockwise, binangle_t *diff)
{
    lineowner_t* cown = own->link[!antiClockwise];
    LineDef* other = cown->lineDef;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

    if(!other->L_backsidedef || other->L_frontsector != other->L_backsector)
    {
        if(sector) // Must one of the sectors match?
        {
            if(other->L_frontsector == sector ||
               (other->L_backsidedef && other->L_backsector == sector))
                return other;
        }
        else
        {
            return other;
        }
    }

    // Not suitable, try the next.
    return R_FindLineNeighbor(sector, line, cown, antiClockwise, diff);
}

LineDef* R_FindSolidLineNeighbor(const Sector* sector, const LineDef* line,
    const lineowner_t* own, boolean antiClockwise, binangle_t* diff)
{
    lineowner_t* cown = own->link[!antiClockwise];
    LineDef* other = cown->lineDef;
    int side;

    if(other == line) return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

    if(!((other->inFlags & LF_BSPWINDOW) && other->L_frontsector != sector))
    {
        if(!other->L_frontsidedef || !other->L_backsidedef)
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
        if(other->L_sidedef(side)->SW_middlematerial)
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

                if(!R_MiddleMaterialCoversLineOpening(other, side, false))
                    return 0;
            }
        }
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

LineDef* R_FindLineBackNeighbor(const Sector* sector, const LineDef* line,
    const lineowner_t* own, boolean antiClockwise, binangle_t* diff)
{
    lineowner_t* cown = own->link[!antiClockwise];
    LineDef* other = cown->lineDef;

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle : own->angle);

    if(!other->L_backsidedef || other->L_frontsector != other->L_backsector ||
       (other->inFlags & LF_BSPWINDOW))
    {
        if(!(other->L_frontsector == sector ||
             (other->L_backsidedef && other->L_backsector == sector)))
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineBackNeighbor(sector, line, cown, antiClockwise, diff);
}

LineDef* R_FindLineAlignNeighbor(const Sector* sec, const LineDef* line,
    const lineowner_t* own, boolean antiClockwise, int alignment)
{
#define SEP 10

    lineowner_t* cown = own->link[!antiClockwise];
    LineDef* other = cown->lineDef;
    binangle_t diff;

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
    if((!other->L_backsidedef || !other->L_frontsidedef))
        return NULL;

    // Not suitable, try the next.
    return R_FindLineAlignNeighbor(sec, line, cown, antiClockwise, alignment);

#undef SEP
}

/**
 * The test is done on BSP leafs.
 */
#if 0 /* Currently unused. */
static Sector *getContainingSectorOf(GameMap* map, Sector* sec)
{
    uint                i;
    float               cdiff = -1, diff;
    float               inner[4], outer[4];
    Sector*             other, *closest = NULL;

    memcpy(inner, sec->bBox, sizeof(inner));

    // Try all sectors that fit in the bounding box.
    for(i = 0, other = map->sectors; i < map->numSectors; other++, ++i)
    {
        if(!other->lineDefCount)
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

static __inline void initSurfaceMaterialOffset(Surface* suf)
{
    assert(suf);
    suf->visOffset[VX] = suf->oldOffset[0][VX] =
        suf->oldOffset[1][VX] = suf->offset[VX];
    suf->visOffset[VY] = suf->oldOffset[0][VY] =
        suf->oldOffset[1][VY] = suf->offset[VY];
}

/**
 * Set intial values of various tracked and interpolated properties
 * (lighting, smoothed planes etc).
 */
void R_MapInitSurfaces(boolean forceUpdate)
{
    if(novideo) return;

    { uint i;
    for(i = 0; i < NUM_SECTORS; ++i)
    {
        Sector* sec = SECTOR_PTR(i);
        uint j;

        R_UpdateSector(sec, forceUpdate);
        for(j = 0; j < sec->planeCount; ++j)
        {
            Plane* pln = sec->SP_plane(j);

            pln->visHeight = pln->oldHeight[0] = pln->oldHeight[1] = pln->height;
            initSurfaceMaterialOffset(&pln->surface);
        }
    }}

    { uint i;
    for(i = 0; i < NUM_SIDEDEFS; ++i)
    {
        SideDef* si = SIDE_PTR(i);

        initSurfaceMaterialOffset(&si->SW_topsurface);
        initSurfaceMaterialOffset(&si->SW_middlesurface);
        initSurfaceMaterialOffset(&si->SW_bottomsurface);
    }}
}

static void addToSurfaceLists(Surface* suf, material_t* mat)
{
    if(!suf || !mat) return;

    if(Material_HasGlow(mat))         R_SurfaceListAdd(GameMap_GlowingSurfaces(theMap),   suf);
    if(Materials_HasDecorations(mat)) R_SurfaceListAdd(GameMap_DecoratedSurfaces(theMap), suf);
}

void R_MapInitSurfaceLists(void)
{
    if(novideo) return;

    R_SurfaceListClear(GameMap_DecoratedSurfaces(theMap));
    R_SurfaceListClear(GameMap_GlowingSurfaces(theMap));

    { uint i;
    for(i = 0; i < NUM_SIDEDEFS; ++i)
    {
        SideDef* side = SIDE_PTR(i);

        addToSurfaceLists(&side->SW_middlesurface, side->SW_middlematerial);
        addToSurfaceLists(&side->SW_topsurface,    side->SW_topmaterial);
        addToSurfaceLists(&side->SW_bottomsurface, side->SW_bottommaterial);
    }}

    { uint i;
    for(i = 0; i < NUM_SECTORS; ++i)
    {
        Sector* sec = SECTOR_PTR(i);
        if(0 == sec->lineDefCount)
            continue;

        { uint j;
        for(j = 0; j < sec->planeCount; ++j)
            addToSurfaceLists(&sec->SP_planesurface(j), sec->SP_planematerial(j));
        }
    }}
}

void R_SetupMap(int mode, int flags)
{
    DENG_UNUSED(flags);

    switch(mode)
    {
    case DDSMM_INITIALIZE:
        // A new map is about to be setup.
        ddMapSetup = true;

        Materials_PurgeCacheQueue();
        return;

    case DDSMM_AFTER_LOADING:
        assert(theMap);

        // Update everything again. Its possible that after loading we
        // now have more HOMs to fix, etc..
        GameMap_InitSkyFix(theMap);
        R_MapInitSurfaces(true);
        GameMap_InitPolyobjs(theMap);
        DD_ResetTimer();
        return;

    case DDSMM_FINALIZE: {
        ded_mapinfo_t* mapInfo;
        float startTime;
        char cmd[80];
        int i;

        assert(theMap);

        if(gameTime > 20000000 / TICSPERSEC)
        {
            // In very long-running games, gameTime will become so large that it cannot be
            // accurately converted to 35 Hz integer tics. Thus it needs to be reset back
            // to zero.
            gameTime = 0;
        }

        // We are now finished with the game data, map object db.
        P_DestroyGameMapObjDB(&theMap->gameObjData);

        // Init server data.
        Sv_InitPools();

        // Recalculate the light range mod matrix.
        Rend_CalcLightModRange();

        GameMap_InitPolyobjs(theMap);
        P_MapSpawnPlaneParticleGens();

        R_MapInitSurfaces(true);
        R_MapInitSurfaceLists();

        startTime = Sys_GetSeconds();
        R_PrecacheForMap();
        Materials_ProcessCacheQueue();
        VERBOSE( Con_Message("Precaching took %.2f seconds.\n", Sys_GetSeconds() - startTime) )

        S_SetupForChangedMap();

        // Map setup has been completed.

        // Run any commands specified in Map Info.
        mapInfo = Def_GetMapInfo(GameMap_Uri(theMap));
        if(mapInfo && mapInfo->execute)
        {
            Con_Execute(CMDS_SCRIPT, mapInfo->execute, true, false);
        }

        // Run the special map setup command, which the user may alias to do something useful.
        { ddstring_t* mapPath = Uri_Resolved(GameMap_Uri(theMap));
        sprintf(cmd, "init-%s", Str_Text(mapPath));
        Str_Delete(mapPath);
        if(Con_IsValidCommand(cmd))
        {
            Con_Executef(CMDS_SCRIPT, false, "%s", cmd);
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

            //clients[i].numTics = 0;

            // Determine if the player is in the void.
            ddpl->inVoid = true;
            if(ddpl->mo)
            {
                BspLeaf* bspLeaf = P_BspLeafAtPoint(ddpl->mo->origin);

                /// @todo $nplanes
                if(bspLeaf && ddpl->mo->origin[VZ] >= bspLeaf->sector->SP_floorvisheight && ddpl->mo->origin[VZ] < bspLeaf->sector->SP_ceilvisheight - 4)
                   ddpl->inVoid = false;
            }
        }

        // Reset the map tick timer.
        ddMapTime = 0;

        // We've finished setting up the map.
        ddMapSetup = false;

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;

        Z_PrintStatus();
        return;
      }

    default:
        Con_Error("R_SetupMap: Unknown setup mode %i", mode);
        exit(1); // Unreachable.
    }
}

void R_ClearSectorFlags(void)
{
    uint i;
    for(i = 0; i < NUM_SECTORS; ++i)
    {
        Sector* sec = SECTOR_PTR(i);
        // Clear all flags that can be cleared before each frame.
        sec->frameFlags &= ~SIF_FRAME_CLEAR;
    }
}

boolean R_IsGlowingPlane(const Plane* pln)
{
    /// @todo We should not need to prepare to determine this.
    material_t* mat = pln->surface.material;
    if(mat)
    {
        const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
            MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
        const materialsnapshot_t* ms = Materials_Prepare(mat, spec, true);

        if(!Material_IsDrawable(mat) || ms->glowing > 0) return true;
    }
    return Surface_IsSkyMasked(&pln->surface);
}

float R_GlowStrength(const Plane* pln)
{
    material_t* mat = pln->surface.material;
    if(mat)
    {
        if(Material_IsDrawable(mat) && !Surface_IsSkyMasked(&pln->surface))
        {
            /// @todo We should not need to prepare to determine this.
            const materialvariantspecification_t* spec = Materials_VariantSpecificationForContext(
                MC_MAPSURFACE, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT, -1, -1, -1, true, true, false, false);
            const materialsnapshot_t* ms = Materials_Prepare(mat, spec, true);

            return ms->glowing;
        }
    }
    return 0;
}

/**
 * Does the specified sector contain any sky surfaces?
 *
 * @return              @c true, if one or more surfaces in the given sector
 *                      use the special sky mask material.
 */
boolean R_SectorContainsSkySurfaces(const Sector* sec)
{
    boolean sectorContainsSkySurfaces = false;
    uint n = 0;
    do
    {
        if(Surface_IsSkyMasked(&sec->SP_planesurface(n)))
            sectorContainsSkySurfaces = true;
        else
            n++;
    } while(!sectorContainsSkySurfaces && n < sec->planeCount);
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
static material_t* chooseFixMaterial(SideDef* s, SideDefSection section)
{
    material_t* choice1 = 0, *choice2 = 0;
    LineDef* line = s->line;
    byte side = (line->L_frontsidedef == s? FRONT : BACK);
    Sector* frontSec = line->L_sector(side);
    Sector* backSec  = line->L_sidedef(side^1)? line->L_sector(side^1) : 0;

    if(backSec)
    {
        // Our first choice is a material in the other sector.
        if(section == SS_BOTTOM)
        {
            if(frontSec->SP_floorheight < backSec->SP_floorheight)
            {
                choice1 = backSec->SP_floormaterial;
            }
        }
        else if(section == SS_TOP)
        {
            if(frontSec->SP_ceilheight  > backSec->SP_ceilheight)
            {
                choice1 = backSec->SP_ceilmaterial;
            }
        }

        // In the special case of sky mask on the back plane, our best
        // choice is always this material.
        if(choice1 && Material_IsSkyMasked(choice1))
        {
            return choice1;
        }
    }
    else
    {
        // Our first choice is a material on an adjacent wall section.
        // Try the left neighbor first.
        LineDef* other = R_FindLineNeighbor(frontSec, line, line->L_vo(side),
                                            false /*next clockwise*/, NULL/*angle delta is irrelevant*/);
        if(!other)
            // Try the right neighbor.
            other = R_FindLineNeighbor(frontSec, line, line->L_vo(side^1),
                                       true /*next anti-clockwise*/, NULL/*angle delta is irrelevant*/);

        if(other)
        {
            if(!other->L_backsidedef)
            {
                // Our choice is clear - the middle material.
                choice1 = other->L_frontsidedef->SW_middlematerial;
            }
            else
            {
                // Compare the relative heights to decide.
                SideDef* otherSide = other->L_sidedef(other->L_frontsector == frontSec? FRONT : BACK);
                Sector* otherSec = other->L_sector(other->L_frontsector == frontSec? BACK  : FRONT);
                if(otherSec->SP_ceilheight <= frontSec->SP_floorheight)
                    choice1 = otherSide->SW_topmaterial;
                else if(otherSec->SP_floorheight >= frontSec->SP_ceilheight)
                    choice1 = otherSide->SW_bottommaterial;
                else if(otherSec->SP_ceilheight < frontSec->SP_ceilheight)
                    choice1 = otherSide->SW_topmaterial;
                else if(otherSec->SP_floorheight > frontSec->SP_floorheight)
                    choice1 = otherSide->SW_bottommaterial;
                // else we'll settle for a plane material.
            }
        }
    }

    // Our second choice is a material from this sector.
    choice2 = frontSec->SP_plane(section == SS_BOTTOM? PLN_FLOOR : PLN_CEILING)->PS_material;

    // Prefer a non-animated, non-masked material.
    if(choice1 && !Material_IsGroupAnimated(choice1) && !Material_IsSkyMasked(choice1))
        return choice1;
    if(choice2 && !Material_IsGroupAnimated(choice2) && !Material_IsSkyMasked(choice2))
        return choice2;

    // Prefer a non-masked material.
    if(choice1 && !Material_IsSkyMasked(choice1))
        return choice1;
    if(choice2 && !Material_IsSkyMasked(choice2))
        return choice2;

    // At this point we'll accept anything if it means avoiding HOM.
    if(choice1) return choice1;
    if(choice2) return choice2;

    // We'll assign the special "missing" material...
    return Materials_ToMaterial(Materials_ResolveUriCString(MN_SYSTEM_NAME":missing"));
}

static void addMissingMaterial(SideDef* s, SideDefSection section)
{
    Surface* suf;

    // A material must be missing for this test to apply.
    suf = &s->sections[section];
    if(suf->material) return;

    // Look for a suitable replacement.
    Surface_SetMaterial(suf, chooseFixMaterial(s, section));
    suf->inFlags |= SUIF_FIX_MISSING_MATERIAL;

    // During map load we log missing materials.
    if(ddMapSetup)
    {
        VERBOSE(
            Uri* uri = suf->material? Materials_ComposeUri(Materials_Id(suf->material)) : 0;
            AutoStr* path = uri? Uri_ToString(uri) : 0;
            Con_Message("Warning: SideDef #%u is missing a material for the %s section.\n"
                        "  %s was chosen to complete the definition.\n", s->buildData.index-1,
                        (section == SS_MIDDLE? "middle" : section == SS_TOP? "top" : "bottom"),
                        path? Str_Text(path) : "<null>");
            if(uri) Uri_Delete(uri);
        )
    }
}

void R_UpdateLinedefsOfSector(Sector* sec)
{
    uint i;

    if(!sec) return;

    for(i = 0; i < sec->lineDefCount; ++i)
    {
        LineDef* li = sec->lineDefs[i];
        SideDef* front, *back;
        Sector* frontSec, *backSec;

        if(LINE_SELFREF(li)) continue;

        front = li->L_frontsidedef;
        back  = li->L_backsidedef;
        frontSec = li->L_frontsector;
        backSec = li->L_backsector;

        // Do not fix "windows".
        if(!front || (!back && backSec)) continue;

        /**
         * Do as in the original Doom if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless it is skymasked),
         * or if there is a midtexture use that instead.
         */

        if(backSec)
        {
            // Bottom section.
            if(frontSec->SP_floorheight < backSec->SP_floorheight)
                addMissingMaterial(front, SS_BOTTOM);
            else if(frontSec->SP_floorheight > backSec->SP_floorheight)
                addMissingMaterial(back, SS_BOTTOM);

            // Top section.
            if(backSec->SP_ceilheight < frontSec->SP_ceilheight)
                addMissingMaterial(front, SS_TOP);
            else if(backSec->SP_ceilheight > frontSec->SP_ceilheight)
                addMissingMaterial(back, SS_TOP);
        }
        else
        {
            // Middle section.
            addMissingMaterial(front, SS_MIDDLE);
        }
    }
}

boolean R_UpdatePlane(Plane* pln, boolean forceUpdate)
{
    Sector* sec = pln->sector;
    boolean changed = false;

    // Geometry change?
    if(forceUpdate || pln->height != pln->oldHeight[1])
    {
        // Check if there are any camera players in this sector. If their
        // height is now above the ceiling/below the floor they are now in
        // the void.
        { uint i;
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t* plr = &ddPlayers[i];
            ddplayer_t* ddpl = &plr->shared;

            if(!ddpl->inGame || !ddpl->mo || !ddpl->mo->bspLeaf)
                continue;

            /// @todo $nplanes
            if((ddpl->flags & DDPF_CAMERA) && ddpl->mo->bspLeaf->sector == sec &&
               (ddpl->mo->origin[VZ] > sec->SP_ceilheight - 4 || ddpl->mo->origin[VZ] < sec->SP_floorheight))
            {
                ddpl->inVoid = true;
            }
        }}

        // Update the base origins for this plane and all relevant sidedef surfaces.
        Surface_UpdateBaseOrigin(&pln->surface);
        { uint i;
        for(i = 0; i < sec->lineDefCount; ++i)
        {
            LineDef* line = sec->lineDefs[i];
            if(line->L_frontsidedef) // $degenleaf
            {
                SideDef_UpdateBaseOrigins(line->L_frontsidedef);
            }
            if(line->L_backsidedef)
            {
                SideDef_UpdateBaseOrigins(line->L_backsidedef);
            }
        }}

        // Inform the shadow bias of changed geometry.
        if(sec->bspLeafs && *sec->bspLeafs)
        {
            BspLeaf** bspLeafIter = sec->bspLeafs;
            for(; *bspLeafIter; bspLeafIter++)
            {
                BspLeaf* bspLeaf = *bspLeafIter;
                if(bspLeaf->hedge)
                {
                    HEdge* hedge = bspLeaf->hedge;
                    do
                    {
                        if(hedge->lineDef)
                        {
                            uint i;
                            for(i = 0; i < 3; ++i)
                            {
                                SB_SurfaceMoved(hedge->bsuf[i]);
                            }
                        }
                    } while((hedge = hedge->next) != bspLeaf->hedge);
                }

                SB_SurfaceMoved(bspLeaf->bsuf[pln->planeID]);
            }
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
boolean R_UpdateBspLeaf(BspLeaf* bspLeaf, boolean forceUpdate)
{
    return false; // Not changed.
}
#endif

boolean R_UpdateSector(Sector* sec, boolean forceUpdate)
{
    boolean changed = false, planeChanged = false;
    uint i;

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

    if(forceUpdate || planeChanged)
    {
        Sector_UpdateBaseOrigin(sec);
        R_UpdateLinedefsOfSector(sec);
        S_MarkSectorReverbDirty(sec);
        changed = true;
    }

    return planeChanged;
}

/**
 * Stub.
 */
boolean R_UpdateLinedef(LineDef* line, boolean forceUpdate)
{
    return false; // Not changed.
}

/**
 * Stub.
 */
boolean R_UpdateSidedef(SideDef* side, boolean forceUpdate)
{
    return false; // Not changed.
}

/**
 * Stub.
 */
boolean R_UpdateSurface(Surface* suf, boolean forceUpdate)
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

float R_ExtraLightDelta(void)
{
    return extraLightDelta;
}

float R_CheckSectorLight(float lightlevel, float min, float max)
{
    // Has a limit been set?
    if(min == max) return 1;
    Rend_ApplyLightAdaptation(&lightlevel);
    return MINMAX_OF(0, (lightlevel - min) / (float) (max - min), 1);
}

const float* R_GetSectorLightColor(const Sector* sector)
{
    static vec3f_t skyLightColor, oldSkyAmbientColor = { -1, -1, -1 };
    static float oldRendSkyLight = -1;
    if(rendSkyLight > .001f && R_SectorContainsSkySurfaces(sector))
    {
        const ColorRawf* ambientColor = R_SkyAmbientColor();
        if(rendSkyLight != oldRendSkyLight ||
           !INRANGE_OF(ambientColor->red,   oldSkyAmbientColor[CR], .001f) ||
           !INRANGE_OF(ambientColor->green, oldSkyAmbientColor[CG], .001f) ||
           !INRANGE_OF(ambientColor->blue,  oldSkyAmbientColor[CB], .001f))
        {
            vec3f_t white = { 1, 1, 1 };
            V3f_Copy(skyLightColor, ambientColor->rgb);
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            V3f_Lerp(skyLightColor, skyLightColor, white, 1-rendSkyLight);

            // When the sky light color changes we must update the lightgrid.
            LG_MarkAllForUpdate();
            V3f_Copy(oldSkyAmbientColor, ambientColor->rgb);
        }
        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }
    // A non-skylight sector (i.e., everything else!)
    return sector->rgb; // The sector's ambient light color.
}

coord_t R_SkyCapZ(BspLeaf* bspLeaf, int skyCap)
{
    const planetype_t plane = (skyCap & SKYCAP_UPPER)? PLN_CEILING : PLN_FLOOR;
    if(!bspLeaf) Con_Error("R_SkyCapZ: Invalid bspLeaf argument (=NULL).");
    if(!bspLeaf->sector || !P_IsInVoid(viewPlayer)) return GameMap_SkyFix(theMap, plane == PLN_CEILING);
    return bspLeaf->sector->SP_planevisheight(plane);
}
