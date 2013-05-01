/** @file rend_dynlight.h
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

#ifndef DENG_RENDER_DYNLIGHT_H
#define DENG_RENDER_DYNLIGHT_H

#include <de/Vector>

#include "render/rendpoly.h"
#include "render/walldiv.h"

/// Paramaters for Rend_RenderLightProjections (POD).
typedef struct {
    uint lastIdx;
    rvertex_t const *rvertices;
    uint numVertices, realNumVertices;
    de::Vector3d const *texTL;
    de::Vector3d const *texBR;
    bool isWall;
    struct {
        struct {
            de::WallDivs::Intercept *firstDiv;
            uint divCount;
        } left;
        struct {
            de::WallDivs::Intercept *firstDiv;
            uint divCount;
        } right;
    } wall;
} renderlightprojectionparams_t;

/**
 * Render all dynlights in projection list @a listIdx according to @a paramaters
 * writing them to the renderering lists for the current frame.
 *
 * @note If multi-texturing is being used for the first light; it is skipped.
 *
 * @return  Number of lights rendered.
 */
uint Rend_RenderLightProjections(uint listIdx, renderlightprojectionparams_t &parameters);

#endif // DENG_RENDER_DYNLIGHT_H
