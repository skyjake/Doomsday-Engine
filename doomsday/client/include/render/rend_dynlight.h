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

#ifndef LIBDENG_RENDER_DYNLIGHT_H
#define LIBDENG_RENDER_DYNLIGHT_H

/// Paramaters for Rend_RenderLightProjections (POD).
typedef struct {
    uint lastIdx;
    rvertex_t const *rvertices;
    uint numVertices, realNumVertices;
    coord_t const *texTL, *texBR;
    boolean isWall;
    struct {
        struct {
            walldivnode_t *firstDiv;
            uint divCount;
        } left;
        struct {
            walldivnode_t *firstDiv;
            uint divCount;
        } right;
    } wall;
} renderlightprojectionparams_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Render all dynlights in projection list @a listIdx according to @a paramaters
 * writing them to the renderering lists for the current frame.
 *
 * @note If multi-texturing is being used for the first light; it is skipped.
 *
 * @return  Number of lights rendered.
 */
uint Rend_RenderLightProjections(uint listIdx, renderlightprojectionparams_t *paramaters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_DYNLIGHT_H */
