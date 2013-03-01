/** @file r_lgrid.h
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Smoothed ambient sector lighting.
 */

#ifndef LIBDENG_REFRESH_LIGHT_GRID_H
#define LIBDENG_REFRESH_LIGHT_GRID_H

#include "dd_types.h"
#include "map/p_mapdata.h"

#ifdef __cplusplus
extern "C" {
#endif

void LG_Register(void);
void LG_InitForMap(void);
void LG_Update(void);

/**
 * Called when a setting is changed which affects the lightgrid.
 */
void LG_MarkAllForUpdate(void);

void LG_SectorChanged(Sector* sector);

/**
 * Calculate the light color for a 3D point in the world.
 *
 * @param point      3D point.
 * @param destColor  Evaluated color of the point (return value).
 */
void LG_Evaluate(coord_t const point[3], float destColor[3]);

/**
 * Calculate the light level for a 3D point in the world.
 *
 * @param point  3D point.
 * @return  Evaluated light level of the point.
 */
float LG_EvaluateLightLevel(coord_t const point[3]);

void LG_Debug(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG_REFRESH_LIGHT_GRID_H
