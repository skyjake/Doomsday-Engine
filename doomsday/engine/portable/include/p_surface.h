/**\file p_surface.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
int Surface_GetProperty(const surface_t* suf, setargs_t* args);

/**
 * Update the surface, property is selected by DMU_* name.
 */
 int Surface_SetProperty(surface_t* suf, const setargs_t* args);

void            Surface_Update(surface_t* suf);

boolean         Surface_IsAttachedToMap(surface_t* suf);
boolean         Surface_SetMaterial(surface_t* suf, material_t* mat);
boolean         Surface_SetMaterialOffsetX(surface_t* suf, float x);
boolean         Surface_SetMaterialOffsetY(surface_t* suf, float y);
boolean         Surface_SetMaterialOffsetXY(surface_t* suf, float x, float y);
boolean         Surface_SetColorR(surface_t* suf, float r);
boolean         Surface_SetColorG(surface_t* suf, float g);
boolean         Surface_SetColorB(surface_t* suf, float b);
boolean         Surface_SetColorA(surface_t* suf, float a);
boolean         Surface_SetColorRGBA(surface_t* suf, float r, float g, float b, float a);
boolean         Surface_SetBlendMode(surface_t* suf, blendmode_t blendMode);

#endif /* LIBDENG_MAP_SURFACE_H */
