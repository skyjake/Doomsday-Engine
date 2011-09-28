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
 * @defgroup surfaceProjectionListFlags  Surface Projection List Flags
 * @{
 */
#define SPLF_SORT_LUMINOUS_DESC  0x1 /// Sort by luminosity in descending order.
/**@}*/

typedef struct {
    int flags; /// @see surfaceProjectionListFlags
    listnode_t* head;
} surfaceprojectionlist_t;

/// Orientation is toward the projectee.
typedef struct {
    int flags; /// @see surfaceProjectLightFlags
    float blendFactor; /// Multiplied with projection alpha.
    pvec3_t v1; /// Top left vertex of the surface being projected to.
    pvec3_t v2; /// Bottom right vertex of the surface being projected to.
    pvec3_t tangent; /// Normalized tangent of the surface being projected to.
    pvec3_t bitangent; /// Normalized bitangent of the surface being projected to.
    pvec3_t normal; /// Normalized normal of the surface being projected to.
} surfaceprojectparams_t;

// List nodes.
static listnode_t* firstNode, *cursorNode;

// Surface projection lists.
static uint projectionListCount, cursorList;
static surfaceprojectionlist_t* projectionLists;

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
 * @param flags  @see surfaceProjectionListFlags
 * @return  Unique identifier attributed to the new list.
 */
static uint newList(int flags)
{
    surfaceprojectionlist_t* list;

    // Do we need to allocate more lists?
    if(++cursorList >= projectionListCount)
    {
        projectionListCount *= 2;
        if(!projectionListCount) projectionListCount = 2;

        projectionLists = (surfaceprojectionlist_t*)Z_Realloc(projectionLists, projectionListCount * sizeof *projectionLists, PU_MAP);
        if(!projectionLists) Con_Error(__FILE__":newList failed on allocation of %lu bytes resizing the projection list.", (unsigned long) (projectionListCount * sizeof *projectionLists));
    }

    list = &projectionLists[cursorList-1];
    list->head = NULL;
    list->flags = flags;

    return cursorList;
}

/**
 * @param listIdx  Address holding the list index to retrieve.
 *      If the referenced list index is non-zero return the associated list.
 *      Otherwise allocate a new list and write it's index back to this address.
 * @param flags  @see ProjectionListFlags
 * @return  ProjectionList associated with the (possibly newly attributed) index.
 */
static surfaceprojectionlist_t* getList(uint* listIdx, int flags)
{
    // Do we need to allocate a list?
    if(!(*listIdx))
    {
        *listIdx = newList(flags);
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

static listnode_t* newProjection(DGLuint texture, const float s[2],
    const float t[2], const float color[3], float alpha)
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
    tp->color.rgba[CR] = color[CR];
    tp->color.rgba[CG] = color[CG];
    tp->color.rgba[CB] = color[CB];
    tp->color.rgba[CA] = MINMAX_OF(0, alpha, 1);

    return node;
    }
}

static __inline float calcProjectionLuminosity(textureprojection_t* tp)
{
    assert(tp);
    return RColor_AverageColorMulAlpha(&tp->color);
}

/// @return  Same as @a node for convenience (chaining).
static listnode_t* linkProjectionToList(listnode_t* node, surfaceprojectionlist_t* list)
{
    assert(node && list);
    if((list->flags & SPLF_SORT_LUMINOUS_DESC) && list->head)
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
 * @param listIdx  Address holding the list index to retrieve.
 *      If the referenced list index is non-zero return the associated list.
 *      Otherwise allocate a new list and write it's index back to this address.
 * @param flags  @see ProjectionListFlags
 *      Used when constructing a new projection list to configure it.
 * @param texture  GL identifier to texture attributed to the new projection.
 * @param s  GL texture coordinates on the S axis [left, right] in texture space.
 * @param t  GL texture coordinates on the T axis [bottom, top] in texture space.
 * @param colorRGB  RGB color attributed to the new projection.
 * @param alpha  Alpha attributed to the new projection.
 */
void R_NewTextureProjection(uint* listIdx, int flags, DGLuint texture,
    const float s[2], const float t[2], const float colorRGB[3], float alpha)
{
    linkProjectionToList(newProjection(texture, s, t, colorRGB, alpha), getList(listIdx, flags));
}

/**
 * Blend the given light value with the lumobj's color, apply any global
 * modifiers and output the result.
 *
 * @param outRGB  Calculated result will be written here.
 * @param color  Lumobj color.
 * @param light  Ambient light level of the surface being projected to.
 */
static void calcLightColor(float outRGB[3], const float color[3], float light)
{
    light = MINMAX_OF(0, light, 1) * dynlightFactor;
    // In fog additive blending is used; the normal fog color is way too bright.
    if(usingFog) light *= dynlightFogBright;

    // Multiply light with (ambient) color.
    { int i;
    for(i = 0; i < 3; ++i)
    {
        outRGB[i] = light * color[i];
    }}
}

typedef struct {
    uint listIdx;
    surfaceprojectparams_t spParams;
} projectlighttosurfaceiteratorparams_t;

/**
 * Project a plane glow onto the surface. If valid and the surface is
 * contacted a new projection node will constructed and returned.
 *
 * @param lum  Lumobj representing the light being projected.
 * @param paramaters  ProjectLightToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
static int projectPlaneLightToSurface(const lumobj_t* lum, void* paramaters)
{
    assert(lum && paramaters);
    {
    projectlighttosurfaceiteratorparams_t* p = (projectlighttosurfaceiteratorparams_t*)paramaters;
    surfaceprojectparams_t* spParams = &p->spParams;
    float bottom = spParams->v2[VZ], top = spParams->v1[VZ];
    float glowHeight, s[2], t[2], color[3];

    if(spParams->flags & PLF_NO_PLANE) return 0; // Continue iteration.
    
    // No lightmap texture?
    if(!LUM_PLANE(lum)->tex) return 0; // Continue iteration.

    // No height?
    if(bottom >= top) return 0; // Continue iteration.

    // Do not make too small glows.
    glowHeight = (GLOW_HEIGHT_MAX * LUM_PLANE(lum)->intensity) * glowHeightFactor;
    if(glowHeight <= 2) return 0; // Continue iteration.

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
    if(!(t[0] <= 1 || t[1] >= 0)) return 0; // Continue iteration.

    // The horizontal direction is easy.
    s[0] = 0;
    s[1] = 1;

    calcLightColor(color, LUM_PLANE(lum)->color, LUM_PLANE(lum)->intensity);

    R_NewTextureProjection(&p->listIdx, ((spParams->flags & PLF_SORT_LUMINOSITY_DESC)? SPLF_SORT_LUMINOUS_DESC : 0),
        LUM_PLANE(lum)->tex, s, t, color, 1 * spParams->blendFactor);

    return 0; // Continue iteration.
    }
}

static boolean genTexCoords(pvec2_t s, pvec2_t t, const_pvec3_t point, float scale,
    const_pvec3_t v1, const_pvec3_t v2, const_pvec3_t tangent, const_pvec3_t bitangent)
{
    // Counteract aspect correction slightly (not too round mind).
    return R_GenerateTexCoords(s, t, point, scale, scale * 1.08f, v1, v2, tangent, bitangent);
}

static DGLuint chooseOmniLightTexture(lumobj_t* lum, const surfaceprojectparams_t* spParams)
{
    assert(lum && lum->type == LT_OMNI && spParams);
    if(spParams->flags & PLF_TEX_CEILING)
        return LUM_OMNI(lum)->ceilTex;
    if(spParams->flags & PLF_TEX_FLOOR)
        return LUM_OMNI(lum)->floorTex;
    return LUM_OMNI(lum)->tex;
}

/**
 * Project a omni light onto the surface. If valid and the surface is
 * contacted a new projection node will constructed and returned.
 *
 * @param lum  Lumobj representing the light being projected.
 * @param paramaters  ProjectLightToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
static int projectOmniLightToSurface(lumobj_t* lum, void* paramaters)
{
    assert(lum && paramaters);
    {
    projectlighttosurfaceiteratorparams_t* p = (projectlighttosurfaceiteratorparams_t*)paramaters;
    surfaceprojectparams_t* spParams = &p->spParams;
    float dist, luma, scale, color[3];
    vec3_t lumCenter, vToLum;
    vec3_t point;
    uint lumIdx;
    DGLuint tex;
    vec2_t s, t;

    // Early test of the external blend factor for quick rejection.
    if(spParams->blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN) return 0; // Continue iteration.

    // No lightmap texture?
    tex = chooseOmniLightTexture(lum, spParams);
    if(!tex) return 0; // Continue iteration.

    // Has this already been occluded?
    lumIdx = LO_ToIndex(lum);
    if(LO_IsHidden(lumIdx, viewPlayer - ddPlayers)) return 0; // Continue iteration.

    V3_Set(lumCenter, lum->pos[VX], lum->pos[VY], lum->pos[VZ] + LUM_OMNI(lum)->zOff);
    V3_Subtract(vToLum, spParams->v1, lumCenter);

    // On the right side?
    if(V3_DotProduct(vToLum, spParams->normal) > 0.f) return 0; // Continue iteration.

    // Calculate 3D distance between surface and lumobj.
    V3_ClosestPointOnPlane(point, spParams->normal, spParams->v1, lumCenter);
    dist = V3_Distance(point, lumCenter);
    if(dist <= 0 || dist > LUM_OMNI(lum)->radius) return 0; // Continue iteration.

    // Calculate the final surface light attribution factor.
    luma = 1.5f - 1.5f * dist / LUM_OMNI(lum)->radius;

    // If distance limit is set this light will fade out.
    if(lum->maxDistance > 0)
    {
        float distance = LO_DistanceToViewer(lumIdx, viewPlayer - ddPlayers);
        luma *= LO_AttenuationFactor(lumIdx, distance);
    }

    // Would this light be seen?
    if(luma * spParams->blendFactor < OMNILIGHT_SURFACE_LUMINOSITY_ATTRIBUTION_MIN) return 0; // Continue iteration.

    // Project this light.
    scale = 1.0f / ((2.f * LUM_OMNI(lum)->radius) - dist);
    if(!genTexCoords(s, t, point, scale, spParams->v1, spParams->v2, spParams->tangent, spParams->bitangent)) return 0; // Continue iteration.

    // Attach to the projection list.
    calcLightColor(color, LUM_OMNI(lum)->color, luma);
    R_NewTextureProjection(&p->listIdx, ((spParams->flags & PLF_SORT_LUMINOSITY_DESC)? SPLF_SORT_LUMINOUS_DESC : 0),
        tex, s, t, color, 1 * spParams->blendFactor);

    return 0; // Continue iteration.
    }
}

/**
 * @param paramaters  ProjectLightToSurfaceIterator paramaters.
 *
 * @return  @c 0 = continue iteration.
 */
int RIT_ProjectLightToSurfaceIterator(void* obj, void* paramaters)
{
    lumobj_t* lum = (lumobj_t*)obj;
    assert(obj);
    switch(lum->type)
    {
    case LT_OMNI:  return  projectOmniLightToSurface(lum, paramaters);
    case LT_PLANE: return projectPlaneLightToSurface(lum, paramaters);
    default:
        Con_Error("RIT_ProjectLightToSurface: Invalid lumobj type %i.", (int) lum->type);
        exit(1); // Unreachable.
    }
}

uint R_ProjectLightsToSurface(int flags, subsector_t* ssec, float blendFactor,
    vec3_t topLeft, vec3_t bottomRight, vec3_t tangent, vec3_t bitangent, vec3_t normal)
{
    projectlighttosurfaceiteratorparams_t p;

    p.listIdx = 0;
    p.spParams.blendFactor = blendFactor;
    p.spParams.flags = flags;
    p.spParams.v1 = topLeft;
    p.spParams.v2 = bottomRight;
    p.spParams.tangent = tangent;
    p.spParams.bitangent = bitangent;
    p.spParams.normal = normal;

    R_IterateSubsectorContacts2(ssec, OT_LUMOBJ, RIT_ProjectLightToSurfaceIterator, (void*)&p);
    // Did we produce a projection list?
    return p.listIdx;
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
