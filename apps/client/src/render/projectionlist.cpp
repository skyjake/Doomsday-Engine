/** @file projectionlist.cpp  Projection list.
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

#include "render/projectionlist.h"

#include <de/legacy/memoryzone.h>

using namespace de;

ProjectionList::Node *ProjectionList::firstNode = nullptr;
ProjectionList::Node *ProjectionList::cursorNode = nullptr;

void ProjectionList::init()  // static
{
    static bool firstTime = true;
    if(firstTime)
    {
        firstNode  = 0;
        cursorNode = 0;
        firstTime  = false;
    }
}

void ProjectionList::rewind()  // static
{
    // Start reusing nodes.
    cursorNode = firstNode;
}

/// Averaged-color * alpha.
static dfloat luminosity(const ProjectedTextureData &pt)  // static
{
    return (pt.color.x + pt.color.y + pt.color.z) / 3 * pt.color.w;
}

ProjectionList &ProjectionList::add(ProjectedTextureData &texp)
{
    Node *node = newNode();
    node->projection = texp;

    if(head && sortByLuma)
    {
        dfloat luma = luminosity(node->projection);
        Node *iter = head;
        Node *last = iter;
        do
        {
            // Is this brighter than that being added?
            if(luminosity(iter->projection) > luma)
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

ProjectionList::Node *ProjectionList::newNode()  // static
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
