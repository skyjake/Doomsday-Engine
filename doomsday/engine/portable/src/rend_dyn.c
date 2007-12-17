/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define LUM_FACTOR(dist, radius) (1.5f - 1.5f*(dist)/(radius))

// TYPES -------------------------------------------------------------------

typedef struct dynnode_s {
	struct dynnode_s *next, *nextUsed;
    dynlight_t      dyn;
} dynnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     useDynLights = true, dlBlend = 0;
float   dlFactor = 0.7f;        // was 0.6f
int     useWallGlow = true;
float   glowHeightFactor = 3; // glow height as a multiplier
int     glowHeightMax = 100; // 100 is the default (0-1024)
float   glowFogBright = .15f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Dynlight nodes.
static dynnode_t *dynFirst, *dynCursor;

// Surface light link lists.
static uint numDynlightLinkLists = 0, dynlightLinkListCursor = 0;
static dynnode_t **dynlightLinkLists;

// CODE --------------------------------------------------------------------

void DL_Register(void)
{
    // Cvars
    C_VAR_INT("rend-glow", &glowingTextures, 0, 0, 1);
    C_VAR_INT("rend-glow-wall", &useWallGlow, 0, 0, 1);
    C_VAR_INT("rend-glow-height", &glowHeightMax, 0, 0, 1024);
    C_VAR_FLOAT("rend-glow-scale", &glowHeightFactor, 0, 0.1f, 10);
    C_VAR_FLOAT("rend-glow-fog-bright", &glowFogBright, 0, 0, 1);

    C_VAR_INT("rend-light", &useDynLights, 0, 0, 1);
    C_VAR_INT("rend-light-blend", &dlBlend, 0, 0, 2);

    C_VAR_FLOAT("rend-light-bright", &dlFactor, 0, 0, 1);
    C_VAR_INT("rend-light-multitex", &useMultiTexLights, 0, 0, 1);
    C_VAR_INT("rend-mobj-light-auto", &useMobjAutoLights, 0, 0, 1);
    LO_Register();
    Rend_DecorRegister();
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
        memset(dynlightLinkLists, 0, numDynlightLinkLists * sizeof(dynnode_t*));
}

