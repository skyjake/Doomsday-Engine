/**
 * @file rend_fakeradio.h Faked Radiosity Lighting
 *
 * Perhaps the most distinctive characteristic of radiosity lighting
 * is that the corners of a room are slightly dimmer than the rest of
 * the surfaces.  (It's not the only characteristic, however.)  We
 * will fake these shadowed areas by generating shadow polygons for
 * wall segments and determining, which BSP leaf vertices will be
 * shadowed.
 *
 * In other words, walls use shadow polygons (over entire hedges), while
 * planes use vertex lighting.  Since planes are usually tesselated
 * into a great deal of BSP leafs (and triangles), they are better
 * suited for vertex lighting.  In some cases we will be forced to
 * split a BSP leaf into smaller pieces than strictly necessary in
 * order to achieve better accuracy in the shadow effect.
 *
 * @author Copyright &copy; 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_RENDER_FAKERADIO_H
#define LIBDENG_RENDER_FAKERADIO_H

#include "map/linedef.h"
#include "render/rendpoly.h"

typedef struct shadowlink_s {
    struct shadowlink_s *next;
    LineDef *lineDef;
    byte side;
} shadowlink_t;

typedef struct {
    float shadowRGB[3], shadowDark;
    float shadowSize;
    shadowcorner_t const *botCn, *topCn, *sideCn;
    edgespan_t const *spans;
    coord_t const *segOffset;
    coord_t const *segLength;
    coord_t const *linedefLength;
    Sector const *frontSec, *backSec;
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
} rendsegradio_params_t;

#ifdef __cplusplus
extern "C" {
#endif

/// Register the console commands, variables, etc..., of this module.
void Rend_RadioRegister(void);

/**
 * To be called after map load to perform necessary initialization within this module.
 */
void Rend_RadioInitForMap(void);

/// @return  @c true if @a lineDef qualifies as a (edge) shadow caster.
boolean Rend_RadioIsShadowingLineDef(LineDef *lineDef);

/**
 * Updates all the shadow offsets for the given vertex.
 *
 * @pre Lineowner rings must be set up.
 *
 * @param vtx  Vertex to be updated.
 */
void Rend_RadioUpdateVertexShadowOffsets(Vertex *vtx);

float Rend_RadioCalcShadowDarkness(float lightLevel);

/**
 * Called to update the shadow properties used when doing FakeRadio for the
 * given linedef.
 */
void Rend_RadioUpdateLinedef(LineDef *line, boolean backSide);

/**
 * Render FakeRadio for the given hedge section.
 */
void Rend_RadioSegSection(rvertex_t const *rvertices, rendsegradio_params_t const *params);

/**
 * Render FakeRadio for the given BSP leaf.
 */
void Rend_RadioBspLeafEdges(BspLeaf *bspLeaf);

/**
 * Render the shadow poly vertices, for debug.
 */
void Rend_DrawShadowOffsetVerts(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_RENDER_FAKERADIO_H
