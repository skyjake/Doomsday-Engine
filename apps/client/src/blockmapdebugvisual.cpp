/** @file blockmapdebugvisual.cpp
 *
 * @authors Copyright (c) 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include "blockmapdebugvisual.h"
#include "gl/gl_main.h"

using namespace de;

void BlockmapDebugVisual::draw(const world::Blockmap &bmap) // static
{
    // TODO: Needs refactoring. libdoomsday has the private implementation, which
    // this relies on. world::Blockmap should expose the relevant info here?
    
#if 0
    const auto &d = bmap.d;

    const float UNIT_SIZE = 1;

    // We'll be changing the color, so query the current and restore later.
    GLfloat oldColor[4];
    DGL_CurrentColor(oldColor);

    /*
     * Draw the Quadtree.
     */
    DGL_Color4f(1.f, 1.f, 1.f, 1.f / d->nodes.front().size);
    for (const auto &node : d->nodes)
    {
        // Only leafs with user data.
        if(!node.isLeaf()) continue;
        if(!node.leafData) continue;

        const Vec2f topLeft     = node.cell * UNIT_SIZE;
        const Vec2f bottomRight = topLeft + Vec2f(UNIT_SIZE, UNIT_SIZE);

        DGL_Begin(DGL_LINE_STRIP);
        DGL_Vertex2f(topLeft.x,     topLeft.y);
        DGL_Vertex2f(bottomRight.x, topLeft.y);
        DGL_Vertex2f(bottomRight.x, bottomRight.y);
        DGL_Vertex2f(topLeft.x,     bottomRight.y);
        DGL_Vertex2f(topLeft.x,     topLeft.y);
        DGL_End();
    }

    /*
     * Draw the bounds.
     */
    Vec2f start;
    Vec2f end = start + d->dimensions * UNIT_SIZE;

    DGL_Color3f(1, .5f, .5f);
    DGL_Begin(DGL_LINES);
    DGL_Vertex2f(start.x, start.y);
    DGL_Vertex2f(  end.x, start.y);

    DGL_Vertex2f(  end.x, start.y);
    DGL_Vertex2f(  end.x,   end.y);

    DGL_Vertex2f(  end.x,   end.y);
    DGL_Vertex2f(start.x,   end.y);

    DGL_Vertex2f(start.x,   end.y);
    DGL_Vertex2f(start.x, start.y);
    DGL_End();

    // Restore GL state.
    DGL_Color4fv(oldColor);
#endif
}