static dynnode_t *newDynNode(void)
{
	dynnode_t	*node;

    // Have we run out of nodes?
    if(dynCursor == NULL)
    {
        node = Z_Malloc(sizeof(dynnode_t), PU_STATIC, NULL);

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
static dynnode_t *newDynLight(float *s, float *t)
{
	dynnode_t	*node = newDynNode();
    dynlight_t	*dyn = &node->dyn;

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

/**
 * Link the given dynlight node to list.
 */
static void __inline linkDynNode(dynnode_t *node, dynnode_t **list, uint index)
{
    node->next = list[index];
    list[index] = node;
}

static void linkToSurfaceLightList(dynnode_t *node, uint listIndex,
                                   boolean sortBrightestFirst)
{
    dynnode_t     **list = &dynlightLinkLists[listIndex];

    if(sortBrightestFirst && *list)
    {
        float           light =
            (node->dyn.color[0] + node->dyn.color[1] + node->dyn.color[2]) / 3;
        dynnode_t      *last, *iter;

        last = iter = *list;
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

    node->next = *list;
    *list = node;
}

/**
 * Blend the given light value with the lumobj's color, apply any global
 * modifiers and output the result.
 *
 * @param outRGB    The location the calculated result will be written to.
 * @param lum       Ptr to the lumobj from which the color will be used.
 * @param light     The light value to be used in the calculation.
 */
static void calcDynLightColor(float *outRGB, const lumobj_t *lum, float light)
{
    uint        i;

    if(light < 0)
        light = 0;
    else if(light > 1)
        light = 1;
    light *= dlFactor;

    // If fog is enabled, make the light dimmer.
    // \fixme This should be a cvar.
    if(usingFog)
        light *= .5f;           // Would be too much.

    // Multiply with the light color.
    for(i = 0; i < 3; ++i)
    {
        outRGB[i] = light * lum->color[i];
    }
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
 * Project the given planelight onto the specified seg. If it would be lit,
 * a new dynlight node will be created and returned.
 *
 * @param lum       Ptr to the lumobj lighting the seg.
 * @param bottom    Z height (bottom) of the section being lit.
 * @param top       Z height (top) of the section being lit.
 *
 * @return          Ptr to the projected light, ELSE @c NULL.
 */
static dynnode_t *projectPlaneGlowOnSegSection(const lumobj_t *lum, float bottom,
                                               float top)
{
    uint        i;
    float       glowHeight;
    dynlight_t *dyn;
    dynnode_t  *node;
    float       s[2], t[2];

    if(bottom >= top)
        return NULL; // No height.

    if(!LUM_PLANE(lum)->tex)
        return NULL;

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

        if(t[0] > 1 || t[1] < 0)
            return NULL;
    }
    else
    {   // Light is cast upwards.
        t[0] = t[1] = (bottom - lum->pos[VZ]) / glowHeight;
        t[0]+= (top - bottom) / glowHeight;

        if(t[1] > 1 || t[0] < 0)
            return NULL;
    }

    // The horizontal direction is easy.
    s[0] = 0;
    s[1] = 1;

    node = newDynLight(s, t);
    dyn = &node->dyn;
    dyn->texture = LUM_PLANE(lum)->tex;

    for(i = 0; i < 3; ++i)
    {
        dyn->color[i] = lum->color[i] * dlFactor;

        // In fog, additive blending is used. The normal fog color
        // is way too bright.
        if(usingFog)
            dyn->color[i] *= glowFogBright;
    }

    return node;
}

/**
 * Project the given omnilight onto the specified seg. If it would be lit, a new
 * dynlight node will be created and returned.
 *
 * @param lum       Ptr to the lumobj lighting the seg.
 * @param seg       Ptr to the seg being lit.
 * @param bottom    Z height (bottom) of the section being lit.
 * @param top       Z height (top) of the section being lit.
 *
 * @return          Ptr to the projected light, ELSE @c NULL.
 */
static dynnode_t *projectOmniLightOnSegSection(const lumobj_t *lum, seg_t *seg,
                                               float bottom, float top)
{
    float       s[2], t[2];
    float       dist, pntLight[2];
    dynlight_t *dyn;
    dynnode_t  *node;
    float       lumRGB[3], lightVal;
    float       radius, radiusX2;

    if(!LUM_OMNI(lum)->tex)
        return NULL;

    radius = LUM_OMNI(lum)->radius / DYN_ASPECT;
    radiusX2 = 2 * radius;

    if(bottom >= top || radiusX2 == 0)
        return NULL;

    // Clip to the valid z height range first.
    t[0] = (lum->pos[VZ] + LUM_OMNI(lum)->zOff + radius - top) / radiusX2;
    t[1] = t[0] + (top - bottom) / radiusX2;

    if(!(t[0] < 1 && t[1] > 0))
        return NULL;

    pntLight[VX] = lum->pos[VX];
    pntLight[VY] = lum->pos[VY];

    // Calculate 2D distance between seg and light source.
    dist = ((seg->SG_v1pos[VY] - pntLight[VY]) * (seg->SG_v2pos[VX] - seg->SG_v1pos[VX]) -
            (seg->SG_v1pos[VX] - pntLight[VX]) * (seg->SG_v2pos[VY] - seg->SG_v1pos[VY]))
            / seg->length;

    // Is it close enough and on the right side?
    if(dist < 0 || dist > LUM_OMNI(lum)->radius)
        return NULL; // Nope.

    lightVal = LUM_FACTOR(dist, LUM_OMNI(lum)->radius);

    if(lightVal < .05f)
        return NULL; // Too dim to see.

    // Do a scalar projection for the offset.
    s[0] = (-((seg->SG_v1pos[VY] - pntLight[VY]) * (seg->SG_v1pos[VY] - seg->SG_v2pos[VY]) -
              (seg->SG_v1pos[VX] - pntLight[VX]) * (seg->SG_v2pos[VX] - seg->SG_v1pos[VX]))
              / seg->length +
               LUM_OMNI(lum)->radius) / (2 * LUM_OMNI(lum)->radius);

    s[1] = s[0] + seg->length / (2 * LUM_OMNI(lum)->radius);

    // Would the light be visible?
    if(s[0] >= 1 || s[1] <= 0)
        return NULL;  // Is left/right of the seg on the X/Y plane.

    calcDynLightColor(lumRGB, lum, lightVal);

    node = newDynLight(s, t);
    dyn = &node->dyn;
    memcpy(dyn->color, lumRGB, sizeof(float) * 3);
    dyn->texture = LUM_OMNI(lum)->tex;

    return node;
}

/**
 * Process the given lumobj to maybe add a dynamic light for the plane.
 *
 * @param lum       Ptr to the lumobj on which the dynlight will be based.
 */
static dynnode_t *projectOmniLightOnSubSectorPlane(const lumobj_t *lum,
                    uint planeType, float height)
{
    DGLuint     lightTex;
    float       diff, lightStrength, srcRadius;
    float       s[2], t[2];
    float       pos[3];

    pos[VX] = lum->pos[VX];
    pos[VY] = lum->pos[VY];
    pos[VZ] = lum->pos[VZ];

    // Center the Z.
    pos[VZ] += LUM_OMNI(lum)->zOff;
    srcRadius = LUM_OMNI(lum)->radius / 4;
    if(srcRadius == 0)
        srcRadius = 1;

    // Determine on which side of the plane the light is.
    lightTex = 0;
    lightStrength = 0;

    if(planeType == PLN_FLOOR)
    {
        if((lightTex = LUM_OMNI(lum)->floorTex) != 0)
        {
            if(pos[VZ] > height)
                lightStrength = 1;
            else if(pos[VZ] > height - srcRadius)
                lightStrength = 1 - (height - pos[VZ]) / srcRadius;
        }
    }
    else
    {
        if((lightTex = LUM_OMNI(lum)->ceilTex) != 0)
        {
            if(pos[VZ] < height)
                lightStrength = 1;
            else if(pos[VZ] < height + srcRadius)
                lightStrength = 1 - (pos[VZ] - height) / srcRadius;
        }
    }

    // Is there light in this direction? Is it strong enough?
    if(!lightTex || !lightStrength)
        return NULL;

    // Check that the height difference is tolerable.
    if(planeType == PLN_CEILING)
        diff = height - pos[VZ];
    else
        diff = pos[VZ] - height;

    // Clamp it.
    if(diff < 0)
        diff = 0;

    if(diff < LUM_OMNI(lum)->radius)
    {
        float       lightVal = LUM_FACTOR(diff, LUM_OMNI(lum)->radius);

        if(lightVal >= .05f)
        {
            dynlight_t *dyn;
            dynnode_t  *node;

            // Calculate dynlight position. It may still be outside
            // the bounding box the subsector.
            s[0] = -pos[VX] + LUM_OMNI(lum)->radius;
            t[0] = pos[VY] + LUM_OMNI(lum)->radius;
            s[1] = t[1] = 1.0f / (2 * LUM_OMNI(lum)->radius);

            // A dynamic light will be generated.
            node = newDynLight(s, t);
            dyn = &node->dyn;
            dyn->texture = lightTex;

            calcDynLightColor(dyn->color, lum, lightVal * lightStrength);
            return node;
        }
    }

    return NULL;
}

static uint newDynlightList(void)
{
    // Ran out of light link lists?
    if(++dynlightLinkListCursor >= numDynlightLinkLists)
    {
        uint        i;
        uint        newNum = numDynlightLinkLists * 2;
        dynnode_t **newList;

        if(!newNum)
            newNum = 2;

        newList = Z_Calloc(newNum * sizeof(dynnode_t*), PU_LEVEL, 0);

        // Copy existing links.
        for(i = 0; i < dynlightLinkListCursor - 1; ++i)
        {
            newList[i] = dynlightLinkLists[i];
        }

        if(dynlightLinkLists)
            Z_Free(dynlightLinkLists);
        dynlightLinkLists = newList;
        numDynlightLinkLists = newNum;
    }

    return dynlightLinkListCursor - 1;
}

typedef struct seglumobjiterparams_s {
    seg_t          *seg;
    float           bottom, top;
    boolean         sortBrightestFirst;
    boolean         haveList;
    uint            listIdx;
} seglumobjiterparams_t;

boolean DLIT_SegLumobjContacts(lumobj_t *lum, void *data)
{
    dynnode_t      *node = NULL;
    seglumobjiterparams_t *params = data;

    switch(lum->type)
    {
    case LT_OMNI:
        node = projectOmniLightOnSegSection(lum, params->seg,
                                            params->bottom, params->top);
        break;

    case LT_PLANE:
        node = projectPlaneGlowOnSegSection(lum, params->bottom, params->top);
        break;
    }

    if(node)
    {   // Got a list for this surface yet?
        if(!params->haveList)
        {
            params->listIdx = newDynlightList();
            params->haveList = true;
        }

        linkToSurfaceLightList(node, params->listIdx, params->sortBrightestFirst);
    }

    return true; // Continue iteration.
}

/**
 * Process dynamic lights for the specified seg.
 */
uint DL_ProcessSegSection(seg_t *seg, float bottom, float top,
                          boolean sortBrightestFirst)
{
    seglumobjiterparams_t params;

    if(!useDynLights)
        return 0; // Disabled.

    if(!seg || !seg->subsector)
        return 0;

    params.bottom = bottom;
    params.top = top;
    params.seg = seg;
    params.sortBrightestFirst = sortBrightestFirst;
    params.haveList = false;
    params.listIdx = 0;

    // Process each lumobj contacting the subsector.
    LO_IterateSubsectorContacts(seg->subsector, DLIT_SegLumobjContacts,
                                (void*) &params);

    // Did we generate a light list?
    if(params.haveList)
        return params.listIdx + 1;

    return 0; // Nope.
}

typedef struct planelumobjiterparams_s {
    uint            plane;
    float           height;
    boolean         sortBrightestFirst;
    boolean         haveList;
    uint            listIdx;
} planelumobjiterparams_t;

boolean DLIT_PlaneLumobjContacts(lumobj_t *lum, void *data)
{
    planelumobjiterparams_t *params = data;
    dynnode_t *node = NULL;

    switch(lum->type)
    {
    case LT_OMNI:
        node = projectOmniLightOnSubSectorPlane(lum, params->plane,
                                                params->height);
        break;

    case LT_PLANE: // Planar lights don't affect planes currently.
        node = NULL;
        break;
    }

    if(node)
    {
        // Got a list for this surface yet?
        if(!params->haveList)
        {
            params->listIdx = newDynlightList();
            params->haveList = true;
        }

        // Link to this plane's list.
        linkToSurfaceLightList(node, params->listIdx, params->sortBrightestFirst);
    }

    return true; // Continue iteration.
}

uint DL_ProcessSubSectorPlane(subsector_t *ssec, uint plane)
{
    sector_t       *linkSec;
    float           height;
    boolean         isLit;

    if(!useDynLights)
        return 0; // Disabled.

    if(!ssec)
        return 0;

    // Sanity check.
    assert(plane < ssec->sector->planecount);

    linkSec = R_GetLinkedSector(ssec, plane);
    if(R_IsSkySurface(&linkSec->SP_planesurface(plane)))
        return 0;

    // View height might prevent us from seeing the light.
    isLit = true;
    height = linkSec->SP_planevisheight(plane);
    if(ssec->SP_plane(plane)->type == PLN_FLOOR)
    {
        if(vy < height)
            isLit = false;
    }
    else
    {
        if(vy > height)
            isLit = false;
    }

    if(isLit)
    {
        planelumobjiterparams_t params;

        params.plane = plane;
        params.height = height;
        params.sortBrightestFirst = false;
        params.haveList = false;
        params.listIdx = 0;

        // Process each lumobj contacting the subsector.
        LO_IterateSubsectorContacts(ssec, DLIT_PlaneLumobjContacts,
                                    (void*) &params);

        // Did we generate a light list?
        if(params.haveList)
            return params.listIdx + 1;
    }

    return 0; // Nope.
}

/**
 * Calls func for all projected dynlights in the given list.
 *
 * @param listIdx   Identifier of the list to process.
 * @param data      Ptr to pass to the callback.
 * @param func      Callback to make for each object.
 *
 * @return          @c true, iff every callback returns @c true, else @c false.
 */
boolean DL_ListIterator(uint listIdx, void *data,
                        boolean (*func) (const dynlight_t *, void *data))
{
    dynnode_t  *node;
    boolean     retVal, isDone;

    if(listIdx == 0 || listIdx > numDynlightLinkLists)
        return true;
    listIdx--;

    node = dynlightLinkLists[listIdx];
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
