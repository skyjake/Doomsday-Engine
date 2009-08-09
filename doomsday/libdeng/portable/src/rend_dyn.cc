/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * rend_dyn.c: Projected lumobjs (dynlight) lists.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define LUM_FACTOR(dist, radius) (1.5f - 1.5f*(dist)/(radius))

// TYPES -------------------------------------------------------------------

typedef struct dynnode_s {
    struct dynnode_s* next, *nextUsed;
    dynlight_t      dyn;
} dynnode_t;

typedef struct dynlist_s {
    boolean         sortBrightestFirst;
    dynnode_t*      head;
} dynlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int useDynLights = true, dlBlend = 0;
float dlFactor = .7f;
float dlFogBright = .15f;

int useWallGlow = true;
float glowHeightFactor = 3; // Glow height as a multiplier.
int glowHeightMax = 100; // 100 is the default (0-1024).

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Dynlight nodes.
static dynnode_t* dynFirst, *dynCursor;

// Surface light link lists.
static uint numDynlightLinkLists = 0, dynlightLinkListCursor = 0;
static dynlist_t* dynlightLinkLists;

// CODE --------------------------------------------------------------------

void DL_Register(void)
{
    // Cvars
    C_VAR_INT("rend-glow", &glowingTextures, 0, 0, 1);
    C_VAR_INT("rend-glow-wall", &useWallGlow, 0, 0, 1);
    C_VAR_INT("rend-glow-height", &glowHeightMax, 0, 0, 1024);
    C_VAR_FLOAT("rend-glow-scale", &glowHeightFactor, 0, 0.1f, 10);

    C_VAR_INT2("rend-light", &useDynLights, 0, 0, 1, LO_UnlinkMobjLumobjs);
    C_VAR_INT("rend-light-blend", &dlBlend, 0, 0, 2);
    C_VAR_FLOAT("rend-light-bright", &dlFactor, 0, 0, 1);
    C_VAR_FLOAT("rend-light-fog-bright", &dlFogBright, 0, 0, 1);
    C_VAR_INT("rend-light-multitex", &useMultiTexLights, 0, 0, 1);

    C_VAR_INT("rend-mobj-light-auto", &useMobjAutoLights, 0, 0, 1);
    LO_Register();
    Rend_DecorRegister();
}

/**
 * Initialize the dynlight system in preparation for rendering view(s) of the
 * game world. Called by R_InitLevel().
 */
void DL_InitForMap(void)
{
    dynlightLinkLists = NULL;
    numDynlightLinkLists = 0, dynlightLinkListCursor = 0;
}

/**
 * Moves all used dynlight nodes to the list of unused nodes, so they
 * can be reused.
 */
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
 * @param sortBrightestFirst Nodes in the list will be auto-sorted when
 *                      added by the brightness of the associated dynlight
 *                      in descending order.
 * @return              Identifier for the new list.
 */
static uint newDynlightList(boolean sortBrightestFirst)
{
    dynlist_t*          list;

    // Ran out of light link lists?
    if(++dynlightLinkListCursor >= numDynlightLinkLists)
    {
        uint                newNum = numDynlightLinkLists * 2;

        if(!newNum)
            newNum = 2;

        dynlightLinkLists = (dynlist_t*)
            Z_Realloc(dynlightLinkLists, newNum * sizeof(dynlist_t), PU_MAP);
        numDynlightLinkLists = newNum;
    }

    list = &dynlightLinkLists[dynlightLinkListCursor-1];
    list->head = NULL;
    list->sortBrightestFirst = sortBrightestFirst;

    return dynlightLinkListCursor - 1;
}

