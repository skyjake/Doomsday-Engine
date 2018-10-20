/** @file vectorlightlist.cpp Vector light list.
 *
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "render/vectorlightlist.h"

#include <de/legacy/memoryzone.h>

VectorLightList::Node *VectorLightList::firstNode  = nullptr;
VectorLightList::Node *VectorLightList::cursorNode = nullptr;

void VectorLightList::init()  // static
{
    static bool firstTime = true;
    if(firstTime)
    {
        firstNode  = 0;
        cursorNode = 0;
        firstTime  = false;
    }
}

void VectorLightList::rewind()  // static
{
    // Start reusing nodes from the first one in the list.
    cursorNode = firstNode;
}

VectorLightList &VectorLightList::add(VectorLightData &vlight)
{
    Node *node = newNode();
    node->vlight = vlight;

    if(head)
    {
        Node *iter = head;
        Node *last = iter;
        do
        {
            VectorLightData *vlight = &node->vlight;

            // Is this closer than the one being added?
            if(node->vlight.approxDist > vlight->approxDist)
            {
                last = iter;
                iter = iter->next;
            }
            else
            {
                // Insert it here.
                node->next = last->next;
                last->next = node;
                return *this;
            }
        } while(iter);
    }

    node->next = head;
    head = node;

    return *this;
}

VectorLightList::Node *VectorLightList::newNode()  // static
{
    Node *node;

    // Do we need to allocate more nodes?
    if(!cursorNode)
    {
        node = (Node *) Z_Malloc(sizeof(*node), PU_APPSTATIC, nullptr);

        // Link the new node to the list.
        node->nextUsed = firstNode;
        firstNode = node;
    }
    else
    {
        node = cursorNode;
        cursorNode = cursorNode->nextUsed;
    }

    node->next = nullptr;
    return node;
}
