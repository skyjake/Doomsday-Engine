/**\file rend_dyn.c
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

typedef struct dynnode_s {
    struct dynnode_s* next, *nextUsed;
    dynlight_t dyn;
} dynnode_t;

/**
 * @defgroup projectionListFlags  Projection List Flags
 * @{
 */
#define PLF_SORT_LUMINOUS_DESC  0x1 /// Sort by luminosity in descending order.
/**@}*/

typedef struct dynlist_s {
    int flags; /// @see projectionListFlags
    dynnode_t* head;
} dynlist_t;

typedef struct {
    int flags; /// @see dynlightProjectFlags
    vec3_t v1; /// Top left vertex of the surface being projected to.
    vec3_t v2; /// Bottom right vertex of the surface being projected to.
    vec3_t normal; /// Normalized normal of the surface being projected to.
} surfaceprojectparams_t;

// Dynlight nodes.
static dynnode_t* dynFirst, *dynCursor;

// Surface light link lists.
static uint numDynlightLinkLists = 0, dynlightLinkListCursor = 0;
static dynlist_t* dynlightLinkLists;

void DL_InitForMap(void)
{
    dynlightLinkLists = NULL;
    numDynlightLinkLists = 0, dynlightLinkListCursor = 0;
}

void DL_InitForNewFrame(void)
{
    // Start reusing nodes from the first one in the list.
    dynCursor = dynFirst;

    // Clear the surface light link lists.
    dynlightLinkListCursor = 0;
    if(numDynlightLinkLists)
        memset(dynlightLinkLists, 0, numDynlightLinkLists * sizeof(dynlist_t));
}

/**
 * Create a new dynlight list.
 *
 * @param flags  @see projectionListFlags
 * @return  Identifier for the new list.
 */
static uint newDynlightList(int flags)
{
    dynlist_t* list;

    // Ran out of light link lists?
    if(++dynlightLinkListCursor >= numDynlightLinkLists)
    {
        uint newNum = numDynlightLinkLists * 2;
        if(!newNum) newNum = 2;

        dynlightLinkLists = Z_Realloc(dynlightLinkLists, newNum * sizeof(dynlist_t), PU_MAP);
        numDynlightLinkLists = newNum;
    }

    list = &dynlightLinkLists[dynlightLinkListCursor-1];
    list->head = NULL;
    list->flags = flags;

    return dynlightLinkListCursor - 1;
}

static dynnode_t* newDynNode(void)
{
    dynnode_t* node;

    // Have we run out of nodes?
    if(dynCursor == NULL)
    {
        node = Z_Malloc(sizeof(dynnode_t), PU_APPSTATIC, NULL);

        // Link the new node to the list.
        node->nextUsed = dynFirst;
        dynFirst = node;
    }
    else
    {
        node = dynCursor;
        dynCursor = dynCursor->nextUsed;
    }

    node->next = NULL;
    return node;
}

static dynnode_t* newDynLight(DGLuint texture, const float s[2], const float t[2],
    const float color[3])
{
    assert(texture != 0 && s && t && color);
    {
    dynnode_t* node = newDynNode();
    dynlight_t* dyn = &node->dyn;

    dyn->texture = texture;
    dyn->s[0] = s[0];
    dyn->s[1] = s[1];
    dyn->t[0] = t[0];
    dyn->t[1] = t[1];
    dyn->color[CR] = color[CR];
    dyn->color[CG] = color[CG];
    dyn->color[CB] = color[CB];

    return node;
    }
}

