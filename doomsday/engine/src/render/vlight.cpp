/**
 * @file vlight.cpp Vector lights
 * @ingroup render
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_render.h"

#include "gl/gl_main.h"

#include <de/mathutil.h> // for M_ApproxDistance
#include <de/memoryzone.h>

struct vlightnode_t
{
    vlightnode_t *next, *nextUsed;
    vlight_t vlight;
};

struct vlightlist_t
{
    vlightnode_t *head;
    boolean sortByDist;
};

struct vlightiterparams_t
{
    vec3d_t origin;
    boolean haveList;
    uint listIdx;
};

// vlight nodes.
static vlightnode_t *vLightFirst, *vLightCursor;

// vlight link lists.
static uint numVLightLinkLists = 0, vLightLinkListCursor = 0;
static vlightlist_t *vLightLinkLists;

static float const worldLight[3] = {-.400891f, -.200445f, .601336f};

/**
 * Create a new vlight list.
 *
 * @param sortByDist  Lights in this list will be automatically sorted by
 *                    their approximate distance from the point being lit.
 *
 * @return  Identifier for the new list.
 */
static uint newVLightList(boolean sortByDist)
{
    vlightlist_t *list;

    // Ran out of vlight link lists?
    if(++vLightLinkListCursor >= numVLightLinkLists)
    {
        uint newNum = numVLightLinkLists * 2;
        if(!newNum) newNum = 2;

        vLightLinkLists = (vlightlist_t *) Z_Realloc(vLightLinkLists, newNum * sizeof(vlightlist_t), PU_MAP);
        numVLightLinkLists = newNum;
    }

    list = &vLightLinkLists[vLightLinkListCursor-1];
    list->head = NULL;
    list->sortByDist = sortByDist;

    return vLightLinkListCursor - 1;
}

static vlightnode_t *newVLightNode()
{
    vlightnode_t *node;

    // Have we run out of nodes?
    if(!vLightCursor)
    {
        node = (vlightnode_t *) Z_Malloc(sizeof(vlightnode_t), PU_APPSTATIC, 0);

        // Link the new node to the list.
        node->nextUsed = vLightFirst;
        vLightFirst = node;
    }
    else
    {
        node = vLightCursor;
        vLightCursor = vLightCursor->nextUsed;
    }

    node->next = 0;
    return node;
}

/**
 * @return  Ptr to a new vlight node. If the list of unused nodes is empty,
 *          a new node is created.
 */
static vlightnode_t *newVLight()
{
    vlightnode_t *node = newVLightNode();
    //vlight_t *vlight = &node->vlight;

    /// @todo move vlight setup here.
    return node;
}

