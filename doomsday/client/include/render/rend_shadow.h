/** @file rend_shadow.h
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_MOBJ_SHADOW_H
#define DENG_RENDER_MOBJ_SHADOW_H

#include <de/Vector>
#include "render/rendersystem.h"
#include "WallEdge"

/// Parameters for Rend_RenderShadowProjections (POD).
typedef struct {
    uint lastIdx;
    union {
        de::Vector3f const *rvertices;
        WorldVBuf::Index const *indices;
    };
    WorldVBuf::Index numVertices;
    de::Vector3d const *topLeft;
    de::Vector3d const *bottomRight;
    bool isWall;
    struct {
        de::WallEdgeSection const *leftEdge;
        de::WallEdgeSection const *rightEdge;
    } wall;
} rendershadowprojectionparams_t;

/**
 * Render all shadows in projection list @a listIdx according to @a parameters
 * writing them to the renderering lists for the current frame.
 */
void Rend_RenderShadowProjections(uint listIdx, rendershadowprojectionparams_t &parameters);

#endif // DENG_RENDER_MOBJ_SHADOW_H
