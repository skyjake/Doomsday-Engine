/** @file rend_clip.h Angle Clipper (clipnodes and oranges).
 *
 * @ingroup render
 *
 * The idea is to keep track of occluded angles around the camera.
 * Since BSP leafs are rendered front-to-back, the occlusion lists
 * start a frame empty and eventually fill up to cover the whole 360
 * degrees around the camera.
 *
 * Oranges (occlusion ranges) clip a half-space on an angle range.
 * These are produced by horizontal edges that have empty space behind.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_RENDER_ANGLE_CLIPPER
#define DENG_RENDER_ANGLE_CLIPPER

#include <de/binangle.h>

#include <de/Vector>

class BspLeaf;

DENG_EXTERN_C int devNoCulling;

void C_Init();

/**
 * @return  Non-zero if clipnodes cover the whole range [0..360] degrees.
 */
int C_IsFull();

void C_ClearRanges();

int C_SafeAddRange(binangle_t startAngle, binangle_t endAngle);

/**
 * @return  Non-zero if the point is visible after checking both the clipnodes
 *          and the occlusion planes.
 */
int C_IsPointVisible(coord_t x, coord_t y, coord_t height);

/**
 * @return  Non-zero if the angle is visible after checking both the clipnodes
 *          and the occlusion planes.
 */
int C_IsAngleVisible(binangle_t bang);

/**
 * @return  Non-zero if @a bspLeaf might be visible, else @c 0.
 */
int C_CheckBspLeaf(BspLeaf &bspLeaf);

/**
 * Add a segment relative to the current viewpoint.
 */
void C_AddRangeFromViewRelPoints(coord_t const from[2], coord_t const to[2]);

inline void C_AddRangeFromViewRelPoints(de::Vector2d const &from, de::Vector2d const &to)
{
    coord_t fromV1[2] = { from.x, from.y };
    coord_t toV1[2]   = {   to.x,   to.y };
    C_AddRangeFromViewRelPoints(fromV1, toV1);
}

/**
 * Add an occlusion segment relative to the current viewpoint.
 */
void C_AddViewRelOcclusion(coord_t const from[2], coord_t const to[2],
                           coord_t height, bool tophalf);

inline void C_AddViewRelOcclusion(de::Vector2d const &from, de::Vector2d const &to,
                                  coord_t height, bool tophalf)
{
    coord_t fromV1[2] = { from.x, from.y };
    coord_t toV1[2]   = {   to.x,   to.y };
    C_AddViewRelOcclusion(fromV1, toV1, height, tophalf);
}

/**
 * Check a segment relative to the current viewpoint.
 */
int C_CheckRangeFromViewRelPoints(coord_t const from[2], coord_t const to[2]);

inline int C_CheckRangeFromViewRelPoints(de::Vector2d const &from, de::Vector2d const &to)
{
    coord_t fromV1[2] = { from.x, from.y };
    coord_t toV1[2]   = {   to.x,   to.y };
    return C_CheckRangeFromViewRelPoints(fromV1, toV1);
}

#ifdef DENG_DEBUG
/**
 * A debugging aid: checks if clipnode links are valid.
 */
void C_Ranger();
#endif

#endif // DENG_RENDER_ANGLE_CLIPPER
