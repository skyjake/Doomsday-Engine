/**\file r_textureprojection.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"

typedef struct listnode_s {
    struct listnode_s* next, *nextUsed;
    textureprojection_t projection;
} listnode_t;

/**
 * @defgroup projectionListFlags  Projection List Flags
 * @{
 */
#define PLF_SORT_LUMINOUS_DESC  0x1 /// Sort by luminosity in descending order.
/**@}*/

typedef struct dynlist_s {
    int flags; /// @see projectionListFlags
    listnode_t* head;
} projectionlist_t;

/// Orientation is toward the projectee.
typedef struct {
    int flags; /// @see surfaceProjectFlags
    vec3_t v1; /// Top left vertex of the surface being projected to.
    vec3_t v2; /// Bottom right vertex of the surface being projected to.
    vec3_t normal; /// Normalized normal of the surface being projected to.
} surfaceprojectparams_t;

// List nodes.
static listnode_t* firstNode, *cursorNode;

// Surface projection lists.
static uint projectionListCount, cursorList;
static projectionlist_t* projectionLists;

void R_InitSurfaceProjectionListsForMap(void)
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

void R_InitSurfaceProjectionListsForNewFrame(void)
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

/**
 * Create a new projection list.
 *
 * @param flags  @see projectionListFlags
 * @return  Unique identifier attributed to the new list.
 */
static uint newList(int flags)
{
    projectionlist_t* list;

    // Do we need to allocate more lists?
    if(++cursorList >= projectionListCount)
    {
        projectionListCount *= 2;
        if(!projectionListCount) projectionListCount = 2;

        projectionLists = (projectionlist_t*)Z_Realloc(projectionLists, projectionListCount * sizeof *projectionLists, PU_MAP);
        if(!projectionLists) Con_Error(__FILE__":newList failed on allocation of %lu bytes resizing the projection list.", (unsigned long) (projectionListCount * sizeof *projectionLists));
    }

    list = &projectionLists[cursorList-1];
    list->head = NULL;
    list->flags = flags;

    return cursorList - 1;
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

static listnode_t* newProjection(DGLuint texture, const float s[2],
    const float t[2], const float color[3])
{
    assert(texture != 0 && s && t && color);
    {
    listnode_t* node = newListNode();
    textureprojection_t* tp = &node->projection;

    tp->texture = texture;
    tp->s[0] = s[0];
    tp->s[1] = s[1];
    tp->t[0] = t[0];
    tp->t[1] = t[1];
    tp->color[CR] = color[CR];
    tp->color[CG] = color[CG];
    tp->color[CB] = color[CB];

    return node;
    }
}

static float calcProjectionLuminosity(const textureprojection_t* tp)
{
    assert(tp);
    return (tp->color[CR] + tp->color[CG] + tp->color[CB]) / 3;
}

static void linkProjectionToList(listnode_t* node, projectionlist_t* list)
{
    assert(node && list);
    if((list->flags & PLF_SORT_LUMINOUS_DESC) && list->head)
    {
        float luma = calcProjectionLuminosity(&node->projection);
        listnode_t* iter = list->head, *last = iter;
        do
        {
            // Is this brighter than that being added?
            if(calcProjectionLuminosity(&iter->projection) > luma)
            {
                last = iter;
                iter = iter->next;
            }
            else
            {
                // Insert it here.
                node->next = last->next;
                last->next = node;
                return;
            }
        } while(iter);
    }

    node->next = list->head;
    list->head = node;
}

/**
 * Blend the given light value with the lumobj's color, apply any global
 * modifiers and output the result.
 *
 * @param outRGB  Calculated result will be written here.
 * @param color  Lumobj color.
 * @param light  Ambient light level of the surface being projected to.
 */
static void calcDynlightColor(float outRGB[3], const float color[3], float light)
{
    uint i;

    light = MINMAX_OF(0, light, 1) * dynlightFactor;
    // In fog additive blending is used; the normal fog color is way too bright.
    if(usingFog)
        light *= dynlightFogBright;

    // Multiply with the light color.
    for(i = 0; i < 3; ++i)
    {
        outRGB[i] = light * color[i];
    }
}

/**
 * Project a plane glow onto the surface. If valid and the surface is
 * contacted a new projection node will constructed and returned.
 *
 * @param lum  Lumobj representing the light being projected.
 * @param spParams  Surface projection paramaters.
 *
 * @return  Resultant projection node else @c NULL.
 */
static listnode_t* projectPlaneLightOnSurface(const lumobj_t* lum,
    const surfaceprojectparams_t* spParams)
{
    assert(lum && spParams);
    {
    float bottom = spParams->v2[VZ], top = spParams->v1[VZ];
    float glowHeight, s[2], t[2], color[3];

    // No lightmap texture?
    if(!LUM_PLANE(lum)->tex) return NULL;

    // No height?
    if(bottom >= top) return NULL;

    // Do not make too small glows.
    glowHeight = (GLOW_HEIGHT_MAX * LUM_PLANE(lum)->intensity) * glowHeightFactor;
    if(glowHeight <= 2) return NULL;

    if(glowHeight > glowHeightMax)
        glowHeight = glowHeightMax;

    // Calculate texture coords for the light.
    if(LUM_PLANE(lum)->normal[VZ] < 0)
    {
        // Light is cast downwards.
        t[1] = t[0] = (lum->pos[VZ] - top) / glowHeight;
        t[1]+= (top - bottom) / glowHeight;
    }
    else
    {
        // Light is cast upwards.
        t[0] = t[1] = (bottom - lum->pos[VZ]) / glowHeight;
        t[0]+= (top - bottom) / glowHeight;
    }

    // Above/below on the Y axis?
    if(!(t[0] <= 1 || t[1] >= 0)) return NULL; 

    // The horizontal direction is easy.
    s[0] = 0;
    s[1] = 1;

    calcDynlightColor(color, LUM_PLANE(lum)->color, LUM_PLANE(lum)->intensity);
    return newProjection(LUM_PLANE(lum)->tex, s, t, color);
    }
}

/**
 * Given a normalized normal, construct up and right vectors, oriented to
 * the original normal. Note all vectors and normals are in world-space.
 *
 * @param up  The up vector will be written back here.
 * @param right  The right vector will be written back here.
 * @param normal  Normal to construct vectors for.
 */
static void buildUpRight(pvec3_t up, pvec3_t right, const pvec3_t normal)
{
    const vec3_t rotm[3] = {
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f}
    };
    int axis = VX;
    vec3_t fn;

    V3_Set(fn, fabsf(normal[VX]), fabsf(normal[VY]), fabsf(normal[VZ]));

    if(fn[VY] > fn[axis])
        axis = VY;
    if(fn[VZ] > fn[axis])
        axis = VZ;

    if(fabsf(fn[VX] - 1.0f) < FLT_EPSILON ||
       fabsf(fn[VY] - 1.0f) < FLT_EPSILON ||
       fabsf(fn[VZ] - 1.0f) < FLT_EPSILON)
    {
        // We must build the right vector manually.
        if(axis == VX && normal[VX] > 0.f)
        {
            V3_Set(right, 0.f, 1.f, 0.f);
        }
        else if(axis == VX)
        {
            V3_Set(right, 0.f, -1.f, 0.f);
        }

        if(axis == VY && normal[VY] > 0.f)
        {
            V3_Set(right, -1.f, 0.f, 0.f);
        }
        else if(axis == VY)
        {
            V3_Set(right, 1.f, 0.f, 0.f);
        }

        if(axis == VZ)
        {
            V3_Set(right, 1.f, 0.f, 0.f);
        }
    }
    else
    {   // Can use a cross product of the surface normal.
        V3_CrossProduct(right, (pvec3_t) rotm[axis], normal);
        V3_Normalize(right);
    }

    V3_CrossProduct(up, right, normal);
    V3_Normalize(up);
}

