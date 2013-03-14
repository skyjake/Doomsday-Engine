/** @file rend_fakeradio.h Faked Radiosity Lighting.
 *
 * Perhaps the most distinctive characteristic of radiosity lighting is that
 * the corners of a room are slightly dimmer than the rest of the surfaces.
 * (It's not the only characteristic, however.)  We will fake these shadowed
 * areas by generating shadow polygons for wall segments and determining which
 * BSP leaf vertices will be shadowed.
 *
 * In other words, walls use shadow polygons (over entire lines), while planes
 * use vertex lighting. As sectors are usually partitioned into a great many
 * BSP leafs (and tesselated into triangles), they are better suited for vertex
 * lighting. In some cases we will be forced to split a BSP leaf into smaller
 * pieces than strictly necessary in order to achieve better accuracy in the
 * shadow effect.
 *
 * @author Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "map/sidedef.h"
#include "map/vertex.h"
#include "map/sector.h"
#include "render/rendpoly.h"
#include "render/walldiv.h"

/**
 * Used to link a line to a BSP leaf for the purposes of shadowing.
 */
struct ShadowLink
{
    ShadowLink *next;
    LineDef *line;
    byte side;

    LineDef::Side &lineSide()
    {
        DENG_ASSERT(line);
        return line->side(side);
    }

    LineDef::Side const &lineSide() const
    {
        DENG_ASSERT(line);
        return line->side(side);
    }
};

/**
 * Register the console commands, variables, etc..., of this module.
 */
void Rend_RadioRegister();

/**
 * To be called after map load to perform necessary initialization within this module.
 */
void Rend_RadioInitForMap();

/**
 * Returns @c true iff @a line qualifies for (edge) shadow casting.
 */
boolean Rend_RadioIsShadowingLine(LineDef const &line);

/**
 * Updates all the shadow offsets for the given vertex.
 *
 * @pre Lineowner rings must be set up.
 *
 * @param vtx  Vertex to be updated.
 */
void Rend_RadioUpdateVertexShadowOffsets(Vertex &vtx);

/**
 * Returns the global shadow darkness factor, derived from values in Config.
 */
float Rend_RadioCalcShadowDarkness(float lightLevel);

/**
 * Called to update the shadow properties used when doing FakeRadio for @a line.
 */
void Rend_RadioUpdateLine(LineDef &line, int backSide);

/**
 * Arguments for Rend_RadioWallSection()
 */
struct RendRadioWallSectionParms
{
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
};

/**
 * Render FakeRadio for the specified wall section. Generates and then draws all
 * shadow geometry for the wall section.
 *
 * Note that unlike @a Rend_RadioBspLeafEdges() there is no guard to ensure shadow
 * geometry is rendered only once per frame.
 */
void Rend_RadioWallSection(rvertex_t const *rvertices, RendRadioWallSectionParms const &parms);

/**
 * Render FakeRadio for the given BSP leaf. Draws all shadow geometry linked to the
 * BspLeaf, that has not already been rendered.
 */
void Rend_RadioBspLeafEdges(BspLeaf &bspLeaf);

/**
 * Render the shadow poly vertices, for debug.
 */
#ifdef DENG_DEBUG
void Rend_DrawShadowOffsetVerts();
#endif

#endif // LIBDENG_RENDER_FAKERADIO_H