static dynnode_t* newDynNode(void)
{
    dynnode_t*          node;

    // Have we run out of nodes?
    if(dynCursor == NULL)
    {
        node = (dynnode_t*) Z_Malloc(sizeof(dynnode_t), PU_STATIC, NULL);

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

/**
 * Returns a new dynlight node. If the list of unused nodes is empty,
 * a new node is created.
 */
static dynnode_t* newDynLight(float* s, float* t)
{
    dynnode_t*          node = newDynNode();
    dynlight_t*         dyn = &node->dyn;

    if(s)
    {
        dyn->s[0] = s[0];
        dyn->s[1] = s[1];
    }
    if(t)
    {
        dyn->t[0] = t[0];
        dyn->t[1] = t[1];
    }

    return node;
}

static void linkDynNodeToList(dynnode_t* node, uint listIndex)
{
    dynlist_t*          list = &dynlightLinkLists[listIndex];

    if(list->sortBrightestFirst && list->head)
    {
        float               light = (node->dyn.color[0] +
            node->dyn.color[1] + node->dyn.color[2]) / 3;
        dynnode_t*          last, *iter;

        last = iter = list->head;
        do
        {
            // Is this brighter than the one being added?
            if((iter->dyn.color[0] + iter->dyn.color[1] +
                iter->dyn.color[2]) / 3 > light)
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
 * @param outRGB        The location the calculated result will be written to.
 * @param lum           Ptr to the lumobj from which the color will be used.
 * @param light         The light value to be used in the calculation.
 */
static void calcDynLightColor(float* outRGB, const float* inRGB, float light)
{
    uint                i;

    if(light < 0)
        light = 0;
    else if(light > 1)
        light = 1;
    light *= dlFactor;

    // In fog, additive blending is used. The normal fog color
    // is way too bright.
    if(usingFog)
        light *= dlFogBright; // Would be too much.

    // Multiply with the light color.
    for(i = 0; i < 3; ++i)
    {
        outRGB[i] = light * inRGB[i];
    }
}

/**
 * Project the given planelight onto the specified seg. If it would be lit,
 * a new dynlight node will be created and returned.
 *
 * @param lum           Ptr to the lumobj lighting the seg.
 * @param bottom        Z height (bottom) of the section being lit.
 * @param top           Z height (top) of the section being lit.
 *
 * @return              Ptr to the projected light, ELSE @c NULL.
 */
static dynnode_t* projectPlaneGlowOnSegSection(const lumobj_t* lum,
                                               float bottom, float top)
{
    float               glowHeight;
    float               s[2], t[2];

    if(bottom >= top)
        return NULL; // No height.

    glowHeight =
        (MAX_GLOWHEIGHT * LUM_PLANE(lum)->intensity) * glowHeightFactor;

    // Don't make too small or too large glows.
    if(glowHeight <= 2)
        return NULL;

    if(glowHeight > glowHeightMax)
        glowHeight = glowHeightMax;

    // Calculate texture coords for the light.
    if(LUM_PLANE(lum)->normal[VZ] < 0)
    {   // Light is cast downwards.
        t[1] = t[0] = (lum->pos[VZ] - top) / glowHeight;
        t[1]+= (top - bottom) / glowHeight;
    }
    else
    {   // Light is cast upwards.
        t[0] = t[1] = (bottom - lum->pos[VZ]) / glowHeight;
        t[0]+= (top - bottom) / glowHeight;
    }

    if(!(t[0] <= 1 || t[1] >= 0))
        return NULL; // Is above/below on the Y axis.

    // The horizontal direction is easy.
    s[0] = 0;
    s[1] = 1;

    return newDynLight(s, t);
}

/**
 * Given a normalized normal, construct up and right vectors, oriented to
 * the original normal. Note all vectors and normals are in world-space.
 *
 * @param up            The up vector will be written back here.
 * @param right         The right vector will be written back here.
 * @param normal        Normal to construct vectors for.
 */
static void buildUpRight(pvec3_t up, pvec3_t right, const pvec3_t normal)
{
    const vec3_t rotm[3] = {
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 1.f}
    };
    int                 axis = VX;
    vec3_t              fn;

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
        V3_CrossProduct(right, (const pvec3_t) rotm[axis], normal);
        V3_Normalize(right);
    }

    V3_CrossProduct(up, right, normal);
    V3_Normalize(up);
}

/**
 * Generate texcoords on surface centered on point.
 *
 * @param point         Point on surface around which texture is centered.
 * @param scale         Scale multiplier for texture.
 * @param s             Texture s coords written back here.
 * @param t             Texture t coords written back here.
 * @param v1            Top left vertex of the surface being projected on.
 * @param v2            Bottom right vertex of the surface being projected on.
 * @param normal        Normal of the surface being projected on.
 *
 * @return              @c true, if the generated coords are within bounds.
 */
static boolean genTexCoords(const pvec3_t point, float scale,
                            pvec2_t s, pvec2_t t, const pvec3_t v1,
                            const pvec3_t v2, const pvec3_t normal)
{
    vec3_t              vToPoint, right, up;

    buildUpRight(up, right, normal);
    V3_Subtract(vToPoint, v1, point);
    s[0] = V3_DotProduct(vToPoint, right) * scale + .5f;
    t[0] = V3_DotProduct(vToPoint, up) * scale + .5f;

    V3_Subtract(vToPoint, v2, point);
    s[1] = V3_DotProduct(vToPoint, right) * scale + .5f;
    t[1] = V3_DotProduct(vToPoint, up) * scale + .5f;

    // Would the light be visible?
    if(!(s[0] <= 1 || s[1] >= 0))
        return false; // Is right/left on the X axis.

    if(!(t[0] <= 1 || t[1] >= 0))
        return false; // Is above/below on the Y axis.

    return true;
}

typedef struct surfacelumobjiterparams_s {
    vec3_t          v1, v2;
    vec3_t          normal;
    boolean         haveList;
    uint            listIdx;
    byte            flags; // DLF_* flags.
} surfacelumobjiterparams_t;

boolean DLIT_SurfaceLumobjContacts(void* ptr, void* data)
{
    lumobj_t*           lum = (lumobj_t*) ptr;
    dynnode_t*          node = NULL;
    surfacelumobjiterparams_t* params = (surfacelumobjiterparams_t*) data;
    DGLuint             tex = 0;
    float               lightBrightness = 1;
    float*              lightRGB;
    uint                lumIdx;

    if(!(lum->type == LT_OMNI || lum->type == LT_PLANE))
        return true; // Continue iteration.

    lumIdx = LO_ToIndex(lum);

    switch(lum->type)
    {
    case LT_OMNI:
        if(LO_IsHidden(lumIdx, viewPlayer - ddPlayers))
            return true;

        if(params->flags & DLF_TEX_CEILING)
            tex = LUM_OMNI(lum)->ceilTex;
        else if(params->flags & DLF_TEX_FLOOR)
            tex = LUM_OMNI(lum)->floorTex;
        else
            tex = LUM_OMNI(lum)->tex;

        lightRGB = LUM_OMNI(lum)->color;

        if(tex)
        {
            vec3_t              lumCenter, vToLum;

            V3_Set(lumCenter, lum->pos[VX], lum->pos[VY],
                   lum->pos[VZ] + LUM_OMNI(lum)->zOff);

            // On the right side?
            V3_Subtract(vToLum, params->v1, lumCenter);
            if(V3_DotProduct(vToLum, params->normal) < 0.f)
            {
                float               dist;
                vec3_t              point;

                // Calculate 3D distance between surface and lumobj.
                V3_ClosestPointOnPlane(point, params->normal, params->v1, lumCenter);
                dist = V3_Distance(point, lumCenter);
                if(dist > 0 && dist <= LUM_OMNI(lum)->radius)
                {
                    lightBrightness = LUM_FACTOR(dist, LUM_OMNI(lum)->radius);

                    // If a max distance limit is set, lumobjs fade out.
                    if(lum->maxDistance > 0)
                    {
                        float               distFromViewer =
                            LO_DistanceToViewer(lumIdx, viewPlayer - ddPlayers);

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
                        vec2_t              s, t;
                        float               scale =
                            1.0f / ((2.f * LUM_OMNI(lum)->radius) - dist);

                        if(genTexCoords(point, scale, s, t, params->v1,
                                        params->v2, params->normal))
                        {
                            node = newDynLight(s, t);
                        }
                    }
                }
            }
        }
        break;

    case LT_PLANE:
        if(!(params->flags & DLF_NO_PLANAR))
            tex = LUM_PLANE(lum)->tex;

        if(tex)
        {
            lightRGB = LUM_PLANE(lum)->color;

            node = projectPlaneGlowOnSegSection(lum, params->v2[VZ], params->v1[VZ]);
        }
        break;

    default:
        Con_Error("DLIT_SegLumobjContacts: Invalid value, lum->type = %i.",
                  (int) lum->type);
        break;
    }

    if(node)
    {
        dynlight_t*         dyn = &node->dyn;

        dyn->texture = tex;
        calcDynLightColor(dyn->color, lightRGB, lightBrightness);

        // Got a list for this surface yet?
        if(!params->haveList)
        {
            boolean             sortBrightestFirst =
                (params->flags & DLF_SORT_LUMADSC)? true : false;

            params->listIdx = newDynlightList(sortBrightestFirst);
            params->haveList = true;
        }

        linkDynNodeToList(node, params->listIdx);
    }

    return true; // Continue iteration.
}

static uint processSubSector(subsector_t* ssec, surfacelumobjiterparams_t* params)
{
    // Process each lumobj contacting the subsector.
    R_IterateSubsectorContacts(ssec, OT_LUMOBJ, DLIT_SurfaceLumobjContacts,
                               params);

    // Did we generate a light list?
    if(params->haveList)
        return params->listIdx + 1;

    return 0; // Nope.
}

/**
 * Project all lumobjs affecting the given quad (world space), calculate
 * coordinates (in texture space) then store into a new list of dynlights.
 *
 * \assume The coordinates of the given quad must be contained wholly within
 * the subsector specified. This is due to an optimization within the lumobj
 * management which seperates them according to their position in the BSP.
 *
 * @param ssec          Subsector within which the quad wholly resides.
 * @param topLeft       Coordinates of the top left corner of the quad.
 * @param bottomRight   Coordinates of the bottom right corner of the quad.
 * @param normal        Normalized normal of the quad.
 * @param flags         DLF_* flags.
 *
 * @return              Dynlight list name if the quad is lit by one or more
 *                      light sources, else @c 0.
 */
uint DL_ProjectOnSurface(subsector_t* ssec, const vectorcomp_t topLeft[3],
                         const vectorcomp_t bottomRight[3],
                         const vectorcomp_t normal[3], byte flags)
{
    surfacelumobjiterparams_t params;

    if(!useDynLights && !useWallGlow)
        return 0; // Disabled.

    if(!ssec)
        return 0;

    V3_Copy(params.v1, topLeft);
    V3_Copy(params.v2, bottomRight);
    V3_Set(params.normal, normal[VX], normal[VY], normal[VZ]);
    params.flags = flags;
    params.haveList = false;
    params.listIdx = 0;

    return processSubSector(ssec, &params);
}

/**
 * Calls func for all projected dynlights in the given list.
 *
 * @param listIdx       Identifier of the list to process.
 * @param data          Ptr to pass to the callback.
 * @param func          Callback to make for each object.
 *
 * @return              @c true, iff every callback returns @c true.
 */
boolean DL_ListIterator(uint listIdx, void* data,
                        boolean (*func) (const dynlight_t*, void*))
{
    dynnode_t*          node;
    boolean             retVal, isDone;

    if(listIdx == 0 || listIdx > numDynlightLinkLists)
        return true;
    listIdx--;

    node = dynlightLinkLists[listIdx].head;
    retVal = true;
    isDone = false;
    while(node && !isDone)
    {
        if(!func(&node->dyn, data))
        {
            retVal = false;
            isDone = true;
        }
        else
            node = node->next;
    }

    return retVal;
}
