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
 * @authors Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_RENDER_FAKERADIO
#define DENG_RENDER_FAKERADIO

#include "HEdge"
#include "Line"
#include "Sector"
#include "Vertex"

#include "render/rendpoly.h" // r_vertex_t

class SectionEdge;

/**
 * Used to link a line to a BSP leaf for the purposes of FakeRadio shadowing.
 * @ingroup render
 */
struct ShadowLink
{
    ShadowLink *next;
    Line *line;
    byte side;

    Line::Side &lineSide()
    {
        DENG_ASSERT(line);
        return line->side(side);
    }

    Line::Side const &lineSide() const
    {
        DENG_ASSERT(line);
        return line->side(side);
    }
};

/**
 * FakeRadio shadow data.
 * @ingroup render
 */
struct shadowcorner_t
{
    float corner;
    Sector *proximity;
    float pOffset;
    float pHeight;
};

/**
 * FakeRadio connected edge data.
 * @ingroup render
 */
struct edgespan_t
{
    float length;
    float shift;
};

/**
 * Stores the FakeRadio properties of a Line::Side.
 * @ingroup render
 */
struct LineSideRadioData
{
    /// Frame number of last update
    int updateCount;

    shadowcorner_t topCorners[2];
    shadowcorner_t bottomCorners[2];
    shadowcorner_t sideCorners[2];

    /// [left, right]
    edgespan_t spans[2];
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
bool Rend_RadioLineCastsShadow(Line const &line);

/**
 * Returns @c true iff @a plane qualifies for (wall) shadow casting.
 */
bool Rend_RadioPlaneCastsShadow(Plane const &plane);

/**
 * Returns the FakeRadio data for the specified line @a side.
 */
LineSideRadioData &Rend_RadioDataForLineSide(Line::Side &side);

/**
 * To be called to update the shadow properties for the specified line @a side.
 */
void Rend_RadioUpdateForLineSide(Line::Side &side);

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
 * Assumes that light level adaptation has @em NOT yet been applied (it will be).
 */
float Rend_RadioCalcShadowDarkness(float lightLevel);

/**
 * Render FakeRadio for the specified wall section. Generates and then draws all
 * shadow geometry for the wall section.
 *
 * Note that unlike Rend_RadioBspLeafEdges() there is no guard to ensure shadow
 * geometry is rendered only once per frame.
 *
 * @param leftEdge    Geometry for the left edge of the wall section.
 * @param rightEdge   Geometry for the right edge of the wall section.
 * @param shadowDark  Shadow darkness scale factor.
 * @param shadowSize  Shadow size scale factor.
 * @param frontSec    @todo refactor away
 * @param backSec     @todo refactor away
 */
void Rend_RadioWallSection(SectionEdge const &leftEdge, SectionEdge const &rightEdge,
    float shadowDark, float shadowSize, Sector *frontSec, Sector *backSec);

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

#endif // DENG_RENDER_FAKERADIO
