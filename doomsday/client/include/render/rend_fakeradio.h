/** @file rend_fakeradio.h  Faked Radiosity Lighting.
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

#ifndef DENG_CLIENT_RENDER_FAKERADIO
#define DENG_CLIENT_RENDER_FAKERADIO

#include "Line"
#include "Sector"
#include "Vertex"

#include "WallEdge"

#include "gl/gl_texmanager.h"     // lightingtexid_t
#include "render/rendersystem.h"

class ConvexSubspace;

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
 * Stores the FakeRadio properties of a LineSide.
 * @ingroup render
 */
struct LineSideRadioData
{
    /// Frame number of last update
    int updateCount;

    shadowcorner_t topCorners[2];
    shadowcorner_t bottomCorners[2];
    shadowcorner_t sideCorners[2];

    /// [bottom, top]
    edgespan_t spans[2];
};

DENG2_EXTERN_C int rendFakeRadio; ///< cvar

/**
 * Register the console commands, variables, etc..., of this module.
 */
void Rend_RadioRegister();

/**
 * To be called after map load to perform necessary initialization within this module.
 */
void Rend_RadioInitForMap(de::Map &map);

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
LineSideRadioData &Rend_RadioDataForLineSide(LineSide &side);

/**
 * To be called to update the shadow properties for the specified line @a side.
 */
void Rend_RadioUpdateForLineSide(LineSide &side);

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
 * @param leftSection        Geometry for the left wall section edge.
 * @param rightSection       Geometry for the right wall section edge.
 * @param ambientLightColor  Ambient light values for the wall section. This is
 *                           @em not automatically taken from the sector on the
 *                           front side of the wall as various map-hacks dictate
 *                           otherwise.
 */
void prepareAllWallFakeradioShards(de::WallEdgeSection const &leftSection,
    de::WallEdgeSection const &rightSection, de::Vector4f const &ambientLightColor);

/**
 * Render FakeRadio for the given subspace. Draws all shadow geometry linked to
 * the ConvexSubspace, that has not already been rendered.
 */
void Rend_RadioSubspaceEdges(ConvexSubspace const &subspace);

struct rendershadowseg_params_t
{
    lightingtexid_t texture;
    bool horizontal;
    float shadowMul;
    float shadowDark;
    de::Vector2f texOrigin;
    de::Vector2f texDimensions;
    float sectionWidth;

    void setupForTop(float shadowSize, float shadowDark, coord_t top,
        coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil,
        LineSideRadioData const &frData);

    void setupForBottom(float shadowSize, float shadowDark, coord_t top,
        coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil,
        LineSideRadioData const &frData);

    void setupForSide(float shadowSize, float shadowDark, coord_t bottom, coord_t top,
        int rightSide, bool haveBottomShadower, bool haveTopShadower,
        coord_t xOffset, coord_t sectionWidth, coord_t fFloor, coord_t fCeil,
        bool hasBackSector, coord_t bFloor, coord_t bCeil,
        coord_t lineLength, LineSideRadioData const &frData);
};

/**
 * Render the shadow poly vertices, for debug.
 */
#ifdef DENG_DEBUG
void Rend_DrawShadowOffsetVerts();
#endif

#endif // DENG_CLIENT_RENDER_FAKERADIO
