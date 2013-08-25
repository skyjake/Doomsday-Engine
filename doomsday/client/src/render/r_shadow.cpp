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

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <de/vector1.h> /// @todo remove me

#include <de/Matrix>
#include <de/Vector>

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"
#include "de_play.h"
#include "api_map.h"

#include "world/map.h"
#include "BspLeaf"

#include "render/r_shadow.h"

using namespace de;

typedef struct listnode_s {
    struct listnode_s *next, *nextUsed;
    shadowprojection_t projection;
} listnode_t;

typedef struct {
    listnode_t *head;
} shadowprojectionlist_t;

/// Orientation is toward the projectee.
typedef struct {
    float blendFactor; /// Multiplied with projection alpha.
    Vector3d v1; /// Top left vertex of the surface being projected to.
    Vector3d v2; /// Bottom right vertex of the surface being projected to.
    /// Normalized tangent space matrix of the surface being projected to.
    de::Matrix3f tangentMatrix;
} shadowprojectparams_t;

// List nodes.
static listnode_t *firstNode, *cursorNode;

// Projection lists.
static uint projectionListCount, cursorList;
static shadowprojectionlist_t *projectionLists;

/**
 * Create a new projection list.
 *
 * @return  Unique identifier attributed to the new list.
 */
static uint newList()
{
    shadowprojectionlist_t *list;

    // Do we need to allocate more lists?
    if(++cursorList >= projectionListCount)
    {
        projectionListCount *= 2;
        if(!projectionListCount) projectionListCount = 2;

        projectionLists = (shadowprojectionlist_t *)
            Z_Realloc(projectionLists, projectionListCount * sizeof *projectionLists, PU_MAP);
    }

    list = &projectionLists[cursorList - 1];
    list->head = 0;

    return cursorList;
}

/**
 * @param listIdx  Address holding the list index to retrieve.
 *      If the referenced list index is non-zero return the associated list.
 *      Otherwise allocate a new list and write it's index back to this address.
 * @return  ProjectionList associated with the (possibly newly attributed) index.
 */
static shadowprojectionlist_t *getList(uint *listIdx)
{
    // Do we need to allocate a list?
    if(!(*listIdx))
    {
        *listIdx = newList();
    }
    return projectionLists + ((*listIdx)-1); // 1-based index.
}

static listnode_t *newListNode()
{
    listnode_t *node;

    // Do we need to allocate mode nodes?
    if(cursorNode == 0)
    {
        node = (listnode_t *)Z_Malloc(sizeof *node, PU_APPSTATIC, 0);

        // Link the new node to the list.
        node->nextUsed = firstNode;
        firstNode = node;
    }
    else
    {
        node = cursorNode;
        cursorNode = cursorNode->nextUsed;
    }

    node->next = 0;
    return node;
}

static listnode_t *newProjection(Vector2f const &s, Vector2f const &t, float alpha)
{
    listnode_t *node = newListNode();

    shadowprojection_t &sp = node->projection;
    sp.s[0] = s[0];
    sp.s[1] = s[1];
    sp.t[0] = t[0];
    sp.t[1] = t[1];
    sp.alpha = de::clamp(0.f, alpha, 1.f);

    return node;
}

