/** @file r_shadow.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "api_map.h"

typedef struct listnode_s {
    struct listnode_s* next, *nextUsed;
    shadowprojection_t projection;
} listnode_t;

typedef struct {
    listnode_t* head;
} shadowprojectionlist_t;

/// Orientation is toward the projectee.
typedef struct {
    float blendFactor; /// Multiplied with projection alpha.
    coord_t* v1; /// Top left vertex of the surface being projected to.
    coord_t* v2; /// Bottom right vertex of the surface being projected to.
    float* tangent; /// Normalized tangent of the surface being projected to.
    float* bitangent; /// Normalized bitangent of the surface being projected to.
    float* normal; /// Normalized normal of the surface being projected to.
} shadowprojectparams_t;

// List nodes.
static listnode_t* firstNode, *cursorNode;

// Projection lists.
static uint projectionListCount, cursorList;
static shadowprojectionlist_t* projectionLists;

/**
 * Create a new projection list.
 *
 * @param flags  @ref surfaceProjectionListFlags
 * @return  Unique identifier attributed to the new list.
 */
static uint newList(void)
{
    shadowprojectionlist_t* list;

    // Do we need to allocate more lists?
    if(++cursorList >= projectionListCount)
    {
        projectionListCount *= 2;
        if(!projectionListCount) projectionListCount = 2;

        projectionLists = (shadowprojectionlist_t*)Z_Realloc(projectionLists, projectionListCount * sizeof *projectionLists, PU_MAP);
        if(!projectionLists) Con_Error(__FILE__":newList failed on allocation of %lu bytes resizing the projection list.", (unsigned long) (projectionListCount * sizeof *projectionLists));
    }

    list = &projectionLists[cursorList-1];
    list->head = NULL;

    return cursorList;
}

/**
 * @param listIdx  Address holding the list index to retrieve.
 *      If the referenced list index is non-zero return the associated list.
 *      Otherwise allocate a new list and write it's index back to this address.
 * @return  ProjectionList associated with the (possibly newly attributed) index.
 */
static shadowprojectionlist_t* getList(uint* listIdx)
{
    // Do we need to allocate a list?
    if(!(*listIdx))
    {
        *listIdx = newList();
    }
    return projectionLists + ((*listIdx)-1); // 1-based index.
}

static listnode_t* newListNode(void)
{
    listnode_t* node;

    // Do we need to allocate mode nodes?
    if(cursorNode == NULL)
    {
        node = (listnode_t*)Z_Malloc(sizeof *node, PU_APPSTATIC, NULL);
        if(!node) Con_Error(__FILE__":newListNode failed on allocation of %lu bytes for new node.", (unsigned long) sizeof *node);

        // Link the new node to the list.
        node->nextUsed = firstNode;
        firstNode = node;
    }
    else
    {
        node = cursorNode;
        cursorNode = cursorNode->nextUsed;
    }

    node->next = NULL;
    return node;
}

static listnode_t* newProjection(float const s[2], float const t[2], float alpha)
{
    assert(s && t);
    {
    listnode_t* node = newListNode();
    shadowprojection_t* sp = &node->projection;
    sp->s[0] = s[0];
    sp->s[1] = s[1];
    sp->t[0] = t[0];
    sp->t[1] = t[1];
    sp->alpha = MINMAX_OF(0, alpha, 1);
    return node;
    }
}

/// @return  Same as @a node for convenience (chaining).
static listnode_t* linkProjectionToList(listnode_t* node, shadowprojectionlist_t* list)
{
    assert(node && list);
    node->next = list->head;
    list->head = node;
    return node;
}

/**
 * Construct a new shadow projection (and a list, if one has not already been
 * constructed for the referenced index).
 *
 * @param listIdx  Address holding the list index to retrieve.
 *      If the referenced list index is non-zero return the associated list.
 *      Otherwise allocate a new list and write it's index back to this address.
 * @param s  GL texture coordinates on the S axis [left, right] in texture space.
 * @param t  GL texture coordinates on the T axis [bottom, top] in texture space.
 * @param alpha  Alpha attributed to the new projection.
 */
