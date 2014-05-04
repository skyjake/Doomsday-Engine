/** @file vlight.cpp  Vector lights
 * @ingroup render
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "render/vlight.h"

#include "de_graphics.h"
#include "de_render.h"

#include "world/map.h"
#include "BspLeaf"
#include "SectorCluster"
#include "Surface"

#include "Contact"
#include <de/concurrency.h>
#include <de/mathutil.h> // M_ApproxDistance
#include <de/memoryzone.h>

using namespace de;

struct ListNode
{
    ListNode *next, *nextUsed;
    VectorLight vlight;
};

struct List
{
    ListNode *head;
};

// VectorLight List nodes.
static ListNode *firstNode, *cursorNode;

// VectorLight lists.
static uint listCount, cursorList;
static List *lists;

/**
 * Create a new vlight list.
 * @return  Identifier for the new list.
 */
static uint newList()
{
    // Do we need to allocate more lists?
    if(++cursorList >= listCount)
    {
        listCount *= 2;
        if(!listCount) listCount = 2;

        lists = (List *) Z_Realloc(lists, listCount * sizeof(*lists), PU_MAP);
    }

    List *list = &lists[cursorList - 1];
    list->head = 0;

    return cursorList - 1;
}

static ListNode *newListNode()
{
    ListNode *node;

    // Have we run out of nodes?
    if(!cursorNode)
    {
        node = (ListNode *) Z_Malloc(sizeof(ListNode), PU_APPSTATIC, 0);

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

/**
 * @return  Ptr to a new vlight node. If the list of unused nodes is empty,
 *          a new node is created.
 */
static ListNode *newVLight()
{
    ListNode *node = newListNode();
    //vlight_t *vlight = &node->vlight;

    /// @todo move vlight setup here.
    return node;
}

static void linkNodeInList(ListNode *node, uint listIdx)
{
    List *list = &lists[listIdx];

    if(list->head)
    {
        ListNode *iter = list->head;
        ListNode *last = iter;
        do
        {
            VectorLight *vlight = &node->vlight;

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

void VL_InitForMap(Map &map)
{
    DENG_UNUSED(map);
    lists = 0;
    listCount = 0;
    cursorList = 0;
}

void VL_InitForNewFrame()
{
    // Start reusing nodes from the first one in the list.
    cursorNode = firstNode;

    // Clear the mobj vlight link lists.
    cursorList = 0;
    if(listCount)
        std::memset(lists, 0, listCount * sizeof(List));
}

/**
 * World light can both light and shade. Normal objects get more shade than light
 * (preventing them from ending up too bright compared to the ambient light).
 */
static void lightWithWorldLight(Vector3f const &color, bool starkLight, uint listIdx)
{
    static Vector3f const worldLight(-.400891f, -.200445f, .601336f);

    ListNode *node      = newVLight();
    VectorLight *vlight = &node->vlight;

    vlight->direction  = worldLight;
    vlight->color      = color;
    vlight->affectedByAmbient = false;
    vlight->approxDist = 0;

    if(starkLight)
    {
        vlight->lightSide = .35f;
        vlight->darkSide  = .5f;
        vlight->offset    = 0;
    }
    else
    {
        vlight->lightSide = .2f;
        vlight->darkSide  = .8f;
        vlight->offset    = .3f;
    }

    linkNodeInList(node, listIdx);
}

/**
 * Interpret a vlight from the lumobj and add it to the identified list.
 *
 * @param origin   Point in the map being evaluated.
 * @param lum      Luminous object to interpret.
 * @param listIdx  Identifier of the vlight list to update.
 */
static void lightWithLumobj(Vector3d const &origin, Lumobj *lum, uint listIdx)
{
    DENG_ASSERT(lum != 0);

    Vector3d lumCenter = lum->origin();
    lumCenter.z += lum->zOffset();

    // Is this light close enough?
    double dist = M_ApproxDistance(M_ApproxDistance(lumCenter.x - origin.x,
                                                    lumCenter.y - origin.y),
                                   origin.z - lumCenter.z);
    float intensity = 0;
    if(dist < Lumobj::radiusMax())
    {
        intensity = de::clamp(0.f, float(1 - dist / lum->radius()) * 2, 1.f);
    }

    if(intensity < .05f)
        return;

    ListNode *node      = newVLight();
    VectorLight *vlight = &node->vlight;

    vlight->direction         = (lumCenter - origin) / dist;
    vlight->color             = lum->color() * intensity;
    vlight->affectedByAmbient = true;
    vlight->approxDist        = dist;
    vlight->lightSide         = 1;
    vlight->darkSide          = 0;
    vlight->offset            = 0;

    linkNodeInList(node, listIdx);
}

struct lightwithlumobjs_params_t
{
    Vector3d origin;
    uint listIdx;
};

static int lightWithLumobjsWorker(Lumobj &lum, void *context)
{
    lightwithlumobjs_params_t *p = static_cast<lightwithlumobjs_params_t *>(context);
    lightWithLumobj(p->origin, &lum, p->listIdx);
    return false; // Continue iteration.
}

/**
 * Interpret vlights from lumobjs near the @a origin which contact the specified
 * @a subspace and add them to the identified list.
 */
static void lightWithLumobjs(Vector3d const &origin, ConvexSubspace &subspace, uint listIdx)
{
    lightwithlumobjs_params_t parms; zap(parms);
    parms.origin  = origin;
    parms.listIdx = listIdx;
    R_SubspaceLumobjContactIterator(subspace, lightWithLumobjsWorker, &parms);
}

/**
 * Interpret vlights from glowing planes at the @a origin in the specfified
 * @a subspace and add them to the identified list.
 */
static void lightWithPlaneGlows(Vector3d const &origin, ConvexSubspace &subspace, uint listIdx)
{
    SectorCluster &cluster = subspace.cluster();
    for(int i = 0; i < cluster.sector().planeCount(); ++i)
    {
        Plane &plane     = cluster.visPlane(i);
        Surface &surface = plane.surface();

        // Glowing at this moment?
        Vector3f glowColor;
        float intensity = surface.glow(glowColor);
        if(intensity < .05f)
            continue;

        coord_t glowHeight = Rend_PlaneGlowHeight(intensity);
        if(glowHeight < 2)
            continue; // Not too small!

        // In front of the plane?
        Vector3d pointOnPlane = Vector3d(cluster.center(), plane.heightSmoothed());
        double dist = (origin - pointOnPlane).dot(surface.normal());
        if(dist < 0)
            continue;

        intensity *= 1 - dist / glowHeight;
        if(intensity < .05f)
            continue;

        Vector3f color = Rend_LuminousColor(glowColor, intensity);

        if(color != Vector3f(0, 0, 0))
        {
            ListNode *node = newVLight();
            VectorLight *vlight = &node->vlight;

            vlight->direction  = Vector3f(surface.normal().x, surface.normal().y, -surface.normal().z);
            vlight->color      = color;
            vlight->affectedByAmbient = true;
            vlight->approxDist = dist;
            vlight->lightSide  = 1;
            vlight->darkSide   = 0;
            vlight->offset     = .3f;

            linkNodeInList(node, listIdx);
        }
    }
}

uint R_CollectAffectingLights(collectaffectinglights_params_t const *p)
{
    if(!p) return 0;

    uint listIdx = newList();

    // Always apply an ambient world light.
    lightWithWorldLight(p->ambientColor, p->starkLight, listIdx);

    // Add extra light by interpreting nearby sources.
    if(p->subspace)
    {
        lightWithLumobjs(p->origin, *p->subspace, listIdx);

        lightWithPlaneGlows(p->origin, *p->subspace, listIdx);
    }

    return listIdx + 1;
}

int VL_ListIterator(uint listIdx, int (*callback) (VectorLight const *, void *),
                    void *context)
{
    if(callback && listIdx != 0 && listIdx <= listCount)
    {
        ListNode *node = lists[listIdx - 1].head;
        while(node)
        {
            if(int result = callback(&node->vlight, context))
                return result;
            node = node->next;
        }
    }
    return false; // Continue iteration.
}

void Rend_DrawVectorLight(VectorLight const *vlight, float alpha)
{
    if(!vlight) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(alpha < .0001f) return;

    float const unitLength = 100;

    glBegin(GL_LINES);
        glColor4f(vlight->color.x, vlight->color.y, vlight->color.z, alpha);
        glVertex3f(unitLength * vlight->direction.x, unitLength * vlight->direction.z, unitLength * vlight->direction.y);
        glColor4f(vlight->color.x, vlight->color.y, vlight->color.z, 0);
        glVertex3f(0, 0, 0);
    glEnd();
}
