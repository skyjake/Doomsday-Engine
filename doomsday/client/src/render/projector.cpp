/** @file projector.cpp Texture coordinate projector and projection lists.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <de/memoryzone.h>

#include "de_platform.h"
#include "de_graphics.h"
#include "de_render.h"

#include "BspLeaf"
#include "world/p_object.h"
#include "world/p_objlink.h"

#include "render/projector.h"

using namespace de;

struct ListNode
{
    ListNode *next, *nextUsed;
    TexProjection projection;
};

struct List
{
    ListNode *head;
    bool sortByLuma; ///< @c true= Sort from brightest to darkest.
};

// Projection list nodes.
static ListNode *firstNode, *cursorNode;

// Projection lists.
static uint listCount, cursorList;
static List *lists;

/**
 * Find/create a new projection list.
 *
 * @param listIdx     Address holding the list index to retrieve. If the referenced
 *                    list index is non-zero return the associated list. Otherwise
 *                    allocate a new list and write it's index back to this address.
 *
 * @param sortByLuma  @c true= The list should maintain luma-sorted order.
 *
 * @return  ProjectionList associated with the (possibly newly attributed) index.
 */
static List *newList(uint *listIdx, bool sortByLuma)
{
    DENG_ASSERT(listIdx != 0);

    // Do we need to allocate a list?
    if(!(*listIdx))
    {
        // Do we need to allocate more lists?
        if(++cursorList >= listCount)
        {
            listCount *= 2;
            if(!listCount) listCount = 2;

            lists = (List *) Z_Realloc(lists, listCount * sizeof(*lists), PU_MAP);
        }

        List *list = &lists[cursorList-1];
        list->head       = 0;
        list->sortByLuma = sortByLuma;

        *listIdx = cursorList;
    }

    return lists + ((*listIdx) - 1); // 1-based index.
}

