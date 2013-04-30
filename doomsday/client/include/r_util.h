/** @file r_util.h Refresh Utility Routines.
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

#ifndef DENG_REFRESH_UTIL_H
#define DENG_REFRESH_UTIL_H

#include "api_gl.h"

#include <de/vector1.h>
#include <de/Vector>

/**
 * Get a global angle from Cartesian coordinates relative to the viewer.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Angle between the test point and view x,y.
 */
angle_t R_ViewPointXYToAngle(coord_t x, coord_t y);

/**
 * Determine distance to the specified point relative to the viewer.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Distance from the viewer to the test point.
 */
coord_t R_ViewPointXYDistance(coord_t x, coord_t y);

void R_ProjectViewRelativeLine2D(coord_t const center[2], boolean alignToViewPlane,
    coord_t width, coord_t offset, coord_t start[2], coord_t end[2]);

/**
 * Scale @a color uniformly so that the highest component becomes one.
 */
void R_AmplifyColor(de::Vector3f &color);

void R_ScaleAmbientRGB(float *out, float const *in, float mul);

/**
 * Generate texcoords on the surface centered on point.
 *
 * @param s  Texture s coords written back here.
 * @param t  Texture t coords written back here.
 * @param point  Point on surface around which texture is centered.
 * @param xScale  Scale multiplier on the horizontal axis.
 * @param yScale  Scale multiplier on the vertical axis.
 * @param v1  Top left vertex of the surface being projected on.
 * @param v2  Bottom right vertex of the surface being projected on.
 * @param tangent  Tangent vector of the surface being projected on.
 * @param bitangent  Bitangent vector of the surface being projected on.
 *
 * @return  @c true if the generated coords are within bounds.
 */
bool R_GenerateTexCoords(pvec2f_t s, pvec2f_t t, const_pvec3d_t point,
    float xScale, float yScale, const_pvec3d_t v1, const_pvec3d_t v2,
    const_pvec3f_t tangent, const_pvec3f_t bitangent);

char const *R_NameForBlendMode(blendmode_t mode);

#endif // DENG_REFRESH_UTIL_H