/**
 * Generate texcoords on the surface centered on point.
 *
 * @param s  Texture s coords written back here.
 * @param t  Texture t coords written back here.
 * @param point  Point on surface around which texture is centered.
 * @param scale  Scale multiplier for texture.
 * @param v1  Top left vertex of the surface being projected on.
 * @param v2  Bottom right vertex of the surface being projected on.
 * @param normal  Normal of the surface being projected on.
 *
 * @return  @c true, if the generated coords are within bounds.
 */
static boolean genTexCoords(pvec2_t s, pvec2_t t, const pvec3_t point, float scale,
    const pvec3_t v1, const pvec3_t v2, const pvec3_t normal)
{
    vec3_t vToPoint, right, up;

    buildUpRight(up, right, normal);
    up[VZ] *= 1.08f; // Counteract aspect correction slightly (not too round mind).
    V3_Subtract(vToPoint, v1, point);
    s[0] = V3_DotProduct(vToPoint, right) * scale + .5f;
    t[0] = V3_DotProduct(vToPoint, up)    * scale + .5f;

    // Is the origin point visible?
    if(s[0] >= 1 || t[0] >= 1)
        return false; // Right on the X axis or below on the Y axis.

    V3_Subtract(vToPoint, v2, point);
    s[1] = V3_DotProduct(vToPoint, right) * scale + .5f;
    t[1] = V3_DotProduct(vToPoint, up)    * scale + .5f;

    // Is the end point visible?
    if(s[1] <= 0 || t[1] <= 0)
        return false; // Left on the X axis or above on the Y axis.

    return true;
}

static float calcOmniLightAttenuationFactor(const lumobj_t* lum, float distance)
{
    assert(lum && lum->type == LT_OMNI);
    if(distance == 0 || distance > lum->maxDistance)
        return 0;
    if(distance > .67f * lum->maxDistance)
        return (lum->maxDistance - distance) / (.33f * lum->maxDistance);
    return 1;
}

/**
 * Project a omni light onto the surface. If valid and the surface is
 * contacted a new projection node will constructed and returned.
 *
 * @param lum  Lumobj representing the light being projected.
 * @param spParams  Surface projection paramaters.
 *
 * @return  Resultant projection node else @c NULL.
 */
