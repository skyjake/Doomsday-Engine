/** @file rend_list.h Rendering draw lists.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_DRAWLIST_RENDERER_H
#define DENG_CLIENT_RENDER_DRAWLIST_RENDERER_H

#include <de/Vector>

/**
 * Geometry backing store (arrays).
 */
struct Store
{
    /// Texture coordinate array indices.
    enum
    {
        TCA_MAIN, // Main texture.
        TCA_BLEND, // Blendtarget texture.
        TCA_LIGHT, // Dynlight texture.
        NUM_TEXCOORD_ARRAYS
    };

    de::Vector3f *posCoords;
    de::Vector2f *texCoords[NUM_TEXCOORD_ARRAYS];
    de::Vector4ub *colorCoords;

    Store();
    ~Store();

    void rewind();

    void clear();

    uint allocateVertices(uint count);

private:
    uint vertCount, vertMax;
};

Store &RL_Store();

void RL_RenderAllLists();

#endif // DENG_CLIENT_RENDER_DRAWLIST_RENDERER_H
