/** @file rend_fakeradio.h  Geometry generation for faked, radiosity lighting.
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
 * @authors Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_RENDER_FAKERADIO
#define DE_CLIENT_RENDER_FAKERADIO

#ifdef __SERVER__
#  error "rend_fakeradio.h is only for the Client"
#endif

#include <de/libcore.h>

class ConvexSubspace;
class WallEdge;
class Plane;

DE_EXTERN_C int     rendFakeRadio;
DE_EXTERN_C uint8_t devFakeRadioUpdate;

/**
 * Render FakeRadio for the specified wall section. Generates and then draws all shadow geometry
 * for the wall section.
 *
 * Note that unlike Rend_DrawFlatRadio() there is no guard to ensure shadow geometry is rendered
 * only once per frame.
 *
 * @param leftEdge      Geometry for the left edge of the wall section.
 * @param rightEdge     Geometry for the right edge of the wall section.
 * @param ambientLight  Ambient light level/luminosity.
 */
void Rend_DrawWallRadio(const WallEdge &leftEdge, const WallEdge &rightEdge, float ambientLight);

/**
 * Render FakeRadio for the given subspace. Draws all shadow geometry linked to the ConvexSubspace,
 * that has not already been rendered.
 */
void Rend_DrawFlatRadio(const ConvexSubspace &subspace);

/**
 * Register the console commands, variables, etc..., of this module.
 */
void Rend_RadioRegister();

#endif  // DE_CLIENT_RENDER_FAKERADIO
