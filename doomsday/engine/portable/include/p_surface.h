/**\file p_surface.h
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
int Surface_GetProperty(const surface_t* surface, setargs_t* args);

/**
 * Update the surface, property is selected by DMU_* name.
 */
int Surface_SetProperty(surface_t* surface, const setargs_t* args);

/**
 * Mark the surface as requiring a full update. To be called
 * during engine reset.
 */
void Surface_Update(surface_t* surface);

/// @return @c true= Surface is drawable (i.e., a drawable Material is bound).
boolean Surface_IsDrawable(surface_t* surface);

/// @return @c true= Surface is owned by some element of the Map geometry.
boolean Surface_AttachedToMap(surface_t* surface);

/**
 * Change Material bound to this Surface.
 * @param mat  New Material.
 */
boolean Surface_SetMaterial(surface_t* surface, material_t* material);

/**
 * Change Material origin.
 * @param x  New X origin in map space.
 * @param y  New Y origin in map space.
 */
boolean Surface_SetMaterialOrigin(surface_t* surface, float x, float y);

/**
 * Change Material origin X coordinate.
 * @param x  New X origin in map space.
 */
boolean Surface_SetMaterialOriginX(surface_t* surface, float x);

/**
 * Change Material origin Y coordinate.
 * @param y  New Y origin in map space.
 */
boolean Surface_SetMaterialOriginY(surface_t* surface, float y);

boolean Surface_SetColorAndAlpha(surface_t* surface, float red, float green, float blue, float alpha);

boolean Surface_SetColorRed(surface_t* surface, float red);
boolean Surface_SetColorGreen(surface_t* surface, float green);
boolean Surface_SetColorBlue(surface_t* surface, float blue);

boolean Surface_SetAlpha(surface_t* surface, float alpha);

/**
 * Change blendmode.
 * @param blendMode  New blendmode.
 */
boolean Surface_SetBlendMode(surface_t* surface, blendmode_t blendMode);

#endif /* LIBDENG_MAP_SURFACE_H */