static listnode_t* projectOmniLightOnSurface(const lumobj_t* lum,
    surfaceprojectparams_t* spParams)
{
    assert(lum && spParams);
    {
    vec3_t lumCenter, vToLum;
    listnode_t* node = NULL;
    uint lumIdx;
    DGLuint tex;

    // No lightmap texture?
    if(spParams->flags & DLF_TEX_CEILING)
        tex = LUM_OMNI(lum)->ceilTex;
    else if(spParams->flags & DLF_TEX_FLOOR)
        tex = LUM_OMNI(lum)->floorTex;
    else
        tex = LUM_OMNI(lum)->tex;
    if(!tex) return NULL;

    // Has this already been occluded?
    lumIdx = LO_ToIndex(lum);
    if(LO_IsHidden(lumIdx, viewPlayer - ddPlayers)) return NULL;

    V3_Set(lumCenter, lum->pos[VX], lum->pos[VY], lum->pos[VZ] + LUM_OMNI(lum)->zOff);
    V3_Subtract(vToLum, spParams->v1, lumCenter);

    // On the right side?
    if(V3_DotProduct(vToLum, spParams->normal) < 0.f)
    {
        float dist, luma;
        vec3_t point;

        // Calculate 3D distance between surface and lumobj.
        V3_ClosestPointOnPlane(point, spParams->normal, spParams->v1, lumCenter);
        dist = V3_Distance(point, lumCenter);
        if(dist > 0 && dist <= LUM_OMNI(lum)->radius)
        {
            luma = 1.5f - 1.5f * dist / LUM_OMNI(lum)->radius;

            // If distance limit is set this light will fade out.
            if(lum->maxDistance > 0)
            {
                float distanceFromViewer = LO_DistanceToViewer(lumIdx, viewPlayer - ddPlayers);
                luma *= calcOmniLightAttenuationFactor(lum, distanceFromViewer);
            }

            if(luma >= OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN)
            {
                float scale = 1.0f / ((2.f * LUM_OMNI(lum)->radius) - dist);
                vec2_t s, t;

                if(genTexCoords(s, t, point, scale, spParams->v1, spParams->v2, spParams->normal))
                {
                    float color[3];
                    calcDynlightColor(color, LUM_OMNI(lum)->color, luma);
                    node = newProjection(tex, s, t, color);
                }
            }
        }
    }

    return node;
    }
}

typedef struct {
    uint listIdx;
    surfaceprojectparams_t spParams;
} surfacelumobjiterparams_t;

boolean RIT_SurfaceLumobjContacts(void* ptr, void* data)
{
    lumobj_t* lum = (lumobj_t*)ptr;
    surfacelumobjiterparams_t* p = (surfacelumobjiterparams_t*)data;
    listnode_t* node = NULL;

    switch(lum->type)
    {
    case LT_OMNI:
        node = projectOmniLightOnSurface(lum, &p->spParams);
        break;

    case LT_PLANE:
        if(!(p->spParams.flags & DLF_NO_PLANE))
        {
            node = projectPlaneLightOnSurface(lum, &p->spParams);
        }
        break;

    default:
        Con_Error("RIT_SurfaceLumobjContacts: Invalid value, lum->type = %i.", (int) lum->type);
        exit(1); // Unreachable.
    }

    if(node)
    {
        // Do we need to allocate a list for this surface?
        if(!p->listIdx)
        {
            int flags = 0 | ((p->spParams.flags & DLF_SORT_LUMINOUSE_DESC)? PLF_SORT_LUMINOUS_DESC : 0);
            p->listIdx = newList(flags) + 1; // 1-based index.
        }

        linkProjectionToList(node, &projectionLists[p->listIdx-1]);
    }

    return true; // Continue iteration.
}

static uint processSubsector(subsector_t* ssec, surfacelumobjiterparams_t* params)
{
    R_IterateSubsectorContacts(ssec, OT_LUMOBJ, RIT_SurfaceLumobjContacts, params);
    // Did we produce a projection list?
    return params->listIdx;
}

uint R_ProjectOnSurface(subsector_t* ssec, const vectorcomp_t topLeft[3],
    const vectorcomp_t bottomRight[3], const vectorcomp_t normal[3], int flags)
{
    surfacelumobjiterparams_t p;

    p.listIdx = 0;
    p.spParams.flags = flags;
    V3_Copy(p.spParams.v1, topLeft);
    V3_Copy(p.spParams.v2, bottomRight);
    V3_Copy(p.spParams.normal, normal);

    return processSubsector(ssec, &p);
}

int R_IterateSurfaceProjections2(uint listIdx,
    int (*callback) (const textureprojection_t*, void*), void* paramaters)
{
    int result = 0; // Continue iteration.
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

int R_IterateSurfaceProjections(uint listIdx,
    int (*callback) (const textureprojection_t*, void*))
{
    return R_IterateSurfaceProjections2(listIdx, callback, NULL);
}
