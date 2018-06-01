/** @file vectorlightlist.h  Vector light list.
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

#ifndef DE_CLIENT_RENDER_VECTORLIGHTLIST_H
#define DE_CLIENT_RENDER_VECTORLIGHTLIST_H

#include "render/vectorlightdata.h"

struct VectorLightList
{
    struct Node
    {
        Node *next, *nextUsed;
        VectorLightData vlight;
    };

    Node *head = nullptr;

    static void init();
    static void rewind();

    inline VectorLightList &operator << (VectorLightData &texp) { return add(texp); }

    VectorLightList &add(VectorLightData &texp);

private:
    static Node *firstNode;
    static Node *cursorNode;

    static Node *newNode();
};

#endif // DE_CLIENT_RENDER_VECTORLIGHTLIST_H