/// @return  Same as @a node for convenience (chaining).
static listnode_t *linkProjectionToList(listnode_t *node, shadowprojectionlist_t *list)
{
    DENG_ASSERT(node != 0 && list != 0);

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
static inline void newShadowProjection(uint *listIdx, Vector2f const &s,
    Vector2f const &t, float alpha)
{
    linkProjectionToList(newProjection(s, t, alpha), getList(listIdx));
}

static inline bool genTexCoords(Vector2f &s, Vector2f &t,
    Vector3d const &point, float scale, Vector3d const &v1, Vector3d const &v2,
    Matrix3f const &tangentMatrix)
{
    // Counteract aspect correction slightly (not too round mind).
    return R_GenerateTexCoords(s, t, point, scale, scale * 1.08f, v1, v2, tangentMatrix);
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
 * @param parameters  ProjectShadowToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
int RIT_ProjectShadowToSurfaceIterator(void *mobj, void *parameters)
{
    DENG_ASSERT(mobj && parameters);

    mobj_t *mo = (mobj_t *)mobj;
    projectshadowonsurfaceiteratorparams_t *p = (projectshadowonsurfaceiteratorparams_t *)parameters;
    shadowprojectparams_t const &spParams = p->spParams;

    coord_t mobjOrigin[3];
    Mobj_OriginSmoothed(mo, mobjOrigin);

    // Is this too far?
    coord_t distanceFromViewer = 0;
    if(shadowMaxDistance > 0)
    {
        distanceFromViewer = Rend_PointDist2D(mobjOrigin);
        if(distanceFromViewer > shadowMaxDistance)
            return false;
    }

    // Should this mobj even have a shadow?
    float shadowStrength = R_ShadowStrength(mo) * shadowFactor;
    if(usingFog) shadowStrength /= 2;
    if(shadowStrength <= 0)
        return false;

    // Calculate the radius of the shadow.
    float shadowRadius = R_VisualRadius(mo);
    if(shadowRadius <= 0)
        return false;

    if(shadowRadius > (float) shadowMaxRadius)
        shadowRadius = (float) shadowMaxRadius;

    mobjOrigin[VZ] -= mo->floorClip;
    if(mo->ddFlags & DDMF_BOB)
        mobjOrigin[VZ] -= R_GetBobOffset(mo);

    coord_t mobjHeight = mo->height;
    if(!mobjHeight) mobjHeight = 1;

    // If this were a light this is where we would check whether the origin is on
    // the right side of the surface. However this is a shadow and light is moving
    // in the opposite direction (inward toward the mobj's origin), therefore this
    // has "volume/depth".

    // Calculate 3D distance between surface and mobj.
    Vector3d point = R_ClosestPointOnPlane(spParams.tangentMatrix.column(2)/*normal*/,
                                           spParams.v1, mobjOrigin);
    coord_t distanceFromSurface = (Vector3d(mobjOrigin) - Vector3d(point)).length();

    // Too far above or below the shadowed surface?
    if(distanceFromSurface > mo->height) return false;
    if(mobjOrigin[VZ] + mo->height < point[VZ]) return false;
    if(distanceFromSurface > shadowRadius) return false;

    // Calculate the final strength of the shadow's attribution to the surface.
    shadowStrength *= 1.5f - 1.5f * distanceFromSurface / shadowRadius;

    // Fade at half mobj height for smooth fade out when embedded in the surface.
    coord_t halfMobjHeight = mobjHeight / 2;
    if(distanceFromSurface > halfMobjHeight)
    {
        shadowStrength *= 1 - (distanceFromSurface - halfMobjHeight) / (mobjHeight - halfMobjHeight);
    }

    // Fade when nearing the maximum distance?
    shadowStrength *= R_ShadowAttenuationFactor(distanceFromViewer);

    // Apply the external blending factor.
    shadowStrength *= spParams.blendFactor;

    // Would this shadow be seen?
    if(shadowStrength < SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return false;

    // Project this shadow.
    float scale = 1.0f / ((2.f * shadowRadius) - distanceFromSurface);
    Vector2f s, t;
    if(genTexCoords(s, t, point, scale, spParams.v1, spParams.v2, spParams.tangentMatrix))
    {
        // Attach to the projection list.
        newShadowProjection(&p->listIdx, s, t, shadowStrength);
    }

    return false;
}

void R_InitShadowProjectionListsForMap(Map &map)
{
    DENG_UNUSED(map);

    static bool firstTime = true;
    if(firstTime)
    {
        firstNode = 0;
        cursorNode = 0;
        firstTime = false;
    }
    // All memory for the lists is allocated from Zone so we can "forget" it.
    projectionLists = 0;
    projectionListCount = 0;
    cursorList = 0;
}

void R_InitShadowProjectionListsForNewFrame()
{
    // Disabled?
    if(!Rend_MobjShadowsEnabled()) return;

    // Start reusing nodes from the first one in the list.
    cursorNode = firstNode;

    // Clear the lists.
    cursorList = 0;
    if(projectionListCount)
    {
        std::memset(projectionLists, 0, projectionListCount * sizeof *projectionLists);
    }
}

uint R_ProjectShadowsToSurface(BspLeaf *bspLeaf, float blendFactor,
    Vector3d const &topLeft, Vector3d const &bottomRight,
    Matrix3f const &tangentMatrix)
{
    DENG_ASSERT(bspLeaf != 0);

    // Early test of the external blend factor for quick rejection.
    if(blendFactor < SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN) return 0;

    projectshadowonsurfaceiteratorparams_t p;

    p.listIdx = 0;
    p.spParams.blendFactor   = blendFactor;
    p.spParams.v1            = topLeft;
    p.spParams.v2            = bottomRight;
    p.spParams.tangentMatrix = tangentMatrix;

    R_IterateBspLeafContacts(*bspLeaf, OT_MOBJ, RIT_ProjectShadowToSurfaceIterator, (void *)&p);

    // Did we produce a projection list?
    return p.listIdx;
}

int R_IterateShadowProjections(uint listIdx, int (*callback) (shadowprojection_t const *, void *),
    void *parameters)
{
    int result = false; // Continue iteration.
    if(callback && listIdx != 0 && listIdx <= projectionListCount)
    {
        listnode_t *node = projectionLists[listIdx-1].head;
        while(node)
        {
            result = callback(&node->projection, parameters);
            node = (!result? node->next : 0 /* Early out */);
        }
    }
    return result;
}

int RIT_FindShadowPlaneIterator(Sector *sector, void *parameters)
{
    DENG_ASSERT(sector != 0);
    Plane **highest = (Plane **)parameters;
    Plane *compare = &sector->floor();
    if(compare->visHeight() > (*highest)->visHeight())
        *highest = compare;
    return false; // Continue iteration.
}

Plane *R_FindShadowPlane(mobj_t *mo)
{
    DENG_ASSERT(mo);
    if(mo->bspLeaf)
    {
        Plane *plane = &mo->bspLeaf->visFloor();
        /// @todo fixme: Use the visual planes of touched sector clusters.
        P_MobjSectorsIterator(mo, RIT_FindShadowPlaneIterator, (void *)&plane);
    }
    return 0;
}
