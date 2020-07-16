/** @file rendpoly.h RendPoly data buffers
 * @ingroup render
 *
 * @author Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_RENDER_RENDPOLY_H
#define DE_RENDER_RENDPOLY_H

#include <de/liblegacy.h>
#include <de/vector.h>

DE_EXTERN_C byte rendInfoRPolys;

void R_PrintRendPoolInfo();

/**
 * @note Should be called at the start of each map.
 */
void R_InitRendPolyPools();

/**
 * Allocate a new contiguous range of position coordinates from the specialized
 * write-geometry pool
 *
 * @param num  The number of coordinate sets required.
 */
de::Vec3f *R_AllocRendVertices(uint num);

/**
 * Allocate a new contiguous range of color coordinates from the specialized
 * write-geometry pool
 *
 * @param num  The number of coordinate sets required.
 */
de::Vec4f *R_AllocRendColors(uint num);

/**
 * Allocate a new contiguous range of texture coordinates from the specialized
 * write-geometry pool
 *
 * @param num  The number of coordinate sets required.
 */
de::Vec2f *R_AllocRendTexCoords(uint num);

/**
 * @note Doesn't actually free anything (storage is pooled for reuse).
 *
 * @param posCoords  Position coordinates to mark unused.
 */
void R_FreeRendVertices(de::Vec3f *posCoords);

/**
 * @note Doesn't actually free anything (storage is pooled for reuse).
 *
 * @param colorCoords  Color coordinates to mark unused.
 */
void R_FreeRendColors(de::Vec4f *colorCoords);

/**
 * @note Doesn't actually free anything (storage is pooled for reuse).
 *
 * @param texCoords  Texture coordinates to mark unused.
 */
void R_FreeRendTexCoords(de::Vec2f *texCoords);

#endif // DE_RENDER_RENDPOLY_H
