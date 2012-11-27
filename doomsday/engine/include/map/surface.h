/**
 * @file surface.h
 * Logical map surface. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_SURFACE
#define LIBDENG_MAP_SURFACE

#include "resource/r_data.h"
#include "map/p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Mark the surface as requiring a full update. To be called
 * during engine reset.
 */
void Surface_Update(Surface* surface);

/**
 * Update the Surface's map space base origin according to relevant points in
 * the owning object.
 *
 * If this surface is owned by a SideDef then the origin is updated using the
 * points defined by the associated LineDef's vertices and the plane heights of
 * the Sector on that SideDef's side.
 *
 * If this surface is owned by a Plane then the origin is updated using the
 * points defined the center of the Plane's Sector (on the XY plane) and the Z
 * height of the plane.
 *
 * If no owner is presently associated this is a no-op.
 *
 * @param surface  Surface instance.
 */
void Surface_UpdateBaseOrigin(Surface* surface);

/// @return @c true= is drawable (i.e., a drawable Material is bound).
boolean Surface_IsDrawable(Surface* surface);

/// @return @c true= is sky-masked (i.e., a sky-masked Material is bound).
boolean Surface_IsSkyMasked(const Surface* suf);

/// @return @c true= is owned by some element of the Map geometry.
boolean Surface_AttachedToMap(Surface* surface);

/**
 * Change Material bound to this surface.
 *
 * @param surface  Surface instance.
 * @param mat  New Material.
 */
boolean Surface_SetMaterial(Surface* surface, material_t* material);

/**
 * Change Material origin.
 *
 * @param surface  Surface instance.
 * @param x  New X origin in map space.
 * @param y  New Y origin in map space.
 */
boolean Surface_SetMaterialOrigin(Surface* surface, float x, float y);

/**
 * Change Material origin X coordinate.
 *
 * @param surface  Surface instance.
 * @param x  New X origin in map space.
 */
boolean Surface_SetMaterialOriginX(Surface* surface, float x);

/**
 * Change Material origin Y coordinate.
 *
 * @param surface  Surface instance.
 * @param y  New Y origin in map space.
 */
boolean Surface_SetMaterialOriginY(Surface* surface, float y);

/**
 * Change surface color tint and alpha.
 * @param surface  Surface instance.
 */
boolean Surface_SetColorAndAlpha(Surface* surface, float red, float green, float blue, float alpha);

/**
 * Change surfacecolor tint.
 *
 * @param surface  Surface instance.
 */
boolean Surface_SetColorRed(Surface* surface, float red);
boolean Surface_SetColorGreen(Surface* surface, float green);
boolean Surface_SetColorBlue(Surface* surface, float blue);

/**
 * Change surface alpha.
 *
 * @param surface  Surface instance.
 * @param alpha  New alpha value.
 */
boolean Surface_SetAlpha(Surface* surface, float alpha);

/**
 * Change blendmode.
 *
 * @param surface  Surface instance.
 * @param blendMode  New blendmode.
 */
boolean Surface_SetBlendMode(Surface* surface, blendmode_t blendMode);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param surface  Surface instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Surface_GetProperty(const Surface* surface, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param surface  Surface instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int Surface_SetProperty(Surface* surface, const setargs_t* args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_SURFACE
