/**
 * @file kdtree.c
 * Kd-Tree data structure implementation. @ingroup data
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#include <assert.h>
#include <stdlib.h>

#include "de_platform.h"
#include "de_console.h"
#include "kdtree.h"

struct kdtree_s {
    /// Parent of this (sub)tree else @c NULL.
    struct kdtree_s* parent;

    /// Subtree of this (sub)tree else @c NULL.
    struct kdtree_s* subs[2];

    /// Coordinates for this subtree, from lower-left to upper-right corner.
    /// Pseudo-inclusive, i.e (x,y) is inside block if and only if:
    ///     minX <= x < maxX and minY <= y < maxY.
    AABox aaBox;

    /// User data associated with this (sub)tree else @c NULL.
    void* userData;
};

KdTree* KdTree_NewWithUserData(const AABox* bounds, void* userData)
{
    KdTree* kd = calloc(1, sizeof *kd);
    if(!kd) Con_Error("KdTree_New: Failed on allocation of %lu bytes for new KdTree.", sizeof *kd);
    memcpy(&kd->aaBox, bounds, sizeof(kd->aaBox));
    kd->userData = userData;
    return kd;
}

KdTree* KdTree_New(const AABox* bounds)
{
    return KdTree_NewWithUserData(bounds, NULL/*no user data*/);
}

static int deleteKdTree(KdTree* kd, void* parameters)
{
    free(kd);
    return false; // Continue iteration.
}

void KdTree_Delete(KdTree* kd)
{
    KdTree_PostTraverse(kd, deleteKdTree);
}

const AABox* KdTree_Bounds(KdTree* kd)
{
    assert(kd);
    return &kd->aaBox;
}

void* KdTree_UserData(KdTree* kd)
{
    assert(kd);
    return kd->userData;
}

KdTree* KdTree_SetUserData(KdTree* kd, void* userData)
{
    assert(kd);
    kd->userData = userData;
    return kd;
}

KdTree* KdTree_Child(KdTree* kd, int left)
{
    assert(kd);
    return kd->subs[left?1:0];
}

KdTree* KdTree_AddChild(KdTree* kd, const AABox* bounds, int left, void* userData)
{
    KdTree* child;
    assert(kd);

    child = kd->subs[left?1:0];
    if(!child)
    {
        child = kd->subs[left?1:0] = KdTree_New(bounds);
        child->parent = kd;
    }

    child->userData = userData;
    return child;
}

int KdTree_Traverse2(KdTree* kd, int (*callback)(KdTree*, void*),
    void* parameters)
{
    int num, result;
    assert(kd);

    if(!callback) return false; // Continue iteration.

    result = callback(kd, parameters);
    if(result) return result;

    // Recursively handle subtrees.
    for(num = 0; num < 2; ++num)
    {
        KdTree* child = kd->subs[num];
        if(!child) continue;

        result = KdTree_Traverse2(child, callback, parameters);
        if(result) return result;
    }

    return false; // Continue iteration.
}

int KdTree_Traverse(KdTree* kd, int (*callback)(KdTree*, void*),
    void* parameters)
{
    return KdTree_Traverse2(kd, callback, NULL/*no parameters*/);
}

int KdTree_PostTraverse2(KdTree* kd, int(*callback)(KdTree*, void*),
    void* parameters)
{
    int num, result;
    assert(kd);

    if(!callback) return false; // Continue iteration.

    // Recursively handle sub-blocks.
    for(num = 0; num < 2; ++num)
    {
        KdTree* child = kd->subs[num];
        if(!child) continue;

        result = KdTree_PostTraverse2(child, callback, parameters);
        if(result) return result;
    }

    result = callback(kd, parameters);
    if(result) return result;

    return false; // Continue iteration.
}

int KdTree_PostTraverse(KdTree* kd, int(*callback)(KdTree*, void*))
{
    return KdTree_PostTraverse2(kd, callback, NULL/*no parameters*/);
}
