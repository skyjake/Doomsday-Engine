/**\file surface.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

/**
 * Map surface.
 */

#ifndef LIBDENG_MAP_SURFACE_H
#define LIBDENG_MAP_SURFACE_H

#include "r_data.h"
#include "p_dmu.h"

/**
 * Get the value of a surface property, selected by DMU_* name.
 */
int Surface_GetProperty(const Surface* surface, setargs_t* args);

/**
 * Update the surface, property is selected by DMU_* name.
 */
int Surface_SetProperty(Surface* surface, const setargs_t* args);

/**
 * Mark the surface as requiring a full update. To be called
 * during engine reset.
 */
void Surface_Update(Surface* surface);

/// @return @c true= Surface is drawable (i.e., a drawable Material is bound).
boolean Surface_IsDrawable(Surface* surface);

/// @return @c true= Surface is owned by some element of the Map geometry.
boolean Surface_AttachedToMap(Surface* surface);

/**
 * Change Material bound to this Surface.
 * @param mat  New Material.
 */
boolean Surface_SetMaterial(Surface* surface, material_t* material);

/**
 * Change Material origin.
 * @param x  New X origin in map space.
 * @param y  New Y origin in map space.
 */
boolean Surface_SetMaterialOrigin(Surface* surface, float x, float y);

/**
 * Change Material origin X coordinate.
 * @param x  New X origin in map space.
 */
boolean Surface_SetMaterialOriginX(Surface* surface, float x);

/**
 * Change Material origin Y coordinate.
 * @param y  New Y origin in map space.
 */
boolean Surface_SetMaterialOriginY(Surface* surface, float y);

boolean Surface_SetColorAndAlpha(Surface* surface, float red, float green, float blue, float alpha);

boolean Surface_SetColorRed(Surface* surface, float red);
boolean Surface_SetColorGreen(Surface* surface, float green);
boolean Surface_SetColorBlue(Surface* surface, float blue);

boolean Surface_SetAlpha(Surface* surface, float alpha);

/**
 * Change blendmode.
 * @param blendMode  New blendmode.
 */
boolean Surface_SetBlendMode(Surface* surface, blendmode_t blendMode);

#endif /* LIBDENG_MAP_SURFACE_H */
