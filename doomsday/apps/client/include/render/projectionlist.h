/** @file projectionlist.h  Projection list.
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

#ifndef DE_CLIENT_RENDER_PROJECTIONLIST_H
#define DE_CLIENT_RENDER_PROJECTIONLIST_H

#include "projectedtexturedata.h"

struct ProjectionList
{
    struct Node
    {
        Node *next, *nextUsed;
        ProjectedTextureData projection;
    };

    Node *head = nullptr;
    bool sortByLuma;       ///< @c true= Sort from brightest to darkest.

    static void init();
    static void rewind();

    inline ProjectionList &operator << (ProjectedTextureData &texp) { return add(texp); }

    ProjectionList &add(ProjectedTextureData &texp);

private:
    static Node *firstNode;
    static Node *cursorNode;

    static Node *newNode();
};

#endif // DE_CLIENT_RENDER_PROJECTIONLIST_H