static void newShadowProjection(uint* listIdx, float const s[2], float const t[2], float alpha)
{
    linkProjectionToList(newProjection(s, t, alpha), getList(listIdx));
}

static boolean genTexCoords(float s[2], float t[2], coord_t const point[3], float scale,
    coord_t const v1[3], coord_t const v2[3], float const tangent[2], float const bitangent[2])
{
    // Counteract aspect correction slightly (not too round mind).
    return R_GenerateTexCoords(s, t, point, scale, scale * 1.08f, v1, v2, tangent, bitangent);
}

float R_ShadowAttenuationFactor(coord_t distance)
{
    if(shadowMaxDistance > 0 && distance > 3 * shadowMaxDistance / 4)
    {
        return (shadowMaxDistance - distance) / (shadowMaxDistance / 4);
    }
    return 1;
}

typedef struct {
    uint listIdx;
    shadowprojectparams_t spParams;
} projectshadowonsurfaceiteratorparams_t;

/**
 * Project a mobj shadow onto the surface. If valid and the surface is
 * contacted a new projection node will constructed and returned.
 *
 * @param mobj  Mobj for which a shadow may be projected.
 * @param paramaters  ProjectShadowToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
int RIT_ProjectShadowToSurfaceIterator(void* obj, void* paramaters)
{
    assert(obj && paramaters);
    {
    mobj_t* mo = (mobj_t*)obj;
    projectshadowonsurfaceiteratorparams_t* p = (projectshadowonsurfaceiteratorparams_t*)paramaters;
    shadowprojectparams_t* spParams = &p->spParams;
    coord_t distanceFromViewer = 0, mobjHeight, halfMobjHeight, distanceFromSurface;
    float scale, shadowRadius, shadowStrength;
    coord_t mobjOrigin[3], point[3];
    vec2f_t s, t;

    Mobj_OriginSmoothed(mo, mobjOrigin);

    // Is this too far?
    if(shadowMaxDistance > 0)
    {
        distanceFromViewer = Rend_PointDist2D(mobjOrigin);
        if(distanceFromViewer > shadowMaxDistance) return false; // Continue iteration.
    }

    // Should this mobj even have a shadow?
    shadowStrength = R_ShadowStrength(mo) * shadowFactor;
    if(usingFog) shadowStrength /= 2;
    if(shadowStrength <= 0) return false; // Continue iteration.

    // Calculate the radius of the shadow.
    shadowRadius = R_VisualRadius(mo);
    if(shadowRadius <= 0) return false; // Continue iteration.
    if(shadowRadius > (float) shadowMaxRadius)
        shadowRadius = (float) shadowMaxRadius;

    mobjOrigin[VZ] -= mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
        mobjOrigin[VZ] -= R_GetBobOffset(mo);

    mobjHeight = mo->height;
    if(!mobjHeight) mobjHeight = 1;

    // If this were a light this is where we would check whether the origin is on
    // the right side of the surface. However this is a shadow and light is moving
    // in the opposite direction (inward toward the mobj's origin), therefore this
    // has "volume/depth".
    //
    // vec3d_t vToMobj;
    // V3d_Subtract(vToMobj, spParams->v1, mobjOrigin);
    // if(V3d_DotProductf(vToMobj, spParams->normal) > 0) return false; // Continue iteration

    // Calculate 3D distance between surface and mobj.
    V3d_ClosestPointOnPlanef(point, spParams->normal, spParams->v1, mobjOrigin);
    distanceFromSurface = V3d_Distance(point, mobjOrigin);

    // Too far above or below the shadowed surface?
    if(distanceFromSurface > mo->height) return false; // Continue iteration.
    if(mobjOrigin[VZ] + mo->height < point[VZ]) return false; // Continue iteration.
    if(distanceFromSurface > shadowRadius) return false; // Continue iteration.

    // Calculate the final strength of the shadow's attribution to the surface.
    shadowStrength *= 1.5f - 1.5f * distanceFromSurface / shadowRadius;

    // Fade at half mobj height for smooth fade out when embedded in the surface.
    halfMobjHeight = mobjHeight / 2;
    if(distanceFromSurface > halfMobjHeight)
    {
        shadowStrength *= 1 - (distanceFromSurface - halfMobjHeight) / (mobjHeight - halfMobjHeight);
    }

    // Fade when nearing the maximum distance?
    shadowStrength *= R_ShadowAttenuationFactor(distanceFromViewer);

    // Apply the external blending factor.
    shadowStrength *= spParams->blendFactor;

    // Would this shadow be seen?
    if(shadowStrength < SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN) return false; // Continue iteration.

    // Project this shadow.
    scale = 1.0f / ((2.f * shadowRadius) - distanceFromSurface);
    if(!genTexCoords(s, t, point, scale, spParams->v1, spParams->v2, spParams->tangent, spParams->bitangent)) return false; // Continue iteration.

    // Attach to the projection list.
    newShadowProjection(&p->listIdx, s, t, shadowStrength);

    return false; // Continue iteration.
    }
}

void R_InitShadowProjectionListsForMap(void)
{
    static boolean firstTime = true;
    if(firstTime)
    {
        firstNode = NULL;
        cursorNode = NULL;
        firstTime = false;
    }
    // All memory for the lists is allocated from Zone so we can "forget" it.
    projectionLists = NULL;
    projectionListCount = 0;
    cursorList = 0;
}

void R_InitShadowProjectionListsForNewFrame(void)
{
    // Start reusing nodes from the first one in the list.
    cursorNode = firstNode;

    // Clear the lists.
    cursorList = 0;
    if(projectionListCount)
    {
        memset(projectionLists, 0, projectionListCount * sizeof *projectionLists);
    }
}

uint R_ProjectShadowsToSurface(BspLeaf* bspLeaf, float blendFactor, coord_t topLeft[3],
    coord_t bottomRight[3], float tangent[3], float bitangent[3], float normal[3])
{
    projectshadowonsurfaceiteratorparams_t p;

    // Early test of the external blend factor for quick rejection.
    if(blendFactor < SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN) return 0;

    p.listIdx = 0;
    p.spParams.blendFactor = blendFactor;
    p.spParams.v1 = topLeft;
    p.spParams.v2 = bottomRight;
    p.spParams.tangent = tangent;
    p.spParams.bitangent = bitangent;
    p.spParams.normal = normal;

    R_IterateBspLeafContacts2(bspLeaf, OT_MOBJ, RIT_ProjectShadowToSurfaceIterator, (void*)&p);
    // Did we produce a projection list?
    return p.listIdx;
}

int R_IterateShadowProjections2(uint listIdx, int (*callback) (const shadowprojection_t*, void*),
    void* paramaters)
{
    int result = false; // Continue iteration.
    if(callback && listIdx != 0 && listIdx <= projectionListCount)
    {
        listnode_t* node = projectionLists[listIdx-1].head;
        while(node)
        {
            result = callback(&node->projection, paramaters);
            node = (!result? node->next : NULL /* Early out */);
        }
    }
    return result;
}

int R_IterateShadowProjections(uint listIdx, int (*callback) (const shadowprojection_t*, void*))
{
    return R_IterateShadowProjections2(listIdx, callback, NULL);
}

int RIT_FindShadowPlaneIterator(Sector* sector, void* paramaters)
{
    Plane** highest = (Plane**)paramaters;
    Plane* compare = sector->SP_plane(PLN_FLOOR);
    if(compare->visHeight > (*highest)->visHeight)
        *highest = compare;
    return false; // Continue iteration.
}

Plane* R_FindShadowPlane(mobj_t* mo)
{
    Plane* plane = NULL;
    assert(mo);
    if(mo->bspLeaf)
    {
        plane = mo->bspLeaf->sector->SP_plane(PLN_FLOOR);
        P_MobjSectorsIterator(mo, RIT_FindShadowPlaneIterator, (void*)&plane);
    }
    return plane;
}