static ListNode *newListNode()
{
    ListNode *node;

    // Do we need to allocate mode nodes?
    if(!cursorNode)
    {
        node = (ListNode *) Z_Malloc(sizeof(*node), PU_APPSTATIC, NULL);

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

static ListNode *newProjection(DGLuint texture, Vector2f const &topLeft,
    Vector2f const &bottomRight, Vector3f const &color, float alpha)
{
    DENG_ASSERT(texture != 0);

    ListNode *node = newListNode();

    TexProjection &tp = node->projection;
    tp.texture     = texture;
    tp.topLeft     = topLeft;
    tp.bottomRight = bottomRight;
    tp.color       = Vector4f(color, de::clamp(0.f, alpha, 1.f));

    return node;
}

/// Average color * alpha.
static inline float dynlightLuminosity(TexProjection *tp)
{
    DENG_ASSERT(tp != 0);
    return (tp->color.x + tp->color.y + tp->color.z) / 3 * tp->color.w;
}

static ListNode *linkNodeInList(ListNode *node, List *list)
{
    DENG_ASSERT(node && list);

    if(list->head && list->sortByLuma)
    {
        float luma = dynlightLuminosity(&node->projection);
        ListNode *iter = list->head, *last = iter;
        do
        {
            // Is this brighter than that being added?
            if(dynlightLuminosity(&iter->projection) > luma)
            {
                last = iter;
                iter = iter->next;
            }
            else
            {
                // Insert it here.
                node->next = last->next;
                last->next = node;
                return node;
            }
        } while(iter);
    }

    node->next = list->head;
    list->head = node;
    return node;
}

/**
 * Construct a new surface projection (and a list, if one has not already been
 * constructed for the referenced index).
 *
 * @param listIdx     Address holding the list index to retrieve. If the referenced
 *                    list index is non-zero return the associated list. Otherwise
 *                    allocate a new list and write it's index back to this address.
 * @param sortByLuma  @c true= The list should maintain luma-sorted order.
 * @param texture     GL identifier to texture attributed to the new projection.
 * @param topLeft     GL texture coordinates for the top left point in texture space.
 * @param bottomRight GL texture coordinates for the bottom right point in texture space.
 * @param color       RGB color attributed to the new projection.
 * @param alpha       Alpha attributed to the new projection.
 */
static inline void newProjection(uint *listIdx, bool sortByLuma, DGLuint texture,
    Vector2f const &topLeft, Vector2f const &bottomRight, Vector3f const &color,
    float alpha)
{
    linkNodeInList(newProjection(texture, topLeft, bottomRight, color, alpha),
                   newList(listIdx, sortByLuma));
}

static Lumobj::LightmapSemantic semanticFromFlags(int flags)
{
    if(flags & PLF_TEX_FLOOR)   return Lumobj::Down;
    if(flags & PLF_TEX_CEILING) return Lumobj::Up;
    return Lumobj::Side;
}

/// Returns the texture variant specification for lightmaps.
static texturevariantspecification_t &lightmapSpec()
{
    return GL_TextureVariantSpec(TC_MAPSURFACE_LIGHTMAP, 0, 0, 0, 0,
                                 GL_CLAMP, GL_CLAMP, 1, -1, -1,
                                 false, false, false, true);
}

static DGLuint prepareLightmap(Texture *tex = 0)
{
    if(tex)
    {
        if(TextureVariant *variant = tex->prepareVariant(lightmapSpec()))
        {
            return variant->glName();
        }
        // Dang...
    }
    // Prepare the default/fallback lightmap instead.
    return GL_PrepareLSTexture(LST_DYNAMIC);
}

void Rend_ProjectorInitForMap(Map &map)
{
    static bool firstTime = true;
    if(firstTime)
    {
        firstNode  = 0;
        cursorNode = 0;
        firstTime  = false;
    }

    // All memory for the lists is allocated from Zone so we can "forget" it.
    lists = 0;
    listCount = 0;
    cursorList = 0;
}

void Rend_ProjectorReset()
{
    // Start reusing nodes.
    cursorNode = firstNode;

    // Clear the lists.
    cursorList = 0;
    if(listCount)
    {
        std::memset(lists, 0, listCount * sizeof *lists);
    }
}

struct project_params_t
{
    uint *listIdx;                 ///< Index of any resulting projection list.
    int flags;                     ///< @ref lightProjectionFlags.
    float blendFactor;             ///< Multiplied with projection alpha.
    Vector3d const *topLeft;       ///< Top left vertex of the surface.
    Vector3d const *bottomRight;   ///< Bottom right vertex of the surface.
    Matrix3f const *tangentMatrix; ///< Normalized surface tangent space vectors.
};

/**
 * Project the given lumobj onto the surface.
 *
 * @param lum      Lumobj representing the light being projected.
 * @param parm     project_params_t parameters.
 */
static void projectLumobj(Lumobj &lum, project_params_t &parm)
{
    // Has this already been occluded?
    if(R_ViewerLumobjIsHidden(lum.indexInMap()))
        return;

    // No lightmap texture?
    DGLuint tex = prepareLightmap(lum.lightmap(semanticFromFlags(parm.flags)));
    if(!tex) return;

    Vector3d lumCenter = lum.origin();
    lumCenter.z += lum.zOffset();

    // On the right side?
    Vector3d topLeftToLum = *parm.topLeft - lumCenter;
    if(topLeftToLum.dot(parm.tangentMatrix->column(2)) > 0.f)
        return;

    // Calculate 3D distance between surface and lumobj.
    Vector3d pointOnPlane =
        R_ClosestPointOnPlane(parm.tangentMatrix->column(2)/*normal*/, *parm.topLeft, lumCenter);

    coord_t distToLum = (lumCenter - pointOnPlane).length();
    if(distToLum <= 0 || distToLum > lum.radius())
        return;

    // Calculate the final surface light attribution factor.
    float luma = 1.5f - 1.5f * distToLum / lum.radius();

    // Fade out as distance from viewer increases?
    if(lum.maxDistance() > 0)
    {
        luma *= lum.attenuation(R_ViewerLumobjDistance(lum.indexInMap()));
    }

    // Would this be seen?
    if(luma * parm.blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return;

    // Project, counteracting aspect correction slightly.
    Vector2f s, t;
    float const scale = 1.0f / ((2.f * lum.radius()) - distToLum);
    if(!R_GenerateTexCoords(s, t, pointOnPlane, scale, scale * 1.08f,
                            *parm.topLeft, *parm.bottomRight, *parm.tangentMatrix))
        return;

    // Write to the projection list.
    newProjection(parm.listIdx, (parm.flags & PLF_SORT_LUMINOSITY_DESC) != 0,
                  tex, Vector2f(s[0], t[0]), Vector2f(s[1], t[1]),
                  Rend_LuminousColor(lum.color(), luma), parm.blendFactor);
}

static int projectLumobjWorker(void *lum, void *context)
{
    projectLumobj(*static_cast<Lumobj *>(lum), *static_cast<project_params_t *>(context));
    return false; // Continue iteration.
}

void Rend_ProjectLumobjs(int flags, BspLeaf *bspLeaf, float blendFactor,
    Vector3d const &topLeft, Vector3d const &bottomRight, Matrix3f const &tangentMatrix,
    uint &listIdx)
{
    DENG_ASSERT(bspLeaf != 0);

    if(blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return;

    project_params_t parm; zap(parm);
    parm.listIdx       = &listIdx;
    parm.flags         = flags;
    parm.blendFactor   = blendFactor;
    parm.topLeft       = &topLeft;
    parm.bottomRight   = &bottomRight;
    parm.tangentMatrix = &tangentMatrix;

    R_IterateBspLeafContacts(*bspLeaf, OT_LUMOBJ, projectLumobjWorker, &parm);
}

/**
 * Project a plane glow onto the surface.
 *
 * @param surface  Glowing plane to project the light from.
 * @param origin   Origin of the glow in the map coordinate space.
 * @param parm     project_params_t parameters.
 */
static void projectGlow(Surface &surface, Vector3d const &origin,
    project_params_t &parm)
{
    // Is the material glowing at this moment?
    Vector3f color;
    float intensity = surface.glow(color);
    if(intensity < .05f)
        return;

    coord_t glowHeight = Rend_PlaneGlowHeight(intensity);
    if(glowHeight < 2)
        return; // Not too small!

    // Calculate coords.
    float bottom, top;
    if(surface.normal().z < 0)
    {
        // Cast downward.
              bottom = (origin.z - parm.topLeft->z) / glowHeight;
        top = bottom + (parm.topLeft->z - parm.bottomRight->z) / glowHeight;
    }
    else
    {
        // Cast upward.
                 top = (parm.bottomRight->z - origin.z) / glowHeight;
        bottom = top + (parm.topLeft->z - parm.bottomRight->z) / glowHeight;
    }

    // Above/below on the Z axis?
    if(!(bottom <= 1 || top >= 0))
        return;

    // Write to the projection list.
    newProjection(parm.listIdx, (parm.flags & PLF_SORT_LUMINOSITY_DESC) != 0,
                  GL_PrepareLSTexture(LST_GRADIENT),
                  Vector2f(0, bottom), Vector2f(1, top),
                  Rend_LuminousColor(color, intensity), parm.blendFactor);
}

void Rend_ProjectPlaneGlows(int flags, BspLeaf *bspLeaf, float blendFactor,
    Vector3d const &topLeft, Vector3d const &bottomRight, Matrix3f const &tangentMatrix,
    uint &listIdx)
{
    DENG_ASSERT(bspLeaf != 0);

    if(bottomRight.z >= topLeft.z)
        return;

    if(blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return;

    project_params_t parm; zap(parm);
    parm.listIdx       = &listIdx;
    parm.flags         = flags;
    parm.blendFactor   = blendFactor;
    parm.topLeft       = &topLeft;
    parm.bottomRight   = &bottomRight;
    parm.tangentMatrix = &tangentMatrix;

    for(int i = 0; i < bspLeaf->sector().planeCount(); ++i)
    {
        Plane &plane = bspLeaf->visPlane(i);
        Vector3d pointOnPlane(bspLeaf->cluster().center(), plane.visHeight());

        projectGlow(plane.surface(), pointOnPlane, parm);
    }
}

/**
 * Project a mobj shadow onto the surface. If valid and the surface is
 * contacted a new projection node will constructed and returned.
 *
 * @param mobj  Mobj for which a shadow may be projected.
 * @param parameters  ProjectShadowToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
static void projectMobjShadow(mobj_t &mobj, project_params_t &parm)
{
    coord_t mobjOrigin[3];
    Mobj_OriginSmoothed(&mobj, mobjOrigin);

    // Is this too far?
    coord_t distanceFromViewer = 0;
    if(shadowMaxDistance > 0)
    {
        distanceFromViewer = Rend_PointDist2D(mobjOrigin);
        if(distanceFromViewer > shadowMaxDistance)
            return;
    }

    // Should this mobj even have a shadow?
    float shadowStrength = Mobj_ShadowStrength(&mobj) * shadowFactor;
    if(usingFog) shadowStrength /= 2;
    if(shadowStrength <= 0)
        return;

    // Calculate the radius of the shadow.
    float shadowRadius = Mobj_VisualRadius(&mobj);
    if(shadowRadius <= 0)
        return;

    if(shadowRadius > shadowMaxRadius)
        shadowRadius = shadowMaxRadius;

    mobjOrigin[VZ] -= mobj.floorClip;
    if(mobj.ddFlags & DDMF_BOB)
        mobjOrigin[VZ] -= Mobj_BobOffset(&mobj);

    coord_t mobjHeight = mobj.height;
    if(!mobjHeight) mobjHeight = 1;

    // If this were a light this is where we would check whether the origin is on
    // the right side of the surface. However this is a shadow and light is moving
    // in the opposite direction (inward toward the mobj's origin), therefore this
    // has "volume/depth".

    // Calculate 3D distance between surface and mobj.
    Vector3d point = R_ClosestPointOnPlane(parm.tangentMatrix->column(2)/*normal*/,
                                           *parm.topLeft, mobjOrigin);
    coord_t distanceFromSurface = (Vector3d(mobjOrigin) - Vector3d(point)).length();

    // Too far above or below the shadowed surface?
    if(distanceFromSurface > mobj.height)
        return;
    if(mobjOrigin[VZ] + mobj.height < point[VZ])
        return;
    if(distanceFromSurface > shadowRadius)
        return;

    // Calculate the final strength of the shadow's attribution to the surface.
    shadowStrength *= 1.5f - 1.5f * distanceFromSurface / shadowRadius;

    // Fade at half mobj height for smooth fade out when embedded in the surface.
    coord_t halfMobjHeight = mobjHeight / 2;
    if(distanceFromSurface > halfMobjHeight)
    {
        shadowStrength *= 1 - (distanceFromSurface - halfMobjHeight) / (mobjHeight - halfMobjHeight);
    }

    // Fade when nearing the maximum distance?
    shadowStrength *= Rend_ShadowAttenuationFactor(distanceFromViewer);

    // Apply the external blending factor.
    shadowStrength *= parm.blendFactor;

    // Would this shadow be seen?
    if(shadowStrength < SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return;

    // Project, counteracting aspect correction slightly.
    Vector2f s, t;
    float const scale = 1.0f / ((2.f * shadowRadius) - distanceFromSurface);
    if(!R_GenerateTexCoords(s, t, point, scale, scale * 1.08f,
                            *parm.topLeft, *parm.bottomRight, *parm.tangentMatrix))
        return;

    // Write to the projection list.
    newProjection(parm.listIdx, 0, GL_PrepareLSTexture(LST_DYNAMIC),
                  Vector2f(s[0], t[0]), Vector2f(s[1], t[1]),
                  Vector3f(0, 0, 0) /*Shadows are black*/, shadowStrength);
}

static int projectMobjShadowWorker(void *mobj, void *context)
{
    projectMobjShadow(*static_cast<mobj_t *>(mobj), *static_cast<project_params_t *>(context));
    return false; // Continue iteration.
}

void Rend_ProjectMobjShadows(BspLeaf *bspLeaf, float blendFactor,
    Vector3d const &topLeft, Vector3d const &bottomRight,
    Matrix3f const &tangentMatrix, uint &listIdx)
{
    DENG_ASSERT(bspLeaf != 0);

    if(blendFactor < SHADOW_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
        return;

    project_params_t parm; zap(parm);
    parm.listIdx       = &listIdx;
    parm.blendFactor   = blendFactor;
    parm.topLeft       = &topLeft;
    parm.bottomRight   = &bottomRight;
    parm.tangentMatrix = &tangentMatrix;

    R_IterateBspLeafContacts(*bspLeaf, OT_MOBJ, projectMobjShadowWorker, &parm);
}

int Rend_IterateProjectionList(uint listIdx, int (*callback) (TexProjection const *, void *),
                               void *context)
{
    if(callback && listIdx != 0 && listIdx <= listCount)
    {
        ListNode *node = lists[listIdx - 1].head;
        while(node)
        {
            if(int result = callback(&node->projection, context))
                return result;
            node = node->next;
        }
    }
    return 0; // Continue iteration.
}
