/**\file rend_shadow.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBDENG_RENDER_MOBJ_SHADOW_H
#define LIBDENG_RENDER_MOBJ_SHADOW_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This value defines the offset from the shadowed surface applied to
 * shadows rendered with the simple drop-to-highest-floor method.
 * As such shadows are drawn using additional primitives on top of the map
 * surface they touch; a small visual offset is used to avoid z-fighting.
 */
#define SHADOW_ZOFFSET              (.8f)

/// @return  @c true if rendering of mobj shadows is currently enabled.
boolean Rend_MobjShadowsEnabled(void);

/**
 * Use the simple, drop-to-highest-floor algorithm for rendering mobj shadows
 * selected for this method. Rendering lists are not used.
 */
void Rend_RenderMobjShadows(void);

/// Parameters for Rend_RenderShadowProjections (POD).
typedef struct {
    uint lastIdx;
    const rvertex_t* rvertices;
    uint numVertices, realNumVertices;
    const coord_t* texTL, *texBR;
    boolean isWall;
    struct {
        struct {
            walldivnode_t* firstDiv;
            uint divCount;
        } left;
        struct {
            walldivnode_t* firstDiv;
            uint divCount;
        } right;
    } wall;
} rendershadowprojectionparams_t;

/**
 * Render all shadows in projection list @a listIdx according to @a parameters
 * writing them to the renderering lists for the current frame.
 */
void Rend_RenderShadowProjections(uint listIdx, rendershadowprojectionparams_t* paramaters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_MOBJ_SHADOW_H */