static void linkDynNodeToList(dynnode_t* node, uint listIndex)
{
    dynlist_t* list = &dynlightLinkLists[listIndex];

    if((list->flags & PLF_SORT_LUMINOUS_DESC) && list->head)
    {
        float light = (node->dyn.color[0] + node->dyn.color[1] + node->dyn.color[2]) / 3;
        dynnode_t* last, *iter;

        last = iter = list->head;
        do
        {
            // Is this brighter than the one being added?
            if((iter->dyn.color[0] + iter->dyn.color[1] + iter->dyn.color[2]) / 3 > light)
            {
                last = iter;
                iter = iter->next;
            }
            else
            {   // Need to insert it here.
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
 * @param outRGB  The location the calculated result will be written to.
 * @param lum  Ptr to the lumobj from which the color will be used.
 * @param light  The light value to be used in the calculation.
 */
static void calcDynLightColor(float* outRGB, const float* inRGB, float light)
{
    uint i;

    light = MINMAX_OF(0, light, 1) * dynlightFactor;
    // In fog additive blending is used; the normal fog color is way too bright.
    if(usingFog)
        light *= dynlightFogBright;

    // Multiply with the light color.
    for(i = 0; i < 3; ++i)
    {
        outRGB[i] = light * inRGB[i];
    }
}

/**
 * Project a plane glow onto the surface. If valid and contacted a new projection
 * node will constructed and returned.
 *
 * @param lum  Lumobj representing the light being projected.
 * @param spParams  Surface projection paramaters.
 *
 * @return  Resultant projection node else @c NULL.
 */
static dynnode_t* projectPlaneLightOnSurface(const lumobj_t* lum, const surfaceprojectparams_t* spParams)
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

    calcDynLightColor(color, LUM_PLANE(lum)->color, LUM_PLANE(lum)->intensity);
    return newDynLight(LUM_PLANE(lum)->tex, s, t, color);
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
    {   // We must build the right vector manually.
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
 * Generate texcoords on surface centered on point.
 *
 * @param point  Point on surface around which texture is centered.
 * @param scale  Scale multiplier for texture.
 * @param s  Texture s coords written back here.
 * @param t  Texture t coords written back here.
 * @param v1  Top left vertex of the surface being projected on.
 * @param v2  Bottom right vertex of the surface being projected on.
 * @param normal  Normal of the surface being projected on.
 *
 * @return  @c true, if the generated coords are within bounds.
 */
static boolean genTexCoords(const pvec3_t point, float scale, pvec2_t s,
    pvec2_t t, const pvec3_t v1, const pvec3_t v2, const pvec3_t normal)
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

/**
 * Project an omni light onto the surface. If valid and contacted a new projection
 * node will constructed and returned.
 *
 * @param lum  Lumobj representing the light being projected.
 * @param spParams  Surface projection paramaters.
 *
 * @return  Resultant projection node else @c NULL.
 */
static dynnode_t* projectOmniLightOnSurface(const lumobj_t* lum, surfaceprojectparams_t* spParams)
{
    assert(lum && spParams);
    {
    vec3_t lumCenter, vToLum;
    dynnode_t* node = NULL;
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
        float dist, lightBrightness;
        vec3_t point;

        // Calculate 3D distance between surface and lumobj.
        V3_ClosestPointOnPlane(point, spParams->normal, spParams->v1, lumCenter);
        dist = V3_Distance(point, lumCenter);
        if(dist > 0 && dist <= LUM_OMNI(lum)->radius)
        {
            lightBrightness = 1.5f - 1.5f * dist / LUM_OMNI(lum)->radius;

            // If a max distance limit is set, lumobjs fade out.
            if(lum->maxDistance > 0)
            {
                float distFromViewer = LO_DistanceToViewer(lumIdx, viewPlayer - ddPlayers);

                if(distFromViewer > lum->maxDistance)
                    lightBrightness = 0;

                if(distFromViewer > .67f * lum->maxDistance)
                {
                    lightBrightness *= (lum->maxDistance - distFromViewer) /
                        (.33f * lum->maxDistance);
                }
            }

            if(lightBrightness >= .05f)
            {
                float scale = 1.0f / ((2.f * LUM_OMNI(lum)->radius) - dist);
                vec2_t s, t;

                if(genTexCoords(point, scale, s, t, spParams->v1, spParams->v2, spParams->normal))
                {
                    float color[3];
                    calcDynLightColor(color, LUM_OMNI(lum)->color, lightBrightness);
                    node = newDynLight(tex, s, t, color);
                }
            }
        }
    }

    return node;
    }
}

typedef struct {
    boolean haveList;
    uint listIdx;
    surfaceprojectparams_t spParams;
} surfacelumobjiterparams_t;

boolean DLIT_SurfaceLumobjContacts(void* ptr, void* data)
{
    lumobj_t* lum = (lumobj_t*)ptr;
    surfacelumobjiterparams_t* p = (surfacelumobjiterparams_t*)data;
    dynnode_t* node = NULL;

    switch(lum->type)
    {
    case LT_OMNI:
        node = projectOmniLightOnSurface(lum, &p->spParams);
        break;

    case LT_PLANE:
        if(!(p->spParams.flags & DLF_NO_PLANAR))
        {
            node = projectPlaneLightOnSurface(lum, &p->spParams);
        }
        break;

    default:
        Con_Error("DLIT_SurfaceLumobjContacts: Invalid value, lum->type = %i.", (int) lum->type);
        exit(1); // Unreachable.
    }

    if(node)
    {
        // Got a list for this surface yet?
        if(!p->haveList)
        {
            int flags = 0 | ((p->spParams.flags & DLF_SORT_LUMINOUSE_DESC)? PLF_SORT_LUMINOUS_DESC : 0);
            p->listIdx = newDynlightList(flags);
            p->haveList = true;
        }

        linkDynNodeToList(node, p->listIdx);
    }

    return true; // Continue iteration.
}

static uint processSubSector(subsector_t* ssec, surfacelumobjiterparams_t* params)
{
    // Process each node contacting the subsector.
    R_IterateSubsectorContacts(ssec, OT_LUMOBJ, DLIT_SurfaceLumobjContacts, params);
    // Did we generate a projection list?
    if(params->haveList)
        return params->listIdx + 1;
    return 0; // Nope.
}

uint DL_ProjectOnSurface(subsector_t* ssec, const vectorcomp_t topLeft[3],
    const vectorcomp_t bottomRight[3], const vectorcomp_t normal[3], int flags)
{
    surfacelumobjiterparams_t p;

    p.haveList = false;
    p.listIdx = 0;

    p.spParams.flags = flags;
    V3_Copy(p.spParams.v1, topLeft);
    V3_Copy(p.spParams.v2, bottomRight);
    V3_Set(p.spParams.normal, normal[VX], normal[VY], normal[VZ]);

    return processSubSector(ssec, &p);
}

boolean DL_ListIterator(uint listIdx, void* data, boolean (*func) (const dynlight_t*, void*))
{
    boolean result = true; // Continue iteration.
    if(listIdx != 0 && listIdx <= numDynlightLinkLists)
    {
        dynnode_t* node = dynlightLinkLists[listIdx-1].head;
        while(node)
        {
            result = func(&node->dyn, data);
            node = (result? node->next : NULL /* Early out */);
        }
    }
    return result;
}