static void linkVLightNodeToList(vlightnode_t *node, uint listIndex)
{
    vlightlist_t *list = &vLightLinkLists[listIndex];

    if(list->sortByDist && list->head)
    {
        vlightnode_t *last, *iter;
        vlight_t *vlight;

        last = iter = list->head;
        do
        {
            vlight = &node->vlight;

            // Is this closer than the one being added?
            if(node->vlight.approxDist > vlight->approxDist)
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

void VL_InitForMap()
{
    vLightLinkLists = 0;
    numVLightLinkLists = 0;
    vLightLinkListCursor = 0;
}

void VL_InitForNewFrame()
{
    // Start reusing nodes from the first one in the list.
    vLightCursor = vLightFirst;

    // Clear the mobj vlight link lists.
    vLightLinkListCursor = 0;
    if(numVLightLinkLists)
        memset(vLightLinkLists, 0, numVLightLinkLists * sizeof(vlightlist_t));
}

/*
static void scaleFloatRGB(float *out, float const *in, float mul)
{
    memset(out, 0, sizeof(float) * 3);
    R_ScaleAmbientRGB(out, in, mul);
}
*/

/**
 * Iterator for processing light sources around a vissprite.
 */
int RIT_VisSpriteLightIterator(lumobj_t const *lum, coord_t xyDist, void *paramaters)
{
    vlightiterparams_t *params = (vlightiterparams_t *)paramaters;

    if(!(lum->type == LT_OMNI || lum->type == LT_PLANE)) return 0; // Continue iteration.

    boolean addLight = false;
    coord_t dist;
    float intensity;

    // Is the light close enough to make the list?
    switch(lum->type)
    {
    case LT_OMNI: {
        coord_t zDist = params->origin[VZ] - lum->origin[VZ] + LUM_OMNI(lum)->zOff;

        dist = M_ApproxDistance(xyDist, zDist);

        if(dist < (coord_t) loMaxRadius)
        {
            // The intensity of the light.
            intensity = MINMAX_OF(0, (1 - dist / LUM_OMNI(lum)->radius) * 2, 1);
            if(intensity > .05f)
                addLight = true;
        }
        break;
      }
    case LT_PLANE:
        if(LUM_PLANE(lum)->intensity &&
           (LUM_PLANE(lum)->color[0] > 0 || LUM_PLANE(lum)->color[1] > 0 ||
            LUM_PLANE(lum)->color[2] > 0))
        {
            float glowHeight = (GLOW_HEIGHT_MAX * LUM_PLANE(lum)->intensity) * glowHeightFactor;

            // Don't make too small or too large glows.
            if(glowHeight > 2)
            {
                if(glowHeight > glowHeightMax)
                    glowHeight = glowHeightMax;

                coord_t delta[3];
                V3d_Subtract(delta, params->origin, lum->origin);
                if(!((dist = V3d_DotProductf(delta, LUM_PLANE(lum)->normal)) < 0))
                {
                    // Is on the front of the glow plane.
                    addLight = true;
                    intensity = 1 - dist / glowHeight;
                }
            }
        }
        break;

    default:
        Con_Error("RIT_VisSpriteLightIterator: Invalid value, lum->type = %i.", (int) lum->type);
        exit(1); // Unreachable.
    }

    // If the light is not close enough, skip it.
    if(addLight)
    {
        vlightnode_t *node = newVLight();
        if(node)
        {
            vlight_t *vlight = &node->vlight;

            switch(lum->type)
            {
            case LT_OMNI:
                vlight->affectedByAmbient = true;
                vlight->approxDist = dist;
                vlight->lightSide = 1;
                vlight->darkSide = 0;
                vlight->offset = 0;

                // Calculate the normalized direction vector, pointing out of
                // the light origin.
                vlight->vector[VX] = (lum->origin[VX] - params->origin[VX]) / dist;
                vlight->vector[VY] = (lum->origin[VY] - params->origin[VY]) / dist;
                vlight->vector[VZ] = (lum->origin[VZ] + LUM_OMNI(lum)->zOff - params->origin[VZ]) / dist;

                vlight->color[CR] = LUM_OMNI(lum)->color[CR] * intensity;
                vlight->color[CG] = LUM_OMNI(lum)->color[CG] * intensity;
                vlight->color[CB] = LUM_OMNI(lum)->color[CB] * intensity;
                break;

            case LT_PLANE:
                vlight->affectedByAmbient = true;
                vlight->approxDist = dist;
                vlight->lightSide = 1;
                vlight->darkSide = 0;
                vlight->offset = .3f;

                /**
                 * Calculate the normalized direction vector, pointing out of
                 * the vissprite.
                 *
                 * @todo Project the nearest point on the surface to determine
                 *        the real direction vector.
                 */
                vlight->vector[VX] =  LUM_PLANE(lum)->normal[VX];
                vlight->vector[VY] =  LUM_PLANE(lum)->normal[VY];
                vlight->vector[VZ] = -LUM_PLANE(lum)->normal[VZ];

                vlight->color[CR] = LUM_PLANE(lum)->color[CR] * intensity;
                vlight->color[CG] = LUM_PLANE(lum)->color[CG] * intensity;
                vlight->color[CB] = LUM_PLANE(lum)->color[CB] * intensity;
                break;

            default:
                Con_Error("visSpriteLightIterator: Invalid value, lum->type = %i.", int(lum->type));
                exit(1); // Unreachable.
            }

            linkVLightNodeToList(node, params->listIdx);
        }
    }

    return 0; // Continue iteration.
}

uint R_CollectAffectingLights(collectaffectinglights_params_t const *params)
{
    if(!params) return 0;

    // Determine the lighting properties that affect this vissprite.
    uint vLightListIdx = 0;

    vlightnode_t *node = newVLight();
    vlight_t *vlight = &node->vlight;

    // Should always be lit with world light.
    vlight->affectedByAmbient = false;
    vlight->approxDist = 0;

    for(uint i = 0; i < 3; ++i)
    {
        vlight->vector[i] = worldLight[i];
        vlight->color[i] = params->ambientColor[i];
    }

    if(params->starkLight)
    {
        vlight->lightSide = .35f;
        vlight->darkSide  = .5f;
        vlight->offset    = 0;
    }
    else
    {
        /**
         * World light can both light and shade. Normal objects get
         * more shade than light (to prevent them from becoming too
         * bright when compared to ambient light).
         */
        vlight->lightSide = .2f;
        vlight->darkSide  = .8f;
        vlight->offset    = .3f;
    }

    vLightListIdx = newVLightList(true /* sort by distance */);
    linkVLightNodeToList(node, vLightListIdx);

    // Add extra light by interpreting lumobjs into vlights.
    if(loInited && params->bspLeaf)
    {
        vlightiterparams_t vars;

        V3d_Copy(vars.origin, params->origin);
        vars.haveList = true;
        vars.listIdx = vLightListIdx;

        LO_LumobjsRadiusIterator2(params->bspLeaf, params->origin[VX], params->origin[VY],
                                  float(loMaxRadius), RIT_VisSpriteLightIterator, (void *)&vars);
    }

    return vLightListIdx + 1;
}

boolean VL_ListIterator(uint listIdx, void* data, boolean (*func) (vlight_t const *, void *))
{
    if(listIdx == 0 || listIdx > numVLightLinkLists) return true;
    listIdx--;

    vlightnode_t *node = vLightLinkLists[listIdx].head;
    boolean retVal = true;
    boolean isDone = false;
    while(node && !isDone)
    {
        if(!func(&node->vlight, data))
        {
            retVal = false;
            isDone = true;
        }
        else
        {
            node = node->next;
        }
    }

    return retVal;
}

boolean R_DrawVLightVector(vlight_t const *light, void *context)
{
    coord_t distFromViewer = fabs(*((coord_t *)context));
    if(distFromViewer < 1600-8)
    {
        float alpha = 1 - distFromViewer / 1600, scale = 100;

        LIBDENG_ASSERT_IN_MAIN_THREAD();
        LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

        glBegin(GL_LINES);
            glColor4f(light->color[CR], light->color[CG], light->color[CB], alpha);
            glVertex3f(scale * light->vector[VX], scale * light->vector[VZ], scale * light->vector[VY]);
            glColor4f(light->color[CR], light->color[CG], light->color[CB], 0);
            glVertex3f(0, 0, 0);
        glEnd();
    }
    return true; // Continue iteration.
}
