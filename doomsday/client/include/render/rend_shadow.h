/** @file rend_shadow.h
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef DENG_CLIENT_RENDER_DYNSHADOW_H
#define DENG_CLIENT_RENDER_DYNSHADOW_H

#include <de/Vector>
#include "render/rendersystem.h"
#include "WallEdge"

class ConvexSubspace;

/**
 * Parameters for Rend_DrawProjectedShadows (POD).
 * @ingroup render
 */
typedef struct {
    ConvexSubspace *subspace;
    uint lastIdx;
    WorldVBuf::Index vertCount;
    union {
        de::Vector3f const *posCoords;
        WorldVBuf::Index const *indices;
    };
    de::Vector3d const *topLeft;
    de::Vector3d const *bottomRight;

    // Wall section edges:
    // Both are provided or none at all. If present then this is a wall geometry.
    de::WallEdgeSection const *leftSection;
    de::WallEdgeSection const *rightSection;
} rendershadowprojectionparams_t;

/**
 * Render all shadows in projection list @a listIdx according to @a parm
 * writing them to the renderering lists for the current frame.
 */
void Rend_DrawProjectedShadows(uint listIdx, rendershadowprojectionparams_t &parm);

#endif // DENG_CLIENT_RENDER_DYNSHADOW_H
