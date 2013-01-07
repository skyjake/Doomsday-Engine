/**\file p_intercept.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"

#define MININTERCEPTS       128

struct interceptnode_s {
    struct interceptnode_s* next;
    struct interceptnode_s* prev;
    intercept_t intercept;
};

// Blockset from which intercepts are allocated.
static zblockset_t* interceptNodeSet = NULL;
// Head of the used intercept list.
static InterceptNode* interceptFirst;

// Trace nodes.
static InterceptNode head;
static InterceptNode tail;
static InterceptNode* mru;

static __inline boolean isSentinel(const InterceptNode* node)
{
    assert(node);
    return node == &tail || node == &head;
}

static InterceptNode* newInterceptNode(void)
{
    // Can we reuse an existing intercept?
    if(!isSentinel(interceptFirst))
    {
        InterceptNode* node = interceptFirst;
        interceptFirst = node->next;
        return node;
    }
    return ZBlockSet_Allocate(interceptNodeSet);
}

void P_ClearIntercepts(void)
{
    if(!interceptNodeSet)
    {
        interceptNodeSet = ZBlockSet_New(sizeof(InterceptNode), MININTERCEPTS, PU_APPSTATIC);

        // Configure the static head and tail.
        head.intercept.distance = 0.0f;
        head.next = &tail;
        head.prev = NULL;
        tail.intercept.distance = 1.0f;
        tail.prev = &head;
        tail.next = NULL;
    }

    // Start reusing intercepts (may point to a sentinel but that is Ok).
    interceptFirst = head.next;

    // Reset the trace.
    head.next = &tail;
    tail.prev = &head;
    mru = NULL;
}

InterceptNode* P_AddIntercept(intercepttype_t type, float distance, void* object)
{
    InterceptNode* newNode, *before;
    intercept_t* in;

    if(!object)
        Con_Error("P_AddIntercept: Invalid arguments (object=NULL).");

    // First reject vs our sentinels
    if(distance < head.intercept.distance) return NULL;
    if(distance > tail.intercept.distance) return NULL;

    // Find the new intercept's ordered place along the trace.
    if(mru && mru->intercept.distance <= distance)
        before = mru->next;
    else
        before = head.next;
    while(before->next && distance >= before->intercept.distance) { before = before->next; }

    // Pull a new intercept from the used queue.
    newNode = newInterceptNode();

    // Configure the new intercept.
    in = &newNode->intercept;
    in->type = type;
    in->distance = distance;
    switch(in->type)
    {
    case ICPT_MOBJ:
        in->d.mobj = object;
        break;
    case ICPT_LINE:
        in->d.lineDef = object;
        break;
    default:
        Con_Error("P_AddIntercept: Invalid type %i.", (int)type);
        exit(1); // Unreachable.
    }

    // Link it in.
    newNode->next = before;
    newNode->prev = before->prev;

    newNode->prev->next = newNode;
    newNode->next->prev = newNode;

    mru = newNode;
    return newNode;
}

int P_TraverseIntercepts(traverser_t callback, void* paramaters)
{
    int result = false; // Continue iteration.
    const InterceptNode* node;
    for(node = head.next; !isSentinel(node); node = node->next)
    {
        result = callback(&node->intercept, paramaters);
        if(result) break; // Stop iteration.
    }
    return result;
}
